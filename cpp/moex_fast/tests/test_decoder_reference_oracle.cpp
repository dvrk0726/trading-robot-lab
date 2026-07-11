// Independent test-only FIX FAST 1.1 reference oracle.
// Verifies the oracle encoder against hard-coded expected byte vectors.
// Does NOT link against moex_decode or use WireCursor / fast_decoder.
#include "fast_reference_encoder.hpp"
#include "test_check.hpp"
#include <vector>
#include <cstring>

using fast_oracle::push_byte;
using fast_oracle::encode_presence_map;
using fast_oracle::encode_stopbit_u32;
using fast_oracle::encode_stopbit_u64;
using fast_oracle::encode_stopbit_i32;
using fast_oracle::encode_stopbit_i64;
using fast_oracle::encode_nullable_u32;
using fast_oracle::encode_nullable_u64;
using fast_oracle::encode_nullable_i32;
using fast_oracle::encode_nullable_i64;
using fast_oracle::encode_ascii_string;
using fast_oracle::encode_unicode_string;
using fast_oracle::encode_nullable_unicode;
using fast_oracle::encode_byte_vector;
using fast_oracle::encode_nullable_byte_vector;
using fast_oracle::encode_decimal;
using fast_oracle::encode_null_decimal;

using byte_vec = std::vector<std::uint8_t>;

static void check_bytes(const byte_vec& actual, const byte_vec& expected,
                        const char* label, int line) {
    if (actual != expected) {
        std::cerr << "CHECK BYTES FAILED: " << label
                  << "\n  expected (" << expected.size() << "):";
        for (auto b : expected) std::cerr << " " << std::hex << (int)b;
        std::cerr << "\n  actual   (" << actual.size() << "):";
        for (auto b : actual) std::cerr << " " << std::hex << (int)b;
        std::cerr << std::dec << "\n  at line " << line << "\n";
        std::exit(1);
    }
}
#define CHECK_BYTES(actual, expected) check_bytes(actual, expected, #actual " == " #expected, __LINE__)

// --- Presence map ---
static void test_presence_map() {
    // Single byte, 7 bits: 0110000 with stop -> 0xB0
    {
        byte_vec expected{0xB0};
        bool bits[] = {false, true, true, false, false, false, false};
        byte_vec actual;
        encode_presence_map(actual, bits, 7);
        CHECK_EQ(actual.size(), expected.size());
        CHECK_EQ(actual[0], expected[0]);
        TEST_PASS("pmap 7 bits single byte");
    }
    // Two bytes: 7 ones + 5 zeros, 12 bits
    // byte0=0x7F (no stop), byte1=0x80 (stop, 5 data zeros)
    {
        byte_vec expected{0x7F, 0x80};
        bool bits[12];
        for (int i = 0; i < 7; ++i) bits[i] = true;
        for (int i = 7; i < 12; ++i) bits[i] = false;
        byte_vec actual;
        encode_presence_map(actual, bits, 12);
        CHECK_EQ(actual.size(), 2u);
        CHECK_EQ(actual[0], expected[0]);
        CHECK_EQ(actual[1], expected[1]);
        TEST_PASS("pmap 12 bits two bytes");
    }
    // All zeros, 3 bits: first byte stop, data=0000000 -> 0x80
    {
        byte_vec expected{0x80};
        bool bits[] = {false, false, false};
        byte_vec actual;
        encode_presence_map(actual, bits, 3);
        CHECK_EQ(actual.size(), 1u);
        CHECK_EQ(actual[0], expected[0]);
        TEST_PASS("pmap 3 bits all zero");
    }
    // 14 bits all zero: needs 2 bytes (ceil(14/7)=2)
    // byte0=0x00 (no stop), byte1=0x80 (stop)
    {
        byte_vec expected{0x00, 0x80};
        bool bits[14] = {};
        byte_vec actual;
        encode_presence_map(actual, bits, 14);
        CHECK_EQ(actual.size(), 2u);
        CHECK_EQ(actual[0], expected[0]);
        CHECK_EQ(actual[1], expected[1]);
        TEST_PASS("pmap 14 bits all zero");
    }
    // All ones, 7 bits: 0xFF
    {
        byte_vec expected{0xFF};
        bool bits[] = {true, true, true, true, true, true, true};
        byte_vec actual;
        encode_presence_map(actual, bits, 7);
        CHECK_EQ(actual.size(), 1u);
        CHECK_EQ(actual[0], expected[0]);
        TEST_PASS("pmap 7 bits all one");
    }
}

// --- uInt32 stop-bit encoding ---
static void test_stopbit_u32() {
    // 0 -> 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_stopbit_u32(actual, 0u);
        CHECK_BYTES(actual, expected);
    }
    // 1 -> 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_stopbit_u32(actual, 1u);
        CHECK_BYTES(actual, expected);
    }
    // 127 -> 0xFF
    {
        byte_vec expected{0xFF};
        byte_vec actual; encode_stopbit_u32(actual, 127u);
        CHECK_BYTES(actual, expected);
    }
    // 128 -> 0x01 0x80
    {
        byte_vec expected{0x01, 0x80};
        byte_vec actual; encode_stopbit_u32(actual, 128u);
        CHECK_BYTES(actual, expected);
    }
    // 256 -> 0x02 0x80
    {
        byte_vec expected{0x02, 0x80};
        byte_vec actual; encode_stopbit_u32(actual, 256u);
        CHECK_BYTES(actual, expected);
    }
    // UINT32_MAX -> 0x0F 0x7F 0x7F 0x7F 0xFF
    {
        byte_vec expected{0x0F, 0x7F, 0x7F, 0x7F, 0xFF};
        byte_vec actual; encode_stopbit_u32(actual, 0xFFFFFFFFu);
        CHECK_BYTES(actual, expected);
    }
    // 16383 -> 0x7F 0xFF
    {
        byte_vec expected{0x7F, 0xFF};
        byte_vec actual; encode_stopbit_u32(actual, 16383u);
        CHECK_BYTES(actual, expected);
    }
    // 16384 -> 0x01 0x00 0x80
    {
        byte_vec expected{0x01, 0x00, 0x80};
        byte_vec actual; encode_stopbit_u32(actual, 16384u);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("stopbit_u32");
}

// --- uInt64 stop-bit encoding ---
static void test_stopbit_u64() {
    // 0 -> 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_stopbit_u64(actual, 0ull);
        CHECK_BYTES(actual, expected);
    }
    // 16384 -> 0x01 0x00 0x80
    {
        byte_vec expected{0x01, 0x00, 0x80};
        byte_vec actual; encode_stopbit_u64(actual, 16384ull);
        CHECK_BYTES(actual, expected);
    }
    // UINT64_MAX -> 0x01 0x7F 0x7F 0x7F 0x7F 0x7F 0x7F 0x7F 0x7F 0xFF
    {
        byte_vec expected{0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF};
        byte_vec actual; encode_stopbit_u64(actual, 0xFFFFFFFFFFFFFFFFull);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("stopbit_u64");
}

// --- int32 stop-bit encoding ---
static void test_stopbit_i32() {
    // 0 -> 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_stopbit_i32(actual, 0);
        CHECK_BYTES(actual, expected);
    }
    // 1 -> 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_stopbit_i32(actual, 1);
        CHECK_BYTES(actual, expected);
    }
    // -1 -> 0xFF
    {
        byte_vec expected{0xFF};
        byte_vec actual; encode_stopbit_i32(actual, -1);
        CHECK_BYTES(actual, expected);
    }
    // 63 -> 0xBF (1 byte, 7 bits: sign=0, value=0111111, stop=1)
    {
        byte_vec expected{0xBF};
        byte_vec actual; encode_stopbit_i32(actual, 63);
        CHECK_BYTES(actual, expected);
    }
    // 64 -> 0x00 0xC0
    {
        byte_vec expected{0x00, 0xC0};
        byte_vec actual; encode_stopbit_i32(actual, 64);
        CHECK_BYTES(actual, expected);
    }
    // -64 -> 0xC0 (1 byte: sign=1, value=100000, stop=1)
    {
        byte_vec expected{0xC0};
        byte_vec actual; encode_stopbit_i32(actual, -64);
        CHECK_BYTES(actual, expected);
    }
    // -65 -> 0x7F 0xBF
    {
        byte_vec expected{0x7F, 0xBF};
        byte_vec actual; encode_stopbit_i32(actual, -65);
        CHECK_BYTES(actual, expected);
    }
    // INT32_MAX (2147483647 = 0x7FFFFFFF) -> 5 bytes
    // 32 bits 2's complement: sign=0, data=111...1 (31 ones)
    // 5 groups of 7: g4=0x07, g3=0x7F, g2=0x7F, g1=0x7F, g0=0xFF
    {
        byte_vec expected{0x07, 0x7F, 0x7F, 0x7F, 0xFF};
        byte_vec actual; encode_stopbit_i32(actual, 2147483647);
        CHECK_BYTES(actual, expected);
    }
    // INT32_MIN (-2147483648 = 0x80000000) -> 5 bytes
    // 33 bits 2's complement: 5 groups
    // raw=0x80000000: g4=0x08, g3=0x00, g2=0x00, g1=0x00, g0=0x80
    {
        byte_vec expected{0x08, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_stopbit_i32(actual, -2147483647 - 1);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("stopbit_i32");
}

// --- int64 stop-bit encoding ---
static void test_stopbit_i64() {
    // 0 -> 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_stopbit_i64(actual, 0ll);
        CHECK_BYTES(actual, expected);
    }
    // -1 -> 0xFF
    {
        byte_vec expected{0xFF};
        byte_vec actual; encode_stopbit_i64(actual, -1ll);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("stopbit_i64");
}

// --- Nullable uInt32 ---
static void test_nullable_u32() {
    // Null -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_nullable_u32(actual, 0u, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> stopbit(1) = 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_u32(actual, 0u, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 1 -> stopbit(2) = 0x82
    {
        byte_vec expected{0x82};
        byte_vec actual; encode_nullable_u32(actual, 1u, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 127 -> stopbit(128) = 0x01 0x80
    {
        byte_vec expected{0x01, 0x80};
        byte_vec actual; encode_nullable_u32(actual, 127u, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_u32");
}

// --- Nullable uInt64 ---
static void test_nullable_u64() {
    // Null -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_nullable_u64(actual, 0ull, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_u64(actual, 0ull, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_u64");
}

// --- Nullable int32 ---
static void test_nullable_i32() {
    // Null -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_nullable_i32(actual, 0, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> stopbit(0+1) = stopbit(1) = 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_i32(actual, 0, false);
        CHECK_BYTES(actual, expected);
    }
    // Value -1 -> stopbit(-1+1) = stopbit(0) = 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_nullable_i32(actual, -1, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 1 -> stopbit(2) = 0x82
    {
        byte_vec expected{0x82};
        byte_vec actual; encode_nullable_i32(actual, 1, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_i32");
}

// --- Nullable int64 ---
static void test_nullable_i64() {
    // Null -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_nullable_i64(actual, 0ll, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_i64(actual, 0ll, false);
        CHECK_BYTES(actual, expected);
    }
    // Value -1 -> 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_nullable_i64(actual, -1ll, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_i64");
}

// --- ASCII string ---
static void test_ascii_string() {
    // Empty -> 0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_ascii_string(actual, "");
        CHECK_BYTES(actual, expected);
    }
    // "A" -> 0xC1
    {
        byte_vec expected{0xC1};
        byte_vec actual; encode_ascii_string(actual, "A");
        CHECK_BYTES(actual, expected);
    }
    // "AB" -> 0x41 0xC2
    {
        byte_vec expected{0x41, 0xC2};
        byte_vec actual; encode_ascii_string(actual, "AB");
        CHECK_BYTES(actual, expected);
    }
    // "Hi" -> 0x48 0xE9
    {
        byte_vec expected{0x48, 0xE9};
        byte_vec actual; encode_ascii_string(actual, "Hi");
        CHECK_BYTES(actual, expected);
    }
    // Single char 'z' (0x7A) -> 0xFA
    {
        byte_vec expected{0xFA};
        byte_vec actual; encode_ascii_string(actual, "z");
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("ascii_string");
}

// --- Unicode string ---
static void test_unicode_string() {
    // Empty -> length stopbit(0)=0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_unicode_string(actual, "");
        CHECK_BYTES(actual, expected);
    }
    // "A" -> length stopbit(1)=0x81, then 0x41
    {
        byte_vec expected{0x81, 0x41};
        byte_vec actual; encode_unicode_string(actual, "A");
        CHECK_BYTES(actual, expected);
    }
    // Nullable: null -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_nullable_unicode(actual, "", true);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: empty -> length nullable(0)=stopbit(1)=0x81, no bytes
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_unicode(actual, "", false);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: "A" -> length nullable(1)=stopbit(2)=0x82, then 0x41
    {
        byte_vec expected{0x82, 0x41};
        byte_vec actual; encode_nullable_unicode(actual, "A", false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("unicode_string");
}

// --- Byte vector ---
static void test_byte_vector() {
    // Empty -> length stopbit(0)=0x80
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_byte_vector(actual, {});
        CHECK_BYTES(actual, expected);
    }
    // 3 bytes {0xAA, 0xBB, 0xCC} -> length stopbit(3)=0x83, then data
    {
        byte_vec expected{0x83, 0xAA, 0xBB, 0xCC};
        byte_vec data{0xAA, 0xBB, 0xCC};
        byte_vec actual; encode_byte_vector(actual, data);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: null -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_nullable_byte_vector(actual, {}, true);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: empty -> length nullable(0)=stopbit(1)=0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_byte_vector(actual, {}, false);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: 1 byte {0xFF} -> length nullable(1)=stopbit(2)=0x82, then 0xFF
    {
        byte_vec expected{0x82, 0xFF};
        byte_vec data{0xFF};
        byte_vec actual; encode_nullable_byte_vector(actual, data, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("byte_vector");
}

// --- Decimal ---
static void test_decimal() {
    // exponent=2, mantissa=5000 (non-nullable exponent, non-nullable mantissa)
    // exponent stopbit(2)=0x82, mantissa stopbit(5000)
    // 5000 = 0x1388, encoded: 0x27 0x88
    {
        byte_vec expected{0x82, 0x27, 0x88};
        byte_vec actual; encode_decimal(actual, 2, 5000, false, false);
        CHECK_BYTES(actual, expected);
    }
    // exponent=0, mantissa=0
    // exponent stopbit(0)=0x80, mantissa stopbit(0)=0x80
    {
        byte_vec expected{0x80, 0x80};
        byte_vec actual; encode_decimal(actual, 0, 0, false, false);
        CHECK_BYTES(actual, expected);
    }
    // exponent=-2, mantissa=12345
    // exponent stopbit(-2) -> 0xFE (1 byte, raw i32 bottom 7 bits)
    // mantissa stopbit(12345): 12345 = 0x3039, 3 bytes -> groups MSB first: 0x00, 0x60, 0xB9
    {
        byte_vec expected{0xFE, 0x00, 0x60, 0xB9};
        byte_vec actual; encode_decimal(actual, -2, 12345, false, false);
        CHECK_BYTES(actual, expected);
    }
    // Null decimal (nullable exponent) -> 0x00
    {
        byte_vec expected{0x00};
        byte_vec actual; encode_null_decimal(actual);
        CHECK_BYTES(actual, expected);
    }
    // Nullable exponent, non-null: exponent=2 -> nullable stopbit(2+1)=stopbit(3)=0x83
    {
        byte_vec expected{0x83, 0x27, 0x88};
        byte_vec actual; encode_decimal(actual, 2, 5000, true, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("decimal");
}

// --- Boundary / edge cases ---
static void test_boundary_cases() {
    // uInt32: 2^21 - 1 = 2097151 = 0x1FFFFF, 3 bytes
    {
        byte_vec expected{0x7F, 0x7F, 0xFF};
        byte_vec actual; encode_stopbit_u32(actual, 2097151u);
        CHECK_BYTES(actual, expected);
    }
    // uInt32: 2^21 = 2097152, 4 bytes
    {
        byte_vec expected{0x01, 0x00, 0x00, 0x80};
        byte_vec actual; encode_stopbit_u32(actual, 2097152u);
        CHECK_BYTES(actual, expected);
    }
    // i32: -1 single byte boundary
    {
        byte_vec expected{0xFF};
        byte_vec actual; encode_stopbit_i32(actual, -1);
        CHECK_EQ(actual.size(), 1u);
        CHECK_BYTES(actual, expected);
    }
    // i32: 63 single byte max positive
    {
        byte_vec expected{0xBF};
        byte_vec actual; encode_stopbit_i32(actual, 63);
        CHECK_EQ(actual.size(), 1u);
        CHECK_BYTES(actual, expected);
    }
    // i32: 64 needs 2 bytes
    {
        byte_vec expected{0x00, 0xC0};
        byte_vec actual; encode_stopbit_i32(actual, 64);
        CHECK_EQ(actual.size(), 2u);
        CHECK_BYTES(actual, expected);
    }
    // i32: -64 single byte max negative
    {
        byte_vec expected{0xC0};
        byte_vec actual; encode_stopbit_i32(actual, -64);
        CHECK_EQ(actual.size(), 1u);
        CHECK_BYTES(actual, expected);
    }
    // i32: -65 needs 2 bytes
    {
        byte_vec expected{0x7F, 0xBF};
        byte_vec actual; encode_stopbit_i32(actual, -65);
        CHECK_EQ(actual.size(), 2u);
        CHECK_BYTES(actual, expected);
    }
    // Nullable uInt32: max value
    {
        byte_vec expected{0x0F, 0x7F, 0x7F, 0x7F, 0xFF};
        byte_vec actual; encode_nullable_u32(actual, 0xFFFFFFFEu, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("boundary_cases");
}

// --- Verify byte count for known wire lengths ---
static void test_byte_counts() {
    // uInt32 0: 1 byte
    {
        byte_vec actual; encode_stopbit_u32(actual, 0u);
        CHECK_EQ(actual.size(), 1u);
    }
    // uInt32 127: 1 byte
    {
        byte_vec actual; encode_stopbit_u32(actual, 127u);
        CHECK_EQ(actual.size(), 1u);
    }
    // uInt32 128: 2 bytes
    {
        byte_vec actual; encode_stopbit_u32(actual, 128u);
        CHECK_EQ(actual.size(), 2u);
    }
    // uInt32 16383: 2 bytes
    {
        byte_vec actual; encode_stopbit_u32(actual, 16383u);
        CHECK_EQ(actual.size(), 2u);
    }
    // uInt32 16384: 3 bytes
    {
        byte_vec actual; encode_stopbit_u32(actual, 16384u);
        CHECK_EQ(actual.size(), 3u);
    }
    // uInt64 UINT64_MAX: 10 bytes
    {
        byte_vec actual; encode_stopbit_u64(actual, 0xFFFFFFFFFFFFFFFFull);
        CHECK_EQ(actual.size(), 10u);
    }
    // ASCII string "Hello" (5 chars): 5 bytes
    {
        byte_vec actual; encode_ascii_string(actual, "Hello");
        CHECK_EQ(actual.size(), 5u);
    }
    TEST_PASS("byte_counts");
}

int main() {
    test_presence_map();
    test_stopbit_u32();
    test_stopbit_u64();
    test_stopbit_i32();
    test_stopbit_i64();
    test_nullable_u32();
    test_nullable_u64();
    test_nullable_i32();
    test_nullable_i64();
    test_ascii_string();
    test_unicode_string();
    test_byte_vector();
    test_decimal();
    test_boundary_cases();
    test_byte_counts();
    std::cout << "All reference oracle tests passed.\n";
    return 0;
}
