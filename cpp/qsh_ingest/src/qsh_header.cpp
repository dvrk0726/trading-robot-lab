#include "qsh/qsh_header.hpp"
#include "qsh/leb128.hpp"
#include <cstring>

namespace qsh {

HeaderParseResult parse_qsh_header(const uint8_t* data, size_t size) {
    HeaderParseResult result;
    size_t offset = 0;

    // Check signature
    if (size < kQshSignatureLen) {
        result.error = "File too small for QSH signature";
        return result;
    }
    if (std::memcmp(data, kQshSignature, kQshSignatureLen) != 0) {
        result.error = "Invalid QSH signature";
        return result;
    }
    offset = kQshSignatureLen;

    // Version
    if (offset >= size) {
        result.error = "Unexpected end after signature";
        return result;
    }
    uint8_t version = data[offset++];
    if (version != kQshVersion) {
        result.error = "Unsupported QSH version: " + std::to_string(version) + " (expected 4)";
        return result;
    }
    result.header.version = version;

    try {
        // Recorder
        result.header.recorder = read_qsh_string(data, size, offset);
        // Comment
        result.header.comment = read_qsh_string(data, size, offset);
        // Recording time (.NET ticks)
        result.header.recording_time = read_i64_le(data, size, offset);

        // Stream count
        uint8_t stream_count = read_u8(data, size, offset);
        if (stream_count == 0) {
            result.error = "QSH file contains no data streams";
            return result;
        }
        if (stream_count > 1) {
            result.error = "Multi-stream QSH files not supported (stream_count=" +
                           std::to_string(stream_count) + ")";
            return result;
        }

        // Stream type
        uint8_t stream_type_byte = read_u8(data, size, offset);
        result.header.stream = stream_type_from_u8(stream_type_byte);
        if (result.header.stream == StreamType::Unknown) {
            result.error = "Unknown stream type: 0x" +
                           std::to_string(static_cast<int>(stream_type_byte));
            return result;
        }

        // Instrument
        result.header.instrument = read_qsh_string(data, size, offset);

    } catch (const std::exception& e) {
        result.error = std::string("Header parse error: ") + e.what();
        return result;
    }

    result.valid = true;
    return result;
}

std::string dotnet_ticks_to_string(int64_t ticks) {
    // .NET ticks: 100ns intervals since 0001-01-01
    // Convert to Unix milliseconds
    int64_t unix_ms = ticks / 10000 - kDotNetTicksToUnixMs;
    if (unix_ms < 0) {
        return "invalid";
    }

    // Simple conversion to datetime string
    // Days since epoch
    int64_t total_sec = unix_ms / 1000;
    int64_t ms_rem = unix_ms % 1000;

    // Simplified: just show the raw ticks and unix ms
    return std::to_string(unix_ms) + "ms (ticks=" + std::to_string(ticks) + ")";
}

}  // namespace qsh
