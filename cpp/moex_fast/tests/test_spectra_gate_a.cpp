#include "moex_fast/spectra_gate_a.hpp"
#include "test_check.hpp"

#include <cstdio>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

using namespace moex::spectra;

std::vector<std::uint8_t> make_body(std::size_t size, std::uint8_t fill = 0xAB) {
    return std::vector<std::uint8_t>(size, fill);
}

std::vector<std::uint8_t> make_seq_body(std::uint32_t seq, std::size_t size) {
    std::vector<std::uint8_t> body(size);
    for (std::size_t i = 0; i < size; ++i)
        body[i] = static_cast<std::uint8_t>((seq + i) & 0xFF);
    return body;
}

void test_valid_init_and_reset() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    CHECK(!storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    CHECK_EQ(storage.slot_capacity(), 0u);

    storage.initialize(slots, arena, {4, 4 * 64, 64});
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    CHECK_EQ(storage.slot_capacity(), 8u);

    for (std::uint32_t i = 1; i <= 4; ++i)
        CHECK(!storage.is_occupied(i));

    storage.reset();
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    TEST_PASS("test_valid_init_and_reset");
}

void test_zero_max_reorder_messages() {
    std::vector<std::uint8_t> arena(4 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        4, arena, StorageGeometryLimits{0, 4 * 32, 32});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::ZeroMaxReorderMessages));
    TEST_PASS("test_zero_max_reorder_messages");
}

void test_half_range_max_reorder_messages() {
    std::vector<std::uint8_t> arena(4 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        4, arena, StorageGeometryLimits{0x80000000u, 4 * 32, 32});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::MaxReorderMessagesTooLarge));
    TEST_PASS("test_half_range_max_reorder_messages");
}

void test_insufficient_slots() {
    std::vector<std::uint8_t> arena(3 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        3, arena, StorageGeometryLimits{4, 4 * 32, 32});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::SlotCapacityTooSmall));
    TEST_PASS("test_insufficient_slots");
}

void test_zero_max_message_bytes() {
    std::vector<std::uint8_t> arena(4 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        4, arena, StorageGeometryLimits{4, 4 * 32, 0});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::ZeroMaxMessageBytes));
    TEST_PASS("test_zero_max_message_bytes");
}

void test_zero_max_reorder_bytes() {
    std::vector<std::uint8_t> arena(4 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        4, arena, StorageGeometryLimits{4, 0, 32});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::ZeroMaxReorderBytes));
    TEST_PASS("test_zero_max_reorder_bytes");
}

void test_safe_overflow_validation() {
    const std::size_t huge = std::numeric_limits<std::size_t>::max() / 2 + 2;
    std::vector<std::uint8_t> arena(128);
    GeometryCode code = MessageStorage::validate_geometry(
        2, arena, StorageGeometryLimits{2, 128, huge});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::CapacityOverflow));
    TEST_PASS("test_safe_overflow_validation");
}

void test_arena_too_small() {
    std::vector<std::uint8_t> arena(100);
    GeometryCode code = MessageStorage::validate_geometry(
        4, arena, StorageGeometryLimits{4, 4 * 64, 64});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::ArenaTooSmall));
    TEST_PASS("test_arena_too_small");
}

void test_max_reorder_bytes_above_dedicated() {
    std::vector<std::uint8_t> arena(4 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        4, arena, StorageGeometryLimits{4, 4 * 32 + 1, 32});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::MaxReorderBytesExceedsCapacity));
    TEST_PASS("test_max_reorder_bytes_above_dedicated");
}

void test_exact_capacity_boundaries() {
    // Geometry OK: slot_capacity(8) >= max_reorder_messages(4)
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 32);
    GeometryCode code = MessageStorage::validate_geometry(
        8, arena, StorageGeometryLimits{4, 4 * 32, 32});
    CHECK_EQ(static_cast<int>(code), static_cast<int>(GeometryCode::Ok));

    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 32, 32});
    CHECK(storage.initialized());

    // Use non-colliding sequence numbers (1..4 map to indices 1..4 in 8-slot)
    auto body = make_body(32);
    for (std::uint32_t i = 1; i <= 4; ++i) {
        CHECK_EQ(static_cast<int>(storage.insert(i, FeedSide::A, i, i * 100, body)),
                 static_cast<int>(InsertResult::Ok));
    }
    CHECK_EQ(storage.pending_count(), 4u);
    CHECK_EQ(storage.pending_bytes(), 4u * 32);

    // 5th message hits message-count capacity (seq 5 maps to index 5, no collision)
    CHECK_EQ(static_cast<int>(storage.insert(5, FeedSide::A, 5, 500, body)),
             static_cast<int>(InsertResult::PendingMessageCapacityExceeded));
    CHECK_EQ(storage.pending_count(), 4u);
    CHECK_EQ(storage.pending_bytes(), 4u * 32);

    // Body exceeds max_message_bytes
    auto big = make_body(65);
    std::vector<SlotMetadata> slots2(8);
    std::vector<std::uint8_t> arena2(8 * 64);
    MessageStorage storage2;
    storage2.initialize(slots2, arena2, {4, 4 * 64, 64});
    CHECK_EQ(static_cast<int>(storage2.insert(1, FeedSide::A, 1, 100, big)),
             static_cast<int>(InsertResult::BodyTooLarge));
    CHECK_EQ(storage2.pending_count(), 0u);
    CHECK_EQ(storage2.pending_bytes(), 0u);

    // Exact max_message_bytes accepted
    auto exact = make_body(64);
    CHECK_EQ(static_cast<int>(storage2.insert(1, FeedSide::A, 1, 100, exact)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage2.pending_count(), 1u);
    CHECK_EQ(storage2.pending_bytes(), 64u);
    TEST_PASS("test_exact_capacity_boundaries");
}

void test_modulo_lookup_wrap() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    // 0xFFFFFFFA % 8 == 2, so this occupies slot index 2
    auto body = make_seq_body(0xFFFFFFFA, 10);
    CHECK_EQ(static_cast<int>(storage.insert(0xFFFFFFFA, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK(storage.is_occupied(0xFFFFFFFA));
    CHECK_EQ(static_cast<std::size_t>(0xFFFFFFFA) % 8, 2u);

    // An unrelated sequence mapping to a different slot is not occupied
    // seq 3 maps to index 3, not 2
    CHECK(!storage.is_occupied(3));

    const auto& slot = storage.view(0xFFFFFFFA);
    CHECK_EQ(slot.sequence, 0xFFFFFFFAu);
    CHECK(slot.occupied);
    CHECK_EQ(slot.payload_length, 10u);

    // Verify the payload bytes are correct
    const std::uint8_t* stored = arena.data() + slot.payload_offset;
    for (std::size_t i = 0; i < 10; ++i)
        CHECK_EQ(stored[i], static_cast<std::uint8_t>((0xFFFFFFFA + i) & 0xFF));
    TEST_PASS("test_modulo_lookup_wrap");
}

void test_insert_and_view_metadata() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_body(16, 0xCC);
    CHECK_EQ(static_cast<int>(storage.insert(42, FeedSide::B, 100, 9999, body)),
             static_cast<int>(InsertResult::Ok));

    const auto& slot = storage.view(42);
    CHECK_EQ(slot.sequence, 42u);
    CHECK_EQ(static_cast<int>(slot.side), static_cast<int>(FeedSide::B));
    CHECK_EQ(slot.capture_index, 100u);
    CHECK_EQ(slot.capture_monotonic_ns, 9999u);
    CHECK_EQ(slot.payload_length, 16u);
    CHECK(slot.occupied);
    TEST_PASS("test_insert_and_view_metadata");
}

void test_source_bytes_unchanged() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_seq_body(10, 20);
    std::vector<std::uint8_t> original = body;
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK(body == original);
    TEST_PASS("test_source_bytes_unchanged");
}

void test_exactly_one_copy() {
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {2, 2 * 32, 32});

    auto body = make_seq_body(5, 24);
    CHECK_EQ(static_cast<int>(storage.insert(5, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));

    const auto& slot = storage.view(5);
    const std::uint8_t* stored = arena.data() + slot.payload_offset;
    for (std::size_t i = 0; i < body.size(); ++i)
        CHECK_EQ(stored[i], body[i]);

    CHECK_EQ(storage.pending_bytes(), 24u);
    TEST_PASS("test_exactly_one_copy");
}

void test_equal_duplicate() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_body(10, 0xDD);
    CHECK_EQ(static_cast<int>(storage.insert(50, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(static_cast<int>(storage.insert(50, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::DuplicateEqual));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);
    TEST_PASS("test_equal_duplicate");
}

void test_mismatched_duplicate() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_body(10, 0xEE);
    CHECK_EQ(static_cast<int>(storage.insert(50, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));

    auto diff = make_body(10, 0xFF);
    CHECK_EQ(static_cast<int>(storage.insert(50, FeedSide::A, 1, 100, diff)),
             static_cast<int>(InsertResult::DuplicateMismatch));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);

    auto diff_len = make_body(11, 0xEE);
    CHECK_EQ(static_cast<int>(storage.insert(50, FeedSide::A, 1, 100, diff_len)),
             static_cast<int>(InsertResult::DuplicateMismatch));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);
    TEST_PASS("test_mismatched_duplicate");
}

void test_occupied_index_conflict() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body10 = make_seq_body(10, 8);
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 1, 100, body10)),
             static_cast<int>(InsertResult::Ok));

    auto body18 = make_seq_body(18, 8);
    CHECK_EQ(static_cast<int>(storage.insert(18, FeedSide::A, 2, 200, body18)),
             static_cast<int>(InsertResult::InternalInvariantViolation));

    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 8u);
    CHECK(storage.is_occupied(10));

    const auto& slot = storage.view(10);
    CHECK_EQ(slot.sequence, 10u);
    CHECK_EQ(slot.capture_index, 1u);
    TEST_PASS("test_occupied_index_conflict");
}

void test_message_count_capacity() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 32);
    MessageStorage storage;
    storage.initialize(slots, arena, {3, 3 * 32, 32});

    for (std::uint32_t i = 1; i <= 3; ++i) {
        auto body = make_body(10);
        CHECK_EQ(static_cast<int>(storage.insert(i, FeedSide::A, i, i * 100, body)),
                 static_cast<int>(InsertResult::Ok));
    }
    CHECK_EQ(storage.pending_count(), 3u);

    auto body4 = make_body(5);
    CHECK_EQ(static_cast<int>(storage.insert(4, FeedSide::A, 4, 400, body4)),
             static_cast<int>(InsertResult::PendingMessageCapacityExceeded));
    CHECK_EQ(storage.pending_count(), 3u);
    TEST_PASS("test_message_count_capacity");
}

void test_actual_pending_byte_capacity() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 100, 64});

    auto body1 = make_body(60);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body1)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_bytes(), 60u);

    auto body2 = make_body(50);
    CHECK_EQ(static_cast<int>(storage.insert(2, FeedSide::A, 2, 200, body2)),
             static_cast<int>(InsertResult::PendingByteCapacityExceeded));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 60u);

    auto body2ok = make_body(40);
    CHECK_EQ(static_cast<int>(storage.insert(2, FeedSide::A, 2, 200, body2ok)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 2u);
    CHECK_EQ(storage.pending_bytes(), 100u);

    auto body3 = make_body(1);
    CHECK_EQ(static_cast<int>(storage.insert(3, FeedSide::A, 3, 300, body3)),
             static_cast<int>(InsertResult::PendingByteCapacityExceeded));
    CHECK_EQ(storage.pending_count(), 2u);
    CHECK_EQ(storage.pending_bytes(), 100u);
    TEST_PASS("test_actual_pending_byte_capacity");
}

void test_per_message_size_rejection() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 32});

    auto ok = make_body(32);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, ok)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);

    auto too_big = make_body(33);
    CHECK_EQ(static_cast<int>(storage.insert(2, FeedSide::A, 2, 200, too_big)),
             static_cast<int>(InsertResult::BodyTooLarge));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 32u);

    auto huge = make_body(200);
    CHECK_EQ(static_cast<int>(storage.insert(3, FeedSide::A, 3, 300, huge)),
             static_cast<int>(InsertResult::BodyTooLarge));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 32u);
    TEST_PASS("test_per_message_size_rejection");
}

void test_future_style_size_rejection() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 32});

    auto ok_future = make_body(20);
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 1, 100, ok_future)),
             static_cast<int>(InsertResult::Ok));

    auto too_big = make_body(33);
    CHECK_EQ(static_cast<int>(storage.insert(11, FeedSide::B, 2, 200, too_big)),
             static_cast<int>(InsertResult::BodyTooLarge));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 20u);
    CHECK(!storage.is_occupied(11));
    TEST_PASS("test_future_style_size_rejection");
}

void test_no_partial_publication_after_failure() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {2, 2 * 64, 64});

    auto body = make_body(10);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(static_cast<int>(storage.insert(2, FeedSide::A, 2, 200, body)),
             static_cast<int>(InsertResult::Ok));

    CHECK_EQ(static_cast<int>(storage.insert(3, FeedSide::A, 3, 300, body)),
             static_cast<int>(InsertResult::PendingMessageCapacityExceeded));
    CHECK_EQ(storage.pending_count(), 2u);
    CHECK(!storage.is_occupied(3));

    std::vector<SlotMetadata> slots2(8);
    std::vector<std::uint8_t> arena2(8 * 64);
    MessageStorage storage2;
    storage2.initialize(slots2, arena2, {4, 20, 64});

    auto big = make_body(15);
    CHECK_EQ(static_cast<int>(storage2.insert(1, FeedSide::A, 1, 100, big)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage2.pending_bytes(), 15u);

    auto over = make_body(10);
    CHECK_EQ(static_cast<int>(storage2.insert(2, FeedSide::A, 2, 200, over)),
             static_cast<int>(InsertResult::PendingByteCapacityExceeded));
    CHECK_EQ(storage2.pending_count(), 1u);
    CHECK_EQ(storage2.pending_bytes(), 15u);
    CHECK(!storage2.is_occupied(2));

    auto conflict = make_body(5);
    CHECK_EQ(static_cast<int>(storage2.insert(9, FeedSide::A, 3, 300, conflict)),
             static_cast<int>(InsertResult::InternalInvariantViolation));
    CHECK_EQ(storage2.pending_count(), 1u);
    CHECK_EQ(storage2.pending_bytes(), 15u);
    // Slot at index 1 is still occupied by seq 1 (9 % 8 == 1 == 1 % 8)
    CHECK(storage2.is_occupied(1));
    TEST_PASS("test_no_partial_publication_after_failure");
}

void test_release_and_slot_reuse() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body1 = make_body(10, 0x11);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body1)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);

    CHECK(storage.release(1));
    CHECK(!storage.is_occupied(1));
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);

    auto body2 = make_body(20, 0x22);
    CHECK_EQ(static_cast<int>(storage.insert(9, FeedSide::B, 2, 200, body2)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 20u);

    const auto& slot = storage.view(9);
    CHECK_EQ(slot.sequence, 9u);
    CHECK_EQ(static_cast<int>(slot.side), static_cast<int>(FeedSide::B));
    CHECK_EQ(slot.capture_index, 2u);
    CHECK_EQ(slot.capture_monotonic_ns, 200u);
    CHECK_EQ(slot.payload_length, 20u);
    CHECK(slot.occupied);
    TEST_PASS("test_release_and_slot_reuse");
}

void test_deterministic_reset_and_reuse() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_body(10);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(static_cast<int>(storage.insert(5, FeedSide::B, 2, 200, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 2u);
    CHECK_EQ(storage.pending_bytes(), 20u);

    storage.reset();
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    CHECK(!storage.is_occupied(1));
    CHECK(!storage.is_occupied(5));

    // Direct reuse without re-initialize
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 3, 300, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);

    storage.reset();
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    CHECK(!storage.is_occupied(1));
    TEST_PASS("test_deterministic_reset_and_reuse");
}

void test_noexcept_and_noncopyable() {
    static_assert(!std::is_copy_constructible_v<MessageStorage>);
    static_assert(!std::is_copy_assignable_v<MessageStorage>);
    static_assert(!std::is_move_constructible_v<MessageStorage>);
    static_assert(!std::is_move_assignable_v<MessageStorage>);

    static_assert(noexcept(std::declval<MessageStorage&>().initialize(
        std::declval<std::span<SlotMetadata>>(),
        std::declval<std::span<std::uint8_t>>(),
        std::declval<MessageStorageConfig>())));

    static_assert(noexcept(std::declval<MessageStorage&>().insert(
        std::declval<std::uint32_t>(),
        std::declval<FeedSide>(),
        std::declval<std::uint64_t>(),
        std::declval<std::uint64_t>(),
        std::declval<std::span<const std::uint8_t>>())));

    static_assert(noexcept(std::declval<const MessageStorage&>().view(
        std::declval<std::uint32_t>())));

    static_assert(noexcept(std::declval<MessageStorage&>().release(
        std::declval<std::uint32_t>())));

    static_assert(noexcept(std::declval<MessageStorage&>().reset()));

    static_assert(noexcept(std::declval<const MessageStorage&>().is_occupied(
        std::declval<std::uint32_t>())));

    static_assert(noexcept(std::declval<const MessageStorage&>().pending_count()));

    static_assert(noexcept(std::declval<const MessageStorage&>().pending_bytes()));

    static_assert(noexcept(MessageStorage::validate_geometry(
        std::declval<std::size_t>(),
        std::declval<std::span<const std::uint8_t>>(),
        std::declval<StorageGeometryLimits>())));

    TEST_PASS("test_noexcept_and_noncopyable");
}

void test_pre_init_safety() {
    MessageStorage storage;
    CHECK(!storage.is_occupied(0));
    CHECK(!storage.is_occupied(1));

    auto v = storage.view_message(0);
    CHECK(!v.found);
    CHECK_EQ(v.sequence, 0u);
    CHECK(v.body.empty());

    CHECK(!storage.release(0));

    storage.reset();
    CHECK(!storage.initialized());

    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    storage.initialize(slots, arena, {4, 4 * 64, 64});
    CHECK(storage.initialized());
    CHECK(!storage.is_occupied(0));
    TEST_PASS("test_pre_init_safety");
}

void test_colliding_sequence_not_occupied() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_seq_body(10, 8);
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));

    // seq 18 collides with seq 10 (both map to index 2 in 8-slot)
    CHECK_EQ(static_cast<std::size_t>(10) % 8, static_cast<std::size_t>(18) % 8);
    CHECK(!storage.is_occupied(18));

    auto v = storage.view_message(18);
    CHECK(!v.found);
    CHECK(v.body.empty());

    CHECK(storage.is_occupied(10));
    TEST_PASS("test_colliding_sequence_not_occupied");
}

void test_release_colliding_returns_false() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_body(10, 0xAA);
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);

    // Releasing colliding seq returns false, preserves original
    CHECK(!storage.release(18));
    CHECK(storage.is_occupied(10));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);

    const auto& slot = storage.view(10);
    CHECK_EQ(slot.sequence, 10u);
    CHECK_EQ(slot.payload_length, 10u);
    TEST_PASS("test_release_colliding_returns_false");
}

void test_release_reuse_preserves_slice() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    // Insert seq 1 at index 1, payload at offset 1*64=64
    auto body1 = make_body(10, 0x11);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body1)),
             static_cast<int>(InsertResult::Ok));

    // Insert seq 2 at index 2, payload at offset 2*64=128
    auto body2 = make_body(10, 0x22);
    CHECK_EQ(static_cast<int>(storage.insert(2, FeedSide::A, 2, 200, body2)),
             static_cast<int>(InsertResult::Ok));

    // Capture payload pointers before release
    const auto& s1 = storage.view(1);
    const std::uint8_t* ptr1 = arena.data() + s1.payload_offset;
    const auto& s2 = storage.view(2);
    const std::uint8_t* ptr2 = arena.data() + s2.payload_offset;

    CHECK_EQ(s1.payload_offset, static_cast<std::size_t>(64));
    CHECK_EQ(s2.payload_offset, static_cast<std::size_t>(128));

    // Release seq 1, reuse slot for seq 9 (same index 1)
    CHECK(storage.release(1));
    CHECK(!storage.is_occupied(1));

    auto body9 = make_body(10, 0x99);
    CHECK_EQ(static_cast<int>(storage.insert(9, FeedSide::B, 3, 300, body9)),
             static_cast<int>(InsertResult::Ok));

    // seq 9 writes to the same dedicated slice as seq 1
    const auto& s9 = storage.view(9);
    CHECK_EQ(s9.payload_offset, static_cast<std::size_t>(64));
    for (std::size_t i = 0; i < 10; ++i)
        CHECK_EQ(ptr1[i], static_cast<std::uint8_t>(0x99));

    // seq 2's slice is unchanged
    for (std::size_t i = 0; i < 10; ++i)
        CHECK_EQ(ptr2[i], static_cast<std::uint8_t>(0x22));

    TEST_PASS("test_release_reuse_preserves_slice");
}

void test_reset_leaves_initialized_and_reusable() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_body(10);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(static_cast<int>(storage.insert(5, FeedSide::B, 2, 200, body)),
             static_cast<int>(InsertResult::Ok));

    storage.reset();
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    CHECK(!storage.is_occupied(1));
    CHECK(!storage.is_occupied(5));

    // Direct insert without re-initialize
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 3, 300, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);
    TEST_PASS("test_reset_leaves_initialized_and_reusable");
}

void test_borrowed_view_metadata() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    auto body = make_seq_body(42, 16);
    CHECK_EQ(static_cast<int>(storage.insert(42, FeedSide::B, 100, 9999, body)),
             static_cast<int>(InsertResult::Ok));

    auto v = storage.view_message(42);
    CHECK(v.found);
    CHECK_EQ(v.sequence, 42u);
    CHECK_EQ(static_cast<int>(v.side), static_cast<int>(FeedSide::B));
    CHECK_EQ(v.capture_index, 100u);
    CHECK_EQ(v.capture_monotonic_ns, 9999u);
    CHECK_EQ(v.body.size(), 16u);

    // Body pointer must point into the arena
    CHECK(v.body.data() >= arena.data());
    CHECK(v.body.data() + v.body.size() <= arena.data() + arena.size());

    // Verify payload bytes
    for (std::size_t i = 0; i < 16; ++i)
        CHECK_EQ(v.body[i], static_cast<std::uint8_t>((42 + i) & 0xFF));

    TEST_PASS("test_borrowed_view_metadata");
}

void test_empty_view_absent_and_colliding() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    storage.initialize(slots, arena, {4, 4 * 64, 64});

    // Absent sequence
    auto v1 = storage.view_message(5);
    CHECK(!v1.found);
    CHECK_EQ(v1.sequence, 0u);
    CHECK(v1.body.empty());

    // Insert seq 10, then check colliding seq 18
    auto body = make_body(8);
    CHECK_EQ(static_cast<int>(storage.insert(10, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));

    auto v2 = storage.view_message(18);
    CHECK(!v2.found);
    CHECK_EQ(v2.sequence, 0u);
    CHECK(v2.body.empty());

    // Exact match still works
    auto v3 = storage.view_message(10);
    CHECK(v3.found);
    CHECK_EQ(v3.sequence, 10u);
    CHECK_EQ(v3.body.size(), 8u);

    TEST_PASS("test_empty_view_absent_and_colliding");
}

void test_noncopyable_nonmovable() {
    static_assert(!std::is_copy_constructible_v<MessageStorage>);
    static_assert(!std::is_copy_assignable_v<MessageStorage>);
    static_assert(!std::is_move_constructible_v<MessageStorage>);
    static_assert(!std::is_move_assignable_v<MessageStorage>);
    TEST_PASS("test_noncopyable_nonmovable");
}

void test_checked_pending_byte_helper() {
    // Basic: fits
    CHECK(MessageStorage::can_add_pending_bytes(0, 10, 10));
    CHECK(MessageStorage::can_add_pending_bytes(5, 5, 10));
    CHECK(MessageStorage::can_add_pending_bytes(10, 0, 10));

    // Basic: doesn't fit
    CHECK(!MessageStorage::can_add_pending_bytes(5, 6, 10));
    CHECK(!MessageStorage::can_add_pending_bytes(10, 1, 10));

    // Zero addition
    CHECK(MessageStorage::can_add_pending_bytes(0, 0, 0));

    // SIZE_MAX boundary
    constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
    CHECK(MessageStorage::can_add_pending_bytes(0, max, max));
    CHECK(!MessageStorage::can_add_pending_bytes(1, max, max));
    CHECK(!MessageStorage::can_add_pending_bytes(max, 1, max));

    // current > limit is always false
    CHECK(!MessageStorage::can_add_pending_bytes(11, 0, 10));

    // Edge: addition is 0, current == limit
    CHECK(MessageStorage::can_add_pending_bytes(100, 0, 100));

    // Edge: addition exactly fills
    CHECK(MessageStorage::can_add_pending_bytes(99, 1, 100));
    CHECK(!MessageStorage::can_add_pending_bytes(99, 2, 100));

    TEST_PASS("test_checked_pending_byte_helper");
}

} // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
int main() {
    test_valid_init_and_reset();
    test_zero_max_reorder_messages();
    test_half_range_max_reorder_messages();
    test_insufficient_slots();
    test_zero_max_message_bytes();
    test_zero_max_reorder_bytes();
    test_safe_overflow_validation();
    test_arena_too_small();
    test_max_reorder_bytes_above_dedicated();
    test_exact_capacity_boundaries();
    test_modulo_lookup_wrap();
    test_insert_and_view_metadata();
    test_source_bytes_unchanged();
    test_exactly_one_copy();
    test_equal_duplicate();
    test_mismatched_duplicate();
    test_occupied_index_conflict();
    test_message_count_capacity();
    test_actual_pending_byte_capacity();
    test_per_message_size_rejection();
    test_future_style_size_rejection();
    test_no_partial_publication_after_failure();
    test_release_and_slot_reuse();
    test_deterministic_reset_and_reuse();
    test_noexcept_and_noncopyable();
    test_pre_init_safety();
    test_colliding_sequence_not_occupied();
    test_release_colliding_returns_false();
    test_release_reuse_preserves_slice();
    test_reset_leaves_initialized_and_reusable();
    test_borrowed_view_metadata();
    test_empty_view_absent_and_colliding();
    test_noncopyable_nonmovable();
    test_checked_pending_byte_helper();

    std::printf("All %d test cases passed.\n", 35);
    return 0;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
