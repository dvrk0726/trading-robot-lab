#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "moex_fast/decoder_types.hpp"

namespace moex_fast {

// Bounded cursor over a byte span. Never reads past the end.
class WireCursor {
public:
    WireCursor() = default;
    WireCursor(const std::uint8_t* data, std::size_t size);

    const std::uint8_t* data() const;
    std::size_t remaining() const;
    std::size_t position() const;
    std::size_t total_size() const;
    bool at_end() const;

    // Peek without consuming
    DecodeStatus peek_byte(std::uint8_t& out) const;

    // Consume one byte
    DecodeStatus read_byte(std::uint8_t& out);

    // Read N raw bytes
    DecodeStatus read_bytes(std::size_t count, const std::uint8_t*& out_ptr);

    // --- Stop-bit unsigned integer ---
    DecodeStatus read_stopbit_u32(std::uint32_t& out);
    DecodeStatus read_stopbit_u64(std::uint64_t& out);

    // --- Stop-bit signed integer ---
    DecodeStatus read_stopbit_i32(std::int32_t& out);
    DecodeStatus read_stopbit_i64(std::int64_t& out);

    // --- Nullable integer forms ---
    // Nullable: if first byte is 0x00, value is null (consuming 1 byte).
    // Otherwise, stop-bit decode with the first byte as the initial byte.
    DecodeStatus read_nullable_u32(std::uint32_t& out, bool& is_null);
    DecodeStatus read_nullable_u64(std::uint64_t& out, bool& is_null);
    DecodeStatus read_nullable_i32(std::int32_t& out, bool& is_null);
    DecodeStatus read_nullable_i64(std::int64_t& out, bool& is_null);

    // --- Presence map ---
    // Read a presence map. pmap_bits = number of bits needed.
    // Returns the bit array (1 bit per position, MSB-first within each byte).
    DecodeStatus read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits);

    // --- Strings ---
    // ASCII: stop-bit terminated (0x00 byte marks end). Returns bytes up to but not including terminator.
    DecodeStatus read_ascii_string(std::string& out);
    // Nullable ASCII: null if first byte is 0x00 and it's the terminator.
    DecodeStatus read_nullable_ascii(std::string& out, bool& is_null);

    // Unicode: length-prefixed stop-bit uInt32, then that many bytes of UTF-8.
    DecodeStatus read_unicode_string(std::string& out);
    DecodeStatus read_nullable_unicode(std::string& out, bool& is_null);

    // Byte vector: length-prefixed stop-bit uInt32, then that many raw bytes.
    DecodeStatus read_byte_vector(std::vector<std::uint8_t>& out);
    DecodeStatus read_nullable_byte_vector(std::vector<std::uint8_t>& out, bool& is_null);

    // --- Decimal ---
    // Read exponent (i32) then mantissa (i64). Both stop-bit encoded.
    // If exponent's first byte is 0x00 (null terminator for i32), decimal is null.
    DecodeStatus read_decimal(std::int32_t& exponent, std::int64_t& mantissa, bool& is_null);

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
};

}  // namespace moex_fast
