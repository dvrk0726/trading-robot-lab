#include "moex_fast/spectra_gate_a.hpp"
#include "test_check.hpp"

#include <cstdio>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

using namespace moex::spectra;

// --- Test helpers ---

#define CHECK_STORAGE_INIT(result) \
    CHECK_EQ(static_cast<int>((result).code), static_cast<int>(StorageInitCode::Ok)); \
    CHECK_EQ(static_cast<int>((result).geometry_code), static_cast<int>(GeometryCode::Ok))

std::vector<std::uint8_t> make_body(std::size_t size, std::uint8_t fill = 0xAB) {
    return std::vector<std::uint8_t>(size, fill);
}

std::vector<std::uint8_t> make_seq_body(std::uint32_t seq, std::size_t size) {
    std::vector<std::uint8_t> body(size);
    for (std::size_t i = 0; i < size; ++i)
        body[i] = static_cast<std::uint8_t>((seq + i) & 0xFF);
    return body;
}

constexpr std::uint32_t FEED_ID = 42;

struct EmissionSink {
    struct Entry {
        std::uint32_t sequence;
        FeedSide side;
        std::uint64_t capture_index;
        std::uint64_t capture_monotonic_ns;
        std::vector<std::uint8_t> body;
    };
    std::vector<Entry>* log;
    void operator()(const OrderedMessageMetadata& meta, std::span<const std::uint8_t> body) noexcept {
        log->push_back({meta.msg_seq_num, meta.side, meta.capture_index,
                        meta.capture_monotonic_ns,
                        std::vector<std::uint8_t>(body.begin(), body.end())});
    }
};

struct SequencerFixture {
    std::vector<SlotMetadata> slots;
    std::vector<std::uint8_t> arena;
    MessageStorage storage;
    DualFeedSequencer seq;
    std::vector<EmissionSink::Entry> emissions;
    EmissionSink sink;

    SequencerFixture(std::uint32_t max_reorder = 4,
                     std::size_t max_msg_bytes = 64,
                     std::uint64_t wait_ns = 1000)
        : slots(max_reorder), arena(max_reorder * max_msg_bytes), sink{&emissions} {
        auto init_result = storage.initialize(slots, arena,
                           {max_reorder, max_reorder * max_msg_bytes, max_msg_bytes});
        if (init_result.code != StorageInitCode::Ok ||
            init_result.geometry_code != GeometryCode::Ok)
            std::abort();
        SequencerConfig cfg{};
        cfg.logical_feed.value = FEED_ID;
        cfg.max_reorder_distance = max_reorder;
        cfg.reorder_wait_ns = wait_ns;
        cfg.storage = {max_reorder, max_reorder * max_msg_bytes, max_msg_bytes};
        (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
        (void)seq.start(1);
    }
};

FramedMessageView make_view(std::uint32_t seq_num, FeedSide side,
                             std::span<const std::uint8_t> body,
                             std::uint64_t cap_idx = 0) {
    FramedMessageView v{};
    v.feed.value = FEED_ID;
    v.side = side;
    v.msg_seq_num = seq_num;
    v.capture_index = cap_idx;
    v.capture_monotonic_ns = cap_idx * 100;
    v.fast_body = body;
    return v;
}

// --- MessageStorage tests (Phase 1, preserved) ---

void test_valid_init_and_reset() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    CHECK(!storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);
    CHECK_EQ(storage.pending_bytes(), 0u);
    CHECK_EQ(storage.slot_capacity(), 0u);

    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));
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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 32, 32}));
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
    CHECK_STORAGE_INIT(storage2.initialize(slots2, arena2, {4, 4 * 64, 64}));
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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {2, 2 * 32, 32}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {3, 3 * 32, 32}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 100, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 32}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 32}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {2, 2 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage2.initialize(slots2, arena2, {4, 20, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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

    static_assert(std::is_same_v<
        decltype(std::declval<MessageStorage&>().initialize(
            std::declval<std::span<SlotMetadata>>(),
            std::declval<std::span<std::uint8_t>>(),
            std::declval<MessageStorageConfig>())),
        StorageInitResult>);

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));
    CHECK(storage.initialized());
    CHECK(!storage.is_occupied(0));
    TEST_PASS("test_pre_init_safety");
}

void test_colliding_sequence_not_occupied() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

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

// --- DualFeedSequencer tests ---

// Lifecycle tests

void test_sequencer_init_start_running() {
    SequencerFixture f;
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Running));
    CHECK_EQ(f.seq.next_expected_seq(), 1u);
    TEST_PASS("test_sequencer_init_start_running");
}

void test_sequencer_reset_preserves_config_restart() {
    auto body1 = make_body(10);
    SequencerFixture f;
    auto r1 = f.seq.on_message(make_view(3, FeedSide::A, body1), 100, f.sink);
    CHECK_EQ(static_cast<int>(r1.code), static_cast<int>(SequencerCode::BufferedOutOfOrder));
    CHECK_EQ(f.storage.pending_count(), 1u);

    CHECK_EQ(static_cast<int>(f.seq.reset()), static_cast<int>(SequencerCode::Reset));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Stopped));
    CHECK_EQ(f.storage.pending_count(), 0u);

    CHECK_EQ(static_cast<int>(f.seq.start(10)), static_cast<int>(SequencerCode::Started));
    CHECK_EQ(f.seq.next_expected_seq(), 10u);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Running));
    TEST_PASS("test_sequencer_reset_preserves_config_restart");
}

void test_sequencer_failed_closed_requires_reset() {
    SequencerFixture f;
    auto body = make_body(10);
    auto wrong = make_view(99, FeedSide::A, body);
    wrong.feed.value = 999;
    (void)f.seq.on_message(wrong, 100, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));

    auto r = f.seq.on_message(make_view(1, FeedSide::A, body), 200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::FailedClosed));

    auto rt = f.seq.on_time(300, f.sink);
    CHECK_EQ(static_cast<int>(rt.code), static_cast<int>(SequencerCode::FailedClosed));

    CHECK_EQ(static_cast<int>(f.seq.reset()), static_cast<int>(SequencerCode::Reset));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Stopped));
    TEST_PASS("test_sequencer_failed_closed_requires_reset");
}

void test_sequencer_invalid_init_remains_uninitialized() {
    DualFeedSequencer seq;
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));
    SequencerConfig bad{};
    bad.logical_feed.value = 1;
    bad.max_reorder_distance = 0;
    bad.reorder_wait_ns = 1000;
    bad.storage = {4, 4 * 64, 64};
    CHECK_EQ(static_cast<int>(seq.initialize(LogicalFeedId{1}, bad, storage)),
             static_cast<int>(SequencerCode::InvalidConfig));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    TEST_PASS("test_sequencer_invalid_init_remains_uninitialized");
}

void test_sequencer_calls_before_init() {
    DualFeedSequencer seq;
    auto body = make_body(10);
    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    auto r = seq.on_message(make_view(1, FeedSide::A, body), 100, sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::NotInitialized));

    auto rt = seq.on_time(100, sink);
    CHECK_EQ(static_cast<int>(rt.code), static_cast<int>(SequencerCode::NotInitialized));

    CHECK_EQ(static_cast<int>(seq.reset()), static_cast<int>(SequencerCode::NotInitialized));
    CHECK_EQ(static_cast<int>(seq.start(1)), static_cast<int>(SequencerCode::NotInitialized));
    TEST_PASS("test_sequencer_calls_before_init");
}

void test_sequencer_no_emission_after_failed_closed() {
    SequencerFixture f;
    auto body = make_body(10);
    auto bad = make_view(99, FeedSide::A, body);
    bad.feed.value = 999;
    (void)f.seq.on_message(bad, 100, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    f.emissions.clear();

    auto r = f.seq.on_message(make_view(1, FeedSide::A, body), 200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::FailedClosed));
    CHECK(f.emissions.empty());

    auto rt = f.seq.on_time(300, f.sink);
    CHECK_EQ(static_cast<int>(rt.code), static_cast<int>(SequencerCode::FailedClosed));
    CHECK(f.emissions.empty());
    TEST_PASS("test_sequencer_no_emission_after_failed_closed");
}

// Ordered/A-B behavior tests

void test_ab_first_valid_copy_wins() {
    SequencerFixture f;
    auto bodyA = make_body(10, 0xAA);
    auto bodyB = make_body(10, 0xBB);
    auto rA = f.seq.on_message(make_view(1, FeedSide::A, bodyA, 1), 100, f.sink);
    CHECK_EQ(static_cast<int>(rA.code), static_cast<int>(SequencerCode::Emitted));
    CHECK_EQ(f.emissions.size(), 1u);
    CHECK_EQ(static_cast<int>(f.emissions[0].side), static_cast<int>(FeedSide::A));

    (void)f.seq.on_message(make_view(2, FeedSide::B, bodyB, 2), 200, f.sink);
    CHECK_EQ(f.emissions.size(), 2u);
    CHECK_EQ(static_cast<int>(f.emissions[1].side), static_cast<int>(FeedSide::B));
    TEST_PASS("test_ab_first_valid_copy_wins");
}

void test_alternating_winning_side() {
    SequencerFixture f;
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(1, FeedSide::A, body, 1), 100, f.sink);
    (void)f.seq.on_message(make_view(2, FeedSide::B, body, 2), 200, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 300, f.sink);
    CHECK_EQ(f.emissions.size(), 3u);
    CHECK_EQ(static_cast<int>(f.emissions[0].side), static_cast<int>(FeedSide::A));
    CHECK_EQ(static_cast<int>(f.emissions[1].side), static_cast<int>(FeedSide::B));
    CHECK_EQ(static_cast<int>(f.emissions[2].side), static_cast<int>(FeedSide::A));
    TEST_PASS("test_alternating_winning_side");
}

void test_duplicate_second_copy_stale_dropped() {
    SequencerFixture f;
    auto body = make_body(10);
    (void)f.seq.on_message(make_view(1, FeedSide::A, body, 1), 100, f.sink);
    CHECK_EQ(f.emissions.size(), 1u);

    auto r = f.seq.on_message(make_view(1, FeedSide::B, body, 2), 200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::DuplicateDropped));
    CHECK_EQ(f.emissions.size(), 1u);
    TEST_PASS("test_duplicate_second_copy_stale_dropped");
}

void test_wrong_logical_feed() {
    SequencerFixture f;
    auto body = make_body(10);
    auto bad = make_view(1, FeedSide::A, body);
    bad.feed.value = 999;
    auto r = f.seq.on_message(bad, 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::WrongLogicalFeed));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_wrong_logical_feed");
}

void test_invalid_side_and_empty_body() {
    SequencerFixture f;
    auto body = make_body(10);
    auto bad_side = make_view(1, FeedSide::A, body);
    bad_side.side = static_cast<FeedSide>(99);
    auto r1 = f.seq.on_message(bad_side, 100, f.sink);
    CHECK_EQ(static_cast<int>(r1.code), static_cast<int>(SequencerCode::InternalInvariantViolation));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));

    SequencerFixture f2;
    auto empty = make_view(1, FeedSide::A, std::span<const std::uint8_t>{});
    auto r2 = f2.seq.on_message(empty, 100, f2.sink);
    CHECK_EQ(static_cast<int>(r2.code), static_cast<int>(SequencerCode::InternalInvariantViolation));
    CHECK_EQ(static_cast<int>(f2.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_invalid_side_and_empty_body");
}

// Reordering tests

void test_reorder_1_3_2() {
    SequencerFixture f;
    auto body1 = make_body(10, 0x01);
    auto body2 = make_body(10, 0x02);
    auto body3 = make_body(10, 0x03);

    (void)f.seq.on_message(make_view(1, FeedSide::A, body1, 1), 100, f.sink);
    CHECK_EQ(f.emissions.size(), 1u);

    (void)f.seq.on_message(make_view(3, FeedSide::A, body3, 3), 200, f.sink);
    CHECK_EQ(f.emissions.size(), 1u);
    CHECK_EQ(f.storage.pending_count(), 1u);

    (void)f.seq.on_message(make_view(2, FeedSide::A, body2, 2), 300, f.sink);
    CHECK_EQ(f.emissions.size(), 3u);
    CHECK_EQ(f.emissions[0].sequence, 1u);
    CHECK_EQ(f.emissions[1].sequence, 2u);
    CHECK_EQ(f.emissions[2].sequence, 3u);
    CHECK_EQ(f.storage.pending_count(), 0u);
    TEST_PASS("test_reorder_1_3_2");
}

void test_reorder_1_4_3_2_contiguous_flush() {
    SequencerFixture f(8);
    auto body1 = make_body(5, 0x01);
    auto body2 = make_body(5, 0x02);
    auto body3 = make_body(5, 0x03);
    auto body4 = make_body(5, 0x04);

    (void)f.seq.on_message(make_view(1, FeedSide::A, body1, 1), 100, f.sink);
    (void)f.seq.on_message(make_view(4, FeedSide::A, body4, 4), 200, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body3, 3), 300, f.sink);
    CHECK_EQ(f.emissions.size(), 1u);

    (void)f.seq.on_message(make_view(2, FeedSide::A, body2, 2), 400, f.sink);
    CHECK_EQ(f.emissions.size(), 4u);
    CHECK_EQ(f.emissions[0].sequence, 1u);
    CHECK_EQ(f.emissions[1].sequence, 2u);
    CHECK_EQ(f.emissions[2].sequence, 3u);
    CHECK_EQ(f.emissions[3].sequence, 4u);
    TEST_PASS("test_reorder_1_4_3_2_contiguous_flush");
}

void test_deeper_contiguous_flush() {
    SequencerFixture f(8);
    for (std::uint32_t s = 1; s <= 5; ++s) {
        auto body = make_body(5, static_cast<std::uint8_t>(s));
        if (s == 1) {
            (void)f.seq.on_message(make_view(s, FeedSide::A, body, s), s * 100, f.sink);
        }
    }
    CHECK_EQ(f.emissions.size(), 1u);

    auto b5 = make_body(5, 0x05);
    (void)f.seq.on_message(make_view(5, FeedSide::A, b5, 5), 500, f.sink);
    auto b4 = make_body(5, 0x04);
    (void)f.seq.on_message(make_view(4, FeedSide::A, b4, 4), 600, f.sink);
    auto b3 = make_body(5, 0x03);
    (void)f.seq.on_message(make_view(3, FeedSide::A, b3, 3), 700, f.sink);
    CHECK_EQ(f.emissions.size(), 1u);

    auto b2 = make_body(5, 0x02);
    (void)f.seq.on_message(make_view(2, FeedSide::A, b2, 2), 800, f.sink);
    CHECK_EQ(f.emissions.size(), 5u);
    for (std::uint32_t i = 0; i < 5; ++i)
        CHECK_EQ(f.emissions[i].sequence, i + 1);
    TEST_PASS("test_deeper_contiguous_flush");
}

void test_future_duplicate_equal_bytes() {
    SequencerFixture f;
    auto body = make_body(10, 0xCC);

    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);
    CHECK_EQ(f.storage.pending_count(), 1u);

    auto r = f.seq.on_message(make_view(3, FeedSide::B, body, 4), 200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::DuplicateDropped));
    CHECK_EQ(f.storage.pending_count(), 1u);
    TEST_PASS("test_future_duplicate_equal_bytes");
}

void test_future_duplicate_different_length() {
    SequencerFixture f;
    auto body8 = make_body(8, 0xCC);
    auto body10 = make_body(10, 0xCC);

    (void)f.seq.on_message(make_view(3, FeedSide::A, body8, 3), 100, f.sink);
    CHECK_EQ(f.storage.pending_count(), 1u);

    auto r = f.seq.on_message(make_view(3, FeedSide::B, body10, 4), 200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::DuplicatePayloadMismatch));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_future_duplicate_different_length");
}

void test_future_duplicate_different_bytes() {
    SequencerFixture f;
    auto bodyA = make_body(10, 0xCC);
    auto bodyB = make_body(10, 0xDD);

    (void)f.seq.on_message(make_view(3, FeedSide::A, bodyA, 3), 100, f.sink);
    auto r = f.seq.on_message(make_view(3, FeedSide::B, bodyB, 4), 200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::DuplicatePayloadMismatch));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_future_duplicate_different_bytes");
}

void test_pending_metadata_preserved() {
    SequencerFixture f;
    auto body = make_seq_body(3, 10);
    (void)f.seq.on_message(make_view(3, FeedSide::B, body, 100), 500, f.sink);

    auto v = f.storage.view_message(3);
    CHECK(v.found);
    CHECK_EQ(v.sequence, 3u);
    CHECK_EQ(static_cast<int>(v.side), static_cast<int>(FeedSide::B));
    CHECK_EQ(v.capture_index, 100u);
    CHECK_EQ(v.capture_monotonic_ns, 10000u);
    TEST_PASS("test_pending_metadata_preserved");
}

void test_future_beyond_window() {
    SequencerFixture f(4);
    auto body = make_body(10);
    auto r = f.seq.on_message(make_view(10, FeedSide::A, body, 10), 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::ReorderDistanceExceeded));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_future_beyond_window");
}

void test_ambiguous_half_range() {
    SequencerFixture f(4);
    auto body = make_body(10);
    auto r = f.seq.on_message(
        make_view(1u + 0x80000000u, FeedSide::A, body, 1), 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::AmbiguousSequenceRelation));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_ambiguous_half_range");
}

void test_natural_uint32_wrap() {
    SequencerFixture f(8);
    std::vector<SlotMetadata> slots2(8);
    std::vector<std::uint8_t> arena2(8 * 64);
    MessageStorage storage2;
    CHECK_STORAGE_INIT(storage2.initialize(slots2, arena2, {4, 4 * 64, 64}));

    DualFeedSequencer seq2;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};
    (void)seq2.initialize(LogicalFeedId{FEED_ID}, cfg, storage2);
    (void)seq2.start(0xFFFFFFFEu);

    std::vector<EmissionSink::Entry> log2;
    EmissionSink sink2{&log2};

    auto b_max = make_body(5, 0xFE);
    (void)seq2.on_message(make_view(0xFFFFFFFFu, FeedSide::A, b_max, 1), 100, sink2);
    CHECK_EQ(log2.size(), 0u);

    auto b_zero = make_body(5, 0x00);
    (void)seq2.on_message(make_view(0u, FeedSide::A, b_zero, 2), 200, sink2);
    CHECK_EQ(log2.size(), 0u);

    auto b_expected = make_body(5, 0xFE);
    (void)seq2.on_message(make_view(0xFFFFFFFEu, FeedSide::A, b_expected, 3), 300, sink2);
    CHECK_EQ(log2.size(), 3u);
    CHECK_EQ(log2[0].sequence, 0xFFFFFFFEu);
    CHECK_EQ(log2[1].sequence, 0xFFFFFFFFu);
    CHECK_EQ(log2[2].sequence, 0u);
    CHECK_EQ(seq2.next_expected_seq(), 1u);
    TEST_PASS("test_natural_uint32_wrap");
}

void test_modulo_slot_lookup_across_wrap() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(0xFFFFFFFCu);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    auto body_fe = make_body(5, 0xFE);
    (void)seq.on_message(make_view(0xFFFFFFFEu, FeedSide::A, body_fe, 1), 100, sink);

    CHECK_EQ(static_cast<std::size_t>(0xFFFFFFFEu) % 8,
              static_cast<std::size_t>(6u) % 8);
    CHECK(storage.is_occupied(0xFFFFFFFEu));
    TEST_PASS("test_modulo_slot_lookup_across_wrap");
}

// Capacity tests

void test_pending_message_capacity_precedes_byte_capacity() {
    // Config: max_reorder_messages=3, max_reorder_bytes=3*32=96, max_message_bytes=32.
    // max_reorder_distance=8 so seq 6 is FutureWithinWindow (delta=5 <= 8).
    // Fill all 3 pending-message slots with small bodies, then submit a 4th message
    // that is also oversized (>32 bytes). PendingMessageCapacityExceeded must win.
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 32);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {3, 3 * 32, 32}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 8;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {3, 3 * 32, 32};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(1);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    auto small = make_body(5);
    (void)seq.on_message(make_view(3, FeedSide::A, small, 3), 100, sink);
    (void)seq.on_message(make_view(4, FeedSide::A, small, 4), 200, sink);
    (void)seq.on_message(make_view(5, FeedSide::A, small, 5), 300, sink);
    CHECK_EQ(storage.pending_count(), 3u);

    auto oversized = make_body(40);
    auto r = seq.on_message(make_view(6, FeedSide::A, oversized, 6), 400, sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::PendingMessageCapacityExceeded));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::FailedClosed));
    CHECK_EQ(storage.pending_count(), 3u);
    CHECK_EQ(log.size(), 0u);
    CHECK(!storage.is_occupied(6));
    TEST_PASS("test_pending_message_capacity_precedes_byte_capacity");
}

void test_pending_byte_limit() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 50, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 50, 64};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(1);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    auto big = make_body(30);
    (void)seq.on_message(make_view(3, FeedSide::A, big, 3), 100, sink);
    CHECK_EQ(storage.pending_bytes(), 30u);

    auto over = make_body(35);
    auto r = seq.on_message(make_view(4, FeedSide::A, over, 4), 200, sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::PendingByteCapacityExceeded));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_pending_byte_limit");
}

void test_per_message_body_limit() {
    SequencerFixture f(4, 32, 1000);
    auto too_big = make_body(33);
    auto r = f.seq.on_message(make_view(3, FeedSide::A, too_big, 3), 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::PendingByteCapacityExceeded));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_per_message_body_limit");
}

void test_storage_slot_conflict_invariant() {
    // Valid config: slot_capacity >= max_reorder_distance.
    // Simulate the impossible conflicting slot after successful initialization.
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {8, 8 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 8;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {8, 8 * 64, 64};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(1);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    // Directly insert seq 1 into storage, creating an impossible state
    // where the expected sequence occupies a slot.
    auto body1 = make_body(5);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body1)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);

    // seq 9 hashes to same slot as seq 1 (9%8=1, 1%8=1) and is within window (delta=8)
    auto body9 = make_body(5);
    auto r = seq.on_message(make_view(9, FeedSide::A, body9, 9), 100, sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::InternalInvariantViolation));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_storage_slot_conflict_invariant");
}

void test_no_partial_state_on_failure() {
    // Custom fixture: max_reorder_messages=2 but max_reorder_distance=4
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {2, 2 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {2, 2 * 64, 64};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(1);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    auto body = make_body(5);
    (void)seq.on_message(make_view(3, FeedSide::A, body, 3), 100, sink);
    (void)seq.on_message(make_view(4, FeedSide::A, body, 4), 200, sink);
    CHECK_EQ(storage.pending_count(), 2u);

    auto r = seq.on_message(make_view(5, FeedSide::A, body, 5), 300, sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::PendingMessageCapacityExceeded));
    CHECK_EQ(storage.pending_count(), 2u);
    CHECK_EQ(log.size(), 0u);
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_no_partial_state_on_failure");
}

// Deadline/time tests

void test_first_future_creates_deadline() {
    SequencerFixture f(4, 64, 1000);
    auto body = make_body(5);
    auto r = f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::BufferedOutOfOrder));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));
    TEST_PASS("test_first_future_creates_deadline");
}

void test_later_future_does_not_extend_deadline() {
    SequencerFixture f(8, 64, 1000);
    auto body3 = make_body(5, 0x03);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body3, 3), 100, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));

    auto body4 = make_body(5, 0x04);
    (void)f.seq.on_message(make_view(4, FeedSide::A, body4, 4), 500, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));

    auto r = f.seq.on_time(1100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::GapConfirmed));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_later_future_does_not_extend_deadline");
}

void test_equal_duplicate_does_not_extend_deadline() {
    SequencerFixture f(8, 64, 1000);
    auto body = make_body(5, 0xCC);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::B, body, 4), 500, f.sink);

    auto r = f.seq.on_time(1100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::GapConfirmed));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_equal_duplicate_does_not_extend_deadline");
}

void test_partial_resolution_does_not_extend_deadline() {
    SequencerFixture f(8, 64, 1000);
    auto body1 = make_body(5, 0x01);
    auto body2 = make_body(5, 0x02);
    auto body3 = make_body(5, 0x03);
    auto body5 = make_body(5, 0x05);

    (void)f.seq.on_message(make_view(1, FeedSide::A, body1, 1), 100, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body3, 3), 200, f.sink);
    (void)f.seq.on_message(make_view(5, FeedSide::A, body5, 5), 300, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));
    CHECK_EQ(f.storage.pending_count(), 2u);

    // Partial resolution: seq 2 arrives, flushes 2 and 3, but seq 5 remains pending
    (void)f.seq.on_message(make_view(2, FeedSide::A, body2, 500), 500, f.sink);
    CHECK_EQ(f.storage.pending_count(), 1u);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));

    // Original deadline was 200 + 1000 = 1200. Must not be extended.
    auto r = f.seq.on_time(1200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::GapConfirmed));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_partial_resolution_does_not_extend_deadline");
}

void test_pending_becomes_empty_returns_running() {
    SequencerFixture f(8, 64, 1000);
    auto body1 = make_body(5, 0x01);
    auto body3 = make_body(5, 0x03);
    auto body2 = make_body(5, 0x02);

    (void)f.seq.on_message(make_view(1, FeedSide::A, body1, 1), 100, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body3, 3), 200, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));

    (void)f.seq.on_message(make_view(2, FeedSide::A, body2, 2), 300, f.sink);
    CHECK_EQ(f.storage.pending_count(), 0u);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Running));
    TEST_PASS("test_pending_becomes_empty_returns_running");
}

void test_ontime_before_deadline_gap_waiting() {
    SequencerFixture f(8, 64, 1000);
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);

    auto r = f.seq.on_time(500, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::GapWaiting));
    CHECK_EQ(r.observed_seq, 1u);
    CHECK_EQ(r.expected_seq, 1u);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));
    TEST_PASS("test_ontime_before_deadline_gap_waiting");
}

void test_ontime_running_no_action() {
    SequencerFixture f;
    auto r = f.seq.on_time(100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::NoAction));
    CHECK_EQ(r.observed_seq, 1u);
    CHECK_EQ(r.expected_seq, 1u);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Running));
    TEST_PASS("test_ontime_running_no_action");
}

void test_event_exactly_at_deadline_gap_confirmed() {
    SequencerFixture f(8, 64, 1000);
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);

    auto r = f.seq.on_time(1100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::GapConfirmed));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_event_exactly_at_deadline_gap_confirmed");
}

void test_expected_message_at_deadline_rejected() {
    SequencerFixture f(8, 64, 1000);
    auto body1 = make_body(5, 0x01);
    auto body3 = make_body(5, 0x03);
    auto body2 = make_body(5, 0x02);

    (void)f.seq.on_message(make_view(1, FeedSide::A, body1, 1), 100, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body3, 3), 200, f.sink);

    std::size_t count_before = f.emissions.size();
    auto r = f.seq.on_message(make_view(2, FeedSide::A, body2, 2), 1200, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::GapConfirmed));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    CHECK_EQ(f.emissions.size(), count_before);
    TEST_PASS("test_expected_message_at_deadline_rejected");
}

void test_clock_regression_running() {
    SequencerFixture f;
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(1, FeedSide::A, body, 1), 200, f.sink);
    CHECK_EQ(f.emissions.size(), 1u);

    auto r = f.seq.on_message(make_view(2, FeedSide::A, body, 2), 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::ClockRegression));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_clock_regression_running");
}

void test_clock_regression_gap_wait() {
    SequencerFixture f(8, 64, 1000);
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 200, f.sink);

    auto r = f.seq.on_time(100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::ClockRegression));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_clock_regression_gap_wait");
}

void test_deadline_overflow() {
    SequencerFixture f(4, 64, 1000);
    auto body = make_body(5);
    auto view = make_view(3, FeedSide::A, body, 3);
    auto r = f.seq.on_message(view, std::numeric_limits<std::uint64_t>::max(), f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::DeadlineOverflow));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    CHECK_EQ(f.storage.pending_count(), 0u);
    TEST_PASS("test_deadline_overflow");
}

// Determinism tests

void test_deterministic_replay() {
    struct RunRecord {
        SequencerCode code;
        std::uint32_t observed_seq;
        std::uint32_t expected_seq;
        SequencerState post_state;
        std::uint32_t post_next_expected;
    };
    struct RunOutcome {
        std::vector<RunRecord> results;
        std::vector<EmissionSink::Entry> emissions;
        SequencerState final_state;
        std::uint32_t final_next_expected;
    };

    auto run_once = []() -> RunOutcome {
        SequencerFixture f(8, 64, 1000);
        auto b1 = make_body(5, 0x01);
        auto b3 = make_body(5, 0x03);
        auto b2 = make_body(5, 0x02);
        auto b5 = make_body(5, 0x05);

        RunOutcome out;
        SequencerResult r;

        // Event 1: on_message seq 1 (expected, on_time)
        r = f.seq.on_message(make_view(1, FeedSide::A, b1, 1), 100, f.sink);
        out.results.push_back({r.code, r.observed_seq, r.expected_seq,
                               f.seq.state(), f.seq.next_expected_seq()});

        // Event 2: on_message seq 3 (future, creates GapWait)
        r = f.seq.on_message(make_view(3, FeedSide::B, b3, 3), 200, f.sink);
        out.results.push_back({r.code, r.observed_seq, r.expected_seq,
                               f.seq.state(), f.seq.next_expected_seq()});

        // Event 3: on_time before deadline (GapWaiting, on_time event)
        r = f.seq.on_time(500, f.sink);
        out.results.push_back({r.code, r.observed_seq, r.expected_seq,
                               f.seq.state(), f.seq.next_expected_seq()});

        // Event 4: on_message seq 5 (future, deeper reorder)
        r = f.seq.on_message(make_view(5, FeedSide::A, b5, 5), 600, f.sink);
        out.results.push_back({r.code, r.observed_seq, r.expected_seq,
                               f.seq.state(), f.seq.next_expected_seq()});

        // Event 5: on_message seq 2 (expected, flushes 2+3, leaves seq 5 pending)
        r = f.seq.on_message(make_view(2, FeedSide::A, b2, 2), 700, f.sink);
        out.results.push_back({r.code, r.observed_seq, r.expected_seq,
                               f.seq.state(), f.seq.next_expected_seq()});

        // Event 6: on_message seq 4 (expected, flushes 4+5, pending empty -> Running)
        r = f.seq.on_message(make_view(4, FeedSide::B, b3, 4), 800, f.sink);
        out.results.push_back({r.code, r.observed_seq, r.expected_seq,
                               f.seq.state(), f.seq.next_expected_seq()});

        out.emissions = f.emissions;
        out.final_state = f.seq.state();
        out.final_next_expected = f.seq.next_expected_seq();
        return out;
    };

    auto run1 = run_once();
    auto run2 = run_once();

    // Compare every SequencerResult plus post-event state
    CHECK(run1.results.size() == run2.results.size());
    for (std::size_t i = 0; i < run1.results.size(); ++i) {
        CHECK_EQ(static_cast<int>(run1.results[i].code), static_cast<int>(run2.results[i].code));
        CHECK_EQ(run1.results[i].observed_seq, run2.results[i].observed_seq);
        CHECK_EQ(run1.results[i].expected_seq, run2.results[i].expected_seq);
        CHECK_EQ(static_cast<int>(run1.results[i].post_state),
                 static_cast<int>(run2.results[i].post_state));
        CHECK_EQ(run1.results[i].post_next_expected,
                 run2.results[i].post_next_expected);
    }

    // Compare complete ordered emissions (metadata + body bytes)
    CHECK(run1.emissions.size() == run2.emissions.size());
    for (std::size_t i = 0; i < run1.emissions.size(); ++i) {
        CHECK_EQ(run1.emissions[i].sequence, run2.emissions[i].sequence);
        CHECK_EQ(static_cast<int>(run1.emissions[i].side), static_cast<int>(run2.emissions[i].side));
        CHECK_EQ(run1.emissions[i].capture_index, run2.emissions[i].capture_index);
        CHECK_EQ(run1.emissions[i].capture_monotonic_ns, run2.emissions[i].capture_monotonic_ns);
        CHECK(run1.emissions[i].body == run2.emissions[i].body);
    }

    // Compare final state and next_expected
    CHECK_EQ(static_cast<int>(run1.final_state), static_cast<int>(run2.final_state));
    CHECK_EQ(run1.final_next_expected, run2.final_next_expected);
    TEST_PASS("test_deterministic_replay");
}

void test_exact_emitted_sequence_order() {
    SequencerFixture f(8, 64, 1000);
    auto b1 = make_body(5, 0x01);
    auto b3 = make_body(5, 0x03);
    auto b2 = make_body(5, 0x02);

    (void)f.seq.on_message(make_view(1, FeedSide::A, b1, 10), 100, f.sink);
    (void)f.seq.on_message(make_view(3, FeedSide::B, b3, 30), 200, f.sink);
    (void)f.seq.on_message(make_view(2, FeedSide::A, b2, 20), 300, f.sink);

    CHECK_EQ(f.emissions.size(), 3u);
    CHECK_EQ(f.emissions[0].sequence, 1u);
    CHECK_EQ(f.emissions[0].capture_index, 10u);
    CHECK_EQ(f.emissions[1].sequence, 2u);
    CHECK_EQ(f.emissions[1].capture_index, 20u);
    CHECK_EQ(f.emissions[2].sequence, 3u);
    CHECK_EQ(f.emissions[2].capture_index, 30u);
    TEST_PASS("test_exact_emitted_sequence_order");
}

void test_compile_time_checks() {
    static_assert(!std::is_copy_constructible_v<DualFeedSequencer>);
    static_assert(!std::is_copy_assignable_v<DualFeedSequencer>);
    static_assert(!std::is_move_constructible_v<DualFeedSequencer>);
    static_assert(!std::is_move_assignable_v<DualFeedSequencer>);

    static_assert(noexcept(std::declval<DualFeedSequencer&>().state()));
    static_assert(noexcept(std::declval<DualFeedSequencer&>().next_expected_seq()));

    static_assert(noexcept(std::declval<DualFeedSequencer&>().initialize(
        std::declval<LogicalFeedId>(),
        std::declval<SequencerConfig>(),
        std::declval<MessageStorage&>())));

    static_assert(noexcept(std::declval<DualFeedSequencer&>().start(
        std::declval<std::uint32_t>())));

    static_assert(noexcept(std::declval<DualFeedSequencer&>().reset()));

    using TestSink = EmissionSink;
    static_assert(std::is_nothrow_invocable_r_v<void, TestSink&,
        const OrderedMessageMetadata&, std::span<const std::uint8_t>>);

    static_assert(noexcept(std::declval<DualFeedSequencer&>().on_message(
        std::declval<const FramedMessageView&>(),
        std::declval<std::uint64_t>(),
        std::declval<TestSink&>())));

    static_assert(noexcept(std::declval<DualFeedSequencer&>().on_time(
        std::declval<std::uint64_t>(),
        std::declval<TestSink&>())));

    TEST_PASS("test_compile_time_checks");
}

void test_result_field_entry_state() {
    SequencerFixture f;
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(1, FeedSide::A, body, 1), 100, f.sink);

    auto r = f.seq.on_message(make_view(5, FeedSide::A, body, 5), 200, f.sink);
    CHECK_EQ(r.observed_seq, 5u);
    CHECK_EQ(r.expected_seq, 2u);

    auto rt = f.seq.on_time(300, f.sink);
    CHECK_EQ(rt.observed_seq, 2u);
    CHECK_EQ(rt.expected_seq, 2u);
    CHECK_EQ(f.seq.next_expected_seq(), 2u);
    TEST_PASS("test_result_field_entry_state");
}

void test_init_zero_reorder_wait_invalid() {
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));
    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = 1;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 0;
    cfg.storage = {4, 4 * 64, 64};
    auto r = seq.initialize(LogicalFeedId{1}, cfg, storage);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidConfig));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    TEST_PASS("test_init_zero_reorder_wait_invalid");
}

void test_double_init_invalid() {
    SequencerFixture f;
    std::vector<SlotMetadata> slots2(4);
    std::vector<std::uint8_t> arena2(4 * 64);
    MessageStorage storage2;
    CHECK_STORAGE_INIT(storage2.initialize(slots2, arena2, {4, 4 * 64, 64}));
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};
    auto r = f.seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage2);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidTransition));
    TEST_PASS("test_double_init_invalid");
}

void test_start_from_non_stopped() {
    SequencerFixture f;
    auto r = f.seq.start(100);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidTransition));
    TEST_PASS("test_start_from_non_stopped");
}

void test_reset_from_running() {
    SequencerFixture f;
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Running));
    auto r = f.seq.reset();
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::Reset));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Stopped));
    TEST_PASS("test_reset_from_running");
}

void test_reset_from_gap_wait() {
    SequencerFixture f(8, 64, 1000);
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::GapWait));

    auto r = f.seq.reset();
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::Reset));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Stopped));
    CHECK_EQ(f.storage.pending_count(), 0u);
    TEST_PASS("test_reset_from_gap_wait");
}

void test_reset_from_failed_closed() {
    SequencerFixture f;
    auto body = make_body(10);
    auto bad = make_view(1, FeedSide::A, body);
    bad.feed.value = 999;
    (void)f.seq.on_message(bad, 100, f.sink);
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));

    auto r = f.seq.reset();
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::Reset));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::Stopped));
    TEST_PASS("test_reset_from_failed_closed");
}

void test_result_fields_on_time() {
    SequencerFixture f(8, 64, 1000);
    auto body = make_body(5);
    (void)f.seq.on_message(make_view(3, FeedSide::A, body, 3), 100, f.sink);
    CHECK_EQ(f.seq.next_expected_seq(), 1u);

    auto r = f.seq.on_time(500, f.sink);
    CHECK_EQ(r.observed_seq, 1u);
    CHECK_EQ(r.expected_seq, 1u);
    TEST_PASS("test_result_fields_on_time");
}

// --- Architecture-review correction tests ---

void test_feed_mismatch_invalid_config() {
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};

    // Feed argument does not match config.logical_feed
    auto r = seq.initialize(LogicalFeedId{999}, cfg, storage);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidConfig));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    TEST_PASS("test_feed_mismatch_invalid_config");
}

void test_insufficient_storage_capacity() {
    // Storage max_reorder_messages (2) < declared config (4)
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {2, 2 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};

    auto r = seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidConfig));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    TEST_PASS("test_insufficient_storage_capacity");
}

void test_slot_capacity_below_reorder_distance() {
    // slot_capacity(4) < max_reorder_distance(6)
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 6;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};

    auto r = seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidConfig));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    TEST_PASS("test_slot_capacity_below_reorder_distance");
}

void test_non_empty_storage_rejected() {
    std::vector<SlotMetadata> slots(4);
    std::vector<std::uint8_t> arena(4 * 64);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    auto body = make_body(5);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};

    auto r = seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    CHECK_EQ(static_cast<int>(r), static_cast<int>(SequencerCode::InvalidConfig));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::Uninitialized));
    TEST_PASS("test_non_empty_storage_rejected");
}

void test_declared_limits_enforced_with_larger_storage() {
    // Actual storage is larger than declared limits.
    // Sequencer must enforce declared limits, not storage's larger limits.
    std::vector<SlotMetadata> slots(16);
    std::vector<std::uint8_t> arena(16 * 128);
    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {8, 8 * 128, 128}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 8;
    cfg.reorder_wait_ns = 1000;
    // Declared limits are smaller than actual storage
    cfg.storage = {3, 3 * 32, 32};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(1);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    // Insert future messages up to declared message limit (3)
    auto body10 = make_body(20);
    (void)seq.on_message(make_view(3, FeedSide::A, body10, 3), 100, sink);
    (void)seq.on_message(make_view(4, FeedSide::A, body10, 4), 200, sink);
    (void)seq.on_message(make_view(5, FeedSide::A, body10, 5), 300, sink);
    CHECK_EQ(storage.pending_count(), 3u);

    // 4th future message exceeds declared message limit (3), even though
    // actual storage could hold more (8)
    auto r = seq.on_message(make_view(6, FeedSide::A, body10, 6), 400, sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::PendingMessageCapacityExceeded));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::FailedClosed));
    TEST_PASS("test_declared_limits_enforced_with_larger_storage");
}

void test_b_copy_first_expected_wins() {
    // B copy arrives first for expected sequence; later A copy is stale-dropped.
    SequencerFixture f;
    auto bodyB = make_body(10, 0xBB);
    auto bodyA = make_body(10, 0xAA);

    auto rB = f.seq.on_message(make_view(1, FeedSide::B, bodyB, 1), 100, f.sink);
    CHECK_EQ(static_cast<int>(rB.code), static_cast<int>(SequencerCode::Emitted));
    CHECK_EQ(f.emissions.size(), 1u);
    CHECK_EQ(static_cast<int>(f.emissions[0].side), static_cast<int>(FeedSide::B));

    // Later A copy is stale (next_expected already advanced to 2)
    auto rA = f.seq.on_message(make_view(1, FeedSide::A, bodyA, 2), 200, f.sink);
    CHECK_EQ(static_cast<int>(rA.code), static_cast<int>(SequencerCode::DuplicateDropped));
    CHECK_EQ(f.emissions.size(), 1u);
    TEST_PASS("test_b_copy_first_expected_wins");
}

void test_oversized_expected_rejected() {
    // Expected message exceeding declared max_message_bytes must fail
    // without emission or sequence advance.
    SequencerFixture f(4, 32, 1000);
    auto too_big = make_body(33);

    auto r = f.seq.on_message(make_view(1, FeedSide::A, too_big, 1), 100, f.sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::PendingByteCapacityExceeded));
    CHECK_EQ(static_cast<int>(f.seq.state()), static_cast<int>(SequencerState::FailedClosed));
    CHECK(f.emissions.empty());
    CHECK_EQ(f.seq.next_expected_seq(), 1u);
    TEST_PASS("test_oversized_expected_rejected");
}

void test_deadline_overflow_arena_unchanged() {
    // Deadline overflow must leave the target arena slice byte-for-byte unchanged.
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    // Fill arena with a known pattern
    for (std::size_t i = 0; i < arena.size(); ++i)
        arena[i] = static_cast<std::uint8_t>(i & 0xFF);

    std::vector<std::uint8_t> arena_snapshot = arena;

    MessageStorage storage;
    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    DualFeedSequencer seq;
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = 4;
    cfg.reorder_wait_ns = 1000;
    cfg.storage = {4, 4 * 64, 64};
    (void)seq.initialize(LogicalFeedId{FEED_ID}, cfg, storage);
    (void)seq.start(1);

    std::vector<EmissionSink::Entry> log;
    EmissionSink sink{&log};

    auto body = make_body(10);
    auto r = seq.on_message(make_view(3, FeedSide::A, body, 3),
                            std::numeric_limits<std::uint64_t>::max(), sink);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(SequencerCode::DeadlineOverflow));
    CHECK_EQ(static_cast<int>(seq.state()), static_cast<int>(SequencerState::FailedClosed));
    CHECK_EQ(storage.pending_count(), 0u);

    // Arena must be byte-for-byte unchanged
    CHECK(arena == arena_snapshot);
    TEST_PASS("test_deadline_overflow_arena_unchanged");
}

// --- RT4 Gate A: StorageInitResult contract tests ---

void test_storage_initialize_invalid_geometry_is_observable() {
    struct Case {
        std::uint32_t max_reorder_messages;
        std::size_t max_reorder_bytes;
        std::size_t max_message_bytes;
        std::size_t slot_capacity;
        std::size_t arena_size;
        GeometryCode expected;
    };
    Case cases[] = {
        {0, 4*32, 32, 4, 4*32, GeometryCode::ZeroMaxReorderMessages},
        {0x80000000u, 4*32, 32, 4, 4*32, GeometryCode::MaxReorderMessagesTooLarge},
        {4, 4*32, 0, 4, 4*32, GeometryCode::ZeroMaxMessageBytes},
        {4, 4*32, 32, 3, 3*32, GeometryCode::SlotCapacityTooSmall},
        {2, 128, std::numeric_limits<std::size_t>::max() / 2 + 2, 2, 128, GeometryCode::CapacityOverflow},
        {4, 4*64, 64, 4, 100, GeometryCode::ArenaTooSmall},
        {4, 0, 32, 4, 4*32, GeometryCode::ZeroMaxReorderBytes},
        {4, 4*32+1, 32, 4, 4*32, GeometryCode::MaxReorderBytesExceedsCapacity},
    };

    for (const auto& c : cases) {
        std::vector<SlotMetadata> slots(c.slot_capacity);
        std::vector<std::uint8_t> arena(c.arena_size);
        // Fill arena with a sentinel pattern
        for (auto& b : arena) b = 0xDE;

        std::vector<std::uint8_t> arena_snap = arena;

        MessageStorage storage;
        auto r = storage.initialize(slots, arena,
            {c.max_reorder_messages, c.max_reorder_bytes, c.max_message_bytes});
        CHECK_EQ(static_cast<int>(r.code), static_cast<int>(StorageInitCode::InvalidGeometry));
        CHECK_EQ(static_cast<int>(r.geometry_code), static_cast<int>(c.expected));

        // Object must remain completely uninitialized
        CHECK(!storage.initialized());
        CHECK_EQ(storage.pending_count(), 0u);
        CHECK_EQ(storage.pending_bytes(), 0u);
        CHECK_EQ(storage.slot_capacity(), 0u);

        // Arena must be unchanged
        CHECK(arena == arena_snap);
    }
    TEST_PASS("test_storage_initialize_invalid_geometry_is_observable");
}

void test_storage_initialize_valid_retry_after_invalid_geometry() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;

    // First attempt with invalid geometry
    auto r1 = storage.initialize(slots, arena, {0, 0, 0});
    CHECK_EQ(static_cast<int>(r1.code), static_cast<int>(StorageInitCode::InvalidGeometry));
    CHECK(!storage.initialized());

    // Retry with valid geometry
    auto r2 = storage.initialize(slots, arena, {4, 4 * 64, 64});
    CHECK_EQ(static_cast<int>(r2.code), static_cast<int>(StorageInitCode::Ok));
    CHECK_EQ(static_cast<int>(r2.geometry_code), static_cast<int>(GeometryCode::Ok));
    CHECK(storage.initialized());
    CHECK_EQ(storage.slot_capacity(), 8u);
    TEST_PASS("test_storage_initialize_valid_retry_after_invalid_geometry");
}

void test_storage_initialize_second_valid_call_preserves_state() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;

    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    // Insert a pending message
    auto body = make_body(10, 0xAB);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    CHECK_EQ(storage.pending_count(), 1u);
    CHECK_EQ(storage.pending_bytes(), 10u);

    // Capture old state
    auto old_slots = slots;
    auto old_arena = arena;
    auto old_pending_count = storage.pending_count();
    auto old_pending_bytes = storage.pending_bytes();
    auto old_capacity = storage.slot_capacity();

    // Second valid call must return AlreadyInitialized
    std::vector<SlotMetadata> new_slots(16);
    std::vector<std::uint8_t> new_arena(16 * 128);
    auto r = storage.initialize(new_slots, new_arena, {8, 8 * 128, 128});
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(StorageInitCode::AlreadyInitialized));
    CHECK_EQ(static_cast<int>(r.geometry_code), static_cast<int>(GeometryCode::Ok));

    // Old state must be fully preserved
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), old_pending_count);
    CHECK_EQ(storage.pending_bytes(), old_pending_bytes);
    CHECK_EQ(storage.slot_capacity(), old_capacity);
    CHECK(storage.is_occupied(1));

    // Old payload must remain in old arena
    const auto& slot = storage.view(1);
    CHECK_EQ(slot.sequence, 1u);
    CHECK_EQ(slot.payload_length, 10u);

    // New buffers must be unchanged
    for (auto& s : new_slots)
        CHECK_EQ(s.occupied, false);
    for (auto& b : new_arena)
        CHECK_EQ(b, static_cast<std::uint8_t>(0));

    TEST_PASS("test_storage_initialize_second_valid_call_preserves_state");
}

void test_storage_initialize_second_invalid_call_has_lifecycle_precedence() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;

    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    // Insert a pending message
    auto body = make_body(10, 0xCD);
    CHECK_EQ(static_cast<int>(storage.insert(3, FeedSide::B, 5, 500, body)),
             static_cast<int>(InsertResult::Ok));
    auto old_pending = storage.pending_count();

    // Call with empty spans and zero config — would be InvalidGeometry if not initialized
    std::span<SlotMetadata> empty_slots;
    std::span<std::uint8_t> empty_arena;
    auto r = storage.initialize(empty_slots, empty_arena, {0, 0, 0});
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(StorageInitCode::AlreadyInitialized));
    CHECK_EQ(static_cast<int>(r.geometry_code), static_cast<int>(GeometryCode::Ok));

    // State preserved
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), old_pending);
    CHECK(storage.is_occupied(3));
    TEST_PASS("test_storage_initialize_second_invalid_call_has_lifecycle_precedence");
}

void test_storage_initialize_after_reset_returns_already_initialized() {
    std::vector<SlotMetadata> slots(8);
    std::vector<std::uint8_t> arena(8 * 64);
    MessageStorage storage;

    CHECK_STORAGE_INIT(storage.initialize(slots, arena, {4, 4 * 64, 64}));

    // Insert and reset
    auto body = make_body(5);
    CHECK_EQ(static_cast<int>(storage.insert(1, FeedSide::A, 1, 100, body)),
             static_cast<int>(InsertResult::Ok));
    storage.reset();
    CHECK(storage.initialized());
    CHECK_EQ(storage.pending_count(), 0u);

    // After reset, initialize must still return AlreadyInitialized
    std::vector<SlotMetadata> new_slots(4);
    std::vector<std::uint8_t> new_arena(4 * 32);
    auto r = storage.initialize(new_slots, new_arena, {2, 2 * 32, 32});
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(StorageInitCode::AlreadyInitialized));
    CHECK_EQ(static_cast<int>(r.geometry_code), static_cast<int>(GeometryCode::Ok));

    // Must not rebind: old capacity preserved
    CHECK_EQ(storage.slot_capacity(), 8u);
    CHECK(storage.initialized());
    TEST_PASS("test_storage_initialize_after_reset_returns_already_initialized");
}

} // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
int main() {
    // MessageStorage tests (Phase 1)
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

    // DualFeedSequencer tests (Phase 2)
    test_sequencer_init_start_running();
    test_sequencer_reset_preserves_config_restart();
    test_sequencer_failed_closed_requires_reset();
    test_sequencer_invalid_init_remains_uninitialized();
    test_sequencer_calls_before_init();
    test_sequencer_no_emission_after_failed_closed();
    test_ab_first_valid_copy_wins();
    test_alternating_winning_side();
    test_duplicate_second_copy_stale_dropped();
    test_wrong_logical_feed();
    test_invalid_side_and_empty_body();
    test_reorder_1_3_2();
    test_reorder_1_4_3_2_contiguous_flush();
    test_deeper_contiguous_flush();
    test_future_duplicate_equal_bytes();
    test_future_duplicate_different_length();
    test_future_duplicate_different_bytes();
    test_pending_metadata_preserved();
    test_future_beyond_window();
    test_ambiguous_half_range();
    test_natural_uint32_wrap();
    test_modulo_slot_lookup_across_wrap();
    test_pending_message_capacity_precedes_byte_capacity();
    test_pending_byte_limit();
    test_per_message_body_limit();
    test_storage_slot_conflict_invariant();
    test_no_partial_state_on_failure();
    test_first_future_creates_deadline();
    test_later_future_does_not_extend_deadline();
    test_equal_duplicate_does_not_extend_deadline();
    test_partial_resolution_does_not_extend_deadline();
    test_pending_becomes_empty_returns_running();
    test_ontime_before_deadline_gap_waiting();
    test_ontime_running_no_action();
    test_event_exactly_at_deadline_gap_confirmed();
    test_expected_message_at_deadline_rejected();
    test_clock_regression_running();
    test_clock_regression_gap_wait();
    test_deadline_overflow();
    test_deterministic_replay();
    test_exact_emitted_sequence_order();
    test_compile_time_checks();
    test_result_field_entry_state();
    test_init_zero_reorder_wait_invalid();
    test_double_init_invalid();
    test_start_from_non_stopped();
    test_reset_from_running();
    test_reset_from_gap_wait();
    test_reset_from_failed_closed();
    test_result_fields_on_time();

    // Architecture-review correction tests
    test_feed_mismatch_invalid_config();
    test_insufficient_storage_capacity();
    test_slot_capacity_below_reorder_distance();
    test_non_empty_storage_rejected();
    test_declared_limits_enforced_with_larger_storage();
    test_b_copy_first_expected_wins();
    test_oversized_expected_rejected();
    test_deadline_overflow_arena_unchanged();

    // RT4 Gate A: StorageInitResult contract tests
    test_storage_initialize_invalid_geometry_is_observable();
    test_storage_initialize_valid_retry_after_invalid_geometry();
    test_storage_initialize_second_valid_call_preserves_state();
    test_storage_initialize_second_invalid_call_has_lifecycle_precedence();
    test_storage_initialize_after_reset_returns_already_initialized();

    std::printf("All %d test cases passed.\n", 98);
    return 0;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
