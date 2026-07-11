#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/crc32c.hpp"
#include "test_check.hpp"
#include <iostream>
#include <cstring>

int main() {
    using namespace moex_raw;

    // Known CRC32C vectors from spec
    CHECK(crc32c("", 0) == 0x00000000);
    CHECK(crc32c("123456789", 9) == 0xE3069283);

    // Deterministic
    {
        const char* data = "test data for crc";
        CHECK(crc32c(data, std::strlen(data)) == crc32c(data, std::strlen(data)));
    }

    // Different data gives different CRC
    CHECK(crc32c("aaa", 3) != crc32c("bbb", 3));

    // Zero-length
    CHECK(crc32c("", 0) == 0x00000000);

    // Single byte non-zero
    {
        std::uint8_t b = 0xFF;
        CHECK(crc32c(&b, 1) != 0);
    }

    std::cout << "test_crc32c: ALL PASSED\n";
    return 0;
}
