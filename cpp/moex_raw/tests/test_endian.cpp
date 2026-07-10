#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/endian.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

int main() {
    using namespace moex_raw;

    // u16 LE exact bytes
    {
        std::vector<std::uint8_t> buf;
        write_u16_le(buf, 0x0102);
        assert(buf.size() == 2);
        assert(buf[0] == 0x02);
        assert(buf[1] == 0x01);
    }

    // u32 LE exact bytes
    {
        std::vector<std::uint8_t> buf;
        write_u32_le(buf, 0x01020304);
        assert(buf.size() == 4);
        assert(buf[0] == 0x04);
        assert(buf[3] == 0x01);
    }

    // u64 LE exact bytes
    {
        std::vector<std::uint8_t> buf;
        write_u64_le(buf, 0x0102030405060708ULL);
        assert(buf.size() == 8);
        assert(buf[0] == 0x08);
        assert(buf[7] == 0x01);
    }

    // Round-trip u16
    {
        std::vector<std::uint8_t> buf;
        write_u16_le(buf, 0xABCD);
        assert(read_u16_le(buf.data()) == 0xABCD);
    }

    // Round-trip u32
    {
        std::vector<std::uint8_t> buf;
        write_u32_le(buf, 0xDEADBEEF);
        assert(read_u32_le(buf.data()) == 0xDEADBEEF);
    }

    // Round-trip u64
    {
        std::vector<std::uint8_t> buf;
        write_u64_le(buf, 0xCAFEBABE12345678ULL);
        assert(read_u64_le(buf.data()) == 0xCAFEBABE12345678ULL);
    }

    // Zero values
    {
        std::vector<std::uint8_t> buf;
        write_u16_le(buf, 0);
        write_u32_le(buf, 0);
        write_u64_le(buf, 0);
        assert(read_u16_le(buf.data()) == 0);
        assert(read_u32_le(buf.data() + 2) == 0);
        assert(read_u64_le(buf.data() + 6) == 0);
    }

    // Max values
    {
        std::vector<std::uint8_t> buf;
        write_u16_le(buf, 0xFFFF);
        write_u32_le(buf, 0xFFFFFFFF);
        write_u64_le(buf, 0xFFFFFFFFFFFFFFFFULL);
        assert(read_u16_le(buf.data()) == 0xFFFF);
        assert(read_u32_le(buf.data() + 2) == 0xFFFFFFFF);
        assert(read_u64_le(buf.data() + 6) == 0xFFFFFFFFFFFFFFFFULL);
    }

    // Checked add: no overflow
    {
        std::uint64_t r = 0;
        assert(checked_add_u64(100, 200, r));
        assert(r == 300);
    }

    // Checked add: overflow
    {
        std::uint64_t r = 0;
        assert(!checked_add_u64(0xFFFFFFFFFFFFFFFFULL, 1, r));
        (void)r;
    }

    // Checked mul: no overflow
    {
        std::uint64_t r = 0;
        assert(checked_mul_u64(100, 200, r));
        assert(r == 20000);
    }

    // Checked mul: overflow
    {
        std::uint64_t r = 0;
        assert(!checked_mul_u64(0xFFFFFFFFFFFFFFFFULL, 2, r));
        (void)r;
    }

    // Checked mul: zero
    {
        std::uint64_t r = 99;
        assert(checked_mul_u64(0, 12345, r));
        assert(r == 0);
    }

    // write_bytes
    {
        const char* data = "hello";
        std::vector<std::uint8_t> buf;
        write_bytes(buf, data, 5);
        assert(buf.size() == 5);
        assert(std::memcmp(buf.data(), "hello", 5) == 0);
    }

    std::cout << "test_endian: ALL PASSED\n";
    return 0;
}
