#include "moex_fast/spectra_sequence_arithmetic.hpp"
#include "test_check.hpp"
#include <cstdint>
#include <type_traits>

using namespace moex::spectra;

// --- Compile-time checks ---
static_assert(classify_sequence_relation(0, 0, 1).relation == SequenceRelation::Expected);
static_assert(classify_sequence_relation(0, 0, 1).delta == 0);
static_assert(classify_sequence_relation(0, 0, 0).relation == SequenceRelation::InvalidConfig);
static_assert(noexcept(classify_sequence_relation(0, 0, 0)));
static_assert(std::is_trivially_copyable_v<SequenceClassification>);

// --- Invalid config: max == 0 ---
static void test_invalid_max_zero() {
    auto r = classify_sequence_relation(100, 100, 0);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::InvalidConfig));
    CHECK_EQ(r.delta, 0u);
    TEST_PASS("invalid max 0");
}

// --- Invalid config: max == 0x80000000 ---
static void test_invalid_max_half() {
    auto r = classify_sequence_relation(100, 100, 0x80000000u);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::InvalidConfig));
    CHECK_EQ(r.delta, 0u);
    TEST_PASS("invalid max 0x80000000");
}

// --- Invalid config: max == UINT32_MAX ---
static void test_invalid_max_max() {
    auto r = classify_sequence_relation(100, 100, 0xFFFFFFFFu);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::InvalidConfig));
    CHECK_EQ(r.delta, 0u);
    TEST_PASS("invalid max UINT32_MAX");
}

// --- Expected ---
static void test_expected() {
    auto r = classify_sequence_relation(100, 100, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::Expected));
    CHECK_EQ(r.delta, 0u);
    TEST_PASS("expected 100,100,16");
}

// --- FutureWithinWindow delta 1 ---
static void test_future_within_1() {
    auto r = classify_sequence_relation(101, 100, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureWithinWindow));
    CHECK_EQ(r.delta, 1u);
    TEST_PASS("future within window delta 1");
}

// --- FutureWithinWindow delta 16 (exact max) ---
static void test_future_within_max() {
    auto r = classify_sequence_relation(116, 100, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureWithinWindow));
    CHECK_EQ(r.delta, 16u);
    TEST_PASS("future within window delta 16");
}

// --- FutureBeyondWindow delta 17 ---
static void test_future_beyond() {
    auto r = classify_sequence_relation(117, 100, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureBeyondWindow));
    CHECK_EQ(r.delta, 17u);
    TEST_PASS("future beyond window delta 17");
}

// --- FutureBeyondWindow at 0x7FFFFFFF ---
static void test_future_beyond_large() {
    auto r = classify_sequence_relation(0x7FFFFFFFu, 0, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureBeyondWindow));
    CHECK_EQ(r.delta, 0x7FFFFFFFu);
    TEST_PASS("future beyond window 0x7FFFFFFF");
}

// --- Ambiguous at exact 0x80000000 ---
static void test_ambiguous() {
    auto r = classify_sequence_relation(0x80000064u, 100, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::Ambiguous));
    CHECK_EQ(r.delta, 0x80000000u);
    TEST_PASS("ambiguous delta 0x80000000");
}

// --- Stale delta UINT32_MAX ---
static void test_stale() {
    auto r = classify_sequence_relation(99, 100, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::Stale));
    CHECK_EQ(r.delta, 0xFFFFFFFFu);
    TEST_PASS("stale delta UINT32_MAX");
}

// --- Natural wrap: UINT32_MAX -> 0, FutureWithinWindow delta 1 ---
static void test_wrap_forward() {
    auto r = classify_sequence_relation(0, 0xFFFFFFFFu, 1);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureWithinWindow));
    CHECK_EQ(r.delta, 1u);
    TEST_PASS("wrap UINT32_MAX->0 delta 1");
}

// --- Multi-step wrap: UINT32_MAX -> 1, FutureWithinWindow delta 2 ---
static void test_wrap_forward_2() {
    auto r = classify_sequence_relation(1, 0xFFFFFFFFu, 2);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureWithinWindow));
    CHECK_EQ(r.delta, 2u);
    TEST_PASS("wrap UINT32_MAX->1 delta 2");
}

// --- Multi-step wrap: UINT32_MAX -> 2, FutureBeyondWindow delta 3 ---
static void test_wrap_beyond() {
    auto r = classify_sequence_relation(2, 0xFFFFFFFFu, 2);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureBeyondWindow));
    CHECK_EQ(r.delta, 3u);
    TEST_PASS("wrap UINT32_MAX->2 delta 3");
}

// --- Stale: 0 -> UINT32_MAX ---
static void test_stale_wrap() {
    auto r = classify_sequence_relation(0xFFFFFFFFu, 0, 16);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::Stale));
    CHECK_EQ(r.delta, 0xFFFFFFFFu);
    TEST_PASS("stale UINT32_MAX->0");
}

// --- Largest valid window: 0x7FFFFFFF ---
static void test_largest_window() {
    auto r = classify_sequence_relation(0x7FFFFFFFu, 0, 0x7FFFFFFFu);
    CHECK_EQ(static_cast<int>(r.relation), static_cast<int>(SequenceRelation::FutureWithinWindow));
    CHECK_EQ(r.delta, 0x7FFFFFFFu);
    TEST_PASS("largest window 0x7FFFFFFF");
}

int main() {
    test_invalid_max_zero();
    test_invalid_max_half();
    test_invalid_max_max();
    test_expected();
    test_future_within_1();
    test_future_within_max();
    test_future_beyond();
    test_future_beyond_large();
    test_ambiguous();
    test_stale();
    test_wrap_forward();
    test_wrap_forward_2();
    test_wrap_beyond();
    test_stale_wrap();
    test_largest_window();
    return 0;
}
