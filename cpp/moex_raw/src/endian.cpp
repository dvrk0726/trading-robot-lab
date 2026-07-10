#include "moex_raw/endian.hpp"
#include <cstring>

namespace moex_raw {

void write_u16_le(std::vector<std::uint8_t>& buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>(v));
    buf.push_back(static_cast<std::uint8_t>(v >> 8));
}

void write_u32_le(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    buf.push_back(static_cast<std::uint8_t>(v));
    buf.push_back(static_cast<std::uint8_t>(v >> 8));
    buf.push_back(static_cast<std::uint8_t>(v >> 16));
    buf.push_back(static_cast<std::uint8_t>(v >> 24));
}

void write_u64_le(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
}

void write_bytes(std::vector<std::uint8_t>& buf, const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    buf.insert(buf.end(), p, p + len);
}

std::uint16_t read_u16_le(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) |
           (static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint32_t read_u32_le(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::uint64_t read_u64_le(const std::uint8_t* data) {
    return static_cast<std::uint64_t>(data[0]) |
           (static_cast<std::uint64_t>(data[1]) << 8) |
           (static_cast<std::uint64_t>(data[2]) << 16) |
           (static_cast<std::uint64_t>(data[3]) << 24) |
           (static_cast<std::uint64_t>(data[4]) << 32) |
           (static_cast<std::uint64_t>(data[5]) << 40) |
           (static_cast<std::uint64_t>(data[6]) << 48) |
           (static_cast<std::uint64_t>(data[7]) << 56);
}

bool checked_add_u64(std::uint64_t a, std::uint64_t b, std::uint64_t& result) {
    if (a > 0xFFFFFFFFFFFFFFFFULL - b) return false;
    result = a + b;
    return true;
}

bool checked_mul_u64(std::uint64_t a, std::uint64_t b, std::uint64_t& result) {
    if (a == 0 || b == 0) { result = 0; return true; }
    if (a > 0xFFFFFFFFFFFFFFFFULL / b) return false;
    result = a * b;
    return true;
}

}  // namespace moex_raw
