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

namespace moex_spectra_pipeline {

// --- Probe state ---

enum class SequenceResetProbeState : std::uint8_t {
    Uninitialized,
    Ready,
    Failed
};

// --- Probe result codes ---

enum class SequenceResetProbeCode : std::uint8_t {
    NormalMessage,
    SequenceReset,
    NotInitialized,
    AlreadyInitialized,
    InvalidConfig,
    HeaderDecodeFailed,
    MissingPreviousResetTemplate,
    DecodeFailed,
    UnexpectedTemplate,
    MissingTag34,
    DuplicateTag34,
    InvalidTag34,
    Tag34OutOfRange,
    MissingTag35,
    DuplicateTag35,
    InvalidTag35,
    NotSequenceReset,
    MissingTag36,
    DuplicateTag36,
    InvalidTag36,
    Tag36OutOfRange,
    ExternalInternalSequenceMismatch,
    InternalInvariantViolation
};

// --- Probe options ---

struct SequenceResetProbeOptions {
    bool allow_implicit_template_id = false;
};

// --- Decoded SequenceReset message (no owning DecodedMessage tree) ---

struct SequenceResetMessage {
    moex::spectra::OrderedMessageMetadata transport{};
    std::uint32_t msg_seq_num{};
    std::uint32_t new_seq_no{};
};

// --- Initialization result (fail-closed: default = InvalidConfig) ---

struct SequenceResetProbeInitResult {
    SequenceResetProbeCode code = SequenceResetProbeCode::InvalidConfig;
};

// --- Probe result (fail-closed: default = NotInitialized) ---

struct SequenceResetProbeResult {
    SequenceResetProbeCode code = SequenceResetProbeCode::NotInitialized;
    moex_fast::DecodeStatus decode_status = moex_fast::DecodeStatus::InternalError;
    std::vector<moex_fast::DecodeIssue> decode_issues;
    std::optional<SequenceResetMessage> reset_message;
};

// --- SequenceReset probe (move-only, PImpl) ---

class SequenceResetProbe {
public:
    SequenceResetProbe();
    ~SequenceResetProbe();

    SequenceResetProbe(const SequenceResetProbe&) = delete;
    SequenceResetProbe& operator=(const SequenceResetProbe&) = delete;
    SequenceResetProbe(SequenceResetProbe&&) noexcept;
    SequenceResetProbe& operator=(SequenceResetProbe&&) noexcept;

    [[nodiscard]] SequenceResetProbeInitResult initialize(
        const moex_fast::CompiledTemplateSet& compiled,
        const moex_fast::DecodeLimits& limits = {}
    );

    [[nodiscard]] SequenceResetProbeResult probe(
        const moex::spectra::OrderedMessageMetadata& transport,
        std::span<const std::uint8_t> fast_body,
        SequenceResetProbeOptions options = {}
    );

    [[nodiscard]] SequenceResetProbeState state() const noexcept;
    [[nodiscard]] SequenceResetProbeCode terminal_code() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace moex_spectra_pipeline
