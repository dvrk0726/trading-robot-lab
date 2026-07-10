#pragma once
#include <cstddef>
#include <cstdint>

namespace moex_raw {

// CRC32C (Castagnoli) — pure C++ table implementation.
std::uint32_t crc32c(const void* data, std::size_t len);

}  // namespace moex_raw
