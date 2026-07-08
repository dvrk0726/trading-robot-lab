#include "qsh/leb128.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace qsh;

static void test_uleb128_basic() {
    // Test encoding: 62 -> 0x3E
    uint8_t data[] = {0x3E};
    size_t offset = 0;
    assert(read_uleb128(data, 1, offset) == 62);
    assert(offset == 1);
    std::cout << "  PASS: uleb128 basic (62)" << std::endl;
}

static void test_uleb128_multibyte() {
    // 300 = 0x12C -> encoded as 0xAC 0x02
    uint8_t data[] = {0xAC, 0x02};
    size_t offset = 0;
    assert(read_uleb128(data, 2, offset) == 300);
    assert(offset == 2);
    std::cout << "  PASS: uleb128 multibyte (300)" << std::endl;
}

static void test_uleb128_large() {
    // 624485 = 0x098965 -> encoded as 0xE5 0x8E 0x26
    uint8_t data[] = {0xE5, 0x8E, 0x26};
    size_t offset = 0;
    assert(read_uleb128(data, 3, offset) == 624485);
    assert(offset == 3);
    std::cout << "  PASS: uleb128 large (624485)" << std::endl;
}

static void test_leb128_positive() {
    // 62 -> 0x3E
    uint8_t data[] = {0x3E};
    size_t offset = 0;
    assert(read_leb128(data, 1, offset) == 62);
    std::cout << "  PASS: leb128 positive (62)" << std::endl;
}

static void test_leb128_negative() {
    // -123456 = 0xC0 0xBB 0x78
    uint8_t data[] = {0xC0, 0xBB, 0x78};
    size_t offset = 0;
    assert(read_leb128(data, 3, offset) == -123456);
    std::cout << "  PASS: leb128 negative (-123456)" << std::endl;
}

static void test_leb128_minus_one() {
    // -1 -> 0x7F
    uint8_t data[] = {0x7F};
    size_t offset = 0;
    assert(read_leb128(data, 1, offset) == -1);
    std::cout << "  PASS: leb128 -1" << std::endl;
}

static void test_growing_normal() {
    // Normal value: 100
    uint8_t data[] = {0x64};
    size_t offset = 0;
    assert(read_growing(data, 1, offset) == 100);
    std::cout << "  PASS: growing normal (100)" << std::endl;
}

static void test_growing_sentinel() {
    // Sentinel 268435455 = 0xFF 0xFF 0xFF 0x7F, followed by leb128 -42 = 0x56
    uint8_t data[] = {0xFF, 0xFF, 0xFF, 0x7F, 0x56};
    size_t offset = 0;
    assert(read_growing(data, 5, offset) == -42);
    assert(offset == 5);
    std::cout << "  PASS: growing sentinel (-42)" << std::endl;
}

static void test_read_u8() {
    uint8_t data[] = {0x42};
    size_t offset = 0;
    assert(read_u8(data, 1, offset) == 0x42);
    std::cout << "  PASS: read_u8" << std::endl;
}

static void test_read_u16_le() {
    uint8_t data[] = {0x34, 0x12};
    size_t offset = 0;
    assert(read_u16_le(data, 2, offset) == 0x1234);
    std::cout << "  PASS: read_u16_le" << std::endl;
}

static void test_read_i64_le() {
    // 0x0000000000000001 in LE
    uint8_t data[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    size_t offset = 0;
    assert(read_i64_le(data, 8, offset) == 1);
    std::cout << "  PASS: read_i64_le" << std::endl;
}

static void test_read_f64_le() {
    // 1.0 in IEEE 754: 3FF0000000000000 -> LE: 00 00 00 00 00 00 F0 3F
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F};
    size_t offset = 0;
    assert(read_f64_le(data, 8, offset) == 1.0);
    std::cout << "  PASS: read_f64_le" << std::endl;
}

static void test_read_qsh_string() {
    // "ABC" = length 3 (leb128) + 'A' 'B' 'C'
    uint8_t data[] = {0x03, 0x41, 0x42, 0x43};
    size_t offset = 0;
    std::string s = read_qsh_string(data, 4, offset);
    assert(s == "ABC");
    assert(offset == 4);
    std::cout << "  PASS: read_qsh_string" << std::endl;
}

static void test_uleb128_overflow() {
    // 10 bytes of continuation bits should throw
    uint8_t data[10];
    std::memset(data, 0xFF, 10);
    size_t offset = 0;
    bool threw = false;
    try {
        read_uleb128(data, 10, offset);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "  PASS: uleb128 overflow throws" << std::endl;
}

int main() {
    std::cout << "=== test_leb128 ===" << std::endl;
    test_uleb128_basic();
    test_uleb128_multibyte();
    test_uleb128_large();
    test_leb128_positive();
    test_leb128_negative();
    test_leb128_minus_one();
    test_growing_normal();
    test_growing_sentinel();
    test_read_u8();
    test_read_u16_le();
    test_read_i64_le();
    test_read_f64_le();
    test_read_qsh_string();
    test_uleb128_overflow();
    std::cout << "\nAll LEB128 tests passed." << std::endl;
    return 0;
}
