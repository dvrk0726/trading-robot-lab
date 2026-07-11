#include "moex_fast/wire_cursor.hpp"
#include "test_check.hpp"
#include <vector>
#include <cstring>

using namespace moex_fast;

// Helper: create a byte vector from a C array
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
    // UINT32_MAX: encoded as 5 bytes
    // 0xFFFFFFFF = 32 data bits => 5 stop-bit bytes (7*5=35 bits, 3 leading zeros)
    // 35-bit value: 000_11111111_11111111_11111111_11111111
    // 7-bit groups: 0x0F, 0x7F, 0x7F, 0x7F, 0x7F (last gets stop bit 0x80)
    // Bytes: 0x0F 0x7F 0x7F 0x7F 0xFF
    {
        auto bytes = make_bytes({0x0F, 0x7F, 0x7F, 0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, 0xFFFFFFFFu);
    }
    // Truncated
    {
        auto bytes = make_bytes({0x01});  // no stop bit
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::NeedMoreData);
    }
    // Empty
    {
        WireCursor c(nullptr, 0);
        std::uint32_t val = 0;
        CHECK(c.read_stopbit_u32(val) == DecodeStatus::NeedMoreData);
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
    // -1 -> 0x7F 0xFF (2-byte encoding with sign extension)
    {
        auto bytes = make_bytes({0x7F, 0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, -1);
    }
    // -1 single byte: 0xFF -> 7 bits = 1111111 = 127
    // Sign bit (bit 6) = 1, so negative. Sign extend from bit 6:
    // raw = 0x7F, sig_bits=7, sign_bit = 1<<6 = 0x40
    // 0x7F & 0x40 = 0x40 (non-zero) -> mask = ~0x7F = 0xFFFFFF80
    // raw = 0x7F | 0xFFFFFF80 = 0xFFFFFFFF = -1
    {
        auto bytes = make_bytes({0xFF});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, -1);
    }
    TEST_PASS("stopbit_i32");
}

// --- Nullable integer tests ---
static void test_nullable_u32() {
    // Null: 0x00 byte
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 999;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
    }
    // Non-null: 0x81 -> 1
    {
        auto bytes = make_bytes({0x81});
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = true;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val, 1u);
    }
    TEST_PASS("nullable_u32");
}

// --- Presence map tests ---
static void test_presence_map() {
    // Single byte: 0xB0 = 10110000 with stop bit
    // Bits (MSB first from data bits): 01, 10, 00, 0 -> 1, 0, 1, 1, 0, 0, 0
    // Wait: 0xB0 = 10110000
    // Stop bit (bit 7) = 1
    // Data bits (6..0): 0110000
    // Reading MSB first from data bits: bit6=0, bit5=1, bit4=1, bit3=0, bit2=0, bit1=0, bit0=0
    // So bits: 0, 1, 1, 0, 0, 0, 0
    {
        auto bytes = make_bytes({0xB0});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> bits;
        CHECK(c.read_presence_map(7, bits) == DecodeStatus::Ok);
        CHECK_EQ(bits.size(), 7u);
        CHECK_EQ(bits[0], false);
        CHECK_EQ(bits[1], true);
        CHECK_EQ(bits[2], true);
        CHECK_EQ(bits[3], false);
        CHECK_EQ(bits[4], false);
        CHECK_EQ(bits[5], false);
        CHECK_EQ(bits[6], false);
    }
    // Two bytes: 0x7F 0x80 (first byte NO stop bit, second byte HAS stop bit)
    // Byte 0: 0x7F = 01111111, stop bit = 0, data bits = 1111111 (7 bits, all 1)
    // Byte 1: 0x80 = 10000000, stop bit = 1, data bits = 0000000 (7 bits, all 0)
    // Total 12 bits: 111111100000
    {
        auto bytes = make_bytes({0x7F, 0x80});
        WireCursor c(bytes.data(), bytes.size());
        std::vector<bool> bits;
        CHECK(c.read_presence_map(12, bits) == DecodeStatus::Ok);
        CHECK_EQ(bits.size(), 12u);
        for (int i = 0; i < 7; ++i) CHECK_EQ(bits[i], true);
        for (int i = 7; i < 12; ++i) CHECK_EQ(bits[i], false);
    }
    TEST_PASS("presence_map");
}

// --- ASCII string tests ---
static void test_ascii_string() {
    // Empty string: just terminator 0x00
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK(val.empty());
    }
    // "A" -> 0x41 0x00
    {
        auto bytes = make_bytes({0x41, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "A");
    }
    // "Hi" -> 0x48 0x69 0x00
    {
        auto bytes = make_bytes({0x48, 0x69, 0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::Ok);
        CHECK_EQ(val, "Hi");
    }
    // Unterminated
    {
        auto bytes = make_bytes({0x48, 0x69});
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val) == DecodeStatus::NeedMoreData);
    }
    TEST_PASS("ascii_string");
}

// --- Decimal tests ---
static void test_decimal() {
    // Simple decimal: exponent=2, mantissa=5000
    // Exponent 2 -> 0x82 (stop bit, value 2)
    // Mantissa 5000 -> stop-bit encoding: 5000 >> 7 = 39, 5000 & 0x7F = 8
    // 39 -> 0x27 (no stop bit), 8 -> 0x88 (stop bit)
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
    // Null decimal: exponent starts with 0x00
    {
        auto bytes = make_bytes({0x00});
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t exp = 99;
        std::int64_t man = 99;
        bool is_null = true;
        CHECK(c.read_decimal(exp, man, is_null, true, false) == DecodeStatus::Ok);
        CHECK(is_null);
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
    TEST_PASS("byte_vector");
}

int main() {
    test_stopbit_u32();
    test_stopbit_u64();
    test_stopbit_i32();
    test_nullable_u32();
    test_presence_map();
    test_ascii_string();
    test_decimal();
    test_byte_vector();
    std::cout << "All decoder primitive tests passed.\n";
    return 0;
}
