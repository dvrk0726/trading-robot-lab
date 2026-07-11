#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "moex_fast/decoder_types.hpp"

namespace moex_fast {

class WireCursor {
public:
    WireCursor() = default;
    WireCursor(const std::uint8_t* data, std::size_t size);

    const std::uint8_t* data() const;
    std::size_t remaining() const;
    std::size_t position() const;
    std::size_t total_size() const;
    bool at_end() const;

    DecodeStatus peek_byte(std::uint8_t& out) const;
    DecodeStatus read_byte(std::uint8_t& out);
    DecodeStatus read_bytes(std::size_t count, const std::uint8_t*& out_ptr);

    // Stop-bit unsigned integers (normative FIX FAST 1.1)
    // Max 5 bytes for uInt32, max 10 bytes for uInt64.
    // Overflow checked before each shift/add.
    // Non-canonical leading zero bytes rejected.
    DecodeStatus read_stopbit_u32(std::uint32_t& out);
    DecodeStatus read_stopbit_u64(std::uint64_t& out);

    // Stop-bit signed integers (normative FIX FAST 1.1)
    // Sign extension from bit 6 of first byte.
    // Max 5 bytes for int32, max 10 bytes for int64.
    // Overflow checked before shift/add.
    DecodeStatus read_stopbit_i32(std::int32_t& out);
    DecodeStatus read_stopbit_i64(std::int64_t& out);

    // Nullable integer forms (normative FIX FAST 1.1)
    // 0x00 first byte => null (consuming 1 byte).
    // Otherwise, normal stop-bit decode.
    DecodeStatus read_nullable_u32(std::uint32_t& out, bool& is_null);
    DecodeStatus read_nullable_u64(std::uint64_t& out, bool& is_null);
    DecodeStatus read_nullable_i32(std::int32_t& out, bool& is_null);
    DecodeStatus read_nullable_i64(std::int64_t& out, bool& is_null);

    // Presence map: stop-bit terminated byte sequence.
    // Each byte: bit 7 = stop bit, bits 6..0 = data (MSB first).
    // Max max_pmap_bytes (default from DecodeLimits).
    // Unterminated map => error.
    // Implicit zero bits after transmitted bits where FAST permits.
    DecodeStatus read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits,
                                   std::size_t max_pmap_bytes = 64);

    // ASCII string: stop-bit terminated by 0x00.
    // Valid domain: 0x01..0x7F. Bytes outside => error.
    // Terminator consumed but not included.
    DecodeStatus read_ascii_string(std::string& out);

    // Nullable ASCII: same wire encoding as mandatory.
    // Null vs empty distinction handled at operator/pmap level.
    DecodeStatus read_nullable_ascii(std::string& out, bool& is_null);

    // Unicode string: length-prefixed stop-bit uInt32, then that many bytes of UTF-8.
    // Strict UTF-8 validation (no overlong, no surrogates, no > U+10FFFF).
    DecodeStatus read_unicode_string(std::string& out);
    DecodeStatus read_nullable_unicode(std::string& out, bool& is_null);

    // Byte vector: length-prefixed stop-bit uInt32, then that many raw bytes.
    DecodeStatus read_byte_vector(std::vector<std::uint8_t>& out);
    DecodeStatus read_nullable_byte_vector(std::vector<std::uint8_t>& out, bool& is_null);

    // Decimal: exponent (i32) then mantissa (i64).
    // If exponent nullable and null => whole decimal null, mantissa NOT consumed.
    DecodeStatus read_decimal(std::int32_t& exponent, std::int64_t& mantissa, bool& is_null,
                              bool exponent_nullable, bool mantissa_nullable);

    // Sequence length: stop-bit uInt32.
    DecodeStatus read_sequence_length(std::uint32_t& out);

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
};

// Validate UTF-8 strictly (no overlong, no surrogates, no > U+10FFFF).
bool validate_utf8(const std::uint8_t* data, std::size_t len);

}  // namespace moex_fast
