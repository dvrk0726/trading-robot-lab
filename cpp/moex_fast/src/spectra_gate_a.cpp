#include "moex_fast/spectra_gate_a.hpp"

namespace moex::spectra {

void MessageStorage::initialize(
    std::span<SlotMetadata> slots,
    std::span<std::uint8_t> arena,
    MessageStorageConfig config
) noexcept {
    const StorageGeometryLimits limits{
        config.max_reorder_messages,
        config.max_reorder_bytes,
        config.max_message_bytes
    };
    if (validate_geometry(slots.size(), arena, limits) != GeometryCode::Ok)
        return;

    slots_ = slots;
    payload_arena_ = arena;
    max_message_bytes_ = config.max_message_bytes;
    max_reorder_bytes_ = config.max_reorder_bytes;
    max_reorder_messages_ = config.max_reorder_messages;
    slot_capacity_ = slots.size();

    for (std::size_t i = 0; i < slot_capacity_; ++i) {
        slots_[i] = {};
        slots_[i].payload_offset = i * config.max_message_bytes;
        slots_[i].payload_length = 0;
        slots_[i].occupied = false;
    }

    pending_count_ = 0;
    pending_bytes_ = 0;
    initialized_ = true;
}

InsertResult MessageStorage::insert(
    std::uint32_t msg_seq_num,
    FeedSide side,
    std::uint64_t capture_index,
    std::uint64_t capture_monotonic_ns,
    std::span<const std::uint8_t> body
) noexcept {
    if (!initialized_)
        return InsertResult::NotInitialized;
    if (body.empty())
        return InsertResult::EmptyBody;
    if (body.size() > max_message_bytes_)
        return InsertResult::BodyTooLarge;

    const std::size_t idx = static_cast<std::size_t>(msg_seq_num) % slot_capacity_;
    SlotMetadata& slot = slots_[idx];

    if (slot.occupied) {
        if (slot.sequence != msg_seq_num)
            return InsertResult::InternalInvariantViolation;

        if (slot.payload_length != body.size())
            return InsertResult::DuplicateMismatch;

        const std::uint8_t* stored = payload_arena_.data() + slot.payload_offset;
        for (std::size_t i = 0; i < body.size(); ++i) {
            if (stored[i] != body[i])
                return InsertResult::DuplicateMismatch;
        }
        return InsertResult::DuplicateEqual;
    }

    if (pending_count_ >= max_reorder_messages_)
        return InsertResult::PendingMessageCapacityExceeded;

    const std::size_t new_bytes = pending_bytes_ + body.size();
    if (new_bytes > max_reorder_bytes_)
        return InsertResult::PendingByteCapacityExceeded;

    std::uint8_t* dst = payload_arena_.data() + slot.payload_offset;
    std::memcpy(dst, body.data(), body.size());

    slot.sequence = msg_seq_num;
    slot.side = side;
    slot.capture_index = capture_index;
    slot.capture_monotonic_ns = capture_monotonic_ns;
    slot.payload_length = body.size();
    slot.occupied = true;

    pending_count_ = pending_count_ + 1;
    pending_bytes_ = new_bytes;

    return InsertResult::Ok;
}

const SlotMetadata& MessageStorage::view(std::uint32_t msg_seq_num) const noexcept {
    return slots_[static_cast<std::size_t>(msg_seq_num) % slot_capacity_];
}

void MessageStorage::release(std::uint32_t msg_seq_num) noexcept {
    if (!initialized_)
        return;

    const std::size_t idx = static_cast<std::size_t>(msg_seq_num) % slot_capacity_;
    SlotMetadata& slot = slots_[idx];

    if (slot.occupied) {
        pending_bytes_ -= slot.payload_length;
        pending_count_ -= 1;
    }

    slot = {};
}

void MessageStorage::reset() noexcept {
    for (std::size_t i = 0; i < slot_capacity_; ++i)
        slots_[i] = {};

    pending_count_ = 0;
    pending_bytes_ = 0;
    initialized_ = false;
}

bool MessageStorage::is_occupied(std::uint32_t msg_seq_num) const noexcept {
    return slots_[static_cast<std::size_t>(msg_seq_num) % slot_capacity_].occupied;
}

std::size_t MessageStorage::pending_count() const noexcept {
    return pending_count_;
}

std::size_t MessageStorage::pending_bytes() const noexcept {
    return pending_bytes_;
}

std::size_t MessageStorage::slot_capacity() const noexcept {
    return slot_capacity_;
}

bool MessageStorage::initialized() const noexcept {
    return initialized_;
}

} // namespace moex::spectra
