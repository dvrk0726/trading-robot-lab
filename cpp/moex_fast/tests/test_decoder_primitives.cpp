#include "moex_fast/wire_cursor.hpp"
#include "test_check.hpp"
#include <vector>
#include <cstring>
#include <limits>

using namespace moex_fast;

static std::vector<std::uint8_t> make_bytes(std::initializer_list<std::uint8_t> init) {
    return std::vector<std::uint8_t>(init);
}

// --- Unsigned stop-bit integer tests ---
static void test_stopbit_u32() {
    // 0 -> 0x80
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 999;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0u);
        CHECK(c.at_end());
    }
    // 1 -> 0x81
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 1u);
    }
    // 127 -> 0xFF
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 127u);
    }
    // 128 -> 0x01 0x80
    {
        auto bytes = make_bytes({0x01, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 128u);
    }
    // 256 -> 0x02 0x80
    {
        auto bytes = make_bytes({0x02, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 256u);
    }
    // UINT32_MAX: 5 bytes
    {
        auto bytes = make_bytes({0x0F, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0xFFFFFFFFu);
    }
    // Truncated
    {
        auto bytes = make_bytes({0x01});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u); // cursor restored
    }
    // Empty
    {
        WireCursor c(nullptr, 0);
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::NeedMoreData);
    }
    // Non-canonical: 0x00 0x80 encodes 0 but uses 2 bytes (should be 1)
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u); // cursor restored
    }
    // Overflow: 2^32 [10 00 00 00 80]
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0u);
    }
    // General overlong: [00 01 80] for value 128 (should be [01 80])
    {
        auto bytes = make_bytes({0x00, 0x01, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0u);
    }
    // Prefix loop: UINT32_MAX [0F 7F 7F 7F FF] lengths 1..4
    {
        std::uint8_t full[] = {0x0F, 0x7F, 0x7F, 0x7F, 0xFF};
        for (int len = 1; len <= 4; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::uint32_t val = 0xDEAD;
            CHECK(c.read_stopbit_u32(val) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 0xDEADu);
        }
    }
    // 5-byte no stop bit: [10 00 00 00 00]
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0xDEAD;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0xDEADu);
    }
    TEST_PASS("stopbit_u32");
}

static void test_stopbit_u64() {
    // 0
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 999;
        CHECK(c.read_stopbit_u64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0ull);
    }
    // 16384 -> 0x01 0x00 0x80
    {
        auto bytes = make_bytes({0x01, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0;
        CHECK(c.read_stopbit_u64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 16384ull);
    }
    // UINT64_MAX: 10 bytes (70 data bits, top 6 bits = 0, bottom 64 = all 1s)
    // Groups MSB: 0000001, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111+stop
    {
        auto bytes = make_bytes({0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0;
        CHECK(c.read_stopbit_u64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0xFFFFFFFFFFFFFFFFull);
    }
    // Prefix loop: UINT64_MAX [01 7F 7F 7F 7F 7F 7F 7F 7F FF] lengths 1..9
    {
        std::uint8_t full[] = {0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF};
        for (int len = 1; len <= 9; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::uint64_t val = 0xDEAD;
            CHECK(c.read_stopbit_u64(val) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 0xDEADull);
        }
    }
    // 10-byte no stop bit: overflow-shaped [02 00 00 00 00 00 00 00 00 00]
    {
        auto bytes = make_bytes({0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0xDEAD;
        CHECK(c.read_stopbit_u64(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0xDEADull);
    }
    TEST_PASS("stopbit_u64");
}

// --- Signed stop-bit integer tests ---
static void test_stopbit_i32() {
    // 0 -> 0x80
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 999;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0);
    }
    // 1 -> 0x81
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 1);
    }
    // -1 -> 0xFF (1 byte, sign bit=1, value=111111=63, sign extended=-1)
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, -1);
    }
    // -1 two-byte encoding 0x7F 0xFF is NON-CANONICAL (fits in 1 byte)
    {
        auto bytes = make_bytes({0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::NonCanonicalEncoding);
    }
    // 63 (max 1-byte positive) -> 0xBF
    {
        auto bytes = make_bytes({0xBF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 63);
    }
    // 64 (needs 2 bytes) -> 0x00 0xC0
    {
        auto bytes = make_bytes({0x00, 0xC0});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 64);
    }
    // -64 (max 1-byte negative) -> 0xC0
    {
        auto bytes = make_bytes({0xC0});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, -64);
    }
    // -65 (needs 2 bytes)
    // -65 in 14-bit 2's complement = 16384 - 65 = 16319 = 0x3FBF
    // 14 bits: 01_111111_0111111
    // Byte 0 (bits 13..7): 1111111 = 0x7F, no stop
    // Byte 1 (bits 6..0 + stop): 0111111 | 0x80 = 0xBF
    {
        auto bytes = make_bytes({0x7F, 0xBF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, -65);
    }
    // Non-canonical: 0x00 0xC0 for value 0 (fits in 1 byte)
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::NonCanonicalEncoding);
    }
    // INT32_MIN: [78 00 00 00 80]
    {
        auto bytes = make_bytes({0x78, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, std::numeric_limits<std::int32_t>::min());
    }
    // INT32_MAX: [07 7F 7F 7F FF]
    {
        auto bytes = make_bytes({0x07, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, std::numeric_limits<std::int32_t>::max());
    }
    // Overflow +2^31: [08 00 00 00 80]
    {
        auto bytes = make_bytes({0x08, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0);
    }
    // Overflow -2^31-1: [77 7F 7F 7F FF]
    {
        auto bytes = make_bytes({0x77, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0);
    }
    // Positive overlong: [00 01 80] for value 128 (should be [01 80])
    {
        auto bytes = make_bytes({0x00, 0x01, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0);
    }
    // Three-byte negative overlong: [7F 7F FF] for -1 (should be [FF])
    {
        auto bytes = make_bytes({0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0);
    }
    // Prefix loop: INT32_MIN [78 00 00 00 80] lengths 1..4
    {
        std::uint8_t full[] = {0x78, 0x00, 0x00, 0x00, 0x80};
        for (int len = 1; len <= 4; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::int32_t val = 12345;
            CHECK(c.read_stopbit_i32(val) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 12345);
        }
    }
    // Prefix loop: INT32_MAX [07 7F 7F 7F FF] lengths 1..4
    {
        std::uint8_t full[] = {0x07, 0x7F, 0x7F, 0x7F, 0xFF};
        for (int len = 1; len <= 4; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::int32_t val = 12345;
            CHECK(c.read_stopbit_i32(val) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 12345);
        }
    }
    // 5-byte no stop bit: [08 00 00 00 00]
    {
        auto bytes = make_bytes({0x08, 0x00, 0x00, 0x00, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 12345;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 12345);
    }
    TEST_PASS("stopbit_i32");
}

static void test_stopbit_i64() {
    // 0
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 999;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0ll);
    }
    // -1
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, -1ll);
    }
    // INT64_MIN: [7F 00 00 00 00 00 00 00 00 80]
    {
        auto bytes = make_bytes({0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, (-9223372036854775807ll - 1ll));
    }
    // INT64_MAX: [00 7F 7F 7F 7F 7F 7F 7F 7F FF]
    {
        auto bytes = make_bytes({0x00, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 9223372036854775807ll);
    }
    // Positive overflow: +2^63 [01 00 00 00 00 00 00 00 00 80]
    {
        auto bytes = make_bytes({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Negative overflow: -2^63-1 [7E 7F 7F 7F 7F 7F 7F 7F 7F FF]
    {
        auto bytes = make_bytes({0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Signed overlong: [00 80] (0 fits in 1 byte)
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Signed overlong: [7F FF] (-1 fits in 1 byte)
    {
        auto bytes = make_bytes({0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Prefix loop: INT64_MIN [7F 00 00 00 00 00 00 00 00 80] lengths 1..9
    {
        std::uint8_t full[] = {0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        for (int len = 1; len <= 9; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::int64_t val = 12345;
            CHECK(c.read_stopbit_i64(val) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 12345ll);
        }
    }
    // 10-byte no stop bit: overflow-shaped [01 00 00 00 00 00 00 00 00 00]
    {
        auto bytes = make_bytes({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 12345;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 12345ll);
    }
    TEST_PASS("stopbit_i64");
}

// --- Nullable integer tests (FIX FAST 1.1 offset encoding) ---
static void test_nullable_u32() {
    // Null: wire [80] (stop-bit 0)
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 999;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(c.position(), 1u);
    }
    // Non-null value 0: encoded as stopbit(0+1) = stopbit(1) = 0x81
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 999;
        bool is_null = true;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 0u);
    }
    // Non-null value 1: encoded as stopbit(2) = 0x82
    {
        auto bytes = make_bytes({0x82});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 1u);
    }
    // Non-null value 127: encoded as stopbit(128) = 0x01 0x80
    {
        auto bytes = make_bytes({0x01, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 127u);
    }
    // UINT32_MAX: raw = 2^32, encoded as [10 00 00 00 80]
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 0xFFFFFFFFu);
    }
    // Overflow: raw = 2^32+1 = [10 00 00 00 81]
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong NULL: [00 80] (2 bytes for value that fits in 1)
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong raw 1: [00 81] (2 bytes for value that fits in 1)
    {
        auto bytes = make_bytes({0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Truncated: [00] (no stop bit, NeedMoreData, cursor restored)
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
    }
    TEST_PASS("nullable_u32");
}

static void test_nullable_i32() {
    // Null: wire [80] (stop-bit 0)
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 999;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
    }
    // Non-null value 0: encoded as stopbit(0+1) = stopbit(1) = 0x81
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 999;
        bool is_null = true;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 0);
    }
    // Non-null value 1: encoded as stopbit(2) = 0x82
    {
        auto bytes = make_bytes({0x82});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 1);
    }
    // Non-null value -1: negative, encoded unchanged as stopbit(-1) = 0xFF
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 999;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, -1);
    }
    // INT32_MIN: raw = -2^31, encoded as [78 00 00 00 80]
    {
        auto bytes = make_bytes({0x78, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, std::numeric_limits<std::int32_t>::min());
    }
    // INT32_MAX: raw = 2^31, encoded as [08 00 00 00 80]
    {
        auto bytes = make_bytes({0x08, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, std::numeric_limits<std::int32_t>::max());
    }
    // Overflow: raw = 2^31+1 = [08 00 00 00 81]
    {
        auto bytes = make_bytes({0x08, 0x00, 0x00, 0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Overflow: below INT32_MIN = [77 7F 7F 7F FF]
    {
        auto bytes = make_bytes({0x77, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong NULL: [00 80] (2 bytes for value that fits in 1)
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong raw 1: [00 81]
    {
        auto bytes = make_bytes({0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong signed -1: [7F FF] (fits in 1 byte as [FF])
    {
        auto bytes = make_bytes({0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Truncated: [00] (no stop bit, NeedMoreData, cursor restored)
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i32(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
    }
    TEST_PASS("nullable_i32");
}

static void test_nullable_u64() {
    // Sequential read of all required vectors
    {
        auto bytes = make_bytes({
            0x80,                                           // NULL
            0x81,                                           // 0
            0x82,                                           // 1
            0x01, 0x80,                                     // 127
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, // UINT64_MAX
            0x80                                            // sentinel
        });
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0;
        bool is_null = false;

        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(c.position(), 1u);

        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 0ull);

        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 1ull);

        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 127ull);

        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 0xFFFFFFFFFFFFFFFFull);

        // Sentinel
        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
    }
    // Overflow: raw 2^64+1 [02 00 00 00 00 00 00 00 00 81]
    {
        auto bytes = make_bytes({0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong NULL: [00 80]
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong raw 1: [00 81]
    {
        auto bytes = make_bytes({0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Truncated prefix (no stop bit) — 10-byte representative
    {
        std::uint8_t full[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        for (int len = 1; len <= 9; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::uint64_t val = 0xDEAD;
            bool is_null = true;
            CHECK(c.read_nullable_u64(val, is_null) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 0xDEADull);
            CHECK(is_null);
        }
    }
    TEST_PASS("nullable_u64");
}

static void test_nullable_i64() {
    // Sequential read of all required vectors
    {
        auto bytes = make_bytes({
            0x80,                                           // NULL
            0x81,                                           // 0
            0x82,                                           // 1
            0xFF,                                           // -1
            0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, // INT64_MIN
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, // INT64_MAX
            0x80                                            // sentinel
        });
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        bool is_null = false;

        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(c.position(), 1u);

        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 0ll);

        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 1ll);

        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, -1ll);

        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, (-9223372036854775807ll - 1ll));

        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 9223372036854775807ll);

        // Sentinel
        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
    }
    // Positive overflow: raw 2^63+1 [01 00 00 00 00 00 00 00 00 81]
    {
        auto bytes = make_bytes({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Below minimum: raw -2^63-1 [7E 7F 7F 7F 7F 7F 7F 7F 7F FF]
    {
        auto bytes = make_bytes({0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
    }
    // Overlong NULL: [00 80]
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // 10-byte overlong NULL: [00 00 00 00 00 00 00 00 00 80]
    {
        auto bytes = make_bytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, 0ll);
        CHECK(!is_null);
    }
    // Signed overlong -1: [7F FF]
    {
        auto bytes = make_bytes({0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }
    // Truncated prefix (no stop bit) — 10-byte representative
    {
        std::uint8_t full[] = {0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        for (int len = 1; len <= 9; ++len) {
            WireCursor c(full, static_cast<std::size_t>(len));
            std::int64_t val = 12345;
            bool is_null = false;
            CHECK(c.read_nullable_i64(val, is_null) == DecodeStatus::NeedMoreData);
            CHECK_EQ(c.position(), 0u);
            CHECK_EQ(val, 12345ll);
            CHECK(!is_null);
        }
    }
    TEST_PASS("nullable_i64");
}

// --- Presence map tests (stop-bit termination, minimal encoding, atomicity) ---
static void test_presence_map() {
    // [B0] with pmap_bits=7: pattern 0110000
    {
        auto bytes = make_bytes({0xB0});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(7, out) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 7u);
        CHECK_EQ(out[0], false);
        CHECK_EQ(out[1], true);
        CHECK_EQ(out[2], true);
        CHECK_EQ(out[3], false);
        CHECK_EQ(out[4], false);
        CHECK_EQ(out[5], false);
        CHECK_EQ(out[6], false);
        CHECK(c.at_end());
    }
    // Canonical cross-byte [00 C0]: 8 bits, first 7 zero, eighth true
    {
        auto bytes = make_bytes({0x00, 0xC0});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out;
        CHECK(c.read_presence_map(8, out) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 8u);
        for (int i = 0; i < 7; ++i) CHECK_EQ(out[i], false);
        CHECK_EQ(out[7], true);
        CHECK(c.at_end());
    }
    // [FF] requesting 12 bits: 7 true bits + 5 implicit false
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out;
        CHECK(c.read_presence_map(12, out) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 12u);
        for (int i = 0; i < 7; ++i) CHECK_EQ(out[i], true);
        for (int i = 7; i < 12; ++i) CHECK_EQ(out[i], false);
        CHECK(c.at_end());
    }
    // [80] with pmap_bits=14 and max_pmap_bytes=1: implicit zero suffix
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out;
        CHECK(c.read_presence_map(14, out, 1) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 14u);
        for (int i = 0; i < 14; ++i) CHECK_EQ(out[i], false);
        CHECK(c.at_end());
    }
    // pmap_bits=0 with [80]: stop bit only, empty output
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out;
        CHECK(c.read_presence_map(0, out) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 0u);
        CHECK_EQ(c.position(), 1u);
    }
    // Exact cursor consumption: [B0] then sentinel byte 0xAA
    {
        auto bytes = make_bytes({0xB0, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out;
        CHECK(c.read_presence_map(7, out) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 7u);
        CHECK_EQ(c.position(), 1u);
        // Sentinel byte still readable
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xAAu);
    }
    // Overlong [00 80]: multi-byte, terminating group zero -> NonCanonicalEncoding
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(7, out) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 3u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // Overlong [7F 80]: multi-byte, terminating group zero -> NonCanonicalEncoding
    {
        auto bytes = make_bytes({0x7F, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(14, out) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 3u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // Overlong [01 00 80]: 3-byte, terminating group zero -> NonCanonicalEncoding
    {
        auto bytes = make_bytes({0x01, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true};
        CHECK(c.read_presence_map(14, out) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 1u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // Truncated [00] at max_pmap_bytes=2: no stop bit, input exhausted -> NeedMoreData
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(7, out, 2) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 3u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // [00] at max_pmap_bytes=1: continuation byte at limit -> LimitExceeded
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(7, out, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 3u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // [00 80] at max_pmap_bytes=1: second byte exceeds limit -> LimitExceeded
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(7, out, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 3u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // [80] at max_pmap_bytes=0: immediate LimitExceeded, no read
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true, true, true};
        CHECK(c.read_presence_map(7, out, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u); // cursor unchanged (no read)
        CHECK_EQ(out.size(), 3u); // unchanged
        CHECK_EQ(out[0], true);
    }
    // ERR R7 regression: [00 A0] with pmap_bits=8. The terminating byte 0xA0
    // has non-zero data bits (0x20) beyond the requested 8-bit output prefix.
    // The decoder must accept this as canonical even though tmp[7]==false.
    {
        auto bytes = make_bytes({0x00, 0xA0, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out;
        CHECK(c.read_presence_map(8, out) == DecodeStatus::Ok);
        CHECK_EQ(out.size(), 8u);
        for (int i = 0; i < 8; ++i) CHECK_EQ(out[i], false);
        CHECK_EQ(c.position(), 2u);
        // Sentinel byte 0xAA still readable (output is atomic)
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xAAu);
    }
    // Default limit exhaustion: exactly 64 continuation bytes without stop bit
    // -> LimitExceeded, cursor rollback, output unchanged
    {
        std::vector<std::uint8_t> bytes(64, 0x00);
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true};
        CHECK(c.read_presence_map(1, out, 63) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 1u);
        CHECK_EQ(out[0], true);
    }
    TEST_PASS("presence_map");
}

// --- ASCII string tests (FIX FAST 1.1 stop-bit encoding) ---
static void test_ascii_string() {
    // Empty string: 0x80 (stop bit, data=0)
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK(c.at_end());
    }
    // "A" -> 0xC1 (data=0x41, stop=1)
    {
        auto bytes = make_bytes({0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
    }
    // "Hi" -> 0x48 0xE9 (data='H', stop=0; data='i'=0x69, stop=1)
    {
        auto bytes = make_bytes({0x48, 0xE9});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "Hi");
    }
    // "AB" -> 0x41 0xC2 (data='A', stop=0; data='B'=0x42, stop=1)
    {
        auto bytes = make_bytes({0x41, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "AB");
    }
    // Unterminated: no stop bit
    {
        auto bytes = make_bytes({0x48, 0x69});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u); // cursor restored
    }
    // Empty input
    {
        WireCursor c(nullptr, 0);
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::NeedMoreData);
    }
    TEST_PASS("ascii_string");
}

// --- Decimal tests ---
static void test_decimal() {
    // Decimal: exponent=2, mantissa=5000
    // Exponent 2 as non-nullable stopbit: 0x82
    // Mantissa 5000 stopbit: 5000 = 0x1388
    // 5000 >> 7 = 39, 5000 & 0x7F = 8
    // 39 >> 7 = 0, 39 & 0x7F = 39
    // Bytes: 0x27 (39, no stop), 0x88 (8, stop)
    {
        auto bytes = make_bytes({0x82, 0x27, 0x88});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t exp = 0;
        std::int64_t man = 0;
        bool is_null = false;
        CHECK(c.read_decimal(exp, man, is_null, false, false) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(exp, 2);
        CHECK_EQ(man, 5000);
    }
    // Null decimal: exponent starts with wire [80] (nullable i32 null)
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t exp = 99;
        std::int64_t man = 99;
        bool is_null = false;
        CHECK(c.read_decimal(exp, man, is_null, true, false) == DecodeStatus::Ok);
        CHECK(is_null);
        // Mantissa NOT consumed
        CHECK_EQ(c.position(), 1u);
    }
    TEST_PASS("decimal");
}

// --- Byte vector tests ---
static void test_byte_vector() {
    // Empty: length=0 -> 0x80
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK(val.empty());
    }
    // 3 bytes: length=3 -> 0x83, then 3 data bytes
    {
        auto bytes = make_bytes({0x83, 0xAA, 0xBB, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 3u);
        CHECK_EQ(val[0], 0xAA);
        CHECK_EQ(val[1], 0xBB);
        CHECK_EQ(val[2], 0xCC);
    }
    // Nullable: null (wire [80] for nullable u32 null)
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = false;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
    }
    // Nullable: empty (length=1 encoded as nullable: stopbit(1+1)=0x82)
    // Wait, for nullable byte vector, the length is nullable uInt32.
    // Length 0 means empty. Nullable uInt32 encoding of 0: stopbit(0+1)=0x81.
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    TEST_PASS("byte_vector");
}

// --- Unicode string tests ---
static void test_unicode_string() {
    // Empty: length=0 -> 0x80
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK(val.empty());
    }
    // "A" (1 byte UTF-8): length=1 -> 0x81, then 0x41
    {
        auto bytes = make_bytes({0x81, 0x41});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
    }
    // Nullable: null (length = nullable wire [80])
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = false;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
    }
    // Nullable: empty (length = nullable stopbit(0+1)=0x81, then 0 bytes)
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    TEST_PASS("unicode_string");
}

// --- JSON escape tests ---
static void test_json_escape() {
    CHECK_EQ(json_escape_string(""), "");
    CHECK_EQ(json_escape_string("abc"), "abc");
    CHECK_EQ(json_escape_string("a\"b"), "a\\\"b");
    CHECK_EQ(json_escape_string("a\\b"), "a\\\\b");
    CHECK_EQ(json_escape_string("a\nb"), "a\\nb");
    CHECK_EQ(json_escape_string("a\rb"), "a\\rb");
    CHECK_EQ(json_escape_string("a\tb"), "a\\tb");
    // Control character
    CHECK_EQ(json_escape_string(std::string(1, '\x01')), "\\u0001");
    TEST_PASS("json_escape");
}

int main() {
    test_stopbit_u32();
    test_stopbit_u64();
    test_stopbit_i32();
    test_stopbit_i64();
    test_nullable_u32();
    test_nullable_i32();
    test_nullable_u64();
    test_nullable_i64();
    test_presence_map();
    test_ascii_string();
    test_decimal();
    test_byte_vector();
    test_unicode_string();
    test_json_escape();
    std::cout << "All decoder primitive tests passed.\n";
    return 0;
}
