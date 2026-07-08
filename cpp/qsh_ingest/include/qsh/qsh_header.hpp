#pragma once

#include "qsh/qsh_types.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qsh {

// Result of parsing the QSH file header.
struct HeaderParseResult {
    Header header;
    bool valid = false;
    std::string error;
};

// Parse QSH header from raw file bytes (after decompression).
// Expects data to point to the start of the decompressed QSH stream.
HeaderParseResult parse_qsh_header(const uint8_t* data, size_t size);

// Convert .NET ticks to ISO-like string (simplified).
std::string dotnet_ticks_to_string(int64_t ticks);

}  // namespace qsh
