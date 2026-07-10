#include "moex_raw/crc32c.hpp"

namespace moex_raw {

namespace {

// CRC32C (Castagnoli) polynomial: 0x1EDC6F41
std::uint32_t crc32c_table[256];
bool table_initialized = false;

void init_table() {
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78U;
            else
                crc >>= 1;
        }
        crc32c_table[i] = crc;
    }
    table_initialized = true;
}

}  // namespace

std::uint32_t crc32c(const void* data, std::size_t len) {
    if (!table_initialized) init_table();

    auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < len; ++i) {
        crc = crc32c_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

}  // namespace moex_raw
