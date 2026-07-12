#pragma once
// Independent FIX FAST 1.1 wire-format encoder for test oracle use only.
// This file must NOT be included by any production code.
// It does NOT use WireCursor, fast_decoder, or any production encoding helpers.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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
// Minimal encoding: trailing all-zero 7-bit groups are stripped.
// Always emits at least one byte (stop bit only for empty/all-zero maps).
inline void encode_presence_map(std::vector<std::uint8_t>& buf,
                                const bool* bits, std::size_t bit_count) {
    // Find the last set bit to determine the effective group count.
    std::size_t last_true = 0;
    bool has_true = false;
    for (std::size_t i = bit_count; i > 0; --i) {
        if (bits[i - 1]) { last_true = i - 1; has_true = true; break; }
    }
    // Groups of 7 bits; strip trailing all-zero groups.
    std::size_t eff = has_true ? (last_true / 7 + 1) : 0;
    if (eff == 0) {
        buf.push_back(0x80);  // stop bit only
        return;
    }
    for (std::size_t g = 0; g < eff; ++g) {
        std::uint8_t byte_val = 0;
        for (int bit = 0; bit < 7; ++bit) {
            std::size_t idx = g * 7 + bit;
            if (idx < bit_count && bits[idx]) {
                byte_val |= static_cast<std::uint8_t>(1u << (6 - bit));
            }
        }
        if (g == eff - 1) byte_val |= 0x80;  // stop bit on last group
        buf.push_back(byte_val);
    }
}

inline void encode_presence_map(std::vector<std::uint8_t>& buf,
                                const std::vector<bool>& bits) {
    if (bits.empty()) {
        buf.push_back(0x80);  // stop bit only
        return;
    }
    // Find the last set bit to determine the effective group count.
    std::size_t last_true = 0;
    bool has_true = false;
    for (std::size_t i = bits.size(); i > 0; --i) {
        if (bits[i - 1]) { last_true = i - 1; has_true = true; break; }
    }
    std::size_t eff = has_true ? (last_true / 7 + 1) : 0;
    if (eff == 0) {
        buf.push_back(0x80);  // stop bit only
        return;
    }
    std::size_t bit_count = bits.size();
    for (std::size_t g = 0; g < eff; ++g) {
        std::uint8_t byte_val = 0;
        for (int bit = 0; bit < 7; ++bit) {
            std::size_t idx = g * 7 + bit;
            if (idx < bit_count && bits[idx]) {
                byte_val |= static_cast<std::uint8_t>(1u << (6 - bit));
            }
        }
        if (g == eff - 1) byte_val |= 0x80;  // stop bit on last group
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
// Uses int64_t accumulator for proper sign extension to 35 bits.
inline void encode_stopbit_i32(std::vector<std::uint8_t>& buf, std::int32_t val) {
    // Portable C++20: no right shift of negative signed value.
    std::uint32_t uval = static_cast<std::uint32_t>(val);
    int nbits;
    if (val >= 0) {
        nbits = 1;
        std::uint32_t u = uval;
        while (u > 0) { nbits++; u >>= 1; }
    } else {
        // Count significant bits via unsigned complement (no signed shift).
        nbits = 1;
        std::uint32_t u = ~uval;
        while (u > 0) { nbits++; u >>= 1; }
    }
    int ngroups = (nbits + 6) / 7;
    // Sign-extend to 64 bits using unsigned operations.
    std::uint64_t raw = static_cast<std::uint64_t>(uval);
    if (val < 0) {
        raw |= 0xFFFFFFFF00000000ULL;
    }
    for (int g = ngroups - 1; g >= 0; --g) {
        std::uint8_t byte = static_cast<std::uint8_t>((raw >> (g * 7)) & 0x7F);
        if (g == 0) byte |= 0x80;
        buf.push_back(byte);
    }
}

inline void encode_stopbit_i64(std::vector<std::uint8_t>& buf, std::int64_t val) {
    std::uint64_t uval;
    std::memcpy(&uval, &val, 8);

    int nbits;
    if (val >= 0) {
        nbits = 1;
        std::uint64_t u = uval;
        while (u > 0) { nbits++; u >>= 1; }
    } else {
        // Count leading 1 bits in unsigned representation
        int leading_ones = 0;
        std::uint64_t tmp = uval;
        while (tmp & (1ULL << 63)) { leading_ones++; tmp <<= 1; }
        nbits = 65 - leading_ones;
    }
    int ngroups = (nbits + 6) / 7;
    std::uint8_t sign_bit = static_cast<std::uint8_t>((uval >> 63) & 1);
    for (int g = ngroups - 1; g >= 0; --g) {
        std::uint8_t byte;
        if (g == ngroups - 1 && ngroups * 7 > 64) {
            // Top group has sign extension bits above 64
            byte = sign_bit ? 0x7F : 0x00;
        } else {
            byte = static_cast<std::uint8_t>((uval >> (g * 7)) & 0x7F);
        }
        if (g == 0) byte |= 0x80;
        buf.push_back(byte);
    }
}

// ---------- Nullable integer (offset-by-1 encoding) ----------
// Nullable uInt32: NULL = stop-bit 0 ([0x80]); non-null value V = stopbit(V+1).
// Widened to uint64_t to avoid wrap on UINT32_MAX+1.
inline void encode_nullable_u32(std::vector<std::uint8_t>& buf, std::uint32_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x80);
    } else {
        std::uint64_t widened = static_cast<std::uint64_t>(val) + 1;
        encode_stopbit_u64(buf, widened);
    }
}

// Nullable uInt64: NULL = stop-bit 0 ([0x80]); non-null value V = stopbit(V+1).
// Widened to avoid wrap on UINT64_MAX+1.
inline void encode_nullable_u64(std::vector<std::uint8_t>& buf, std::uint64_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x80);
    } else if (val == std::numeric_limits<std::uint64_t>::max()) {
        // UINT64_MAX + 1 = 2^64: explicit 70-bit encoding
        buf.push_back(0x02);
        for (int i = 0; i < 8; ++i) buf.push_back(0x00);
        buf.push_back(0x80);
    } else {
        encode_stopbit_u64(buf, val + 1);
    }
}

// Nullable int32: NULL = stop-bit 0 ([0x80]); negative values encoded unchanged;
// non-negative values widened by +1. No UB/wrap.
inline void encode_nullable_i32(std::vector<std::uint8_t>& buf, std::int32_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x80);
    } else if (val < 0) {
        encode_stopbit_i32(buf, val);
    } else {
        // Non-negative: encode val+1 as signed using int64 to avoid overflow.
        encode_stopbit_i64(buf, static_cast<std::int64_t>(val) + 1);
    }
}

// Nullable int64: NULL = stop-bit 0 ([0x80]); negative values encoded unchanged;
// non-negative values widened by +1. No UB/wrap.
inline void encode_nullable_i64(std::vector<std::uint8_t>& buf, std::int64_t val, bool is_null) {
    if (is_null) {
        buf.push_back(0x80);
    } else if (val < 0) {
        encode_stopbit_i64(buf, val);
    } else if (val == std::numeric_limits<std::int64_t>::max()) {
        // INT64_MAX + 1 = 2^63: explicit 70-bit encoding
        buf.push_back(0x01);
        for (int i = 0; i < 8; ++i) buf.push_back(0x00);
        buf.push_back(0x80);
    } else {
        encode_stopbit_i64(buf, static_cast<std::int64_t>(
            static_cast<std::uint64_t>(val) + 1ull));
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

// ---------- Nullable ASCII ----------
// NULL -> [80]; non-null empty -> [00 80]; non-empty -> same wire as mandatory.
inline void encode_nullable_ascii(std::vector<std::uint8_t>& buf,
                                  const std::string& s, bool is_null) {
    if (is_null) {
        buf.push_back(0x80);
    } else if (s.empty()) {
        buf.push_back(0x00);
        buf.push_back(0x80);
    } else {
        encode_ascii_string(buf, s);
    }
}

// ---------- Unicode string (length-prefixed) ----------
// Length is encoded as stopbit uInt32, followed by raw UTF-8 bytes.
inline void encode_unicode_string(std::vector<std::uint8_t>& buf, const std::string& s) {
    encode_stopbit_u32(buf, static_cast<std::uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

// Nullable Unicode: length is nullable uInt32.
// NULL [80] / empty [81] via corrected nullable u32 semantics.
inline void encode_nullable_unicode(std::vector<std::uint8_t>& buf,
                                    const std::string& s, bool is_null) {
    encode_nullable_u32(buf, static_cast<std::uint32_t>(s.size()), is_null);
    if (!is_null) {
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
// NULL [80] / empty [81] via corrected nullable u32 semantics.
inline void encode_nullable_byte_vector(std::vector<std::uint8_t>& buf,
                                        const std::vector<std::uint8_t>& data,
                                        bool is_null) {
    encode_nullable_u32(buf, static_cast<std::uint32_t>(data.size()), is_null);
    if (!is_null) {
        buf.insert(buf.end(), data.begin(), data.end());
    }
}

// ---------- Decimal ----------
// Non-nullable decimal: exponent as non-nullable stopbit i32, mantissa as non-nullable stopbit i64.
// Nullable decimal: exponent as nullable i32 (null = whole decimal is null, no mantissa consumed).
// mantissa_nullable=true is non-normative and must not be used; the test encoder hard-rejects it.
inline void encode_decimal(std::vector<std::uint8_t>& buf,
                           std::int32_t exponent, std::int64_t mantissa,
                           bool exponent_nullable, bool mantissa_nullable) {
    // mantissa_nullable is non-normative: must never be true in test wire generation
    if (mantissa_nullable) {
        std::cerr << "FATAL: encode_decimal called with mantissa_nullable=true (non-normative)\n";
        std::abort();
    }
    if (exponent_nullable) {
        encode_nullable_i32(buf, exponent, false);
    } else {
        encode_stopbit_i32(buf, exponent);
    }
    encode_stopbit_i64(buf, mantissa);
}

// Null decimal (nullable exponent). Exponent NULL [80], no mantissa consumed.
inline void encode_null_decimal(std::vector<std::uint8_t>& buf) {
    encode_nullable_i32(buf, 0, true);
}

}  // namespace fast_oracle
