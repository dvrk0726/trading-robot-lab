#include "moex_fast/wire_cursor.hpp"
#include <cstring>
#include <limits>
#include <sstream>
#include <iomanip>

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

// --- Stop-bit unsigned 32 (FIX FAST 1.1, section 6.3.2) ---
// Each byte: bit 7 = stop (1 = last), bits 6..0 = data (MSB first).
// Max 5 bytes for uInt32 (35 data bits). Overflow checked before shift/add.
// Canonical: multi-byte value must not fit in fewer bytes.
DecodeStatus WireCursor::read_stopbit_u32(std::uint32_t& out) {
    std::size_t start = pos_;
    std::uint32_t result = 0;
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint32_t data_bits = static_cast<std::uint32_t>(b & 0x7Fu);

        // Check overflow before shift
        if (result > (std::numeric_limits<std::uint32_t>::max() >> 7)) {
            pos_ = start; return DecodeStatus::IntegerOverflow;
        }

        result = (result << 7) | data_bits;

        if (b & 0x80u) {
            // Stop bit found — check non-canonical encoding
            if (bytes_read > 1 && result <= 0x7Fu) {
                pos_ = start; return DecodeStatus::NonCanonicalEncoding;
            }
            out = result;
            return DecodeStatus::Ok;
        }
    }
    pos_ = start; return DecodeStatus::InvalidEncoding;  // no stop bit in 5 bytes
}

// --- Stop-bit unsigned 64 (FIX FAST 1.1) ---
// Max 10 bytes for uInt64 (70 data bits). Overflow checked before shift/add.
DecodeStatus WireCursor::read_stopbit_u64(std::uint64_t& out) {
    std::size_t start = pos_;
    std::uint64_t result = 0;
    int bytes_read = 0;
    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);

        if (result > (std::numeric_limits<std::uint64_t>::max() >> 7)) {
            pos_ = start; return DecodeStatus::IntegerOverflow;
        }

        result = (result << 7) | data_bits;

        if (b & 0x80u) {
            if (bytes_read > 1 && result <= 0x7F) {
                pos_ = start; return DecodeStatus::NonCanonicalEncoding;
            }
            out = result;
            return DecodeStatus::Ok;
        }
    }
    pos_ = start; return DecodeStatus::InvalidEncoding;
}

// --- Stop-bit signed 32 (FIX FAST 1.1, section 6.3.2) ---
// Two's complement. Sign bit is bit 6 of first byte.
// Max 5 bytes (35 data bits). Canonical: minimum byte count.
// Uses uint64_t accumulator to avoid premature overflow.
DecodeStatus WireCursor::read_stopbit_i32(std::int32_t& out) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;  // wider accumulator for 35-bit values
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);
        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            int sig_bits = bytes_read * 7;

            // Max-width validation: bits above 32 must be sign extension of bit 31
            if (sig_bits > 32) {
                std::uint64_t sign = (raw >> 31) & 1;
                for (int bit = 32; bit < sig_bits; ++bit) {
                    if (((raw >> bit) & 1) != sign) {
                        pos_ = start; return DecodeStatus::IntegerOverflow;
                    }
                }
                // Mask to 32 bits after validation
                raw &= 0xFFFFFFFFull;
            }

            // Sign extension from the sign bit to 32 bits
            if (sig_bits < 32) {
                std::uint64_t sign_bit = 1ull << (sig_bits - 1);
                if (raw & sign_bit) {
                    std::uint64_t mask = ~((1ull << sig_bits) - 1);
                    raw |= (mask & 0xFFFFFFFFull);
                }
            }

            // Canonical encoding: value must not fit in fewer bytes
            if (bytes_read > 1) {
                int prev_bits = (bytes_read - 1) * 7;
                if (prev_bits >= 1) {
                    std::int32_t decoded = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw));
                    std::int32_t lo = (prev_bits >= 32) ? std::numeric_limits<std::int32_t>::min()
                        : -(static_cast<std::int32_t>(1) << (prev_bits - 1));
                    std::int32_t hi = (prev_bits >= 32) ? std::numeric_limits<std::int32_t>::max()
                        : (static_cast<std::int32_t>(1) << (prev_bits - 1)) - 1;
                    if (decoded >= lo && decoded <= hi) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                }
            }

            out = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw));
            return DecodeStatus::Ok;
        }
    }
    pos_ = start; return DecodeStatus::InvalidEncoding;
}

// --- Stop-bit signed 64 (FIX FAST 1.1) ---
DecodeStatus WireCursor::read_stopbit_i64(std::int64_t& out) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;
    int bytes_read = 0;
    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);

        // For bytes beyond 9 (63 bits accumulated), the shift would overflow uint64_t.
        // Check that the top 7 bits are sign extension before shifting.
        if (bytes_read > 9) {
            std::uint64_t sign = (raw >> 63) & 1;
            std::uint64_t top7 = raw >> 57;  // bits 63:57
            std::uint64_t expected = sign ? 0x7Full : 0x00;
            if (top7 != expected) {
                pos_ = start; return DecodeStatus::IntegerOverflow;
            }
        }

        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            int sig_bits = bytes_read * 7;

            // For sig_bits > 64: the extra bits (above 64) must be sign extension of bit 63
            // But since we're using uint64_t, bits above 63 are lost in the shift.
            // The pre-shift check above handles this for bytes beyond 9.
            // For the exact boundary (bytes_read == 10, sig_bits == 70):
            // bits 69:64 were in the pre-shift top7 check.

            // For sig_bits < 64: sign-extend from the sign bit
            if (sig_bits < 64) {
                std::uint64_t sign_bit = 1ull << (sig_bits - 1);
                if (raw & sign_bit) {
                    std::uint64_t mask = ~((1ull << sig_bits) - 1);
                    raw |= mask;
                }
            }

            // Canonical encoding: value must not fit in fewer bytes
            if (bytes_read > 1) {
                int prev_bits = (bytes_read - 1) * 7;
                if (prev_bits >= 1 && prev_bits < 64) {
                    std::int64_t decoded = static_cast<std::int64_t>(raw);
                    std::int64_t lo = -(static_cast<std::int64_t>(1) << (prev_bits - 1));
                    std::int64_t hi = (static_cast<std::int64_t>(1) << (prev_bits - 1)) - 1;
                    if (decoded >= lo && decoded <= hi) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                }
            }

            out = static_cast<std::int64_t>(raw);
            return DecodeStatus::Ok;
        }
    }
    pos_ = start; return DecodeStatus::InvalidEncoding;
}

// --- Nullable unsigned 32 (FIX FAST 1.1, section 6.3.2) ---
// 0x00 => null (1 byte). Otherwise: stop-bit decode, value = decoded - 1.
// Decoded 0 (wire 0x80) is non-canonical for nullable unsigned.
DecodeStatus WireCursor::read_nullable_u32(std::uint32_t& out, bool& is_null) {
    std::size_t start = pos_;
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    std::uint32_t raw = 0;
    DecodeStatus st = read_stopbit_u32(raw);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    if (raw == 0) { pos_ = start; return DecodeStatus::NonCanonicalEncoding; }
    out = raw - 1;
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_u64(std::uint64_t& out, bool& is_null) {
    std::size_t start = pos_;
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    std::uint64_t raw = 0;
    DecodeStatus st = read_stopbit_u64(raw);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    if (raw == 0) { pos_ = start; return DecodeStatus::NonCanonicalEncoding; }
    out = raw - 1;
    return DecodeStatus::Ok;
}

// --- Nullable signed 32 (FIX FAST 1.1) ---
// 0x00 => null (1 byte). Otherwise: stop-bit decode, value = decoded - 1.
DecodeStatus WireCursor::read_nullable_i32(std::int32_t& out, bool& is_null) {
    std::size_t start = pos_;
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    std::int32_t raw = 0;
    DecodeStatus st = read_stopbit_i32(raw);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    // value = decoded - 1 (signed). raw == INT32_MIN would underflow.
    if (raw == std::numeric_limits<std::int32_t>::min()) {
        pos_ = start; return DecodeStatus::IntegerOverflow;
    }
    out = raw - 1;
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_i64(std::int64_t& out, bool& is_null) {
    std::size_t start = pos_;
    if (pos_ >= size_) return DecodeStatus::NeedMoreData;
    if (data_[pos_] == 0x00) {
        pos_++;
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;
    std::int64_t raw = 0;
    DecodeStatus st = read_stopbit_i64(raw);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    if (raw == std::numeric_limits<std::int64_t>::min()) {
        pos_ = start; return DecodeStatus::IntegerOverflow;
    }
    out = raw - 1;
    return DecodeStatus::Ok;
}

// --- Presence map (FIX FAST 1.1, section 6.3.1) ---
// Stop-bit terminated. MUST find stop bit even if requested bits already filled.
// Implicit zero bits after transmitted bits where FAST permits.
DecodeStatus WireCursor::read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits,
                                            std::size_t max_pmap_bytes) {
    std::size_t start = pos_;
    if (pmap_bits > max_pmap_bytes * 7) return DecodeStatus::LimitExceeded;

    out_bits.clear();
    out_bits.reserve(pmap_bits);
    std::size_t bytes_read = 0;
    bool terminated = false;

    while (!terminated) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        if (bytes_read > max_pmap_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }

        // Extract 7 data bits, MSB first (bit 6 down to bit 0)
        for (int i = 6; i >= 0; --i) {
            if (out_bits.size() < pmap_bits) {
                out_bits.push_back((b >> i) & 1);
            }
        }

        if (b & 0x80u) {
            terminated = true;
        }
    }

    // Pad with implicit zeros if terminated before all bits consumed
    while (out_bits.size() < pmap_bits) {
        out_bits.push_back(false);
    }

    return DecodeStatus::Ok;
}

// --- ASCII string (FIX FAST 1.1, section 6.3.6) ---
// Stop-bit encoded: each byte bit 7 = stop (1 = last), bits 6-0 = character.
// Valid characters: 0x01..0x7F. Empty string: 0x80 (stop, data=0).
// NOT null-terminated. Cursor restored on failure.
DecodeStatus WireCursor::read_ascii_string(std::string& out, std::size_t max_bytes) {
    std::size_t start = pos_;
    out.clear();
    while (pos_ < size_) {
        std::uint8_t b = data_[pos_++];
        std::uint8_t data_bits = b & 0x7Fu;
        bool stop = (b & 0x80u) != 0;

        if (stop) {
            // Last byte: data bits are either a final character or empty terminator
            if (data_bits == 0) {
                // Empty string or end after previous characters
                return DecodeStatus::Ok;
            }
            // Valid final character
            if (out.size() >= max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }
            out.push_back(static_cast<char>(data_bits));
            return DecodeStatus::Ok;
        }

        // Continuation byte: data bits must be a valid character
        if (data_bits == 0) { pos_ = start; return DecodeStatus::InvalidEncoding; }
        if (out.size() >= max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }
        out.push_back(static_cast<char>(data_bits));
    }
    pos_ = start; return DecodeStatus::NeedMoreData;
}

// Nullable ASCII: same wire encoding. Null handled by operator/pmap.
DecodeStatus WireCursor::read_nullable_ascii(std::string& out, bool& is_null, std::size_t max_bytes) {
    is_null = false;
    return read_ascii_string(out, max_bytes);
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
            return false;
        }

        if (i + static_cast<std::size_t>(seq_len) > len) return false;

        bool overlong = false;
        if (seq_len == 2 && cp < 0x02) overlong = true;
        if (seq_len == 3 && cp == 0) overlong = true;
        if (seq_len == 4 && cp == 0) overlong = true;
        if (overlong) return false;

        for (int j = 1; j < seq_len; ++j) {
            std::uint8_t cb = data[i + static_cast<std::size_t>(j)];
            if ((cb & 0xC0u) != 0x80u) return false;
            cp = (cp << 6) | (cb & 0x3Fu);
        }

        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        if (cp > 0x10FFFF) return false;

        if (seq_len == 2 && cp < 0x80) return false;
        if (seq_len == 3 && cp < 0x800) return false;
        if (seq_len == 4 && cp < 0x10000) return false;

        i += static_cast<std::size_t>(seq_len);
    }
    return true;
}

// --- Unicode string (FIX FAST 1.1) ---
// Length-prefixed stop-bit uInt32, then that many bytes of UTF-8.
// Nullable: length encoded as nullable uInt32.
DecodeStatus WireCursor::read_unicode_string(std::string& out, std::size_t max_bytes) {
    std::size_t start = pos_;
    std::uint32_t len = 0;
    auto st = read_stopbit_u32(len);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    if (len > remaining()) { pos_ = start; return DecodeStatus::NeedMoreData; }
    if (len > max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    if (len > 0 && !validate_utf8(ptr, len)) {
        pos_ = start; return DecodeStatus::InvalidEncoding;
    }

    out.assign(reinterpret_cast<const char*>(ptr), len);
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_unicode(std::string& out, bool& is_null, std::size_t max_bytes) {
    std::size_t start = pos_;
    // Nullable unicode: length is nullable uInt32
    std::uint32_t len = 0;
    bool len_null = false;
    auto st = read_nullable_u32(len, len_null);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    if (len_null) {
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;

    if (len > remaining()) { pos_ = start; return DecodeStatus::NeedMoreData; }
    if (len > max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    if (len > 0 && !validate_utf8(ptr, len)) {
        pos_ = start; return DecodeStatus::InvalidEncoding;
    }

    out.assign(reinterpret_cast<const char*>(ptr), len);
    return DecodeStatus::Ok;
}

// --- Byte vector (FIX FAST 1.1) ---
// Length-prefixed stop-bit uInt32, then that many raw bytes.
// Nullable: length encoded as nullable uInt32.
DecodeStatus WireCursor::read_byte_vector(std::vector<std::uint8_t>& out, std::size_t max_bytes) {
    std::size_t start = pos_;
    std::uint32_t len = 0;
    auto st = read_stopbit_u32(len);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    if (len > remaining()) { pos_ = start; return DecodeStatus::NeedMoreData; }
    if (len > max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    out.assign(ptr, ptr + len);
    return DecodeStatus::Ok;
}

DecodeStatus WireCursor::read_nullable_byte_vector(std::vector<std::uint8_t>& out, bool& is_null, std::size_t max_bytes) {
    std::size_t start = pos_;
    std::uint32_t len = 0;
    bool len_null = false;
    auto st = read_nullable_u32(len, len_null);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    if (len_null) {
        is_null = true;
        return DecodeStatus::Ok;
    }
    is_null = false;

    if (len > remaining()) { pos_ = start; return DecodeStatus::NeedMoreData; }
    if (len > max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    out.assign(ptr, ptr + len);
    return DecodeStatus::Ok;
}

// --- Decimal (FIX FAST 1.1, section 6.3.7) ---
// Exponent (i32) then mantissa (i64), each with own operator.
// If exponent nullable and null => whole decimal null, mantissa NOT consumed.
DecodeStatus WireCursor::read_decimal(std::int32_t& exponent, std::int64_t& mantissa, bool& is_null,
                                       bool exponent_nullable, bool mantissa_nullable) {
    std::size_t start = pos_;

    // Read exponent
    if (exponent_nullable) {
        DecodeStatus st = read_nullable_i32(exponent, is_null);
        if (st != DecodeStatus::Ok) { pos_ = start; return st; }
        if (is_null) {
            // Null exponent => null decimal, mantissa NOT consumed
            return DecodeStatus::Ok;
        }
    } else {
        is_null = false;
        DecodeStatus st = read_stopbit_i32(exponent);
        if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    }

    // Read mantissa
    if (mantissa_nullable) {
        bool man_null = false;
        DecodeStatus st = read_nullable_i64(mantissa, man_null);
        if (st != DecodeStatus::Ok) { pos_ = start; return st; }
        if (man_null) mantissa = 0;
    } else {
        DecodeStatus st = read_stopbit_i64(mantissa);
        if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    }

    return DecodeStatus::Ok;
}

// --- Sequence length ---
DecodeStatus WireCursor::read_sequence_length(std::uint32_t& out) {
    return read_stopbit_u32(out);
}

// --- Shared JSON string escaping (RFC 8259) ---
std::string json_escape_string(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

}  // namespace moex_fast
