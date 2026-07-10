#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace moex_raw {

// Little-endian serialization primitives. No raw struct dumps.

void write_u16_le(std::vector<std::uint8_t>& buf, std::uint16_t v);
void write_u32_le(std::vector<std::uint8_t>& buf, std::uint32_t v);
void write_u64_le(std::vector<std::uint8_t>& buf, std::uint64_t v);
void write_bytes(std::vector<std::uint8_t>& buf, const void* data, std::size_t len);

std::uint16_t read_u16_le(const std::uint8_t* data);
std::uint32_t read_u32_le(const std::uint8_t* data);
std::uint64_t read_u64_le(const std::uint8_t* data);

// Checked arithmetic — returns false on overflow.
bool checked_add_u64(std::uint64_t a, std::uint64_t b, std::uint64_t& result);
bool checked_mul_u64(std::uint64_t a, std::uint64_t b, std::uint64_t& result);

}  // namespace moex_raw
