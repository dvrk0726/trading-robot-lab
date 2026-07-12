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
    // -> LimitExceeded at default max_pmap_bytes=64, cursor rollback, output unchanged
    {
        std::vector<std::uint8_t> bytes(64, 0x00);
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> out{true};
        CHECK(c.read_presence_map(1, out) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(out.size(), 1u);
        CHECK_EQ(out[0], true);
    }
    TEST_PASS("presence_map");
}

// --- ASCII string tests (FIX FAST 1.1 stop-bit encoding) ---
static void test_ascii_string() {
    // Mandatory empty: [80]
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
        CHECK(c.at_end());
    }
    // "A" -> [C1]
    {
        auto bytes = make_bytes({0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 1u);
    }
    // "AB" -> [41 C2]
    {
        auto bytes = make_bytes({0x41, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "AB");
        CHECK_EQ(c.position(), 2u);
    }
    // 0x7F payload -> [FF] (max ASCII character)
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0x7Fu);
        CHECK_EQ(c.position(), 1u);
    }
    // Exact cursor consumption: [C1] then sentinel byte 0xAA
    {
        auto bytes = make_bytes({0xC1, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 1u);
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xAAu);
    }
    // Invalid: [00 80] - zero payload in continuation position
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Invalid: [00 C1] - zero payload in continuation position
    {
        auto bytes = make_bytes({0x00, 0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Invalid: [41 80] - zero payload on stop byte after characters
    {
        auto bytes = make_bytes({0x41, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Unterminated: no stop bit [41]
    {
        auto bytes = make_bytes({0x41});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Empty input
    {
        WireCursor c(nullptr, 0);
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Limit=0: empty succeeds
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val, 0) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // Limit=0: non-empty returns LimitExceeded
    {
        auto bytes = make_bytes({0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Limit=1: "A" succeeds
    {
        auto bytes = make_bytes({0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val, 1) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
    }
    // Limit=1: "AB" returns LimitExceeded
    {
        auto bytes = make_bytes({0x41, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    TEST_PASS("ascii_string");
}

// --- Nullable ASCII tests ---
static void test_nullable_ascii() {
    // NULL: [80]
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = false;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val, "sentinel");
        CHECK_EQ(c.position(), 1u);
    }
    // Empty: [00 80]
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 2u);
    }
    // "A": [C1]
    {
        auto bytes = make_bytes({0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 1u);
    }
    // "AB": [41 C2]
    {
        auto bytes = make_bytes({0x41, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, "AB");
        CHECK_EQ(c.position(), 2u);
    }
    // Exact cursor consumption: "A" [C1] then sentinel 0xAA
    {
        auto bytes = make_bytes({0xC1, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = false;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 1u);
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xAAu);
    }
    // Truncated: [00] - NeedMoreData
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Invalid: [00 C1] - malformed preamble
    {
        auto bytes = make_bytes({0x00, 0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Invalid: [00 00 80] - malformed preamble
    {
        auto bytes = make_bytes({0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Limit=0: NULL succeeds
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = false;
        CHECK(c.read_nullable_ascii(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val, "sentinel");
    }
    // Limit=0: empty succeeds
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    // Limit=0: non-empty returns LimitExceeded
    {
        auto bytes = make_bytes({0xC1});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    TEST_PASS("nullable_ascii");
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
    // --- Mandatory production tests ---
    // Empty [80]: length=0
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
        CHECK(c.at_end());
    }
    // One zero byte [81 00]: length=1, body=0x00
    {
        auto bytes = make_bytes({0x81, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(val[0], 0x00u);
        CHECK_EQ(c.position(), 2u);
    }
    // One 0xFF byte [81 FF]: length=1, body=0xFF
    {
        auto bytes = make_bytes({0x81, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(val[0], 0xFFu);
        CHECK_EQ(c.position(), 2u);
    }
    // Mixed raw payload [84 00 FF 80 7F]: length=4
    {
        auto bytes = make_bytes({0x84, 0x00, 0xFF, 0x80, 0x7F});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 4u);
        CHECK_EQ(val[0], 0x00u);
        CHECK_EQ(val[1], 0xFFu);
        CHECK_EQ(val[2], 0x80u);
        CHECK_EQ(val[3], 0x7Fu);
        CHECK_EQ(c.position(), 5u);
    }
    // Exact cursor consumption [81 AA CC]: after decode value=[AA], position=2, next=CC
    {
        auto bytes = make_bytes({0x81, 0xAA, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(val[0], 0xAAu);
        CHECK_EQ(c.position(), 2u);
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xCCu);
    }
    // Length boundary: len=127, literal prefix [FF], exactly 127 raw bytes
    {
        std::vector<std::uint8_t> bytes;
        bytes.push_back(0xFF);  // stopbit(127)
        for (int i = 0; i < 127; ++i) bytes.push_back(static_cast<std::uint8_t>(i));
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 127u);
        for (int i = 0; i < 127; ++i) CHECK_EQ(val[i], static_cast<std::uint8_t>(i));
        CHECK_EQ(c.position(), 128u);
    }
    // Length boundary: len=128, literal prefix [01 80], exactly 128 raw bytes
    {
        std::vector<std::uint8_t> bytes;
        bytes.push_back(0x01); bytes.push_back(0x80);  // stopbit(128)
        for (int i = 0; i < 128; ++i) bytes.push_back(static_cast<std::uint8_t>(i & 0xFF));
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 128u);
        for (int i = 0; i < 128; ++i) CHECK_EQ(val[i], static_cast<std::uint8_t>(i & 0xFF));
        CHECK_EQ(c.position(), 130u);
    }

    // --- Mandatory error tests ---
    // Empty input -> NeedMoreData
    {
        WireCursor c(nullptr, 0);
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(val[0], 0xDEu);
        CHECK_EQ(val[1], 0xADu);
    }
    // Truncated prefix [01] -> NeedMoreData
    {
        auto bytes = make_bytes({0x01});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(val[0], 0xDEu);
    }
    // Non-canonical prefix [00 80] -> NonCanonicalEncoding
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
    }
    // Overflowing uInt32 length [10 00 00 00 80] -> IntegerOverflow
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
    }
    // Partial body [83 AA BB] -> NeedMoreData
    {
        auto bytes = make_bytes({0x83, 0xAA, 0xBB});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
    }

    // --- Mandatory limit tests ---
    // Empty [80] at max_bytes=0 -> Ok
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 0) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // One byte [81 AA] at max_bytes=0 -> LimitExceeded
    {
        auto bytes = make_bytes({0x81, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
    }
    // len=3 at limit=3 -> Ok
    {
        auto bytes = make_bytes({0x83, 0xAA, 0xBB, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val, 3) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 3u);
    }
    // len=3 at limit=2 -> LimitExceeded
    {
        auto bytes = make_bytes({0x83, 0xAA, 0xBB, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
    }
    // Truncated [83 AA] at limit=2 -> LimitExceeded (not NeedMoreData), limit precedence
    {
        auto bytes = make_bytes({0x83, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
    }

    // --- Nullable production tests ---
    // NULL [80]: is_null=true, output sentinel unchanged
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = false;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(val[0], 0xDEu);
        CHECK_EQ(c.position(), 1u);
    }
    // Empty [81]: is_null=false, empty vector
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // One zero byte [82 00]: length=1, body=0x00
    {
        auto bytes = make_bytes({0x82, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(val[0], 0x00u);
        CHECK_EQ(c.position(), 2u);
    }
    // One 0xFF byte [82 FF]: length=1, body=0xFF
    {
        auto bytes = make_bytes({0x82, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(val[0], 0xFFu);
        CHECK_EQ(c.position(), 2u);
    }
    // Mixed payload [85 00 FF 80 7F]: nullable len=4 (stopbit(5)=0x85)
    {
        auto bytes = make_bytes({0x85, 0x00, 0xFF, 0x80, 0x7F});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 4u);
        CHECK_EQ(val[0], 0x00u);
        CHECK_EQ(val[1], 0xFFu);
        CHECK_EQ(val[2], 0x80u);
        CHECK_EQ(val[3], 0x7Fu);
        CHECK_EQ(c.position(), 5u);
    }
    // Exact cursor consumption [82 AA CC]: after decode value=[AA], is_null=false, position=2, next=CC
    {
        auto bytes = make_bytes({0x82, 0xAA, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 1u);
        CHECK_EQ(val[0], 0xAAu);
        CHECK_EQ(c.position(), 2u);
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xCCu);
    }
    // Nullable length boundary: len=127, prefix [01 80] (nullable stopbit(128)), exactly 127 raw bytes
    {
        std::vector<std::uint8_t> bytes;
        bytes.push_back(0x01); bytes.push_back(0x80);  // nullable stopbit(128) -> len=127
        for (int i = 0; i < 127; ++i) bytes.push_back(static_cast<std::uint8_t>(i));
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 127u);
        for (int i = 0; i < 127; ++i) CHECK_EQ(val[i], static_cast<std::uint8_t>(i));
        CHECK_EQ(c.position(), 129u);
    }
    // Nullable length boundary: len=128, prefix [01 81] (nullable stopbit(129)), exactly 128 raw bytes
    {
        std::vector<std::uint8_t> bytes;
        bytes.push_back(0x01); bytes.push_back(0x81);  // nullable stopbit(129) -> len=128
        for (int i = 0; i < 128; ++i) bytes.push_back(static_cast<std::uint8_t>(i & 0xFF));
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 128u);
        for (int i = 0; i < 128; ++i) CHECK_EQ(val[i], static_cast<std::uint8_t>(i & 0xFF));
        CHECK_EQ(c.position(), 130u);
    }

    // --- Nullable error tests ---
    // Empty input -> NeedMoreData
    {
        WireCursor c(nullptr, 0);
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(val[0], 0xDEu);
        CHECK(is_null);
    }
    // Truncated prefix [01] -> NeedMoreData
    {
        auto bytes = make_bytes({0x01});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }
    // Non-canonical prefix [00 80] -> NonCanonicalEncoding
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }
    // Partial body [84 AA BB] -> NeedMoreData (len=3, only 2 body bytes)
    {
        auto bytes = make_bytes({0x84, 0xAA, 0xBB});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }
    // Non-empty limit failure: [82 AA] at limit=0 -> LimitExceeded
    {
        auto bytes = make_bytes({0x82, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }

    // --- Nullable limit tests ---
    // NULL [80] at max_bytes=0 -> Ok, is_null=true
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = false;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val.size(), 2u);
    }
    // Empty [81] at max_bytes=0 -> Ok, is_null=false, empty vector
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    // One byte [82 AA] at limit=0 -> LimitExceeded
    {
        auto bytes = make_bytes({0x82, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }
    // len=3 at limit=3 -> Ok
    {
        auto bytes = make_bytes({0x84, 0xAA, 0xBB, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 3) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 3u);
    }
    // len=3 at limit=2 -> LimitExceeded
    {
        auto bytes = make_bytes({0x84, 0xAA, 0xBB, 0xCC});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }
    // Truncated [84 AA] at limit=2 -> LimitExceeded (not NeedMoreData), limit before body
    {
        auto bytes = make_bytes({0x84, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK(is_null);
    }

    // --- Checked-length test ---
    // Nullable prefix [10 00 00 00 80] = non-null length UINT32_MAX,
    // default max_bytes should return LimitExceeded without allocation,
    // with rollback and unchanged outputs.
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(val[0], 0xDEu);
        CHECK(is_null);
    }
    TEST_PASS("byte_vector");
}

// --- Mandatory Unicode string tests ---
static void test_mandatory_unicode() {
    // Empty [80]: length=0
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // "A" [81 41]: length=1, byte 0x41
    {
        auto bytes = make_bytes({0x81, 0x41});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 2u);
    }
    // U+00A2 [82 C2 A2]: 2-byte UTF-8
    {
        auto bytes = make_bytes({0x82, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0xC2u);
        CHECK_EQ(static_cast<std::uint8_t>(val[1]), 0xA2u);
        CHECK_EQ(c.position(), 3u);
    }
    // U+20AC [83 E2 82 AC]: 3-byte UTF-8
    {
        auto bytes = make_bytes({0x83, 0xE2, 0x82, 0xAC});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 3u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0xE2u);
        CHECK_EQ(static_cast<std::uint8_t>(val[1]), 0x82u);
        CHECK_EQ(static_cast<std::uint8_t>(val[2]), 0xACu);
        CHECK_EQ(c.position(), 4u);
    }
    // U+1F600 [84 F0 9F 98 80]: 4-byte UTF-8
    {
        auto bytes = make_bytes({0x84, 0xF0, 0x9F, 0x98, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 4u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0xF0u);
        CHECK_EQ(static_cast<std::uint8_t>(val[1]), 0x9Fu);
        CHECK_EQ(static_cast<std::uint8_t>(val[2]), 0x98u);
        CHECK_EQ(static_cast<std::uint8_t>(val[3]), 0x80u);
        CHECK_EQ(c.position(), 5u);
    }
    // Exact cursor consumption: [81 41 AA] — after A, position=2, next byte AA
    {
        auto bytes = make_bytes({0x81, 0x41, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 2u);
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xAAu);
    }
    // --- Mandatory Unicode error vectors ---
    // Invalid continuation: [82 C2 41] — 41 is not a continuation byte
    {
        auto bytes = make_bytes({0x82, 0xC2, 0x41});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Overlong UTF-8: [82 C0 AF] — C0 AF is overlong encoding of '/'
    {
        auto bytes = make_bytes({0x82, 0xC0, 0xAF});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Surrogate U+D800: [83 ED A0 80]
    {
        auto bytes = make_bytes({0x83, 0xED, 0xA0, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Code point > U+10FFFF: [84 F4 90 80 80] = U+110000
    {
        auto bytes = make_bytes({0x84, 0xF4, 0x90, 0x80, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Declared body truncation: [82 C2] — len=2 but only 1 byte present
    {
        auto bytes = make_bytes({0x82, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // 1-byte body with truncated UTF-8 lead: [81 C2] — C2 is 2-byte lead
    {
        auto bytes = make_bytes({0x81, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Truncated length prefix: [01] — no stop bit
    {
        auto bytes = make_bytes({0x01});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Non-canonical length prefix: [00 80] — raw 0 in 2 bytes
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Overflowing mandatory uInt32 length: [10 00 00 00 80] = 2^32
    {
        auto bytes = make_bytes({0x10, 0x00, 0x00, 0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::IntegerOverflow);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Empty input
    {
        WireCursor c(nullptr, 0);
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // --- Mandatory Unicode limit tests ---
    // [80] at max_bytes=0 -> Ok empty (empty has zero bytes)
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val, 0) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // U+00A2 [82 C2 A2] at limit=2 -> Ok
    {
        auto bytes = make_bytes({0x82, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val, 2) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 2u);
    }
    // U+00A2 [82 C2 A2] at limit=1 -> LimitExceeded, rollback
    {
        auto bytes = make_bytes({0x82, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // [82 C2] at limit=1 -> LimitExceeded (not NeedMoreData), limit precedence
    {
        auto bytes = make_bytes({0x82, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    TEST_PASS("mandatory_unicode");
}

// --- Nullable Unicode string tests ---
static void test_nullable_unicode() {
    // NULL [80]: nullable u32 null
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = false;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val, "sentinel");
        CHECK_EQ(c.position(), 1u);
    }
    // Empty [81]: nullable u32 len=0 (stopbit(1)), no body bytes
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // "A" [82 41]: nullable u32 len=1 (stopbit(2)), body 0x41
    {
        auto bytes = make_bytes({0x82, 0x41});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 2u);
    }
    // U+00A2 [83 C2 A2]: nullable u32 len=2 (stopbit(3)), body C2 A2
    {
        auto bytes = make_bytes({0x83, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 2u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0xC2u);
        CHECK_EQ(static_cast<std::uint8_t>(val[1]), 0xA2u);
        CHECK_EQ(c.position(), 3u);
    }
    // U+20AC [84 E2 82 AC]: nullable u32 len=3 (stopbit(4)), body E2 82 AC
    {
        auto bytes = make_bytes({0x84, 0xE2, 0x82, 0xAC});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 3u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0xE2u);
        CHECK_EQ(static_cast<std::uint8_t>(val[1]), 0x82u);
        CHECK_EQ(static_cast<std::uint8_t>(val[2]), 0xACu);
        CHECK_EQ(c.position(), 4u);
    }
    // U+1F600 [85 F0 9F 98 80]: nullable u32 len=4 (stopbit(5)), body F0 9F 98 80
    {
        auto bytes = make_bytes({0x85, 0xF0, 0x9F, 0x98, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 4u);
        CHECK_EQ(static_cast<std::uint8_t>(val[0]), 0xF0u);
        CHECK_EQ(static_cast<std::uint8_t>(val[1]), 0x9Fu);
        CHECK_EQ(static_cast<std::uint8_t>(val[2]), 0x98u);
        CHECK_EQ(static_cast<std::uint8_t>(val[3]), 0x80u);
        CHECK_EQ(c.position(), 5u);
    }
    // Exact cursor consumption: [82 41 AA] — after A, position=2, next byte AA
    {
        auto bytes = make_bytes({0x82, 0x41, 0xAA});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = false;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, "A");
        CHECK_EQ(c.position(), 2u);
        std::uint8_t sentinel = 0;
        CHECK(c.read_byte(sentinel) == DecodeStatus::Ok);
        CHECK_EQ(sentinel, 0xAAu);
    }
    // --- Nullable Unicode error/atomicity tests ---
    // Body truncation: [83 C2] — len=2 but only 1 byte present -> NeedMoreData
    {
        auto bytes = make_bytes({0x83, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Invalid continuation: [83 C2 41] — 41 not a continuation byte -> InvalidEncoding
    {
        auto bytes = make_bytes({0x83, 0xC2, 0x41});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::InvalidEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Truncated prefix: [01] — no stop bit -> NeedMoreData
    {
        auto bytes = make_bytes({0x01});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::NeedMoreData);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Non-canonical nullable prefix: [00 80] — raw 0 in 2 bytes -> NonCanonicalEncoding
    {
        auto bytes = make_bytes({0x00, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Limit failure: U+00A2 [83 C2 A2] at limit=1 -> LimitExceeded
    {
        auto bytes = make_bytes({0x83, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // --- Nullable Unicode limit tests ---
    // NULL [80] at max_bytes=0 -> Ok, is_null=true
    {
        auto bytes = make_bytes({0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = false;
        CHECK(c.read_nullable_unicode(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val, "sentinel");
    }
    // Empty [81] at max_bytes=0 -> Ok, is_null=false, string empty
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    // U+00A2 [83 C2 A2] at limit=2 -> Ok
    {
        auto bytes = make_bytes({0x83, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 2) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 2u);
    }
    // U+00A2 [83 C2 A2] at limit=1 -> LimitExceeded
    {
        auto bytes = make_bytes({0x83, 0xC2, 0xA2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // [83 C2] at limit=1 -> LimitExceeded (not NeedMoreData), limit-before-body precedence
    {
        auto bytes = make_bytes({0x83, 0xC2});
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    TEST_PASS("nullable_unicode");
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
    test_nullable_ascii();
    test_decimal();
    test_byte_vector();
    test_mandatory_unicode();
    test_nullable_unicode();
    test_json_escape();
    std::cout << "All decoder primitive tests passed.\n";
    return 0;
}
