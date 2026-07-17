#include "moex_spectra_pipeline/normal_replay_pipeline.hpp"

#include <cstring>
#include <limits>
#include <vector>

namespace moex_spectra_pipeline {

// ---------------------------------------------------------------------------
// PImpl
// ---------------------------------------------------------------------------

struct NormalReplayPipeline::Impl {
    NormalPipelineState state_ = NormalPipelineState::Uninitialized;
    NormalPipelineCode terminal_code_ = NormalPipelineCode::Ok;

    NormalMessageCallback callback_ = nullptr;
    void* callback_context_ = nullptr;
    NormalPipelineConfig config_{};

    moex_raw::ValidatedAbReplayCursor replay_;
    OrderedDecodeSession decoder_;
    moex::spectra::DualFeedSequencer sequencer_;
    moex::spectra::MessageStorage storage_;

    std::vector<moex::spectra::SlotMetadata> slots_;
    std::vector<std::uint8_t> arena_;

    std::uint64_t input_packets_ = 0;
    std::uint64_t emitted_messages_ = 0;

    // Persistent child diagnostics for stable Failed
    moex_raw::AbReplayCode replay_code_ = moex_raw::AbReplayCode::Ok;
    moex::spectra::FrameCode frame_code_ = moex::spectra::FrameCode::Ok;
    moex::spectra::SequencerCode sequencer_code_ = moex::spectra::SequencerCode::NoAction;
    OrderedDecodeCode decode_code_ = OrderedDecodeCode::Ok;
};

// ---------------------------------------------------------------------------
// Special members
// ---------------------------------------------------------------------------

NormalReplayPipeline::NormalReplayPipeline()
    : impl_(std::make_unique<Impl>()) {}

NormalReplayPipeline::~NormalReplayPipeline() = default;

NormalReplayPipeline::NormalReplayPipeline(NormalReplayPipeline&&) noexcept = default;
NormalReplayPipeline& NormalReplayPipeline::operator=(NormalReplayPipeline&&) noexcept = default;

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

NormalPipelineState NormalReplayPipeline::state() const noexcept {
    return impl_->state_;
}

NormalPipelineCode NormalReplayPipeline::terminal_code() const noexcept {
    return impl_->terminal_code_;
}

// ---------------------------------------------------------------------------
// classify_replay_code (table-driven, friend-accessible)
// ---------------------------------------------------------------------------

NormalPipelineCode NormalReplayPipeline::classify_replay_code(
    moex_raw::AbReplayCode code) noexcept {
    switch (code) {
    case moex_raw::AbReplayCode::ValidationFailed:
    case moex_raw::AbReplayCode::IoError:
    case moex_raw::AbReplayCode::StreamChanged:
    case moex_raw::AbReplayCode::ClockRegression:
    case moex_raw::AbReplayCode::InternalInvariantViolation:
        return NormalPipelineCode::ReplayFailed;
    default:
        return NormalPipelineCode::InternalInvariantViolation;
    }
}

// ---------------------------------------------------------------------------
// classify_decode_result (production classifier for sink adapter)
// ---------------------------------------------------------------------------

NormalPipelineCode NormalReplayPipeline::classify_decode_result(
    OrderedDecodeCode code, bool has_message) noexcept {
    if (code != OrderedDecodeCode::Ok)
        return NormalPipelineCode::DecodeFailed;
    return has_message ? NormalPipelineCode::Ok
                       : NormalPipelineCode::InternalInvariantViolation;
}

// ---------------------------------------------------------------------------
// initialize (fully transactional)
// ---------------------------------------------------------------------------

NormalPipelineInitResult NormalReplayPipeline::initialize(
    const moex_raw::StreamSetInfo& first,
    const moex_raw::StreamSetInfo& second,
    const moex_fast::CompiledTemplateSet& templates,
    const NormalPipelineConfig& config,
    NormalMessageCallback callback,
    void* callback_context,
    moex_raw::IFileSystem* first_fs,
    moex_raw::IFileSystem* second_fs
) {
    // State check FIRST: Ready/End/Failed → AlreadyInitialized (no mutation)
    if (impl_->state_ == NormalPipelineState::Ready ||
        impl_->state_ == NormalPipelineState::End ||
        impl_->state_ == NormalPipelineState::Failed) {
        return {NormalPipelineCode::AlreadyInitialized, impl_->replay_code_,
                impl_->frame_code_, impl_->sequencer_code_, impl_->decode_code_};
    }

    // Non-null callback required
    if (!callback) {
        return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                OrderedDecodeCode::Ok};
    }

    // Transactional init: any exception → InternalInvariantViolation, stays Uninitialized
    try {
        auto local = std::make_unique<Impl>();

        // 1. Initialize replay cursor
        auto ab_init = local->replay_.initialize(first, second, config.clock_contract,
                                                 first_fs, second_fs);
        if (ab_init.code != moex_raw::AbReplayCode::Ok) {
            return {NormalPipelineCode::InvalidConfig, ab_init.code,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok};
        }
        if (local->replay_.state() != moex_raw::AbReplayState::Ready) {
            return {NormalPipelineCode::InternalInvariantViolation, local->replay_.terminal_code(),
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok};
        }

        // 2. Get metadata from replay cursor
        const auto* meta_a = local->replay_.metadata(moex_raw::SourceSide::A);
        const auto* meta_b = local->replay_.metadata(moex_raw::SourceSide::B);
        if (!meta_a || !meta_b) {
            return {NormalPipelineCode::InternalInvariantViolation, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok};
        }

        // 3. Validate framing config
        if (config.framing.max_datagram_bytes == 0) {
            return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::InvalidConfig, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok};
        }

        // 4. Initialize decoder session
        auto dec_init = local->decoder_.initialize(templates, *meta_a, *meta_b, config.decode_limits);
        if (dec_init.code != OrderedDecodeCode::Ok) {
            return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    dec_init.code};
        }

        // 5. Validate sequencer/storage config
        const auto slot_count = static_cast<std::size_t>(config.sequencer.storage.max_reorder_messages);
        const auto max_msg_bytes = config.sequencer.storage.max_message_bytes;

        if (slot_count == 0 || max_msg_bytes == 0) {
            return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::InvalidConfig,
                    OrderedDecodeCode::Ok};
        }

        // Multiplication overflow check → InternalInvariantViolation directly
        constexpr std::size_t kSizeMax = (std::numeric_limits<std::size_t>::max)();
        if (max_msg_bytes != 0 && slot_count > kSizeMax / max_msg_bytes) {
            return {NormalPipelineCode::InternalInvariantViolation, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok};
        }

        // 6. Allocate storage
        const std::size_t arena_size = slot_count * max_msg_bytes;
        local->slots_.resize(slot_count);
        local->arena_.resize(arena_size);

        // 7. Initialize storage
        moex::spectra::MessageStorageConfig storage_cfg{};
        storage_cfg.max_reorder_messages = config.sequencer.storage.max_reorder_messages;
        storage_cfg.max_reorder_bytes = config.sequencer.storage.max_reorder_bytes;
        storage_cfg.max_message_bytes = config.sequencer.storage.max_message_bytes;

        auto storage_init = local->storage_.initialize(local->slots_, local->arena_, storage_cfg);
        if (storage_init.code != moex::spectra::StorageInitCode::Ok) {
            return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::InvalidConfig,
                    OrderedDecodeCode::Ok};
        }

        // 8. Initialize sequencer (preserve exact code)
        auto seq_init = local->sequencer_.initialize(config.sequencer.logical_feed,
                                                      config.sequencer,
                                                      local->storage_);
        if (seq_init != moex::spectra::SequencerCode::Initialized) {
            return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, seq_init, OrderedDecodeCode::Ok};
        }

        // 9. Start sequencer (preserve exact code, no InvalidTransition remapping)
        auto start_rc = local->sequencer_.start(config.initial_expected_seq);
        if (start_rc != moex::spectra::SequencerCode::Started) {
            return {NormalPipelineCode::InvalidConfig, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, start_rc, OrderedDecodeCode::Ok};
        }

        // 10. Commit to impl_
        local->callback_ = callback;
        local->callback_context_ = callback_context;
        local->config_ = config;
        local->state_ = NormalPipelineState::Ready;
        local->terminal_code_ = NormalPipelineCode::Ok;

        impl_ = std::move(local);

        return {NormalPipelineCode::Ok, moex_raw::AbReplayCode::Ok,
                moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                OrderedDecodeCode::Ok};
    } catch (...) {
        return {NormalPipelineCode::InternalInvariantViolation, moex_raw::AbReplayCode::Ok,
                moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                OrderedDecodeCode::Ok};
    }
}

// ---------------------------------------------------------------------------
// run_to_end
// ---------------------------------------------------------------------------

NormalPipelineRunResult NormalReplayPipeline::run_to_end() {
    // Uninitialized
    if (impl_->state_ == NormalPipelineState::Uninitialized) {
        return {NormalPipelineCode::NotInitialized, moex_raw::AbReplayCode::Ok,
                moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                OrderedDecodeCode::Ok, 0, 0};
    }

    // Stable End
    if (impl_->state_ == NormalPipelineState::End) {
        return {NormalPipelineCode::End, impl_->replay_code_, impl_->frame_code_,
                impl_->sequencer_code_, impl_->decode_code_,
                impl_->input_packets_, impl_->emitted_messages_};
    }

    // Stable Failed
    if (impl_->state_ == NormalPipelineState::Failed) {
        return {impl_->terminal_code_, impl_->replay_code_, impl_->frame_code_,
                impl_->sequencer_code_, impl_->decode_code_,
                impl_->input_packets_, impl_->emitted_messages_};
    }

    // Ready: main loop
    // Sink adapter for sequencer -> decoder -> callback
    struct SinkAdapter {
        Impl* impl;
        bool decode_error = false;
        bool callback_threw = false;
        bool ok_without_message = false;
        OrderedDecodeCode last_decode_code = OrderedDecodeCode::Ok;

        void operator()(const moex::spectra::OrderedMessageMetadata& meta,
                        std::span<const std::uint8_t> fast_body) noexcept {
            if (decode_error || callback_threw || ok_without_message) return;
            try {
                auto dr = impl->decoder_.decode_ordered(
                    {meta.msg_seq_num, meta.side, meta.capture_index, meta.capture_monotonic_ns},
                    fast_body);

                auto classification = NormalReplayPipeline::classify_decode_result(
                    dr.code, dr.decoded_message.has_value());

                if (classification == NormalPipelineCode::Ok) {
                    impl->callback_(impl->callback_context_, std::move(*dr.decoded_message));
                    impl->emitted_messages_++;
                } else if (classification == NormalPipelineCode::InternalInvariantViolation) {
                    ok_without_message = true;
                } else {
                    decode_error = true;
                    last_decode_code = dr.code;
                }
            } catch (...) {
                callback_threw = true;
            }
        }
    };

    SinkAdapter sink{impl_.get()};
    moex_raw::AbReplayCode replay_code = moex_raw::AbReplayCode::Ok;

    // Main replay loop
    while (true) {
        auto rr = impl_->replay_.next();
        replay_code = rr.code;

        if (rr.code != moex_raw::AbReplayCode::Ok) break;

        impl_->input_packets_++;

        // Map SourceSide -> FeedSide
        moex::spectra::FeedSide side{};
        if (rr.source_side == moex_raw::SourceSide::A) {
            side = moex::spectra::FeedSide::A;
        } else if (rr.source_side == moex_raw::SourceSide::B) {
            side = moex::spectra::FeedSide::B;
        } else {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::InternalInvariantViolation;
            impl_->replay_code_ = moex_raw::AbReplayCode::InternalInvariantViolation;
            return {NormalPipelineCode::InternalInvariantViolation,
                    moex_raw::AbReplayCode::InternalInvariantViolation,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }

        // Frame UDP message
        moex::spectra::DatagramView dgram{};
        dgram.feed = impl_->config_.sequencer.logical_feed;
        dgram.side = side;
        dgram.capture_index = rr.record.capture_index;
        dgram.capture_monotonic_ns = rr.record.capture_monotonic_ns;
        dgram.payload = rr.record.payload;

        moex::spectra::FramedMessageView framed{};
        auto fr = moex::spectra::frame_udp_message(dgram, impl_->config_.framing, framed);
        if (fr.code != moex::spectra::FrameCode::Ok) {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::FramingFailed;
            impl_->frame_code_ = fr.code;
            return {NormalPipelineCode::FramingFailed, moex_raw::AbReplayCode::Ok,
                    fr.code, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }

        // Pass to sequencer
        auto sr = impl_->sequencer_.on_message(framed, rr.record.capture_monotonic_ns, sink);

        // Check sink adapter first
        if (sink.callback_threw) {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::InternalInvariantViolation;
            impl_->decode_code_ = OrderedDecodeCode::InternalInvariantViolation;
            return {NormalPipelineCode::InternalInvariantViolation,
                    moex_raw::AbReplayCode::Ok, moex::spectra::FrameCode::Ok,
                    moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::InternalInvariantViolation,
                    impl_->input_packets_, impl_->emitted_messages_};
        }
        if (sink.ok_without_message) {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::InternalInvariantViolation;
            impl_->decode_code_ = OrderedDecodeCode::Ok;
            return {NormalPipelineCode::InternalInvariantViolation,
                    moex_raw::AbReplayCode::Ok, moex::spectra::FrameCode::Ok,
                    moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }
        if (sink.decode_error) {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::DecodeFailed;
            impl_->decode_code_ = sink.last_decode_code;
            return {NormalPipelineCode::DecodeFailed, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    sink.last_decode_code,
                    impl_->input_packets_, impl_->emitted_messages_};
        }

        // Classify sequencer result
        using SC = moex::spectra::SequencerCode;
        switch (sr.code) {
        case SC::NoAction:
        case SC::Emitted:
        case SC::DuplicateDropped:
        case SC::BufferedOutOfOrder:
        case SC::GapWaiting:
            break;
        default: {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::SequencerFailed;
            impl_->sequencer_code_ = sr.code;
            return {NormalPipelineCode::SequencerFailed, moex_raw::AbReplayCode::Ok,
                    moex::spectra::FrameCode::Ok, sr.code, OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }
        }
    }

    // Handle end of replay
    impl_->replay_code_ = replay_code;

    if (replay_code == moex_raw::AbReplayCode::End) {
        auto seq_state = impl_->sequencer_.state();
        auto pending = impl_->storage_.pending_count();

        // Check FailedClosed FIRST
        if (seq_state == moex::spectra::SequencerState::FailedClosed) {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::SequencerFailed;
            impl_->sequencer_code_ = moex::spectra::SequencerCode::FailedClosed;
            return {NormalPipelineCode::SequencerFailed, moex_raw::AbReplayCode::End,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::FailedClosed,
                    OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }

        if (seq_state == moex::spectra::SequencerState::Running && pending == 0) {
            impl_->state_ = NormalPipelineState::End;
            impl_->terminal_code_ = NormalPipelineCode::End;
            return {NormalPipelineCode::End, moex_raw::AbReplayCode::End,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }

        if (seq_state == moex::spectra::SequencerState::GapWait || pending != 0) {
            impl_->state_ = NormalPipelineState::Failed;
            impl_->terminal_code_ = NormalPipelineCode::ReplayEndedWithPendingGap;
            return {NormalPipelineCode::ReplayEndedWithPendingGap, moex_raw::AbReplayCode::End,
                    moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                    OrderedDecodeCode::Ok,
                    impl_->input_packets_, impl_->emitted_messages_};
        }

        impl_->state_ = NormalPipelineState::Failed;
        impl_->terminal_code_ = NormalPipelineCode::InternalInvariantViolation;
        return {NormalPipelineCode::InternalInvariantViolation, moex_raw::AbReplayCode::End,
                moex::spectra::FrameCode::Ok, moex::spectra::SequencerCode::NoAction,
                OrderedDecodeCode::Ok,
                impl_->input_packets_, impl_->emitted_messages_};
    }

    // Non-End replay codes
    NormalPipelineCode code = classify_replay_code(replay_code);

    impl_->state_ = NormalPipelineState::Failed;
    impl_->terminal_code_ = code;
    return {code, replay_code, moex::spectra::FrameCode::Ok,
            moex::spectra::SequencerCode::NoAction, OrderedDecodeCode::Ok,
            impl_->input_packets_, impl_->emitted_messages_};
}

}  // namespace moex_spectra_pipeline
