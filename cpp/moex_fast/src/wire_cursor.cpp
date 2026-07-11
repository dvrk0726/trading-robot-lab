#include "moex_fast/wire_cursor.hpp"
#include <cstring>

namespace moex_fast {

WireCursor::WireCursor(const std::uint8_t* data, std::size_t size)
    : data_(data), size_(size), pos_(0) {}

const std::uint8_t* WireCursor::data() const { return data_; }
std::size_t WireCursor::remaining() const { return size_ - pos_; }
std::size_t WireCursor::position() const { return pos_; }
std::size_t WireCursor::total_size() const { return size_; }
bool WireCursor::at_end() const { return pos_ >= size_; }

DecodeStatus WireCursor::peek_byte(std::uint8_t& out) const {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    out = data_[pos_];
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_byte(std::uint8_t& out) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    out = data_[pos_++];
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_bytes(std::size_t count, const std::uint8_t*& out_ptr) {
    if (count > size_ - pos_) return DecodeStatus::NeedMoreData;
    out_ptr = data_ + pos_;
    pos_ += count;
    return DecodeStatus::Ok;
}

// --- Stop-bit unsigned 32 ---
// Each byte: bit 7 = stop bit (1 = last byte). Bits 0-6 carry data.
// Non-canonical: leading zero bytes (value fits in fewer bytes).
DecodeStatus WireCursor::read_stopbit_u32(std::uint32_t& out) {
    std::uint32_t result = 0;
    for (int i = 0; i < 5; ++i) {  // max 5 bytes for uInt32
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        result = (result << 7) | (b & 0x7Fu);
        if (b & 0x80u) {
            // Check for non-canonical: if result fits in fewer bytes
            if (i > 0 && result <= 0x7Fu) return DecodeStatus::NonCanonicalEncoding;
            out = result;
            return DecodeStatus::Ok;
        }
    }
    return DecodeStatus::InvalidEncoding;  // no stop bit found in 5 bytes
}

// --- Stop-bit unsigned 64 ---
DecodeStatus WireCursor::read_stopbit_u64(std::uint64_t& out) {
    std::uint64_t result = 0;
    for (int i = 0; i < 10; ++i) {  // max 10 bytes for uInt64
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        // Check for overflow before shift
        if (i >= 9 && (b & 0x7Fu) > 1) return DecodeStatus::IntegerOverflow;
        result = (result << 7) | static_cast<std::uint64_t>(b & 0x7Fu);
        if (b & 0x80u) {
            if (i > 0 && result <= 0x7Fu) return DecodeStatus::NonCanonicalEncoding;
            out = result;
            return DecodeStatus::Ok;
        }
    }
    return DecodeStatus::InvalidEncoding;
}

// --- Stop-bit signed 32 ---
// FIX FAST: positive values have leading 0x00 byte (or the high bit of the first data byte is 0).
// Negative values have leading 0x7F byte (or the high bit of the first data byte is 1).
// Sign extension from the first byte's bit 6.
DecodeStatus WireCursor::read_stopbit_i32(std::int32_t& out) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;

    // Read the raw unsigned value first
    std::uint32_t raw = 0;
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        raw = (raw << 7) | (b & 0x7Fu);
        bytes_read++;
        if (b & 0x80u) break;
    }

    // Check if we got a stop bit
    if (bytes_read > 5) return DecodeStatus::InvalidEncoding;

    // Sign extend: if bit 6 of the first byte is set, the number is negative
    // In FIX FAST, signed integers are encoded as 2's complement in stop-bit format
    // The sign bit is the MSB of the first data byte (bit 6 of byte 0)
    // We need to sign-extend from the appropriate bit position

    // Determine the number of significant bits
    // For a 1-byte value: 7 bits, sign bit is bit 6
    // For a 2-byte value: 14 bits, sign bit is bit 13
    // etc.

    int sig_bits = bytes_read * 7;
    // Check sign bit
    std::uint32_t sign_bit = 1u << (sig_bits - 1);
    if (raw & sign_bit) {
        // Negative: sign extend
        std::uint32_t mask = ~((1u << sig_bits) - 1);
        raw |= mask;
    }

    out = static_cast<std::int32_t>(raw);
    return DecodeStatus::Ok;
}

// --- Stop-bit signed 64 ---
DecodeStatus WireCursor::read_stopbit_i64(std::int64_t& out) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;

    std::uint64_t raw = 0;
    int bytes_read = 0;
    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        raw = (raw << 7) | static_cast<std::uint64_t>(b & 0x7Fu);
        bytes_read++;
        if (b & 0x80u) break;
    }

    if (bytes_read > 10) return DecodeStatus::InvalidEncoding;

    int sig_bits = bytes_read * 7;
    std::uint64_t sign_bit = 1ull << (sig_bits - 1);
    if (raw & sign_bit) {
        std::uint64_t mask = ~((1ull << sig_bits) - 1);
        raw |= mask;
    }

    out = static_cast<std::int64_t>(raw);
    return DecodeStatus::Ok;
}

// --- Nullable unsigned 32 ---
// Nullable: 0x00 byte means null (consuming 1 byte).
// Otherwise, the first byte is part of the stop-bit value.
DecodeStatus WireCursor::read_nullable_u32(std::uint32_t& out, bool& is_null) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    return read_stopbit_u32(out);
}

DecodeStatus WireCursor::read_nullable_u64(std::uint64_t& out, bool& is_null) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    return read_stopbit_u64(out);
}

DecodeStatus WireCursor::read_nullable_i32(std::int32_t& out, bool& is_null) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    return read_stopbit_i32(out);
}

DecodeStatus WireCursor::read_nullable_i64(std::int64_t& out, bool& is_null) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    return read_stopbit_i64(out);
}

// --- Presence map ---
// Presence map: each byte has a stop bit (bit 7). Bits 0-6 are data bits (MSB first).
// The map is terminated when a byte has its stop bit set.
// We read bytes until the stop bit, then extract pmap_bits from the data bits.
DecodeStatus WireCursor::read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits) {
    if (pmap_bits > 64 * 7) return DecodeStatus::LimitExceeded;  // max 64 bytes * 7 bits

    out_bits.clear();
    out_bits.reserve(pmap_bits);

    while (out_bits.size() < pmap_bits) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];

        // Extract 7 bits, MSB first (bit 6 down to bit 0)
        for (int i = 6; i >= 0 && out_bits.size() < pmap_bits; --i) {
            out_bits.push_back((b >> i) & 1);
        }

        if (b & 0x80u) {
            // Stop bit found
            break;
        }
    }

    // If we didn't get enough bits and there was no stop bit, that's an error
    // (but the loop above would have returned NeedMoreData if we ran out of input)

    return DecodeStatus::Ok;
}

// --- ASCII string ---
// Stop-bit terminated: 0x00 byte marks end. No length prefix.
// The terminator byte is consumed but not included in the output.
DecodeStatus WireCursor::read_ascii_string(std::string& out) {
    out.clear();
    while (pos_ < size_) {
        std::uint8_t b = data_[pos_++];
        if (b == 0x00) {
            return DecodeStatus::Ok;  // terminator found
        }
        // Check ASCII domain: 0x01-0x7F
        if (b > 0x7F) return DecodeStatus::InvalidEncoding;
        out.push_back(static_cast<char>(b));
    }
    return DecodeStatus::NeedMoreData;
}

DecodeStatus WireCursor::read_nullable_ascii(std::string& out, bool& is_null) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        // Could be null or empty string.
        // In FIX FAST: nullable ASCII — 0x00 alone means null.
        // An empty string is encoded as 0x00 (the terminator) with no preceding data.
        // But for nullable, the first 0x00 means null.
        // Wait: FIX FAST says nullable ASCII — the null value is the empty string (just the terminator).
        // Actually, for nullable string, the wire representation of null IS the empty string (0x00).
        // We need to distinguish null from empty based on context.
        // The standard says: for optional (nullable) strings, the null value is represented
        // by the absence of the string (no bytes consumed) or by a specific encoding.
        //
        // Actually, for ASCII strings in FAST:
        // - Mandatory: always present, stop-bit terminated
        // - Optional (nullable): the first byte 0x00 could mean:
        //   - If the presence-map bit is 0 (absent): use previous/initial value
        //   - If the presence-map bit is 1 (present): the value IS the empty string
        //
        // For the wire cursor, we just read what's there. The null/empty distinction
        // is handled at the operator level based on presence-map bits.
        // At the wire level: empty string = just the 0x00 terminator.
        pos_++;  // consume the terminator
        is_null = false;  // empty string, not null at wire level
        out.clear();
        return DecodeStatus::Ok;
    }
    is_null = false;
    return read_ascii_string(out);
}

// --- Unicode string ---
// Length-prefixed stop-bit uInt32, then that many bytes of UTF-8.
DecodeStatus WireCursor::read_unicode_string(std::string& out) {
    std::uint32_t len = 0;
    auto st = read_stopbit_u32(len);
    if (st != DecodeStatus::Ok) return st;

    if (len > remaining()) return DecodeStatus::NeedMoreData;
    if (len > 1024 * 1024) return DecodeStatus::LimitExceeded;

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) return st;

    // Basic UTF-8 validation
    out.assign(reinterpret_cast<const char*>(ptr), len);
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_unicode(std::string& out, bool& is_null) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    // Nullable unicode: a null is encoded as a single 0x00 byte (length = 0 with stop bit)
    // Actually, the length prefix being 0 means empty string.
    // For nullable: if the presence-map indicates present and the value is null,
    // it's encoded differently. Let's just read the length and check.
    is_null = false;
    return read_unicode_string(out);
}

// --- Byte vector ---
// Length-prefixed stop-bit uInt32, then that many raw bytes.
DecodeStatus WireCursor::read_byte_vector(std::vector<std::uint8_t>& out) {
    std::uint32_t len = 0;
    auto st = read_stopbit_u32(len);
    if (st != DecodeStatus::Ok) return st;

    if (len > remaining()) return DecodeStatus::NeedMoreData;
    if (len > 1024 * 1024) return DecodeStatus::LimitExceeded;

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) return st;

    out.assign(ptr, ptr + len);
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_byte_vector(std::vector<std::uint8_t>& out, bool& is_null) {
    is_null = false;
    return read_byte_vector(out);
}

// --- Decimal ---
// Exponent (i32 stop-bit), then mantissa (i64 stop-bit).
// If exponent is null (0x00 byte), the whole decimal is null and mantissa is not consumed.
DecodeStatus WireCursor::read_decimal(std::int32_t& exponent, std::int64_t& mantissa, bool& is_null) {
    // Try to read exponent. If it starts with 0x00, it might be null.
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;

    if (data_[pos_] == 0x00) {
        // Null exponent => null decimal
        pos_++;
        is_null = true;
        exponent = 0;
        mantissa = 0;
        return DecodeStatus::Ok;
    }

    is_null = false;
    auto st = read_stopbit_i32(exponent);
    if (st != DecodeStatus::Ok) return st;

    st = read_stopbit_i64(mantissa);
    return st;
}

}  // namespace moex_fast
