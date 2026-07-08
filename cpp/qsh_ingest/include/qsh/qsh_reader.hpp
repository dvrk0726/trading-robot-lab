#pragma once

#include "qsh/qsh_types.hpp"
#include "qsh/qsh_header.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace qsh {

// Holds the decompressed QSH file content.
struct QshFile {
    Header header;
    std::vector<uint8_t> data;  // decompressed stream data (after header)
    size_t data_offset = 0;     // current read position in data
    bool valid = false;
    std::string error;

    bool eof() const { return data_offset >= data.size(); }
};

// Open and decompress a QSH file. Returns the parsed header and decompressed data.
QshFile open_qsh_file(const std::string& path);

// SHA-256 hash of a file (hex string).
std::string file_sha256_hex(const std::string& path);

}  // namespace qsh
