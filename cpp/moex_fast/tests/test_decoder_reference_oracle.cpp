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
using fast_oracle::encode_nullable_ascii;
using fast_oracle::encode_unicode_string;
using fast_oracle::encode_nullable_unicode;
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
    // Empty map -> [80] (stop bit only)
    {
        byte_vec expected{0x80};
        byte_vec actual;
        encode_presence_map(actual, (const bool*)nullptr, 0);
        CHECK_BYTES(actual, expected);
    }
    // All zeros, 3 bits -> [80] (stop bit only, trailing groups stripped)
    {
        byte_vec expected{0x80};
        bool bits[] = {false, false, false};
        byte_vec actual;
        encode_presence_map(actual, bits, 3);
        CHECK_BYTES(actual, expected);
    }
    // 14 bits all zero -> [80] (stop bit only)
    {
        byte_vec expected{0x80};
        bool bits[14] = {};
        byte_vec actual;
        encode_presence_map(actual, bits, 14);
        CHECK_BYTES(actual, expected);
    }
    // Pattern 0110000, 7 bits -> [B0]
    {
        byte_vec expected{0xB0};
        bool bits[] = {false, true, true, false, false, false, false};
        byte_vec actual;
        encode_presence_map(actual, bits, 7);
        CHECK_BYTES(actual, expected);
    }
    // Seven ones + zero suffix, 12 bits -> [FF]
    {
        byte_vec expected{0xFF};
        bool bits[12];
        for (int i = 0; i < 7; ++i) bits[i] = true;
        for (int i = 7; i < 12; ++i) bits[i] = false;
        byte_vec actual;
        encode_presence_map(actual, bits, 12);
        CHECK_BYTES(actual, expected);
    }
    // Seven ones, 7 bits -> [FF]
    {
        byte_vec expected{0xFF};
        bool bits[] = {true, true, true, true, true, true, true};
        byte_vec actual;
        encode_presence_map(actual, bits, 7);
        CHECK_BYTES(actual, expected);
    }
    // Cross-byte eighth bit, 8 bits -> [00 C0]
    {
        byte_vec expected{0x00, 0xC0};
        bool bits[8] = {};
        bits[7] = true;
        byte_vec actual;
        encode_presence_map(actual, bits, 8);
        CHECK_BYTES(actual, expected);
    }
    // Two-byte with non-zero second group, 10 bits -> [7F C0]
    {
        byte_vec expected{0x7F, 0xC0};
        bool bits[10];
        for (int i = 0; i < 7; ++i) bits[i] = true;
        bits[7] = true; bits[8] = false; bits[9] = false;
        byte_vec actual;
        encode_presence_map(actual, bits, 10);
        CHECK_BYTES(actual, expected);
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
    // 35-bit 2's complement sign-extended: groups MSB first
    // raw=0x780000000: g4=0x78, g3=0x00, g2=0x00, g1=0x00, g0=0x80
    {
        byte_vec expected{0x78, 0x00, 0x00, 0x00, 0x80};
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
    // INT64_MIN -> 10 bytes: [7F 00 00 00 00 00 00 00 00 80]
    {
        byte_vec expected{0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_stopbit_i64(actual, (-9223372036854775807ll - 1ll));
        CHECK_BYTES(actual, expected);
    }
    // INT64_MAX -> 10 bytes: [00 7F 7F 7F 7F 7F 7F 7F 7F FF]
    {
        byte_vec expected{0x00, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF};
        byte_vec actual; encode_stopbit_i64(actual, 9223372036854775807ll);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("stopbit_i64");
}

// --- Nullable uInt32 ---
static void test_nullable_u32() {
    // Null -> [80] (stop-bit 0)
    {
        byte_vec expected{0x80};
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
    // UINT32_MAX -> stopbit(2^32) = 0x10 0x00 0x00 0x00 0x80
    {
        byte_vec expected{0x10, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_nullable_u32(actual, 0xFFFFFFFFu, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_u32");
}

// --- Nullable uInt64 ---
static void test_nullable_u64() {
    // Null -> [80] (stop-bit 0)
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_nullable_u64(actual, 0ull, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> stopbit(1) = 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_u64(actual, 0ull, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 1 -> stopbit(2) = 0x82
    {
        byte_vec expected{0x82};
        byte_vec actual; encode_nullable_u64(actual, 1ull, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 127 -> stopbit(128) = 0x01 0x80
    {
        byte_vec expected{0x01, 0x80};
        byte_vec actual; encode_nullable_u64(actual, 127ull, false);
        CHECK_BYTES(actual, expected);
    }
    // UINT64_MAX -> explicit widened: raw 2^64 = [02 00 00 00 00 00 00 00 00 80]
    {
        byte_vec expected{0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_nullable_u64(actual, 0xFFFFFFFFFFFFFFFFull, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_u64");
}

// --- Nullable int32 ---
static void test_nullable_i32() {
    // Null -> [80] (stop-bit 0)
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_nullable_i32(actual, 0, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> stopbit(0+1) = stopbit(1) = 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_i32(actual, 0, false);
        CHECK_BYTES(actual, expected);
    }
    // Value -1 -> negative, encoded unchanged as stopbit(-1) = 0xFF
    {
        byte_vec expected{0xFF};
        byte_vec actual; encode_nullable_i32(actual, -1, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 1 -> stopbit(1+1) = stopbit(2) = 0x82
    {
        byte_vec expected{0x82};
        byte_vec actual; encode_nullable_i32(actual, 1, false);
        CHECK_BYTES(actual, expected);
    }
    // INT32_MIN -> negative, unchanged, stopbit(INT32_MIN) = [78 00 00 00 80]
    {
        byte_vec expected{0x78, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_nullable_i32(actual, -2147483647 - 1, false);
        CHECK_BYTES(actual, expected);
    }
    // INT32_MAX -> non-negative, stopbit(INT32_MAX+1) = stopbit(2^31) = [08 00 00 00 80]
    {
        byte_vec expected{0x08, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_nullable_i32(actual, 2147483647, false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_i32");
}

// --- Nullable int64 ---
static void test_nullable_i64() {
    // Null -> [80] (stop-bit 0)
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_nullable_i64(actual, 0ll, true);
        CHECK_BYTES(actual, expected);
    }
    // Value 0 -> stopbit(0+1) = stopbit(1) = 0x81
    {
        byte_vec expected{0x81};
        byte_vec actual; encode_nullable_i64(actual, 0ll, false);
        CHECK_BYTES(actual, expected);
    }
    // Value 1 -> stopbit(1+1) = stopbit(2) = 0x82
    {
        byte_vec expected{0x82};
        byte_vec actual; encode_nullable_i64(actual, 1ll, false);
        CHECK_BYTES(actual, expected);
    }
    // Value -1 -> negative, encoded unchanged as stopbit(-1) = 0xFF
    {
        byte_vec expected{0xFF};
        byte_vec actual; encode_nullable_i64(actual, -1ll, false);
        CHECK_BYTES(actual, expected);
    }
    // INT64_MIN -> negative, unchanged, stopbit(INT64_MIN) = [7F 00 00 00 00 00 00 00 00 80]
    {
        byte_vec expected{0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_nullable_i64(actual, (-9223372036854775807ll - 1ll), false);
        CHECK_BYTES(actual, expected);
    }
    // INT64_MAX -> non-negative, widened INT64_MAX+1 = 2^63 = [01 00 00 00 00 00 00 00 00 80]
    {
        byte_vec expected{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
        byte_vec actual; encode_nullable_i64(actual, 9223372036854775807ll, false);
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

// --- Nullable ASCII ---
static void test_nullable_ascii() {
    // NULL -> [80]
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_nullable_ascii(actual, "", true);
        CHECK_BYTES(actual, expected);
    }
    // Empty (non-null) -> [00 80]
    {
        byte_vec expected{0x00, 0x80};
        byte_vec actual; encode_nullable_ascii(actual, "", false);
        CHECK_BYTES(actual, expected);
    }
    // "A" -> [C1]
    {
        byte_vec expected{0xC1};
        byte_vec actual; encode_nullable_ascii(actual, "A", false);
        CHECK_BYTES(actual, expected);
    }
    // "AB" -> [41 C2]
    {
        byte_vec expected{0x41, 0xC2};
        byte_vec actual; encode_nullable_ascii(actual, "AB", false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("nullable_ascii");
}

// --- Mandatory Unicode string ---
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
    // U+00A2 -> length stopbit(2)=0x82, then C2 A2
    {
        byte_vec expected{0x82, 0xC2, 0xA2};
        byte_vec actual; encode_unicode_string(actual, "\xC2\xA2");
        CHECK_BYTES(actual, expected);
    }
    // U+20AC -> length stopbit(3)=0x83, then E2 82 AC
    {
        byte_vec expected{0x83, 0xE2, 0x82, 0xAC};
        byte_vec actual; encode_unicode_string(actual, "\xE2\x82\xAC");
        CHECK_BYTES(actual, expected);
    }
    // U+1F600 -> length stopbit(4)=0x84, then F0 9F 98 80
    {
        byte_vec expected{0x84, 0xF0, 0x9F, 0x98, 0x80};
        byte_vec actual; encode_unicode_string(actual, "\xF0\x9F\x98\x80");
        CHECK_BYTES(actual, expected);
    }
    // Nullable: null -> [80] (nullable u32 null)
    {
        byte_vec expected{0x80};
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
    // Nullable: U+00A2 -> length nullable(2)=stopbit(3)=0x83, then C2 A2
    {
        byte_vec expected{0x83, 0xC2, 0xA2};
        byte_vec actual; encode_nullable_unicode(actual, "\xC2\xA2", false);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: U+20AC -> length nullable(3)=stopbit(4)=0x84, then E2 82 AC
    {
        byte_vec expected{0x84, 0xE2, 0x82, 0xAC};
        byte_vec actual; encode_nullable_unicode(actual, "\xE2\x82\xAC", false);
        CHECK_BYTES(actual, expected);
    }
    // Nullable: U+1F600 -> length nullable(4)=stopbit(5)=0x85, then F0 9F 98 80
    {
        byte_vec expected{0x85, 0xF0, 0x9F, 0x98, 0x80};
        byte_vec actual; encode_nullable_unicode(actual, "\xF0\x9F\x98\x80", false);
        CHECK_BYTES(actual, expected);
    }
    TEST_PASS("unicode_string");
}

// --- Decimal ---
static void test_decimal() {
    // --- Mandatory success vectors ---
    // exponent=0, mantissa=0 -> [80 80]
    {
        byte_vec expected{0x80, 0x80};
        byte_vec actual; encode_decimal(actual, 0, 0, false, false);
        CHECK_BYTES(actual, expected);
    }
    // exponent=2, mantissa=5000 -> [82 27 88]
    {
        byte_vec expected{0x82, 0x27, 0x88};
        byte_vec actual; encode_decimal(actual, 2, 5000, false, false);
        CHECK_BYTES(actual, expected);
    }
    // exponent=-2, mantissa=12345 -> [FE 00 60 B9]
    {
        byte_vec expected{0xFE, 0x00, 0x60, 0xB9};
        byte_vec actual; encode_decimal(actual, -2, 12345, false, false);
        CHECK_BYTES(actual, expected);
    }
    // exponent=0, mantissa=-1 -> [80 FF]
    {
        byte_vec expected{0x80, 0xFF};
        byte_vec actual; encode_decimal(actual, 0, -1, false, false);
        CHECK_BYTES(actual, expected);
    }

    // --- Nullable non-NULL success vectors ---
    // nullable exponent=0, mantissa=1 -> [81 81]
    // nullable stopbit(0+1)=stopbit(1)=0x81, ordinary stopbit(1)=0x81
    {
        byte_vec expected{0x81, 0x81};
        byte_vec actual; encode_decimal(actual, 0, 1, true, false);
        CHECK_BYTES(actual, expected);
    }
    // nullable exponent=2, mantissa=5000 -> [83 27 88]
    {
        byte_vec expected{0x83, 0x27, 0x88};
        byte_vec actual; encode_decimal(actual, 2, 5000, true, false);
        CHECK_BYTES(actual, expected);
    }

    // --- Nullable NULL ---
    // Null decimal (nullable exponent) -> [80] (nullable i32 null, no mantissa)
    {
        byte_vec expected{0x80};
        byte_vec actual; encode_null_decimal(actual);
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
    test_nullable_ascii();
    test_unicode_string();
    test_decimal();
    test_boundary_cases();
    test_byte_counts();
    std::cout << "All reference oracle tests passed.\n";
    return 0;
}
