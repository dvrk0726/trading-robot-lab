#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

#include "moex_spectra_pipeline/ordered_decode.hpp"
#include "moex_fast/spectra_gate_a.hpp"
#include "moex_raw/raw_ab_replay_cursor.hpp"

namespace moex_spectra_pipeline {

// --- Pipeline state ---

enum class NormalPipelineState : std::uint8_t {
    Uninitialized,
    Ready,
    End,
    Failed
};

// --- Result codes ---

enum class NormalPipelineCode : std::uint8_t {
    Ok,
    End,
    NotInitialized,
    AlreadyInitialized,
    InvalidConfig,
    ReplayFailed,
    FramingFailed,
    SequencerFailed,
    DecodeFailed,
    ReplayEndedWithPendingGap,
    InternalInvariantViolation
};

// --- Pipeline configuration ---

struct NormalPipelineConfig {
    moex_raw::ClockMergeContract clock_contract = moex_raw::ClockMergeContract::SharedMonotonicTimeline;
    moex::spectra::FramingLimits framing{};
    moex::spectra::SequencerConfig sequencer;
    moex_fast::DecodeLimits decode_limits;
    std::uint32_t initial_expected_seq = 1;
};

// --- Callback type: plain function pointer, non-null; context may be null ---

using NormalMessageCallback = void (*)(void* context, OrderedDecodedMessage&& message);

// --- Init result (fail-closed: default code = InvalidConfig) ---

struct NormalPipelineInitResult {
    NormalPipelineCode code = NormalPipelineCode::InvalidConfig;
    moex_raw::AbReplayCode replay_code = moex_raw::AbReplayCode::Ok;
    moex::spectra::FrameCode frame_code = moex::spectra::FrameCode::Ok;
    moex::spectra::SequencerCode sequencer_code = moex::spectra::SequencerCode::NoAction;
    OrderedDecodeCode decode_code = OrderedDecodeCode::Ok;
};

// --- Run result (fail-closed: default code = NotInitialized) ---

struct NormalPipelineRunResult {
    NormalPipelineCode code = NormalPipelineCode::NotInitialized;
    moex_raw::AbReplayCode replay_code = moex_raw::AbReplayCode::Ok;
    moex::spectra::FrameCode frame_code = moex::spectra::FrameCode::Ok;
    moex::spectra::SequencerCode sequencer_code = moex::spectra::SequencerCode::NoAction;
    OrderedDecodeCode decode_code = OrderedDecodeCode::Ok;
    std::uint64_t input_packets = 0;
    std::uint64_t emitted_messages = 0;
};

// --- Normal replay pipeline (move-only, PImpl) ---

class NormalReplayPipeline {
public:
    NormalReplayPipeline();
    ~NormalReplayPipeline();

    NormalReplayPipeline(const NormalReplayPipeline&) = delete;
    NormalReplayPipeline& operator=(const NormalReplayPipeline&) = delete;
    NormalReplayPipeline(NormalReplayPipeline&&) noexcept;
    NormalReplayPipeline& operator=(NormalReplayPipeline&&) noexcept;

    [[nodiscard]] NormalPipelineInitResult initialize(
        const moex_raw::StreamSetInfo& first,
        const moex_raw::StreamSetInfo& second,
        const moex_fast::CompiledTemplateSet& templates,
        const NormalPipelineConfig& config,
        NormalMessageCallback callback,
        void* callback_context = nullptr,
        moex_raw::IFileSystem* first_fs = nullptr,
        moex_raw::IFileSystem* second_fs = nullptr
    );

    [[nodiscard]] NormalPipelineRunResult run_to_end();

    [[nodiscard]] NormalPipelineState state() const noexcept;
    [[nodiscard]] NormalPipelineCode terminal_code() const noexcept;

private:
    friend struct NormalReplayPipelineTestAccess;
    static NormalPipelineCode classify_replay_code(moex_raw::AbReplayCode code) noexcept;
    static NormalPipelineCode classify_decode_result(OrderedDecodeCode code, bool has_message) noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace moex_spectra_pipeline
