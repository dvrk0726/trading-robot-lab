#include "moex_fast/wire_cursor.hpp"
#include <cstring>
#include <limits>

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
// FIX FAST 1.1 normative:
// Each byte: bit 7 = stop bit (1 = last byte). Bits 6..0 carry data.
// Max 5 bytes for 32-bit value. Non-canonical (leading zero bytes) rejected.
// Overflow checked BEFORE shift/add.
DecodeStatus WireCursor::read_stopbit_u32(std::uint32_t& out) {
    std::uint32_t result = 0;
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint32_t data_bits = static_cast<std::uint32_t>(b & 0x7Fu);

        // Check overflow before shift: if result << 7 would overflow uint32
        if (result > (std::numeric_limits<std::uint32_t>::max() >> 7)) {
            return DecodeStatus::IntegerOverflow;
        }

        result = (result << 7) | data_bits;

        if (b & 0x80u) {
            // Stop bit found — check non-canonical encoding
            if (bytes_read > 1 && result <= 0x7Fu) {
                return DecodeStatus::NonCanonicalEncoding;
            }
            out = result;
            return DecodeStatus::Ok;
        }
    }
    return DecodeStatus::InvalidEncoding;  // no stop bit in 5 bytes
}

// --- Stop-bit unsigned 64 ---
// Max 10 bytes for 64-bit value. Overflow checked before shift/add.
DecodeStatus WireCursor::read_stopbit_u64(std::uint64_t& out) {
    std::uint64_t result = 0;
    int bytes_read = 0;
    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);

        // Check overflow before shift: if result << 7 would overflow uint64
        if (result > (std::numeric_limits<std::uint64_t>::max() >> 7)) {
            return DecodeStatus::IntegerOverflow;
        }

        result = (result << 7) | data_bits;

        if (b & 0x80u) {
            if (bytes_read > 1 && result <= 0x7Fu) {
                return DecodeStatus::NonCanonicalEncoding;
            }
            out = result;
            return DecodeStatus::Ok;
        }
    }
    return DecodeStatus::InvalidEncoding;
}

// --- Stop-bit signed 32 ---
// FIX FAST 1.1: signed integers use 2's complement in stop-bit format.
// The sign bit is bit 6 of the first byte (MSB of first data byte).
// Sign extension from the first byte's bit 6 through the full width.
// Max 5 bytes for 32-bit value.
DecodeStatus WireCursor::read_stopbit_i32(std::int32_t& out) {
    std::uint32_t raw = 0;
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint32_t data_bits = static_cast<std::uint32_t>(b & 0x7Fu);

        // Check overflow before shift
        if (raw > (std::numeric_limits<std::uint32_t>::max() >> 7)) {
            return DecodeStatus::IntegerOverflow;
        }

        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            // Sign extension from the first byte's bit 6
            int sig_bits = bytes_read * 7;
            if (sig_bits < 32) {
                std::uint32_t sign_bit = 1u << (sig_bits - 1);
                if (raw & sign_bit) {
                    std::uint32_t mask = ~((1u << sig_bits) - 1);
                    raw |= mask;
                }
            }
            out = static_cast<std::int32_t>(raw);
            return DecodeStatus::Ok;
        }
    }
    return DecodeStatus::InvalidEncoding;
}

// --- Stop-bit signed 64 ---
DecodeStatus WireCursor::read_stopbit_i64(std::int64_t& out) {
    std::uint64_t raw = 0;
    int bytes_read = 0;
    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);

        // Check overflow before shift
        if (raw > (std::numeric_limits<std::uint64_t>::max() >> 7)) {
            return DecodeStatus::IntegerOverflow;
        }

        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            int sig_bits = bytes_read * 7;
            if (sig_bits < 64) {
                std::uint64_t sign_bit = 1ull << (sig_bits - 1);
                if (raw & sign_bit) {
                    std::uint64_t mask = ~((1ull << sig_bits) - 1);
                    raw |= mask;
                }
            }
            out = static_cast<std::int64_t>(raw);
            return DecodeStatus::Ok;
        }
    }
    return DecodeStatus::InvalidEncoding;
}

// --- Nullable unsigned 32 ---
// Normative FIX FAST 1.1: 0x00 byte means null (consuming 1 byte).
// Otherwise, normal stop-bit decode.
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

// --- Nullable signed 32 ---
// Normative FIX FAST 1.1: 0x00 byte means null for nullable signed integers.
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
// FIX FAST 1.1: each byte has stop bit (bit 7), data bits 6..0 (MSB first).
// Map is terminated when a byte has stop bit set.
// Unterminated map (no stop bit) is an error.
// Implicit zero bits after the transmitted terminating byte.
DecodeStatus WireCursor::read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits,
                                            std::size_t max_pmap_bytes) {
    // Validate requested bits don't exceed max capacity
    if (pmap_bits > max_pmap_bytes * 7) return DecodeStatus::LimitExceeded;

    out_bits.clear();
    out_bits.reserve(pmap_bits);
    std::size_t bytes_read = 0;
    bool terminated = false;

    while (out_bits.size() < pmap_bits) {
        if (pos_ >= size_) return DecodeStatus::NeedMoreData;
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        if (bytes_read > max_pmap_bytes) return DecodeStatus::LimitExceeded;

        // Extract 7 data bits, MSB first (bit 6 down to bit 0)
        for (int i = 6; i >= 0 && out_bits.size() < pmap_bits; --i) {
            out_bits.push_back((b >> i) & 1);
        }

        if (b & 0x80u) {
            terminated = true;
            break;
        }
    }

    // Pad with implicit zeros if terminated before all bits consumed
    while (out_bits.size() < pmap_bits) {
        out_bits.push_back(false);
    }

    // Unterminated map with not enough bits is an error
    if (!terminated && out_bits.size() < pmap_bits) {
        return DecodeStatus::InvalidPresenceMap;
    }

    return DecodeStatus::Ok;
}

// --- ASCII string ---
// FIX FAST 1.1: stop-bit terminated by 0x00 byte.
// Valid domain: 0x01..0x7F. Bytes > 0x7F are invalid.
// Terminator consumed but not included in output.
DecodeStatus WireCursor::read_ascii_string(std::string& out) {
    out.clear();
    while (pos_ < size_) {
        std::uint8_t b = data_[pos_++];
        if (b == 0x00) {
            return DecodeStatus::Ok;  // terminator found
        }
        if (b > 0x7F) return DecodeStatus::InvalidEncoding;
        out.push_back(static_cast<char>(b));
    }
    return DecodeStatus::NeedMoreData;
}

// Nullable ASCII: same wire encoding.
// Null vs empty is determined by operator/pmap, not wire encoding.
DecodeStatus WireCursor::read_nullable_ascii(std::string& out, bool& is_null) {
    is_null = false;
    return read_ascii_string(out);
}

// --- UTF-8 validation ---
bool validate_utf8(const std::uint8_t* data, std::size_t len) {
    std::size_t i = 0;
    while (i < len) {
        std::uint8_t b = data[i];
        std::uint32_t cp = 0;
        int seq_len = 0;

        if (b <= 0x7F) {
            cp = b;
            seq_len = 1;
        } else if ((b & 0xE0u) == 0xC0u) {
            cp = b & 0x1Fu;
            seq_len = 2;
        } else if ((b & 0xF0u) == 0xE0u) {
            cp = b & 0x0Fu;
            seq_len = 3;
        } else if ((b & 0xF8u) == 0xF0u) {
            cp = b & 0x07u;
            seq_len = 4;
        } else {
            return false;  // invalid leading byte
        }

        if (i + static_cast<std::size_t>(seq_len) > len) return false;

        // Check for overlong sequences
        bool overlong = false;
        if (seq_len == 2 && cp < 0x02) overlong = true;
        if (seq_len == 3 && cp == 0) overlong = true;
        if (seq_len == 4 && cp == 0) overlong = true;
        if (overlong) return false;

        // Read continuation bytes
        for (int j = 1; j < seq_len; ++j) {
            std::uint8_t cb = data[i + static_cast<std::size_t>(j)];
            if ((cb & 0xC0u) != 0x80u) return false;
            cp = (cp << 6) | (cb & 0x3Fu);
        }

        // Check for surrogates (U+D800..U+DFFF)
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;

        // Check for code points above U+10FFFF
        if (cp > 0x10FFFF) return false;

        // Verify encoded length matches minimum required
        if (seq_len == 2 && cp < 0x80) return false;
        if (seq_len == 3 && cp < 0x800) return false;
        if (seq_len == 4 && cp < 0x10000) return false;

        i += static_cast<std::size_t>(seq_len);
    }
    return true;
}

// --- Unicode string ---
// FIX FAST 1.1: length-prefixed stop-bit uInt32, then that many bytes of UTF-8.
// Strict UTF-8 validation.
DecodeStatus WireCursor::read_unicode_string(std::string& out) {
    std::uint32_t len = 0;
    auto st = read_stopbit_u32(len);
    if (st != DecodeStatus::Ok) return st;

    if (len > remaining()) return DecodeStatus::NeedMoreData;
    if (len > 1024 * 1024) return DecodeStatus::LimitExceeded;

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) return st;

    if (len > 0 && !validate_utf8(ptr, len)) {
        return DecodeStatus::InvalidEncoding;
    }

    out.assign(reinterpret_cast<const char*>(ptr), len);
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_unicode(std::string& out, bool& is_null) {
    // Nullable unicode: null represented as pmap absent (handled by caller).
    // Wire encoding is the same as mandatory.
    is_null = false;
    return read_unicode_string(out);
}

// --- Byte vector ---
// FIX FAST 1.1: length-prefixed stop-bit uInt32, then that many raw bytes.
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
// FIX FAST 1.1: exponent (i32) then mantissa (i64), each with their own operator.
// If exponent is null (and exponent_nullable), the whole decimal is null.
// Mantissa is NOT consumed after null exponent.
DecodeStatus WireCursor::read_decimal(std::int32_t& exponent, std::int64_t& mantissa, bool& is_null,
                                       bool exponent_nullable, bool mantissa_nullable) {
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;

    // Read exponent
    if (exponent_nullable) {
        DecodeStatus st = read_nullable_i32(exponent, is_null);
        if (st != DecodeStatus::Ok) return st;
        if (is_null) {
            // Null exponent => null decimal, mantissa NOT consumed
            return DecodeStatus::Ok;
        }
    } else {
        is_null = false;
        DecodeStatus st = read_stopbit_i32(exponent);
        if (st != DecodeStatus::Ok) return st;
    }

    // Read mantissa
    if (mantissa_nullable) {
        bool man_null = false;
        DecodeStatus st = read_nullable_i64(mantissa, man_null);
        if (st != DecodeStatus::Ok) return st;
        if (man_null) mantissa = 0;
    } else {
        DecodeStatus st = read_stopbit_i64(mantissa);
        if (st != DecodeStatus::Ok) return st;
    }

    return DecodeStatus::Ok;
}

// --- Sequence length ---
DecodeStatus WireCursor::read_sequence_length(std::uint32_t& out) {
    return read_stopbit_u32(out);
}

}  // namespace moex_fast
