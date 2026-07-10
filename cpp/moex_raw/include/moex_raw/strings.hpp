#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace moex_raw {

static constexpr std::size_t kMaxStringBytes = 128;

// Validate UTF-8, no embedded NUL, fits kMaxStringBytes.
bool validate_utf8_string(const std::string& s);

// Write length-prefixed UTF-8 string: u16 length + bytes.
void write_length_string(std::vector<std::uint8_t>& buf, const std::string& s);

// Read length-prefixed UTF-8 string. Returns false on failure.
bool read_length_string(const std::uint8_t* data, std::size_t available,
                        std::string& out, std::size_t& bytes_consumed);

}  // namespace moex_raw
