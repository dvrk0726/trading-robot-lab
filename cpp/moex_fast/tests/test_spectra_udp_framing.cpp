#include "moex_fast/spectra_udp_framing.hpp"
#include "test_check.hpp"
#include <vector>
#include <cstdint>
#include <limits>
#include <type_traits>

using namespace moex::spectra;

static DatagramView make_input(
    std::span<const std::uint8_t> payload,
    LogicalFeedId feed = {42},
    FeedSide side = FeedSide::A,
    std::uint64_t capture_index = 100,
    std::uint64_t capture_monotonic_ns = 200
) {
    return {feed, side, capture_index, capture_monotonic_ns, payload};
}

static FramingLimits valid_limits() {
    return {1500};
}

// --- Size 0..3: DatagramTooShort ---
static void test_size_0() {
    std::vector<std::uint8_t> data;
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::DatagramTooShort));
    TEST_PASS("size 0 -> DatagramTooShort");
}

static void test_size_1() {
    std::vector<std::uint8_t> data{0x01};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::DatagramTooShort));
    TEST_PASS("size 1 -> DatagramTooShort");
}

static void test_size_2() {
    std::vector<std::uint8_t> data{0x01, 0x02};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::DatagramTooShort));
    TEST_PASS("size 2 -> DatagramTooShort");
}

static void test_size_3() {
    std::vector<std::uint8_t> data{0x01, 0x02, 0x03};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::DatagramTooShort));
    TEST_PASS("size 3 -> DatagramTooShort");
}

// --- Size 4: EmptyFastBody ---
static void test_size_4() {
    std::vector<std::uint8_t> data{0x01, 0x00, 0x00, 0x00};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::EmptyFastBody));
    TEST_PASS("size 4 -> EmptyFastBody");
}

// --- Size 5: minimum valid ---
static void test_size_5_ok() {
    std::vector<std::uint8_t> data{0x01, 0x00, 0x00, 0x00, 0xAB};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.msg_seq_num, 1u);
    CHECK_EQ(out.fast_body.size(), 1u);
    CHECK_EQ(out.fast_body[0], 0xAB);
    TEST_PASS("size 5 -> Ok");
}

// --- Exact maximum ---
static void test_exact_max() {
    FramingLimits limits{10};
    std::vector<std::uint8_t> data(10, 0x00);
    data[0] = 0x07;
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), limits, out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.fast_body.size(), 6u);
    TEST_PASS("exact maximum -> Ok");
}

// --- Maximum + 1: DatagramTooLarge ---
static void test_max_plus_one() {
    FramingLimits limits{10};
    std::vector<std::uint8_t> data(11, 0x00);
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), limits, out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::DatagramTooLarge));
    TEST_PASS("maximum+1 -> DatagramTooLarge");
}

// --- Invalid max below 5: InvalidConfig ---
static void test_invalid_max_below_5() {
    FramingLimits limits{4};
    std::vector<std::uint8_t> data{0x01, 0x00, 0x00, 0x00, 0x00};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), limits, out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::InvalidConfig));
    TEST_PASS("invalid max below 5 -> InvalidConfig");
}

// --- Little-endian vectors ---
static void test_le_01_00_00_00() {
    std::vector<std::uint8_t> data{0x01, 0x00, 0x00, 0x00, 0xFF};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.msg_seq_num, 1u);
    TEST_PASS("01 00 00 00 -> 1 little-endian");
}

static void test_le_00_00_00_01() {
    std::vector<std::uint8_t> data{0x00, 0x00, 0x00, 0x01, 0xFF};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.msg_seq_num, 0x01000000u);
    TEST_PASS("00 00 00 01 -> 0x01000000 little-endian");
}

static void test_le_01_02_03_04() {
    std::vector<std::uint8_t> data{0x01, 0x02, 0x03, 0x04, 0xFF};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.msg_seq_num, 0x04030201u);
    TEST_PASS("01 02 03 04 -> 0x04030201 little-endian");
}

static void test_le_FF_FF_FF_FF() {
    std::vector<std::uint8_t> data{0xFF, 0xFF, 0xFF, 0xFF, 0xAB};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.msg_seq_num, 0xFFFFFFFFu);
    TEST_PASS("FF FF FF FF -> UINT32_MAX little-endian");
}

// --- Exact body offset ---
static void test_body_offset() {
    std::vector<std::uint8_t> data{0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.fast_body.size(), 3u);
    CHECK_EQ(out.fast_body[0], 0x11);
    CHECK_EQ(out.fast_body[1], 0x22);
    CHECK_EQ(out.fast_body[2], 0x33);
    TEST_PASS("exact body offset at byte 4");
}

// --- Metadata propagation ---
static void test_metadata_propagation() {
    std::vector<std::uint8_t> data{0x05, 0x00, 0x00, 0x00, 0xEE};
    LogicalFeedId feed{7};
    FramedMessageView out;
    auto r = frame_udp_message(
        make_input(data, feed, FeedSide::B, 999, 12345),
        valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.feed.value, 7u);
    CHECK_EQ(static_cast<int>(out.side), static_cast<int>(FeedSide::B));
    CHECK_EQ(out.capture_index, 999u);
    CHECK_EQ(out.capture_monotonic_ns, 12345u);
    TEST_PASS("metadata propagation");
}

// --- Borrowed pointer identity ---
static void test_borrowed_pointer() {
    std::vector<std::uint8_t> data{0x01, 0x00, 0x00, 0x00, 0xAA, 0xBB};
    FramedMessageView out;
    auto r = frame_udp_message(make_input(data), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r.code), static_cast<int>(FrameCode::Ok));
    CHECK(out.fast_body.data() == data.data() + 4);
    TEST_PASS("borrowed pointer identity");
}

// --- Source immutability ---
static void test_source_immutability() {
    std::vector<std::uint8_t> data{0x01, 0x02, 0x03, 0x04, 0x05};
    std::vector<std::uint8_t> original = data;
    FramedMessageView out;
    frame_udp_message(make_input(data), valid_limits(), out);
    CHECK(data == original);
    TEST_PASS("source immutability");
}

// --- Output cleared on failure after previously populated ---
static void test_clear_after_failure() {
    std::vector<std::uint8_t> good{0x42, 0x00, 0x00, 0x00, 0xFF};
    std::vector<std::uint8_t> bad;
    FramedMessageView out;

    auto r1 = frame_udp_message(make_input(good), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r1.code), static_cast<int>(FrameCode::Ok));
    CHECK_EQ(out.msg_seq_num, 0x42u);
    CHECK_EQ(out.fast_body.size(), 1u);

    auto r2 = frame_udp_message(make_input(bad), valid_limits(), out);
    CHECK_EQ(static_cast<int>(r2.code), static_cast<int>(FrameCode::DatagramTooShort));
    CHECK_EQ(out.msg_seq_num, 0u);
    CHECK_EQ(out.fast_body.size(), 0u);
    CHECK_EQ(out.capture_index, 0u);
    CHECK_EQ(out.capture_monotonic_ns, 0u);
    TEST_PASS("output cleared after failure");
}

// --- Compile-time: exact function pointer type ---
static void test_function_pointer_type() {
    using Expected = FrameResult(*)(
        const DatagramView&,
        FramingLimits,
        FramedMessageView&
    ) noexcept;
    using Actual = decltype(&frame_udp_message);
    static_assert(std::is_same_v<Expected, Actual>,
        "frame_udp_message pointer type mismatch");
    TEST_PASS("compile-time function pointer type check");
}

// --- Compile-time: noexcept verification ---
static void test_noexcept() {
    static_assert(noexcept(frame_udp_message(
        std::declval<const DatagramView&>(),
        std::declval<FramingLimits>(),
        std::declval<FramedMessageView&>())),
        "frame_udp_message must be noexcept");
    TEST_PASS("compile-time noexcept verification");
}

int main() {
    test_size_0();
    test_size_1();
    test_size_2();
    test_size_3();
    test_size_4();
    test_size_5_ok();
    test_exact_max();
    test_max_plus_one();
    test_invalid_max_below_5();
    test_le_01_00_00_00();
    test_le_00_00_00_01();
    test_le_01_02_03_04();
    test_le_FF_FF_FF_FF();
    test_body_offset();
    test_metadata_propagation();
    test_borrowed_pointer();
    test_source_immutability();
    test_clear_after_failure();
    test_function_pointer_type();
    test_noexcept();
    return 0;
}
