#pragma once
// Independent FIX FAST 1.1 wire-format encoder for test oracle use only.
// This file must NOT be included by any production code.
// It does NOT use WireCursor, fast_decoder, or any production encoding helpers.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fast_oracle {

// Append a single byte to a buffer.
inline void push_byte(std::vector<std::uint8_t>& buf, std::uint8_t b) {
    buf.push_back(b);
}

// ---------- Presence map ----------
// Encodes `bit_count` bits (MSB first) into stop-bit-terminated bytes.
// bits[i] = true means the bit is SET (1).
inline void encode_presence_map(std::vector<std::uint8_t>& buf,
                                const bool* bits, std::size_t bit_count) {
    std::size_t byte_count = (bit_count + 6) / 7;  // ceil(bit_count/7)
    for (std::size_t b = 0; b < byte_count; ++b) {
        std::uint8_t byte_val = 0;
        bool is_last = (b == byte_count - 1);
        for (int bit = 0; bit < 7; ++bit) {
            std::size_t idx = b * 7 + bit;
            if (idx < bit_count && bits[idx]) {
                byte_val |= static_cast<std::uint8_t>(1u << (6 - bit));
            }
        }
        if (is_last) byte_val |= 0x80;  // stop bit
        buf.push_back(byte_val);
    }
}

inline void encode_presence_map(std::vector<std::uint8_t>& buf,
                                const std::vector<bool>& bits) {
    if (bits.empty()) {
        buf.push_back(0x80);  // stop bit only, zero data bits
        return;
    }
    std::size_t bit_count = bits.size();
    std::size_t byte_count = (bit_count + 6) / 7;
    for (std::size_t b = 0; b < byte_count; ++b) {
        std::uint8_t byte_val = 0;
        bool is_last = (b == byte_count - 1);
        for (int bit = 0; bit < 7; ++bit) {
            std::size_t idx = b * 7 + bit;
            if (idx < bit_count && bits[idx]) {
                byte_val |= static_cast<std::uint8_t>(1u << (6 - bit));
            }
        }
        if (is_last) byte_val |= 0x80;
        buf.push_back(byte_val);
    }
}

// ---------- Unsigned integer stop-bit encoding ----------
inline void encode_stopbit_u32(std::vector<std::uint8_t>& buf, std::uint32_t val) {
    // Determine number of 7-bit groups needed.
    int nbits;
    if (val == 0) {
        nbits = 1;
    } else {
        nbits = 0;
        std::uint32_t tmp = val;
        while (tmp > 0) { nbits += 7; tmp >>= 7; }
    }
    int ngroups = (nbits + 6) / 7;
    // Emit groups from MSB to LSB.
    for (int g = ngroups - 1; g >= 0; --g) {
        std::uint8_t byte = static_cast<std::uint8_t>((val >> (g * 7)) & 0x7F);
        if (g == 0) byte |= 0x80;  // stop bit
        buf.push_back(byte);
    }
}

inline void encode_stopbit_u64(std::vector<std::uint8_t>& buf, std::uint64_t val) {
    int nbits;
    if (val == 0) {
        nbits = 1;
    } else {
        nbits = 0;
        std::uint64_t tmp = val;
        while (tmp > 0) { nbits += 7; tmp >>= 7; }
    }
    int ngroups = (nbits + 6) / 7;
    for (int g = ngroups - 1; g >= 0; --g) {
        std::uint8_t byte = static_cast<std::uint8_t>((val >> (g * 7)) & 0x7F);
        if (g == 0) byte |= 0x80;
        buf.push_back(byte);
    }
}

// ---------- Signed integer stop-bit encoding (2's complement) ----------
// FIX FAST 1.1: sign-extend the value to determine the minimum number of
// 7-bit groups. A positive value whose top payload bit is 1 needs an extra
// sign-extension group; a negative value whose top payload bits are all 1
// also needs an extra group.
inline void encode_stopbit_i32(std::vector<std::uint8_t>& buf, std::int32_t val) {
    // Number of bits needed to represent val in 2's complement.
    int nbits;
    if (val >= 0) {
        // Need 1 sign bit (0) + ceil(log2(val+1)) payload bits, min 1.
        nbits = 1;
        std::uint32_t u = static_cast<std::uint32_t>(val);
        while (u > 0) { nbits++; u >>= 1; }
    } else {
        // Negative: find where we can truncate the leading 1s.
        nbits = 1;
        std::int32_t v = val;
        while (v < -1) { nbits++; v >>= 1; }
    }
    int ngroups = (nbits + 6) / 7;
    // Emit from MSB group to LSB group.
    // We need the value in a wide enough unsigned type to extract groups.
    // For i32, 5 groups max (35 bits).
    std::uint32_t raw;
    std::memcpy(&raw, &val, 4);
    for (int g = ngroups - 1; g >= 0; --g) {
        std::uint8_t byte = static_cast<std::uint8_t>((raw >> (g * 7)) & 0x7F);
        if (g == 0) byte |= 0x80;
        buf.push_back(byte);
    }
}

inline void encode_stopbit_i64(std::vector<std::uint8_t>& buf, std::int64_t val) {
    int nbits;
    if (val >= 0) {
        nbits = 1;
        std::uint64_t u = static_cast<std::uint64_t>(val);
        while (u > 0) { nbits++; u >>= 1; }
    } else {
        nbits = 1;
        std::int64_t v = val;
        while (v < -1) { nbits++; v >>= 1; }
    }
    int ngroups = (nbits + 6) / 7;
    std::uint64_t raw;
    std::memcpy(&raw, &val, 8);
    for (int g = ngroups - 1; g >= 0; --g) {
        std::uint8_t byte = static_cast<std::uint8_t>((raw >> (g * 7)) & 0x7F);
        if (g == 0) byte |= 0x80;
        buf.push_back(byte);
    }
}

// ---------- Nullable integer (offset-by-1 encoding) ----------
// Nullable uInt32: null = 0x00; non-null value V = stopbit(V+1).
inline void encode_nullable_u32(std::vector<std::uint8_t>& buf, std::uint32_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x00);
    } else {
        encode_stopbit_u32(buf, val + 1);
    }
}

// Nullable uInt64: null = 0x00; non-null value V = stopbit(V+1).
inline void encode_nullable_u64(std::vector<std::uint8_t>& buf, std::uint64_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x00);
    } else {
        encode_stopbit_u64(buf, val + 1);
    }
}

// Nullable int32: null = 0x00; non-null value V = stopbit(V+1).
inline void encode_nullable_i32(std::vector<std::uint8_t>& buf, std::int32_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x00);
    } else {
        // V+1 as signed. For i32, this wraps correctly for INT32_MAX -> UINT32-ish,
        // but FAST spec says the offset is arithmetic on the signed value encoded
        // as unsigned stop-bit. We use the i32 stop-bit encoder with (val+1).
        // Cast through unsigned to avoid UB on INT32_MAX+1.
        std::int32_t shifted = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(val) + 1u);
        encode_stopbit_i32(buf, shifted);
    }
}

// Nullable int64: null = 0x00; non-null value V = stopbit(V+1).
inline void encode_nullable_i64(std::vector<std::uint8_t>& buf, std::int64_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x00);
    } else {
        std::int64_t shifted = static_cast<std::int64_t>(
            static_cast<std::uint64_t>(val) + 1ull);
        encode_stopbit_i64(buf, shifted);
    }
}

// ---------- FAST ASCII string ----------
// Each character is one byte with the stop bit on the LAST byte only.
inline void encode_ascii_string(std::vector<std::uint8_t>& buf, const std::string& s) {
    if (s.empty()) {
        buf.push_back(0x80);  // stop bit, zero data
        return;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::uint8_t byte = static_cast<std::uint8_t>(s[i]) & 0x7F;
        if (i == s.size() - 1) byte |= 0x80;  // stop bit on last char
        buf.push_back(byte);
    }
}

// ---------- Unicode string (length-prefixed) ----------
// Length is encoded as stopbit uInt32, followed by raw UTF-8 bytes.
inline void encode_unicode_string(std::vector<std::uint8_t>& buf, const std::string& s) {
    encode_stopbit_u32(buf, static_cast<std::uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

// Nullable Unicode: length is nullable uInt32.
inline void encode_nullable_unicode(std::vector<std::uint8_t>& buf,
                                    const std::string& s, bool is_null) {
    if (is_null) {
        buf.push_back(0x00);
    } else {
        encode_stopbit_u32(buf, static_cast<std::uint32_t>(s.size()) + 1);
        buf.insert(buf.end(), s.begin(), s.end());
    }
}

// ---------- Byte vector (length-prefixed) ----------
// Length is encoded as stopbit uInt32, followed by raw bytes.
inline void encode_byte_vector(std::vector<std::uint8_t>& buf,
                               const std::vector<std::uint8_t>& data) {
    encode_stopbit_u32(buf, static_cast<std::uint32_t>(data.size()));
    buf.insert(buf.end(), data.begin(), data.end());
}

// Nullable byte vector: length is nullable uInt32.
inline void encode_nullable_byte_vector(std::vector<std::uint8_t>& buf,
                                        const std::vector<std::uint8_t>& data,
                                        bool is_null) {
    if (is_null) {
        buf.push_back(0x00);
    } else {
        encode_stopbit_u32(buf, static_cast<std::uint32_t>(data.size()) + 1);
        buf.insert(buf.end(), data.begin(), data.end());
    }
}

// ---------- Decimal ----------
// Non-nullable decimal: exponent as non-nullable stopbit i32, mantissa as non-nullable stopbit i64.
// Nullable decimal: exponent as nullable i32 (null = whole decimal is null, no mantissa consumed).
inline void encode_decimal(std::vector<std::uint8_t>& buf,
                           std::int32_t exponent, std::int64_t mantissa,
                           bool exponent_nullable, bool mantissa_nullable) {
    if (exponent_nullable) {
        // If the whole decimal is null, exponent is null.
        // For this oracle, we assume non-null unless caller passes is_null.
        encode_nullable_i32(buf, exponent, false);
    } else {
        encode_stopbit_i32(buf, exponent);
    }
    if (mantissa_nullable) {
        encode_nullable_i64(buf, mantissa, false);
    } else {
        encode_stopbit_i64(buf, mantissa);
    }
}

// Null decimal (nullable exponent).
inline void encode_null_decimal(std::vector<std::uint8_t>& buf) {
    buf.push_back(0x00);  // nullable exponent = null
}

}  // namespace fast_oracle
