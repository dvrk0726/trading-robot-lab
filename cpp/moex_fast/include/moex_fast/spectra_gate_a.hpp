#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

#include "spectra_udp_framing.hpp"
#include "spectra_sequence_arithmetic.hpp"

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

// --- Borrowed payload view ---

struct StoredMessageView {
    bool found{};
    std::uint32_t sequence{};
    FeedSide side{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
    std::span<const std::uint8_t> body{};
};

// --- MessageStorage ---

class MessageStorage {
public:
    MessageStorage() = default;
    MessageStorage(const MessageStorage&) = delete;
    MessageStorage& operator=(const MessageStorage&) = delete;
    MessageStorage(MessageStorage&&) noexcept = delete;
    MessageStorage& operator=(MessageStorage&&) noexcept = delete;
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
    [[nodiscard]] StoredMessageView view_message(std::uint32_t msg_seq_num) const noexcept;
    [[nodiscard]] bool release(std::uint32_t msg_seq_num) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool is_occupied(std::uint32_t msg_seq_num) const noexcept;
    [[nodiscard]] std::size_t pending_count() const noexcept;
    [[nodiscard]] std::size_t pending_bytes() const noexcept;
    [[nodiscard]] std::size_t slot_capacity() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] std::uint32_t max_reorder_messages() const noexcept { return max_reorder_messages_; }
    [[nodiscard]] std::size_t max_reorder_bytes() const noexcept { return max_reorder_bytes_; }
    [[nodiscard]] std::size_t max_message_bytes() const noexcept { return max_message_bytes_; }

    [[nodiscard]] static constexpr bool can_add_pending_bytes(
        std::size_t current, std::size_t addition, std::size_t limit
    ) noexcept {
        if (current > limit) return false;
        if (limit - current < addition) return false;
        return true;
    }

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

// --- Sequencer state machine ---

enum class SequencerState : std::uint8_t {
    Uninitialized,
    Stopped,
    Running,
    GapWait,
    FailedClosed
};

enum class SequencerCode : std::uint16_t {
    NoAction,
    Initialized,
    Started,
    Reset,
    Emitted,
    DuplicateDropped,
    BufferedOutOfOrder,
    GapWaiting,

    NotInitialized,
    InvalidTransition,
    InvalidConfig,
    WrongLogicalFeed,
    DuplicatePayloadMismatch,
    ReorderDistanceExceeded,
    AmbiguousSequenceRelation,
    PendingMessageCapacityExceeded,
    PendingByteCapacityExceeded,
    ClockRegression,
    DeadlineOverflow,
    GapConfirmed,
    FailedClosed,
    InternalInvariantViolation
};

struct SequencerResult {
    SequencerCode code{SequencerCode::NoAction};
    std::uint32_t observed_seq{};
    std::uint32_t expected_seq{};
};

struct SequencerConfig {
    LogicalFeedId logical_feed{};
    std::uint32_t max_reorder_distance{};
    std::uint64_t reorder_wait_ns{};
    MessageStorageConfig storage{};
};

// --- Ordered message metadata for sink callback ---

struct OrderedMessageMetadata {
    std::uint32_t msg_seq_num{};
    FeedSide side{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
};

// --- DualFeedSequencer ---

class DualFeedSequencer {
public:
    DualFeedSequencer() = default;
    DualFeedSequencer(const DualFeedSequencer&) = delete;
    DualFeedSequencer& operator=(const DualFeedSequencer&) = delete;
    DualFeedSequencer(DualFeedSequencer&&) noexcept = delete;
    DualFeedSequencer& operator=(DualFeedSequencer&&) noexcept = delete;
    ~DualFeedSequencer() = default;

    [[nodiscard]] SequencerCode initialize(
        LogicalFeedId feed,
        SequencerConfig config,
        MessageStorage& storage
    ) noexcept;

    [[nodiscard]] SequencerCode start(std::uint32_t initial_expected_seq) noexcept;
    [[nodiscard]] SequencerCode reset() noexcept;

    [[nodiscard]] SequencerState state() const noexcept { return state_; }
    [[nodiscard]] std::uint32_t next_expected_seq() const noexcept { return next_expected_; }

    template<class Sink>
    [[nodiscard]] SequencerResult on_message(
        const FramedMessageView& message,
        std::uint64_t event_monotonic_ns,
        Sink& sink
    ) noexcept;

    template<class Sink>
    [[nodiscard]] SequencerResult on_time(
        std::uint64_t event_monotonic_ns,
        Sink& sink
    ) noexcept;

private:
    [[nodiscard]] SequencerResult make_result(
        SequencerCode code, std::uint32_t observed, std::uint32_t expected
    ) const noexcept {
        return {code, observed, expected};
    }

    void enter_failed_closed(SequencerCode reason) noexcept {
        terminal_code_ = reason;
        state_ = SequencerState::FailedClosed;
    }

    template<class Sink>
    [[nodiscard]] SequencerCode flush_contiguous(Sink& sink) noexcept;

    SequencerState state_{SequencerState::Uninitialized};
    std::uint32_t next_expected_{};
    LogicalFeedId feed_{};
    std::uint32_t max_reorder_distance_{};
    std::uint64_t reorder_wait_ns_{};
    MessageStorage* storage_{};
    std::uint32_t declared_max_reorder_messages_{};
    std::size_t declared_max_reorder_bytes_{};
    std::size_t declared_max_message_bytes_{};
    bool has_deadline_{};
    std::uint64_t gap_start_ns_{};
    std::uint64_t gap_deadline_ns_{};
    std::uint64_t last_event_ns_{};
    SequencerCode terminal_code_{SequencerCode::NoAction};
};

// --- Template implementations ---

template<class Sink>
SequencerResult DualFeedSequencer::on_message(
    const FramedMessageView& message,
    std::uint64_t event_monotonic_ns,
    Sink& sink
) noexcept {
    static_assert(std::is_nothrow_invocable_r_v<void, Sink&,
        const OrderedMessageMetadata&, std::span<const std::uint8_t>>,
        "sink must be noexcept-callable with (const OrderedMessageMetadata&, std::span<const std::uint8_t>)");

    // 1. lifecycle/state validity
    if (state_ == SequencerState::Uninitialized) {
        return make_result(SequencerCode::NotInitialized, message.msg_seq_num, next_expected_);
    }
    if (state_ == SequencerState::Stopped) {
        return make_result(SequencerCode::InvalidTransition, message.msg_seq_num, next_expected_);
    }
    if (state_ == SequencerState::FailedClosed) {
        return make_result(SequencerCode::FailedClosed, message.msg_seq_num, next_expected_);
    }

    const std::uint32_t entry_expected = next_expected_;

    // 2. clock regression
    if (event_monotonic_ns < last_event_ns_) {
        enter_failed_closed(SequencerCode::ClockRegression);
        return make_result(SequencerCode::ClockRegression, message.msg_seq_num, entry_expected);
    }
    last_event_ns_ = event_monotonic_ns;

    // 3. active deadline
    if (has_deadline_ && event_monotonic_ns >= gap_deadline_ns_) {
        enter_failed_closed(SequencerCode::GapConfirmed);
        return make_result(SequencerCode::GapConfirmed, message.msg_seq_num, entry_expected);
    }

    // 4. message feed and structural preconditions
    if (message.feed.value != feed_.value) {
        enter_failed_closed(SequencerCode::WrongLogicalFeed);
        return make_result(SequencerCode::WrongLogicalFeed, message.msg_seq_num, entry_expected);
    }
    if (message.side != FeedSide::A && message.side != FeedSide::B) {
        enter_failed_closed(SequencerCode::InternalInvariantViolation);
        return make_result(SequencerCode::InternalInvariantViolation, message.msg_seq_num, entry_expected);
    }
    if (message.fast_body.empty()) {
        enter_failed_closed(SequencerCode::InternalInvariantViolation);
        return make_result(SequencerCode::InternalInvariantViolation, message.msg_seq_num, entry_expected);
    }

    // 5. A2 sequence classification
    const auto classification = classify_sequence_relation(
        message.msg_seq_num, next_expected_, max_reorder_distance_);

    // 6. relation-specific fatal result
    switch (classification.relation) {
    case SequenceRelation::InvalidConfig:
        enter_failed_closed(SequencerCode::InternalInvariantViolation);
        return make_result(SequencerCode::InternalInvariantViolation, message.msg_seq_num, entry_expected);

    case SequenceRelation::FutureBeyondWindow:
        enter_failed_closed(SequencerCode::ReorderDistanceExceeded);
        return make_result(SequencerCode::ReorderDistanceExceeded, message.msg_seq_num, entry_expected);

    case SequenceRelation::Ambiguous:
        enter_failed_closed(SequencerCode::AmbiguousSequenceRelation);
        return make_result(SequencerCode::AmbiguousSequenceRelation, message.msg_seq_num, entry_expected);

    case SequenceRelation::Stale:
        return make_result(SequencerCode::DuplicateDropped, message.msg_seq_num, entry_expected);

    case SequenceRelation::Expected: {
        // Declared per-message size check before emission
        if (message.fast_body.size() > declared_max_message_bytes_) {
            enter_failed_closed(SequencerCode::PendingByteCapacityExceeded);
            return make_result(SequencerCode::PendingByteCapacityExceeded, message.msg_seq_num, entry_expected);
        }

        // Emit synchronously -- in-order body remains borrowed from FramedMessageView
        OrderedMessageMetadata meta{};
        meta.msg_seq_num = message.msg_seq_num;
        meta.side = message.side;
        meta.capture_index = message.capture_index;
        meta.capture_monotonic_ns = message.capture_monotonic_ns;

        sink(meta, message.fast_body);

        // Increment next_expected modulo 2^32
        next_expected_ = next_expected_ + 1;

        // If we were in GapWait, check if pending is now empty
        if (state_ == SequencerState::GapWait && storage_->pending_count() == 0) {
            has_deadline_ = false;
            state_ = SequencerState::Running;
        }

        // Flush every newly contiguous buffered message
        const SequencerCode flush_rc = flush_contiguous(sink);
        if (flush_rc != SequencerCode::NoAction) {
            return make_result(flush_rc, message.msg_seq_num, entry_expected);
        }

        return make_result(SequencerCode::Emitted, message.msg_seq_num, entry_expected);
    }

    case SequenceRelation::FutureWithinWindow: {
        // 7. pending duplicate comparison
        if (storage_->is_occupied(message.msg_seq_num)) {
            const auto existing = storage_->view_message(message.msg_seq_num);
            if (existing.body.size() == message.fast_body.size()) {
                bool equal = true;
                for (std::size_t i = 0; i < message.fast_body.size(); ++i) {
                    if (existing.body[i] != message.fast_body[i]) {
                        equal = false;
                        break;
                    }
                }
                if (equal) {
                    return make_result(SequencerCode::DuplicateDropped, message.msg_seq_num, entry_expected);
                }
            }
            enter_failed_closed(SequencerCode::DuplicatePayloadMismatch);
            return make_result(SequencerCode::DuplicatePayloadMismatch, message.msg_seq_num, entry_expected);
        }

        // 8. declared limit enforcement before storage insert
        if (storage_->pending_count() >= declared_max_reorder_messages_) {
            enter_failed_closed(SequencerCode::PendingMessageCapacityExceeded);
            return make_result(SequencerCode::PendingMessageCapacityExceeded, message.msg_seq_num, entry_expected);
        }
        if (message.fast_body.size() > declared_max_message_bytes_) {
            enter_failed_closed(SequencerCode::PendingByteCapacityExceeded);
            return make_result(SequencerCode::PendingByteCapacityExceeded, message.msg_seq_num, entry_expected);
        }
        if (!MessageStorage::can_add_pending_bytes(
                storage_->pending_bytes(), message.fast_body.size(), declared_max_reorder_bytes_)) {
            enter_failed_closed(SequencerCode::PendingByteCapacityExceeded);
            return make_result(SequencerCode::PendingByteCapacityExceeded, message.msg_seq_num, entry_expected);
        }

        // 9. deadline overflow detection before storage insert (first future in Running)
        if (state_ == SequencerState::Running && !has_deadline_) {
            if (event_monotonic_ns > UINT64_MAX - reorder_wait_ns_) {
                enter_failed_closed(SequencerCode::DeadlineOverflow);
                return make_result(SequencerCode::DeadlineOverflow, message.msg_seq_num, entry_expected);
            }
        }

        // 10. insert into storage
        const auto insert_result = storage_->insert(
            message.msg_seq_num, message.side, message.capture_index,
            message.capture_monotonic_ns, message.fast_body);

        switch (insert_result) {
        case InsertResult::Ok:
            break;
        case InsertResult::DuplicateEqual:
            return make_result(SequencerCode::DuplicateDropped, message.msg_seq_num, entry_expected);
        case InsertResult::DuplicateMismatch:
            enter_failed_closed(SequencerCode::DuplicatePayloadMismatch);
            return make_result(SequencerCode::DuplicatePayloadMismatch, message.msg_seq_num, entry_expected);
        case InsertResult::PendingMessageCapacityExceeded:
            enter_failed_closed(SequencerCode::PendingMessageCapacityExceeded);
            return make_result(SequencerCode::PendingMessageCapacityExceeded, message.msg_seq_num, entry_expected);
        case InsertResult::PendingByteCapacityExceeded:
        case InsertResult::BodyTooLarge:
            enter_failed_closed(SequencerCode::PendingByteCapacityExceeded);
            return make_result(SequencerCode::PendingByteCapacityExceeded, message.msg_seq_num, entry_expected);
        case InsertResult::InternalInvariantViolation:
            enter_failed_closed(SequencerCode::InternalInvariantViolation);
            return make_result(SequencerCode::InternalInvariantViolation, message.msg_seq_num, entry_expected);
        case InsertResult::NotInitialized:
        case InsertResult::EmptyBody:
        default:
            enter_failed_closed(SequencerCode::InternalInvariantViolation);
            return make_result(SequencerCode::InternalInvariantViolation, message.msg_seq_num, entry_expected);
        }

        // First future message in Running: publish deadline only after successful insert
        if (state_ == SequencerState::Running && !has_deadline_) {
            gap_start_ns_ = event_monotonic_ns;
            gap_deadline_ns_ = event_monotonic_ns + reorder_wait_ns_;
            has_deadline_ = true;
            state_ = SequencerState::GapWait;
        }

        return make_result(SequencerCode::BufferedOutOfOrder, message.msg_seq_num, entry_expected);
    }

    default:
        enter_failed_closed(SequencerCode::InternalInvariantViolation);
        return make_result(SequencerCode::InternalInvariantViolation, message.msg_seq_num, entry_expected);
    }
}

template<class Sink>
SequencerResult DualFeedSequencer::on_time(
    std::uint64_t event_monotonic_ns,
    Sink& /*sink*/
) noexcept {
    static_assert(std::is_nothrow_invocable_r_v<void, Sink&,
        const OrderedMessageMetadata&, std::span<const std::uint8_t>>,
        "sink must be noexcept-callable with (const OrderedMessageMetadata&, std::span<const std::uint8_t>)");

    if (state_ == SequencerState::Uninitialized) {
        return make_result(SequencerCode::NotInitialized, next_expected_, next_expected_);
    }
    if (state_ == SequencerState::Stopped) {
        return make_result(SequencerCode::InvalidTransition, next_expected_, next_expected_);
    }
    if (state_ == SequencerState::FailedClosed) {
        return make_result(SequencerCode::FailedClosed, next_expected_, next_expected_);
    }

    const std::uint32_t entry_expected = next_expected_;

    // clock regression
    if (event_monotonic_ns < last_event_ns_) {
        enter_failed_closed(SequencerCode::ClockRegression);
        return make_result(SequencerCode::ClockRegression, entry_expected, entry_expected);
    }
    last_event_ns_ = event_monotonic_ns;

    if (state_ == SequencerState::Running) {
        return make_result(SequencerCode::NoAction, entry_expected, entry_expected);
    }

    // GapWait
    if (!has_deadline_) {
        return make_result(SequencerCode::GapWaiting, entry_expected, entry_expected);
    }

    if (event_monotonic_ns >= gap_deadline_ns_) {
        enter_failed_closed(SequencerCode::GapConfirmed);
        return make_result(SequencerCode::GapConfirmed, entry_expected, entry_expected);
    }

    return make_result(SequencerCode::GapWaiting, entry_expected, entry_expected);
}

template<class Sink>
SequencerCode DualFeedSequencer::flush_contiguous(Sink& sink) noexcept {
    while (storage_->is_occupied(next_expected_)) {
        auto v = storage_->view_message(next_expected_);
        if (!v.found) break;

        OrderedMessageMetadata meta{};
        meta.msg_seq_num = v.sequence;
        meta.side = v.side;
        meta.capture_index = v.capture_index;
        meta.capture_monotonic_ns = v.capture_monotonic_ns;

        sink(meta, v.body);

        if (!storage_->release(next_expected_)) {
            enter_failed_closed(SequencerCode::InternalInvariantViolation);
            return SequencerCode::InternalInvariantViolation;
        }
        next_expected_ = next_expected_ + 1;
    }

    // Check if GapWait episode ended
    if (state_ == SequencerState::GapWait && storage_->pending_count() == 0) {
        has_deadline_ = false;
        state_ = SequencerState::Running;
    }
    return SequencerCode::NoAction;
}

} // namespace moex::spectra
