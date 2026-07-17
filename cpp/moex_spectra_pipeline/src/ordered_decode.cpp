#include "moex_spectra_pipeline/ordered_decode.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace moex_spectra_pipeline {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool parse_hex64(const std::string& hex, std::uint8_t out[32]) {
    if (hex.size() != 64) return false;
    for (std::size_t i = 0; i < 32; ++i) {
        auto hex_byte = [&](char c, std::uint8_t& nibble) -> bool {
            if (c >= '0' && c <= '9') { nibble = static_cast<std::uint8_t>(c - '0'); return true; }
            if (c >= 'a' && c <= 'f') { nibble = static_cast<std::uint8_t>(c - 'a' + 10); return true; }
            if (c >= 'A' && c <= 'F') { nibble = static_cast<std::uint8_t>(c - 'A' + 10); return true; }
            return false;
        };
        std::uint8_t hi{}, lo{};
        if (!hex_byte(hex[i * 2], hi) || !hex_byte(hex[i * 2 + 1], lo)) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

static bool bytes_equal(const std::uint8_t a[32], const std::uint8_t b[32]) {
    return std::memcmp(a, b, 32) == 0;
}

// ---------------------------------------------------------------------------
// PImpl
// ---------------------------------------------------------------------------

struct OrderedDecodeSession::Impl {
    OrderedDecodeState state_ = OrderedDecodeState::Uninitialized;
    OrderedDecodeCode terminal_code_ = OrderedDecodeCode::Ok;
    moex_fast::CompiledTemplateSet compiled_;
    std::unique_ptr<moex_fast::DecoderSession> decoder_;
    moex_raw::SourceSide side_a_set_ = moex_raw::SourceSide::None;
};

// ---------------------------------------------------------------------------
// OrderedDecodeSession
// ---------------------------------------------------------------------------

OrderedDecodeSession::OrderedDecodeSession()
    : impl_(std::make_unique<Impl>()) {}

OrderedDecodeSession::~OrderedDecodeSession() = default;

OrderedDecodeSession::OrderedDecodeSession(OrderedDecodeSession&&) noexcept = default;
OrderedDecodeSession& OrderedDecodeSession::operator=(OrderedDecodeSession&&) noexcept = default;

OrderedDecodeState OrderedDecodeSession::state() const noexcept {
    return impl_->state_;
}

OrderedDecodeCode OrderedDecodeSession::terminal_code() const noexcept {
    return impl_->terminal_code_;
}

// ---------------------------------------------------------------------------
// initialize
// ---------------------------------------------------------------------------

OrderedDecodeInitResult OrderedDecodeSession::initialize(
    const moex_fast::CompiledTemplateSet& compiled,
    const moex_raw::RawSegmentMetadata& meta_a,
    const moex_raw::RawSegmentMetadata& meta_b,
    const moex_fast::DecodeLimits& limits
) {
    // Already initialized check (no mutation)
    if (impl_->state_ == OrderedDecodeState::Ready ||
        impl_->state_ == OrderedDecodeState::Failed) {
        return {OrderedDecodeCode::AlreadyInitialized};
    }

    // --- Validate compiled templates ---
    if (!compiled.valid() || compiled.empty()) {
        return {OrderedDecodeCode::InvalidConfig};
    }

    // --- Validate sides: exactly one A and one B, independent of argument order ---
    const auto side_a = meta_a.source.source_side;
    const auto side_b = meta_b.source.source_side;

    bool has_a = (side_a == moex_raw::SourceSide::A && side_b == moex_raw::SourceSide::B);
    bool has_b = (side_a == moex_raw::SourceSide::B && side_b == moex_raw::SourceSide::A);

    if (!has_a && !has_b) {
        return {OrderedDecodeCode::InvalidConfig};
    }

    // Determine which metadata is A and which is B
    const moex_raw::RawSegmentMetadata& actual_a = has_a ? meta_a : meta_b;
    const moex_raw::RawSegmentMetadata& actual_b = has_a ? meta_b : meta_a;

    // --- Validate compiled templates_sha256: exactly 64 hex symbols ---
    const std::string& compiled_hex = compiled.templates_sha256();
    std::uint8_t compiled_hash[32]{};
    if (!parse_hex64(compiled_hex, compiled_hash)) {
        return {OrderedDecodeCode::InvalidConfig};
    }

    // --- Compare with metadata A ---
    if (!bytes_equal(compiled_hash, actual_a.source.templates_sha256)) {
        return {OrderedDecodeCode::TemplateHashMismatch};
    }

    // --- Compare with metadata B ---
    if (!bytes_equal(compiled_hash, actual_b.source.templates_sha256)) {
        return {OrderedDecodeCode::TemplateHashMismatch};
    }

    // --- Compare metadata A and B templates_sha256 ---
    if (!bytes_equal(actual_a.source.templates_sha256, actual_b.source.templates_sha256)) {
        return {OrderedDecodeCode::TemplateHashMismatch};
    }

    // --- Validate DecodeLimits against RT-3 hard ceilings ---
    constexpr std::size_t kMaxMessageBytes = 1024 * 1024;       // 1 MiB
    constexpr std::size_t kMaxPresenceMapBytes = 64;
    constexpr std::uint32_t kMaxSequenceEntries = 100000;
    constexpr std::uint32_t kMaxTotalNodes = 1000000;
    constexpr std::size_t kMaxStringBytes = 1024 * 1024;        // 1 MiB

    if (limits.max_message_bytes == 0 || limits.max_message_bytes > kMaxMessageBytes) {
        return {OrderedDecodeCode::InvalidConfig};
    }
    if (limits.max_presence_map_bytes == 0 || limits.max_presence_map_bytes > kMaxPresenceMapBytes) {
        return {OrderedDecodeCode::InvalidConfig};
    }
    if (limits.max_sequence_entries == 0 || limits.max_sequence_entries > kMaxSequenceEntries) {
        return {OrderedDecodeCode::InvalidConfig};
    }
    if (limits.max_total_nodes == 0 || limits.max_total_nodes > kMaxTotalNodes) {
        return {OrderedDecodeCode::InvalidConfig};
    }
    if (limits.max_string_bytes == 0 || limits.max_string_bytes > kMaxStringBytes) {
        return {OrderedDecodeCode::InvalidConfig};
    }

    // --- Transactional: only mutate on full success ---
    impl_->compiled_ = compiled;
    impl_->decoder_ = std::make_unique<moex_fast::DecoderSession>(compiled, limits);
    impl_->state_ = OrderedDecodeState::Ready;
    impl_->terminal_code_ = OrderedDecodeCode::Ok;

    return {OrderedDecodeCode::Ok};
}

// ---------------------------------------------------------------------------
// decode_ordered
// ---------------------------------------------------------------------------

OrderedDecodeResult OrderedDecodeSession::decode_ordered(
    const moex::spectra::OrderedMessageMetadata& transport,
    std::span<const std::uint8_t> fast_body
) {
    // --- Not initialized ---
    if (impl_->state_ == OrderedDecodeState::Uninitialized) {
        return {OrderedDecodeCode::NotInitialized, moex_fast::DecodeStatus::InternalError, {}, std::nullopt};
    }

    // --- Stable Failed: return terminal code, no decoder call ---
    if (impl_->state_ == OrderedDecodeState::Failed) {
        return {impl_->terminal_code_, moex_fast::DecodeStatus::InternalError, {}, std::nullopt};
    }

    // --- Ready: call decode_exact directly on borrowed fast_body ---
    moex_fast::DecodeResult dr = impl_->decoder_->decode_exact(
        fast_body.data(), fast_body.size());

    // --- Any non-Ok DecodeStatus → Failed/DecodeFailed ---
    if (dr.status != moex_fast::DecodeStatus::Ok) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::DecodeFailed;
        return {OrderedDecodeCode::DecodeFailed, dr.status, std::move(dr.issues), std::nullopt};
    }

    // --- bytes_consumed invariant ---
    if (dr.bytes_consumed != fast_body.size() ||
        dr.message.bytes_consumed != fast_body.size()) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::InternalInvariantViolation;
        return {OrderedDecodeCode::InternalInvariantViolation, dr.status, {}, std::nullopt};
    }

    // --- Scan top-level fields for tag 34 and tag 35 ---
    bool found_tag34 = false;
    bool found_tag35 = false;
    std::uint64_t tag34_value = 0;
    std::string tag35_value;
    bool tag34_present = false;
    bool tag34_is_null = false;
    bool tag34_is_wire = false;
    bool tag34_is_uint64 = false;
    bool tag35_present = false;
    bool tag35_is_null = false;
    bool tag35_is_string = false;
    bool tag35_is_constant = false;

    for (const auto& field : dr.message.fields) {
        if (!field.has_fix_tag) continue;

        if (field.fix_tag == 34) {
            if (found_tag34) {
                impl_->state_ = OrderedDecodeState::Failed;
                impl_->terminal_code_ = OrderedDecodeCode::DuplicateTag34;
                return {OrderedDecodeCode::DuplicateTag34, dr.status, {}, std::nullopt};
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

        if (field.fix_tag == 35) {
            if (found_tag35) {
                impl_->state_ = OrderedDecodeState::Failed;
                impl_->terminal_code_ = OrderedDecodeCode::DuplicateTag35;
                return {OrderedDecodeCode::DuplicateTag35, dr.status, {}, std::nullopt};
            }
            found_tag35 = true;
            tag35_present = field.is_present;
            tag35_is_null = field.is_null;
            tag35_is_constant = (field.source == moex_fast::ValueSource::Constant);
            if (auto* p = std::get_if<std::string>(&field.value)) {
                tag35_is_string = true;
                tag35_value = *p;
            }
        }
    }

    // --- Tag 34 checks ---
    if (!found_tag34 || !tag34_present || tag34_is_null) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::MissingTag34;
        return {OrderedDecodeCode::MissingTag34, dr.status, {}, std::nullopt};
    }
    if (!tag34_is_wire || !tag34_is_uint64) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::InvalidTag34;
        return {OrderedDecodeCode::InvalidTag34, dr.status, {}, std::nullopt};
    }
    if (tag34_value > UINT32_MAX) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::Tag34OutOfRange;
        return {OrderedDecodeCode::Tag34OutOfRange, dr.status, {}, std::nullopt};
    }

    // --- Tag 35 checks ---
    if (!found_tag35 || !tag35_present || tag35_is_null) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::MissingTag35;
        return {OrderedDecodeCode::MissingTag35, dr.status, {}, std::nullopt};
    }
    // Constant tag 35 is allowed
    if (!tag35_is_string && !tag35_is_constant) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::InvalidTag35;
        return {OrderedDecodeCode::InvalidTag35, dr.status, {}, std::nullopt};
    }
    if (tag35_value.empty() && !tag35_is_constant) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::InvalidTag35;
        return {OrderedDecodeCode::InvalidTag35, dr.status, {}, std::nullopt};
    }

    // For constant tag 35 without a string value, we need the actual string
    // If tag35 is constant and tag35_value is empty, check if it has a string in value
    if (tag35_is_constant && tag35_value.empty()) {
        // Constant fields store their value; if string variant holds it, we already have it.
        // If not, this is an invariant issue.
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::InternalInvariantViolation;
        return {OrderedDecodeCode::InternalInvariantViolation, dr.status, {}, std::nullopt};
    }

    // --- SequenceReset check (tag 35 == "4") ---
    if (tag35_value == "4") {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::SequenceResetUnsupported;
        return {OrderedDecodeCode::SequenceResetUnsupported, dr.status, {}, std::nullopt};
    }

    // --- External/internal sequence match ---
    const auto internal_seq = static_cast<std::uint32_t>(tag34_value);
    if (internal_seq != transport.msg_seq_num) {
        impl_->state_ = OrderedDecodeState::Failed;
        impl_->terminal_code_ = OrderedDecodeCode::ExternalInternalSequenceMismatch;
        return {OrderedDecodeCode::ExternalInternalSequenceMismatch, dr.status, {}, std::nullopt};
    }

    // --- Success: build OrderedDecodedMessage ---
    OrderedDecodedMessage msg{};
    msg.transport = transport;
    msg.msg_seq_num = internal_seq;
    msg.msg_type = tag35_value;
    msg.message = std::move(dr.message);

    return {OrderedDecodeCode::Ok, moex_fast::DecodeStatus::Ok, {}, std::move(msg)};
}

}  // namespace moex_spectra_pipeline
