#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/crc32c.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

int main() {
    using namespace moex_raw;

    // Known CRC32C vectors from spec
    assert(crc32c("", 0) == 0x00000000);
    assert(crc32c("123456789", 9) == 0xE3069283);

    // Deterministic
    {
        const char* data = "test data for crc";
        assert(crc32c(data, std::strlen(data)) == crc32c(data, std::strlen(data)));
    }

    // Different data gives different CRC
    assert(crc32c("aaa", 3) != crc32c("bbb", 3));

    // Zero-length
    assert(crc32c("", 0) == 0x00000000);

    // Single byte non-zero
    {
        std::uint8_t b = 0xFF;
        assert(crc32c(&b, 1) != 0);
    }

    std::cout << "test_crc32c: ALL PASSED\n";
    return 0;
}
