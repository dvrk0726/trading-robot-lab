#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "moex_fast/decoder_types.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "moex_fast/spectra_gate_a.hpp"
#include "moex_raw/raw_types.hpp"

namespace moex_spectra_pipeline {

// --- Session state ---

enum class OrderedDecodeState : std::uint8_t {
    Uninitialized,
    Ready,
    Failed
};

// --- Result codes ---

enum class OrderedDecodeCode : std::uint8_t {
    Ok,
    NotInitialized,
    AlreadyInitialized,
    InvalidConfig,
    TemplateHashMismatch,
    DecodeFailed,
    MissingTag34,
    DuplicateTag34,
    InvalidTag34,
    Tag34OutOfRange,
    MissingTag35,
    DuplicateTag35,
    InvalidTag35,
    SequenceResetUnsupported,
    ExternalInternalSequenceMismatch,
    InternalInvariantViolation
};

// --- Initialization result (fail-closed: default code = InvalidConfig) ---

struct OrderedDecodeInitResult {
    OrderedDecodeCode code = OrderedDecodeCode::InvalidConfig;
};

// --- Decoded message ---

struct OrderedDecodedMessage {
    moex::spectra::OrderedMessageMetadata transport{};
    std::uint32_t msg_seq_num{};
    std::string msg_type;
    moex_fast::DecodedMessage message;
};

// --- Decode result ---

struct OrderedDecodeResult {
    OrderedDecodeCode code = OrderedDecodeCode::NotInitialized;
    moex_fast::DecodeStatus decode_status = moex_fast::DecodeStatus::InternalError;
    std::vector<moex_fast::DecodeIssue> decode_issues;
    std::optional<OrderedDecodedMessage> decoded_message;
};

// --- Ordered decode session (move-only, PImpl) ---

class OrderedDecodeSession {
public:
    OrderedDecodeSession();
    ~OrderedDecodeSession();

    OrderedDecodeSession(const OrderedDecodeSession&) = delete;
    OrderedDecodeSession& operator=(const OrderedDecodeSession&) = delete;
    OrderedDecodeSession(OrderedDecodeSession&&) noexcept;
    OrderedDecodeSession& operator=(OrderedDecodeSession&&) noexcept;

    [[nodiscard]] OrderedDecodeInitResult initialize(
        const moex_fast::CompiledTemplateSet& compiled,
        const moex_raw::RawSegmentMetadata& meta_a,
        const moex_raw::RawSegmentMetadata& meta_b,
        const moex_fast::DecodeLimits& limits = {}
    );

    [[nodiscard]] OrderedDecodeResult decode_ordered(
        const moex::spectra::OrderedMessageMetadata& transport,
        std::span<const std::uint8_t> fast_body
    );

    [[nodiscard]] OrderedDecodeState state() const noexcept;
    [[nodiscard]] OrderedDecodeCode terminal_code() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace moex_spectra_pipeline
