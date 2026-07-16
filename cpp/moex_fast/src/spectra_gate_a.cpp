#include "moex_fast/spectra_gate_a.hpp"

namespace moex::spectra {

// --- MessageStorage ---

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

    if (!can_add_pending_bytes(pending_bytes_, body.size(), max_reorder_bytes_))
        return InsertResult::PendingByteCapacityExceeded;

    const std::size_t slot_offset = idx * max_message_bytes_;
    std::uint8_t* dst = payload_arena_.data() + slot_offset;
    std::memcpy(dst, body.data(), body.size());

    slot.sequence = msg_seq_num;
    slot.side = side;
    slot.capture_index = capture_index;
    slot.capture_monotonic_ns = capture_monotonic_ns;
    slot.payload_offset = slot_offset;
    slot.payload_length = body.size();
    slot.occupied = true;

    pending_count_ = pending_count_ + 1;
    pending_bytes_ = pending_bytes_ + body.size();

    return InsertResult::Ok;
}

const SlotMetadata& MessageStorage::view(std::uint32_t msg_seq_num) const noexcept {
    static const SlotMetadata empty{};
    if (!initialized_ || slot_capacity_ == 0)
        return empty;
    const std::size_t idx = static_cast<std::size_t>(msg_seq_num) % slot_capacity_;
    const SlotMetadata& slot = slots_[idx];
    if (!slot.occupied || slot.sequence != msg_seq_num)
        return empty;
    return slot;
}

bool MessageStorage::release(std::uint32_t msg_seq_num) noexcept {
    if (!initialized_ || slot_capacity_ == 0)
        return false;

    const std::size_t idx = static_cast<std::size_t>(msg_seq_num) % slot_capacity_;
    SlotMetadata& slot = slots_[idx];

    if (!slot.occupied || slot.sequence != msg_seq_num)
        return false;

    pending_bytes_ -= slot.payload_length;
    pending_count_ -= 1;

    const std::size_t preserved_offset = slot.payload_offset;
    slot = {};
    slot.payload_offset = preserved_offset;

    return true;
}

void MessageStorage::reset() noexcept {
    if (!initialized_)
        return;

    for (std::size_t i = 0; i < slot_capacity_; ++i) {
        const std::size_t preserved_offset = slots_[i].payload_offset;
        slots_[i] = {};
        slots_[i].payload_offset = preserved_offset;
    }

    pending_count_ = 0;
    pending_bytes_ = 0;
}

bool MessageStorage::is_occupied(std::uint32_t msg_seq_num) const noexcept {
    if (!initialized_ || slot_capacity_ == 0)
        return false;
    const std::size_t idx = static_cast<std::size_t>(msg_seq_num) % slot_capacity_;
    const SlotMetadata& slot = slots_[idx];
    return slot.occupied && slot.sequence == msg_seq_num;
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

StoredMessageView MessageStorage::view_message(std::uint32_t msg_seq_num) const noexcept {
    if (!initialized_ || slot_capacity_ == 0)
        return {};
    const std::size_t idx = static_cast<std::size_t>(msg_seq_num) % slot_capacity_;
    const SlotMetadata& slot = slots_[idx];
    if (!slot.occupied || slot.sequence != msg_seq_num)
        return {};
    StoredMessageView v{};
    v.found = true;
    v.sequence = slot.sequence;
    v.side = slot.side;
    v.capture_index = slot.capture_index;
    v.capture_monotonic_ns = slot.capture_monotonic_ns;
    v.body = std::span<const std::uint8_t>(
        payload_arena_.data() + slot.payload_offset, slot.payload_length);
    return v;
}

// --- DualFeedSequencer (non-template methods) ---

SequencerCode DualFeedSequencer::initialize(
    LogicalFeedId feed,
    SequencerConfig config,
    MessageStorage& storage
) noexcept {
    if (state_ != SequencerState::Uninitialized)
        return SequencerCode::InvalidTransition;

    // Feed identity
    if (feed.value != config.logical_feed.value)
        return SequencerCode::InvalidConfig;

    // Valid nonzero configuration values below half-range
    if (config.max_reorder_distance == 0 || config.max_reorder_distance >= 0x80000000u)
        return SequencerCode::InvalidConfig;
    if (config.reorder_wait_ns == 0)
        return SequencerCode::InvalidConfig;
    if (config.storage.max_reorder_messages == 0 || config.storage.max_reorder_messages >= 0x80000000u)
        return SequencerCode::InvalidConfig;
    if (config.storage.max_message_bytes == 0)
        return SequencerCode::InvalidConfig;
    if (config.storage.max_reorder_bytes == 0)
        return SequencerCode::InvalidConfig;

    // Storage must be initialized and empty
    if (!storage.initialized())
        return SequencerCode::InvalidConfig;
    if (storage.pending_count() != 0)
        return SequencerCode::InvalidConfig;
    if (storage.pending_bytes() != 0)
        return SequencerCode::InvalidConfig;

    // Storage capacity must cover the reorder distance
    if (storage.slot_capacity() < static_cast<std::size_t>(config.max_reorder_distance))
        return SequencerCode::InvalidConfig;

    // Actual storage limits must cover declared limits
    if (storage.max_reorder_messages() < config.storage.max_reorder_messages)
        return SequencerCode::InvalidConfig;
    if (storage.max_reorder_bytes() < config.storage.max_reorder_bytes)
        return SequencerCode::InvalidConfig;
    if (storage.max_message_bytes() < config.storage.max_message_bytes)
        return SequencerCode::InvalidConfig;

    feed_ = feed;
    max_reorder_distance_ = config.max_reorder_distance;
    reorder_wait_ns_ = config.reorder_wait_ns;
    storage_ = &storage;
    declared_max_reorder_messages_ = config.storage.max_reorder_messages;
    declared_max_reorder_bytes_ = config.storage.max_reorder_bytes;
    declared_max_message_bytes_ = config.storage.max_message_bytes;

    state_ = SequencerState::Stopped;
    return SequencerCode::Initialized;
}

SequencerCode DualFeedSequencer::start(std::uint32_t initial_expected_seq) noexcept {
    if (state_ == SequencerState::Uninitialized)
        return SequencerCode::NotInitialized;
    if (state_ != SequencerState::Stopped)
        return SequencerCode::InvalidTransition;

    next_expected_ = initial_expected_seq;
    has_deadline_ = false;
    gap_start_ns_ = 0;
    gap_deadline_ns_ = 0;
    last_event_ns_ = 0;
    terminal_code_ = SequencerCode::NoAction;

    state_ = SequencerState::Running;
    return SequencerCode::Started;
}

SequencerCode DualFeedSequencer::reset() noexcept {
    if (state_ == SequencerState::Uninitialized)
        return SequencerCode::NotInitialized;

    if (storage_)
        storage_->reset();

    next_expected_ = 0;
    has_deadline_ = false;
    gap_start_ns_ = 0;
    gap_deadline_ns_ = 0;
    last_event_ns_ = 0;
    terminal_code_ = SequencerCode::NoAction;

    state_ = SequencerState::Stopped;
    return SequencerCode::Reset;
}

} // namespace moex::spectra
