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
    // On failure cursor is unchanged (position restored).
    DecodeStatus read_stopbit_u32(std::uint32_t& out);
    DecodeStatus read_stopbit_u64(std::uint64_t& out);

    // Stop-bit signed integers (normative FIX FAST 1.1)
    // Sign extension from bit 6 of first byte.
    // Max 5 bytes for int32, max 10 bytes for int64.
    // Overflow checked before shift/add.
    // Canonical encoding enforced: minimum byte count, max-width validation.
    // On failure cursor is unchanged (position restored).
    DecodeStatus read_stopbit_i32(std::int32_t& out);
    DecodeStatus read_stopbit_i64(std::int64_t& out);

    // Nullable integer forms (normative FIX FAST 1.1, section 6.3.2)
    // Nullable u32/i32: NULL wire is stop-bit 0 ([0x80]), not literal byte 0x00.
    //   u32: raw 0 = NULL; raw 1..2^32 -> value = raw - 1; raw > 2^32 overflow.
    //   i32: raw 0 = NULL; raw < 0 unchanged; raw > 0 -> value = raw - 1.
    //   Only non-negative signed values use the +1 offset.
    // Nullable u64/i64: NULL wire is stop-bit 0 ([0x80]), not literal byte 0x00.
    DecodeStatus read_nullable_u32(std::uint32_t& out, bool& is_null);
    DecodeStatus read_nullable_u64(std::uint64_t& out, bool& is_null);
    DecodeStatus read_nullable_i32(std::int32_t& out, bool& is_null);
    DecodeStatus read_nullable_i64(std::int64_t& out, bool& is_null);

    // Presence map: stop-bit terminated byte sequence (FIX FAST 1.1, section 6.3.1).
    // Each byte: bit 7 = stop bit (1 = last), bits 6..0 = data (MSB first).
    // Map MUST contain a stop bit; unterminated => NeedMoreData.
    // Minimal encoding: trailing all-zero 7-bit groups are implicit.
    // Implicit zero suffix: missing bits after stop bit filled with false.
    // max_pmap_bytes limits transmitted wire bytes, not logical pmap_bits.
    // Cursor and output are atomic: on any failure both remain unchanged.
    // FAST 1.1 ERR R7: multi-byte map whose terminating 7-bit group data
    // bits are all zero is overlong (NonCanonicalEncoding).
    DecodeStatus read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits,
                                   std::size_t max_pmap_bytes = 64);

    // ASCII string: stop-bit encoded (FIX FAST 1.1, section 6.3.6).
    // Each byte: bit 7 = stop (1 = last), bits 6..0 = character (0x01..0x7F).
    // Empty string: single byte 0x80 (stop bit, data=0).
    // NOT null-terminated. 0x00 in data position is invalid except empty.
    // On failure cursor is unchanged (position restored).
    DecodeStatus read_ascii_string(std::string& out, std::size_t max_bytes = 1024 * 1024);

    // Nullable ASCII: same wire encoding as mandatory.
    // Null vs empty distinction handled at operator/pmap level.
    DecodeStatus read_nullable_ascii(std::string& out, bool& is_null, std::size_t max_bytes = 1024 * 1024);

    // Unicode string: length-prefixed stop-bit uInt32, then that many bytes of UTF-8.
    // Strict UTF-8 validation (no overlong, no surrogates, no > U+10FFFF).
    // Nullable: length encoded as nullable uInt32 (wire [0x80] => null, no body bytes).
    DecodeStatus read_unicode_string(std::string& out, std::size_t max_bytes = 1024 * 1024);
    DecodeStatus read_nullable_unicode(std::string& out, bool& is_null, std::size_t max_bytes = 1024 * 1024);

    // Byte vector: length-prefixed stop-bit uInt32, then that many raw bytes.
    // Nullable: length encoded as nullable uInt32 (wire [0x80] => null, no body bytes).
    DecodeStatus read_byte_vector(std::vector<std::uint8_t>& out, std::size_t max_bytes = 1024 * 1024);
    DecodeStatus read_nullable_byte_vector(std::vector<std::uint8_t>& out, bool& is_null, std::size_t max_bytes = 1024 * 1024);

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

// Shared JSON string escaping (RFC 8259).
std::string json_escape_string(const std::string& s);

}  // namespace moex_fast
