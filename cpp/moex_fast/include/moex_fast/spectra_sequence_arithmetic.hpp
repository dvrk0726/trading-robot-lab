#pragma once

#include <cstdint>

namespace moex::spectra {

enum class SequenceRelation : std::uint8_t {
    Expected,
    FutureWithinWindow,
    FutureBeyondWindow,
    Ambiguous,
    Stale,
    InvalidConfig
};

struct SequenceClassification {
    SequenceRelation relation{SequenceRelation::InvalidConfig};
    std::uint32_t delta{};
};

[[nodiscard]] constexpr SequenceClassification classify_sequence_relation(
    std::uint32_t observed,
    std::uint32_t next_expected,
    std::uint32_t max_reorder_messages
) noexcept {
    if (max_reorder_messages == 0 || max_reorder_messages >= 0x80000000u) {
        return {SequenceRelation::InvalidConfig, 0};
    }
    const std::uint32_t delta = observed - next_expected;
    if (delta == 0) {
        return {SequenceRelation::Expected, 0};
    }
    if (delta <= max_reorder_messages) {
        return {SequenceRelation::FutureWithinWindow, delta};
    }
    if (delta < 0x80000000u) {
        return {SequenceRelation::FutureBeyondWindow, delta};
    }
    if (delta == 0x80000000u) {
        return {SequenceRelation::Ambiguous, delta};
    }
    return {SequenceRelation::Stale, delta};
}

} // namespace moex::spectra
