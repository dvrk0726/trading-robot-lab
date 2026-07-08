#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace qsh {

// Read unsigned LEB128 from a byte buffer.
// Returns the decoded value and advances offset.
inline uint64_t read_uleb128(const uint8_t* data, size_t size, size_t& offset) {
    uint64_t result = 0;
    int shift = 0;
    while (offset < size) {
        uint8_t byte = data[offset++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return result;
        }
        shift += 7;
        if (shift >= 64) {
            throw std::runtime_error("ULEB128 overflow");
        }
    }
    throw std::runtime_error("ULEB128 unexpected end of data");
}

// Read signed LEB128 from a byte buffer.
inline int64_t read_leb128(const uint8_t* data, size_t size, size_t& offset) {
    int64_t result = 0;
    int shift = 0;
    constexpr int kMaxShift = 64;
    while (offset < size) {
        uint8_t byte = data[offset++];
        result |= static_cast<int64_t>(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (shift < kMaxShift && (byte & 0x40)) {
                result |= -(1LL << shift);
            }
            return result;
        }
    }
    throw std::runtime_error("LEB128 unexpected end of data");
}

// Read growing integer: ULEB128, with special sentinel 268435455 meaning read LEB128 instead.
inline int64_t read_growing(const uint8_t* data, size_t size, size_t& offset) {
    uint64_t v = read_uleb128(data, size, offset);
    constexpr uint64_t kSentinel = 268435455ULL;
    if (v == kSentinel) {
        return read_leb128(data, size, offset);
    }
    return static_cast<int64_t>(v);
}

// Read little-endian integer of given byte count.
inline uint64_t read_le_uint(const uint8_t* data, size_t size, size_t& offset, int bytes) {
    if (offset + static_cast<size_t>(bytes) > size) {
        throw std::runtime_error("read_le_uint: unexpected end of data");
    }
    uint64_t result = 0;
    for (int i = 0; i < bytes; ++i) {
        result |= static_cast<uint64_t>(data[offset + i]) << (8 * i);
    }
    offset += bytes;
    return result;
}

inline uint8_t read_u8(const uint8_t* data, size_t size, size_t& offset) {
    if (offset >= size) {
        throw std::runtime_error("read_u8: unexpected end of data");
    }
    return data[offset++];
}

inline uint16_t read_u16_le(const uint8_t* data, size_t size, size_t& offset) {
    return static_cast<uint16_t>(read_le_uint(data, size, offset, 2));
}

inline int64_t read_i64_le(const uint8_t* data, size_t size, size_t& offset) {
    return static_cast<int64_t>(read_le_uint(data, size, offset, 8));
}

inline double read_f64_le(const uint8_t* data, size_t size, size_t& offset) {
    uint64_t bits = read_le_uint(data, size, offset, 8);
    double result;
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    std::memcpy(&result, &bits, 8);
    return result;
}

// Read a QSH string: LEB128 length prefix + UTF-8 bytes.
inline std::string read_qsh_string(const uint8_t* data, size_t size, size_t& offset) {
    int64_t len = read_leb128(data, size, offset);
    if (len < 0 || offset + static_cast<size_t>(len) > size) {
        throw std::runtime_error("read_qsh_string: invalid length or unexpected end");
    }
    std::string result(reinterpret_cast<const char*>(data + offset), static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    return result;
}

}  // namespace qsh
