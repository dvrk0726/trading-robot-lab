#include "moex_spectra_pipeline/sequence_reset_probe.hpp"

#include <cstring>

namespace moex_spectra_pipeline {

// ---------------------------------------------------------------------------
// RT-3 hard ceilings
// ---------------------------------------------------------------------------

static constexpr std::size_t kCeilingMessageBytes = 1024 * 1024;
static constexpr std::size_t kCeilingPresenceMapBytes = 64;
static constexpr std::uint32_t kCeilingSequenceEntries = 100000;
static constexpr std::uint32_t kCeilingTotalNodes = 1000000;
static constexpr std::size_t kCeilingStringBytes = 1024 * 1024;

static constexpr std::uint32_t kSequenceResetTemplateId = 7;
static constexpr std::int32_t kTagMsgSeqNum = 34;
static constexpr std::int32_t kTagMsgType = 35;
static constexpr std::int32_t kTagNewSeqNo = 36;
static constexpr std::string kSequenceResetMsgType = "4";

// ---------------------------------------------------------------------------
// Header parse result (internal)
// ---------------------------------------------------------------------------

struct HeaderParseResult {
    moex_fast::DecodeStatus status = moex_fast::DecodeStatus::InternalError;
    bool template_id_present = false;
    std::uint32_t template_id = 0;
    std::size_t header_bytes = 0;  // bytes consumed by pmap + template-id
};

// ---------------------------------------------------------------------------
// Minimal FAST header parser (bounded, allocation-free)
// ---------------------------------------------------------------------------

static HeaderParseResult parse_fast_header(
    const std::uint8_t* data, std::size_t size, std::size_t max_pmap_bytes
) noexcept {
    HeaderParseResult result;

    if (size == 0) {
        result.status = moex_fast::DecodeStatus::NeedMoreData;
        return result;
    }

    // 1. Scan pmap bytes for stop bit
    std::size_t pmap_byte_count = 0;
    bool stop_found = false;
    bool first_data_bit = false;
    bool got_first_data = false;

    for (std::size_t i = 0; i < size && i < max_pmap_bytes; ++i) {
        std::uint8_t b = data[i];
        ++pmap_byte_count;

        // First iteration: extract bit 6 (template-ID-present) from first byte
        if (!got_first_data) {
            first_data_bit = (b & 0x40) != 0;
            got_first_data = true;
        }

        if (b & 0x80) {
            stop_found = true;
            break;
        }
    }

    if (!stop_found) {
        if (pmap_byte_count >= max_pmap_bytes) {
            result.status = moex_fast::DecodeStatus::LimitExceeded;
        } else {
            result.status = moex_fast::DecodeStatus::NeedMoreData;
        }
        return result;
    }

    // 2. Non-canonical multi-byte pmap check (FAST 1.1 R7)
    // If pmap is multi-byte, the terminating 7-bit group must have at least one
    // data bit set. The terminating byte is the last byte (the one with stop bit).
    // Its data bits are bits 6..0. If all zero, it's non-canonical.
    if (pmap_byte_count > 1) {
        std::uint8_t last_byte = data[pmap_byte_count - 1];
        if ((last_byte & 0x7F) == 0) {
            result.status = moex_fast::DecodeStatus::NonCanonicalEncoding;
            return result;
        }
    }

    // 3. Template-ID-present bit
    result.template_id_present = first_data_bit;

    // 4. If template-id present, read stop-bit u32
    if (result.template_id_present) {
        std::size_t pos = pmap_byte_count;
        std::uint32_t tid = 0;

        // Manually decode stop-bit u32 from remaining bytes
        std::uint32_t value = 0;
        std::size_t bytes_read = 0;
        constexpr std::size_t kMaxStopBitBytes = 5; // u32: max 5 bytes

        for (std::size_t i = 0; i < kMaxStopBitBytes; ++i) {
            if (pos + i >= size) {
                result.status = moex_fast::DecodeStatus::NeedMoreData;
                return result;
            }
            std::uint8_t b = data[pos + i];
            ++bytes_read;

            // Overflow check: for u32, max 5 bytes, max value bits = 35
            if (i == 4) {
                // 5th byte: only bits 0..3 are valid data (4 bits)
                if (b & 0xF0) {
                    result.status = moex_fast::DecodeStatus::IntegerOverflow;
                    return result;
                }
            }

            value = (value << 7) | static_cast<std::uint32_t>(b & 0x7F);

            if (b & 0x80) {
                // Check for non-canonical leading zeros: if we have more than 1 byte
                // and the first byte is zero, it's non-canonical
                if (bytes_read > 1 && data[pos] == 0) {
                    result.status = moex_fast::DecodeStatus::NonCanonicalEncoding;
                    return result;
                }
                tid = value;
                result.template_id = tid;
                result.header_bytes = pmap_byte_count + bytes_read;
                result.status = moex_fast::DecodeStatus::Ok;
                return result;
            }
        }

        // Exceeded max stop-bit bytes without stop bit
        result.status = moex_fast::DecodeStatus::NonCanonicalEncoding;
        return result;
    }

    // No template-id: header is just pmap
    result.header_bytes = pmap_byte_count;
    result.status = moex_fast::DecodeStatus::Ok;
    return result;
}

// ---------------------------------------------------------------------------
// PImpl
// ---------------------------------------------------------------------------

struct SequenceResetProbe::Impl {
    SequenceResetProbeState state_ = SequenceResetProbeState::Uninitialized;
    SequenceResetProbeCode terminal_code_ = SequenceResetProbeCode::InvalidConfig;
    moex_fast::CompiledTemplateSet compiled_;
    std::unique_ptr<moex_fast::DecoderSession> decoder_;
    moex_fast::DecodeLimits limits_{};
    bool has_previous_template_id_ = false;
    std::uint32_t previous_template_id_ = 0;
};

// ---------------------------------------------------------------------------
// SequenceResetProbe
// ---------------------------------------------------------------------------

SequenceResetProbe::SequenceResetProbe()
    : impl_(std::make_unique<Impl>()) {}

SequenceResetProbe::~SequenceResetProbe() = default;

SequenceResetProbe::SequenceResetProbe(SequenceResetProbe&&) noexcept = default;
SequenceResetProbe& SequenceResetProbe::operator=(SequenceResetProbe&&) noexcept = default;

SequenceResetProbeState SequenceResetProbe::state() const noexcept {
    return impl_->state_;
}

SequenceResetProbeCode SequenceResetProbe::terminal_code() const noexcept {
    return impl_->terminal_code_;
}

// ---------------------------------------------------------------------------
// initialize (fully transactional)
// ---------------------------------------------------------------------------

SequenceResetProbeInitResult SequenceResetProbe::initialize(
    const moex_fast::CompiledTemplateSet& compiled,
    const moex_fast::DecodeLimits& limits
) {
    // Already initialized: no mutation
    if (impl_->state_ == SequenceResetProbeState::Ready ||
        impl_->state_ == SequenceResetProbeState::Failed) {
        return {SequenceResetProbeCode::AlreadyInitialized};
    }

    // Validate compiled handle: valid and non-empty
    if (!compiled.valid() || compiled.empty()) {
        return {SequenceResetProbeCode::InvalidConfig};
    }

    // Template ID 7 must exist
    if (compiled.find(kSequenceResetTemplateId) == nullptr) {
        return {SequenceResetProbeCode::InvalidConfig};
    }

    // Validate DecodeLimits: non-zero and not exceeding RT-3 ceilings
    if (limits.max_message_bytes == 0 || limits.max_message_bytes > kCeilingMessageBytes) {
        return {SequenceResetProbeCode::InvalidConfig};
    }
    if (limits.max_presence_map_bytes == 0 || limits.max_presence_map_bytes > kCeilingPresenceMapBytes) {
        return {SequenceResetProbeCode::InvalidConfig};
    }
    if (limits.max_sequence_entries == 0 || limits.max_sequence_entries > kCeilingSequenceEntries) {
        return {SequenceResetProbeCode::InvalidConfig};
    }
    if (limits.max_total_nodes == 0 || limits.max_total_nodes > kCeilingTotalNodes) {
        return {SequenceResetProbeCode::InvalidConfig};
    }
    if (limits.max_string_bytes == 0 || limits.max_string_bytes > kCeilingStringBytes) {
        return {SequenceResetProbeCode::InvalidConfig};
    }

    // Transactional commit
    impl_->compiled_ = compiled;
    impl_->limits_ = limits;
    impl_->decoder_ = std::make_unique<moex_fast::DecoderSession>(compiled, limits);
    impl_->state_ = SequenceResetProbeState::Ready;
    impl_->terminal_code_ = SequenceResetProbeCode::NormalMessage;
    impl_->has_previous_template_id_ = false;
    impl_->previous_template_id_ = 0;

    return {SequenceResetProbeCode::NormalMessage};
}

// ---------------------------------------------------------------------------
// probe
// ---------------------------------------------------------------------------

SequenceResetProbeResult SequenceResetProbe::probe(
    const moex::spectra::OrderedMessageMetadata& transport,
    std::span<const std::uint8_t> fast_body,
    SequenceResetProbeOptions options
) {
    // Not initialized
    if (impl_->state_ == SequenceResetProbeState::Uninitialized) {
        return {SequenceResetProbeCode::NotInitialized,
                moex_fast::DecodeStatus::InternalError, {}, std::nullopt};
    }

    // Stable Failed: return terminal code, empty issues, no reset_message
    if (impl_->state_ == SequenceResetProbeState::Failed) {
        return {impl_->terminal_code_,
                moex_fast::DecodeStatus::InternalError, {}, std::nullopt};
    }

    // Ready: parse minimal FAST header
    auto header = parse_fast_header(
        fast_body.data(), fast_body.size(), impl_->limits_.max_presence_map_bytes);

    if (header.status != moex_fast::DecodeStatus::Ok) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::HeaderDecodeFailed;
        return {SequenceResetProbeCode::HeaderDecodeFailed,
                header.status, {}, std::nullopt};
    }

    // Explicit template ID present
    if (header.template_id_present) {
        std::uint32_t tid = header.template_id;

        if (tid != kSequenceResetTemplateId) {
            // Normal message: return NormalMessage, stay Ready
            // Update previous_template_id for future implicit lookups
            impl_->has_previous_template_id_ = true;
            impl_->previous_template_id_ = tid;
            return {SequenceResetProbeCode::NormalMessage,
                    moex_fast::DecodeStatus::Ok, {}, std::nullopt};
        }

        // Template ID 7: full decode path
        // Update previous state before decode
        impl_->has_previous_template_id_ = true;
        impl_->previous_template_id_ = tid;

        moex_fast::DecodeResult dr = impl_->decoder_->decode_exact(
            fast_body.data(), fast_body.size());

        if (dr.status != moex_fast::DecodeStatus::Ok) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::DecodeFailed;
            return {SequenceResetProbeCode::DecodeFailed,
                    dr.status, std::move(dr.issues), std::nullopt};
        }

        // bytes_consumed invariant
        if (dr.bytes_consumed != fast_body.size() ||
            dr.message.bytes_consumed != fast_body.size()) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::InternalInvariantViolation;
            return {SequenceResetProbeCode::InternalInvariantViolation,
                    dr.status, {}, std::nullopt};
        }

        // template_id must be exactly 7
        if (dr.message.template_id != kSequenceResetTemplateId) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::UnexpectedTemplate;
            return {SequenceResetProbeCode::UnexpectedTemplate,
                    dr.status, {}, std::nullopt};
        }

        // Scan top-level fields once for tag 34, 35, 36
        bool found_tag34 = false, found_tag35 = false, found_tag36 = false;
        std::uint64_t tag34_value = 0;
        std::string tag35_value;
        bool tag34_present = false, tag34_is_null = false;
        bool tag34_is_wire = false, tag34_is_uint64 = false;
        bool tag35_present = false, tag35_is_null = false;
        bool tag35_is_string = false;
        bool tag36_present = false, tag36_is_null = false;
        bool tag36_is_wire = false, tag36_is_uint64 = false;
        std::uint64_t tag36_value = 0;

        for (const auto& field : dr.message.fields) {
            if (!field.has_fix_tag) continue;

            if (field.fix_tag == kTagMsgSeqNum) {
                if (found_tag34) {
                    impl_->state_ = SequenceResetProbeState::Failed;
                    impl_->terminal_code_ = SequenceResetProbeCode::DuplicateTag34;
                    return {SequenceResetProbeCode::DuplicateTag34,
                            dr.status, {}, std::nullopt};
                }
                found_tag34 = true;
                tag34_present = field.is_present;
                tag34_is_null = field.is_null;
                tag34_is_wire = (field.source == moex_fast::ValueSource::Wire);
                if (auto* p = std::get_if<std::uint64_t>(&field.value)) {
                    tag34_is_uint64 = true;
                    tag34_value = *p;
                }
            }

            if (field.fix_tag == kTagMsgType) {
                if (found_tag35) {
                    impl_->state_ = SequenceResetProbeState::Failed;
                    impl_->terminal_code_ = SequenceResetProbeCode::DuplicateTag35;
                    return {SequenceResetProbeCode::DuplicateTag35,
                            dr.status, {}, std::nullopt};
                }
                found_tag35 = true;
                tag35_present = field.is_present;
                tag35_is_null = field.is_null;
                if (auto* p = std::get_if<std::string>(&field.value)) {
                    tag35_is_string = true;
                    tag35_value = *p;
                }
            }

            if (field.fix_tag == kTagNewSeqNo) {
                if (found_tag36) {
                    impl_->state_ = SequenceResetProbeState::Failed;
                    impl_->terminal_code_ = SequenceResetProbeCode::DuplicateTag36;
                    return {SequenceResetProbeCode::DuplicateTag36,
                            dr.status, {}, std::nullopt};
                }
                found_tag36 = true;
                tag36_present = field.is_present;
                tag36_is_null = field.is_null;
                tag36_is_wire = (field.source == moex_fast::ValueSource::Wire);
                if (auto* p = std::get_if<std::uint64_t>(&field.value)) {
                    tag36_is_uint64 = true;
                    tag36_value = *p;
                }
            }
        }

        // Tag 34 checks
        if (!found_tag34) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::MissingTag34;
            return {SequenceResetProbeCode::MissingTag34,
                    dr.status, {}, std::nullopt};
        }
        if (!tag34_present || tag34_is_null || !tag34_is_wire || !tag34_is_uint64) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::InvalidTag34;
            return {SequenceResetProbeCode::InvalidTag34,
                    dr.status, {}, std::nullopt};
        }
        if (tag34_value > UINT32_MAX) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::Tag34OutOfRange;
            return {SequenceResetProbeCode::Tag34OutOfRange,
                    dr.status, {}, std::nullopt};
        }

        // Tag 35 checks
        if (!found_tag35) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::MissingTag35;
            return {SequenceResetProbeCode::MissingTag35,
                    dr.status, {}, std::nullopt};
        }
        if (!tag35_present || tag35_is_null || !tag35_is_string || tag35_value.empty()) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::InvalidTag35;
            return {SequenceResetProbeCode::InvalidTag35,
                    dr.status, {}, std::nullopt};
        }
        if (tag35_value != kSequenceResetMsgType) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::NotSequenceReset;
            return {SequenceResetProbeCode::NotSequenceReset,
                    dr.status, {}, std::nullopt};
        }

        // Tag 36 checks
        if (!found_tag36) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::MissingTag36;
            return {SequenceResetProbeCode::MissingTag36,
                    dr.status, {}, std::nullopt};
        }
        if (!tag36_present || tag36_is_null || !tag36_is_wire || !tag36_is_uint64) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::InvalidTag36;
            return {SequenceResetProbeCode::InvalidTag36,
                    dr.status, {}, std::nullopt};
        }
        if (tag36_value > UINT32_MAX) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::Tag36OutOfRange;
            return {SequenceResetProbeCode::Tag36OutOfRange,
                    dr.status, {}, std::nullopt};
        }

        // External/internal sequence match
        const auto internal_seq = static_cast<std::uint32_t>(tag34_value);
        if (internal_seq != transport.msg_seq_num) {
            impl_->state_ = SequenceResetProbeState::Failed;
            impl_->terminal_code_ = SequenceResetProbeCode::ExternalInternalSequenceMismatch;
            return {SequenceResetProbeCode::ExternalInternalSequenceMismatch,
                    dr.status, {}, std::nullopt};
        }

        // Success: build SequenceResetMessage
        SequenceResetMessage msg{};
        msg.transport = transport;
        msg.msg_seq_num = internal_seq;
        msg.new_seq_no = static_cast<std::uint32_t>(tag36_value);

        return {SequenceResetProbeCode::SequenceReset,
                moex_fast::DecodeStatus::Ok, {}, std::move(msg)};
    }

    // No explicit template ID
    if (!options.allow_implicit_template_id) {
        // Return NormalMessage, no decoder state change
        return {SequenceResetProbeCode::NormalMessage,
                moex_fast::DecodeStatus::Ok, {}, std::nullopt};
    }

    // Implicit template ID allowed: check previous template ID
    if (!impl_->has_previous_template_id_ ||
        impl_->previous_template_id_ != kSequenceResetTemplateId) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::MissingPreviousResetTemplate;
        return {SequenceResetProbeCode::MissingPreviousResetTemplate,
                moex_fast::DecodeStatus::InternalError, {}, std::nullopt};
    }

    // Previous template ID is 7: decode directly
    moex_fast::DecodeResult dr = impl_->decoder_->decode_exact(
        fast_body.data(), fast_body.size());

    if (dr.status != moex_fast::DecodeStatus::Ok) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::DecodeFailed;
        return {SequenceResetProbeCode::DecodeFailed,
                dr.status, std::move(dr.issues), std::nullopt};
    }

    // bytes_consumed invariant
    if (dr.bytes_consumed != fast_body.size() ||
        dr.message.bytes_consumed != fast_body.size()) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::InternalInvariantViolation;
        return {SequenceResetProbeCode::InternalInvariantViolation,
                dr.status, {}, std::nullopt};
    }

    // template_id must be exactly 7
    if (dr.message.template_id != kSequenceResetTemplateId) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::UnexpectedTemplate;
        return {SequenceResetProbeCode::UnexpectedTemplate,
                dr.status, {}, std::nullopt};
    }

    // Scan top-level fields once for tag 34, 35, 36
    bool found_tag34 = false, found_tag35 = false, found_tag36 = false;
    std::uint64_t tag34_value = 0;
    std::string tag35_value;
    bool tag34_present = false, tag34_is_null = false;
    bool tag34_is_wire = false, tag34_is_uint64 = false;
    bool tag35_present = false, tag35_is_null = false;
    bool tag35_is_string = false;
    bool tag36_present = false, tag36_is_null = false;
    bool tag36_is_wire = false, tag36_is_uint64 = false;
    std::uint64_t tag36_value = 0;

    for (const auto& field : dr.message.fields) {
        if (!field.has_fix_tag) continue;

        if (field.fix_tag == kTagMsgSeqNum) {
            if (found_tag34) {
                impl_->state_ = SequenceResetProbeState::Failed;
                impl_->terminal_code_ = SequenceResetProbeCode::DuplicateTag34;
                return {SequenceResetProbeCode::DuplicateTag34,
                        dr.status, {}, std::nullopt};
            }
            found_tag34 = true;
            tag34_present = field.is_present;
            tag34_is_null = field.is_null;
            tag34_is_wire = (field.source == moex_fast::ValueSource::Wire);
            if (auto* p = std::get_if<std::uint64_t>(&field.value)) {
                tag34_is_uint64 = true;
                tag34_value = *p;
            }
        }

        if (field.fix_tag == kTagMsgType) {
            if (found_tag35) {
                impl_->state_ = SequenceResetProbeState::Failed;
                impl_->terminal_code_ = SequenceResetProbeCode::DuplicateTag35;
                return {SequenceResetProbeCode::DuplicateTag35,
                        dr.status, {}, std::nullopt};
            }
            found_tag35 = true;
            tag35_present = field.is_present;
            tag35_is_null = field.is_null;
            if (auto* p = std::get_if<std::string>(&field.value)) {
                tag35_is_string = true;
                tag35_value = *p;
            }
        }

        if (field.fix_tag == kTagNewSeqNo) {
            if (found_tag36) {
                impl_->state_ = SequenceResetProbeState::Failed;
                impl_->terminal_code_ = SequenceResetProbeCode::DuplicateTag36;
                return {SequenceResetProbeCode::DuplicateTag36,
                        dr.status, {}, std::nullopt};
            }
            found_tag36 = true;
            tag36_present = field.is_present;
            tag36_is_null = field.is_null;
            tag36_is_wire = (field.source == moex_fast::ValueSource::Wire);
            if (auto* p = std::get_if<std::uint64_t>(&field.value)) {
                tag36_is_uint64 = true;
                tag36_value = *p;
            }
        }
    }

    // Tag 34 checks
    if (!found_tag34) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::MissingTag34;
        return {SequenceResetProbeCode::MissingTag34,
                dr.status, {}, std::nullopt};
    }
    if (!tag34_present || tag34_is_null || !tag34_is_wire || !tag34_is_uint64) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::InvalidTag34;
        return {SequenceResetProbeCode::InvalidTag34,
                dr.status, {}, std::nullopt};
    }
    if (tag34_value > UINT32_MAX) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::Tag34OutOfRange;
        return {SequenceResetProbeCode::Tag34OutOfRange,
                dr.status, {}, std::nullopt};
    }

    // Tag 35 checks
    if (!found_tag35) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::MissingTag35;
        return {SequenceResetProbeCode::MissingTag35,
                dr.status, {}, std::nullopt};
    }
    if (!tag35_present || tag35_is_null || !tag35_is_string || tag35_value.empty()) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::InvalidTag35;
        return {SequenceResetProbeCode::InvalidTag35,
                dr.status, {}, std::nullopt};
    }
    if (tag35_value != kSequenceResetMsgType) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::NotSequenceReset;
        return {SequenceResetProbeCode::NotSequenceReset,
                dr.status, {}, std::nullopt};
    }

    // Tag 36 checks
    if (!found_tag36) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::MissingTag36;
        return {SequenceResetProbeCode::MissingTag36,
                dr.status, {}, std::nullopt};
    }
    if (!tag36_present || tag36_is_null || !tag36_is_wire || !tag36_is_uint64) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::InvalidTag36;
        return {SequenceResetProbeCode::InvalidTag36,
                dr.status, {}, std::nullopt};
    }
    if (tag36_value > UINT32_MAX) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::Tag36OutOfRange;
        return {SequenceResetProbeCode::Tag36OutOfRange,
                dr.status, {}, std::nullopt};
    }

    // External/internal sequence match
    const auto internal_seq = static_cast<std::uint32_t>(tag34_value);
    if (internal_seq != transport.msg_seq_num) {
        impl_->state_ = SequenceResetProbeState::Failed;
        impl_->terminal_code_ = SequenceResetProbeCode::ExternalInternalSequenceMismatch;
        return {SequenceResetProbeCode::ExternalInternalSequenceMismatch,
                dr.status, {}, std::nullopt};
    }

    // Success: build SequenceResetMessage
    SequenceResetMessage msg{};
    msg.transport = transport;
    msg.msg_seq_num = internal_seq;
    msg.new_seq_no = static_cast<std::uint32_t>(tag36_value);

    return {SequenceResetProbeCode::SequenceReset,
            moex_fast::DecodeStatus::Ok, {}, std::move(msg)};
}

}  // namespace moex_spectra_pipeline
