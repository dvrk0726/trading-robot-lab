#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>

#include "spectra_udp_framing.hpp"

namespace moex::spectra {

// --- Storage geometry validation (pure, no object construction required) ---

struct StorageGeometryLimits {
    std::uint32_t max_reorder_messages{};
    std::size_t max_reorder_bytes{};
    std::size_t max_message_bytes{};
};

enum class GeometryCode : std::uint8_t {
    Ok,
    ZeroMaxReorderMessages,
    MaxReorderMessagesTooLarge,
    ZeroMaxMessageBytes,
    SlotCapacityTooSmall,
    CapacityOverflow,
    ArenaTooSmall,
    ZeroMaxReorderBytes,
    MaxReorderBytesExceedsCapacity
};

[[nodiscard]] constexpr GeometryCode validate_storage_geometry(
    std::size_t slot_capacity,
    std::span<const std::uint8_t> payload_arena,
    StorageGeometryLimits limits
) noexcept {
    if (limits.max_reorder_messages == 0)
        return GeometryCode::ZeroMaxReorderMessages;
    if (limits.max_reorder_messages >= 0x80000000u)
        return GeometryCode::MaxReorderMessagesTooLarge;
    if (limits.max_message_bytes == 0)
        return GeometryCode::ZeroMaxMessageBytes;
    if (slot_capacity < static_cast<std::size_t>(limits.max_reorder_messages))
        return GeometryCode::SlotCapacityTooSmall;

    const std::size_t cap = slot_capacity;
    const std::size_t msg = limits.max_message_bytes;

    const std::size_t required_bytes = cap * msg;
    if (cap != 0 && required_bytes / cap != msg)
        return GeometryCode::CapacityOverflow;
    if (required_bytes > payload_arena.size())
        return GeometryCode::ArenaTooSmall;
    if (limits.max_reorder_bytes == 0)
        return GeometryCode::ZeroMaxReorderBytes;
    if (limits.max_reorder_bytes > required_bytes)
        return GeometryCode::MaxReorderBytesExceedsCapacity;

    return GeometryCode::Ok;
}

// --- Insert result ---

enum class InsertResult : std::uint8_t {
    Ok,
    NotInitialized,
    EmptyBody,
    BodyTooLarge,
    DuplicateEqual,
    DuplicateMismatch,
    PendingMessageCapacityExceeded,
    PendingByteCapacityExceeded,
    InternalInvariantViolation
};

// --- Configuration ---

struct MessageStorageConfig {
    std::uint32_t max_reorder_messages{};
    std::size_t max_reorder_bytes{};
    std::size_t max_message_bytes{};
};

// --- Slot metadata ---

struct SlotMetadata {
    std::uint32_t sequence{};
    FeedSide side{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
    std::size_t payload_offset{};
    std::size_t payload_length{};
    bool occupied{};
};

// --- MessageStorage ---

class MessageStorage {
public:
    MessageStorage() = default;
    MessageStorage(const MessageStorage&) = delete;
    MessageStorage& operator=(const MessageStorage&) = delete;
    MessageStorage(MessageStorage&&) noexcept = default;
    MessageStorage& operator=(MessageStorage&&) noexcept = default;
    ~MessageStorage() = default;

    [[nodiscard]] static constexpr GeometryCode validate_geometry(
        std::size_t slot_capacity,
        std::span<const std::uint8_t> payload_arena,
        StorageGeometryLimits limits
    ) noexcept {
        return validate_storage_geometry(slot_capacity, payload_arena, limits);
    }

    void initialize(
        std::span<SlotMetadata> slots,
        std::span<std::uint8_t> arena,
        MessageStorageConfig config
    ) noexcept;

    [[nodiscard]] InsertResult insert(
        std::uint32_t msg_seq_num,
        FeedSide side,
        std::uint64_t capture_index,
        std::uint64_t capture_monotonic_ns,
        std::span<const std::uint8_t> body
    ) noexcept;

    [[nodiscard]] const SlotMetadata& view(std::uint32_t msg_seq_num) const noexcept;
    void release(std::uint32_t msg_seq_num) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool is_occupied(std::uint32_t msg_seq_num) const noexcept;
    [[nodiscard]] std::size_t pending_count() const noexcept;
    [[nodiscard]] std::size_t pending_bytes() const noexcept;
    [[nodiscard]] std::size_t slot_capacity() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

private:
    std::span<SlotMetadata> slots_{};
    std::span<std::uint8_t> payload_arena_{};
    std::size_t max_message_bytes_{};
    std::size_t max_reorder_bytes_{};
    std::uint32_t max_reorder_messages_{};
    std::size_t slot_capacity_{};
    std::size_t pending_count_{};
    std::size_t pending_bytes_{};
    bool initialized_{};
};

} // namespace moex::spectra
