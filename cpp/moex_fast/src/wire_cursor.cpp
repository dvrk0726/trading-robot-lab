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
// Max 5 bytes for uInt32 (35 data bits). Widened uint64_t accumulator.
// Overflow: raw > UINT32_MAX. Canonical: minimum-group encoding.
// 5 bytes without stop bit => InvalidEncoding before overflow classification.
// On failure cursor restored; out untouched.
DecodeStatus WireCursor::read_stopbit_u32(std::uint32_t& out) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;  // widened accumulator for full 35-bit entity
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);
        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            // Stop bit found
            if (raw > 0xFFFFFFFFull) {
                pos_ = start; return DecodeStatus::IntegerOverflow;
            }
            if (bytes_read > 1) {
                int prev_bits = (bytes_read - 1) * 7;
                if (raw < (1ULL << prev_bits)) {
                    pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                }
            }
            out = static_cast<std::uint32_t>(raw);
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

        // 10th byte without stop bit: InvalidEncoding before overflow
        if (bytes_read == 10 && !(b & 0x80u)) {
            pos_ = start; return DecodeStatus::InvalidEncoding;
        }

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);

        if (result > (std::numeric_limits<std::uint64_t>::max() >> 7)) {
            pos_ = start; return DecodeStatus::IntegerOverflow;
        }

        result = (result << 7) | data_bits;

        if (b & 0x80u) {
            if (bytes_read > 1 && result < (1ULL << (7 * (bytes_read - 1)))) {
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
// Max 5 bytes (35 data bits). Widened uint64_t accumulator.
// Overflow: bits 34..32 must be sign extension of bit 31.
// Canonical: minimum-group encoding via unsigned operations only.
// Value recovery avoids implementation-defined unsigned-to-signed cast.
// 5 bytes without stop bit => InvalidEncoding before domain classification.
// On failure cursor restored; out untouched.
DecodeStatus WireCursor::read_stopbit_i32(std::int32_t& out) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;  // widened accumulator for 35-bit signed entity
    int bytes_read = 0;
    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);
        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            int sig_bits = bytes_read * 7;
            static constexpr std::uint64_t mask35 = (1ULL << 35) - 1;

            // Sign-extend to 35 bits
            if (sig_bits < 35) {
                std::uint64_t sign_bit = 1ULL << (sig_bits - 1);
                if (raw & sign_bit) {
                    raw |= ~((1ULL << sig_bits) - 1);
                }
            }
            raw &= mask35;

            // Overflow: bits 34..32 must be sign extension of bit 31
            if (sig_bits > 32) {
                std::uint64_t sign31 = (raw >> 31) & 1;
                std::uint64_t expected_high = sign31 ? 0x7ULL : 0x0ULL;
                std::uint64_t actual_high = (raw >> 32) & 0x7ULL;
                if (actual_high != expected_high) {
                    pos_ = start; return DecodeStatus::IntegerOverflow;
                }
            }

            // Canonical: value must not fit in fewer bytes (unsigned logic)
            if (bytes_read > 1) {
                int prev_bits = (bytes_read - 1) * 7;
                std::uint64_t prev_mask = (1ULL << prev_bits) - 1;
                std::uint64_t truncated = raw & prev_mask;
                std::uint64_t trunc_sign = 1ULL << (prev_bits - 1);
                if (truncated & trunc_sign) {
                    truncated |= ~prev_mask;
                }
                truncated &= mask35;
                if (truncated == raw) {
                    pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                }
            }

            // Safe conversion: avoid implementation-defined unsigned-to-signed cast
            // For values <= INT32_MAX: direct cast. For values > INT32_MAX:
            // complement-and-negate, which is fully defined in C++20.
            std::uint32_t raw32 = static_cast<std::uint32_t>(raw & 0xFFFFFFFFULL);
            if (raw32 <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
                out = static_cast<std::int32_t>(raw32);
            } else {
                out = -static_cast<std::int32_t>(~raw32) - 1;
            }
            return DecodeStatus::Ok;
        }
    }
    pos_ = start; return DecodeStatus::InvalidEncoding;
}

// --- Stop-bit signed 64 (FIX FAST 1.1) ---
// Max 10 bytes (70 data bits). The 70-bit two's complement entity is mapped to
// a 64-bit result. Before the 10th byte is shifted in, bits 62:57 (= entity
// bits 69:64) are validated as sign extension of bit 56 (= entity bit 63).
// This avoids undefined behaviour from shifting a uint64_t by 64+.
DecodeStatus WireCursor::read_stopbit_i64(std::int64_t& out) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;
    int bytes_read = 0;
    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);

        // 10th byte without stop bit: InvalidEncoding before representability checks
        if (bytes_read == 10 && !(b & 0x80u)) {
            pos_ = start; return DecodeStatus::InvalidEncoding;
        }

        // Before processing the 10th byte, validate that entity bits 69:64
        // (= raw bits 62:57) are sign extension of entity bit 63 (= raw bit 56).
        if (bytes_read > 9) {
            std::uint64_t sign = (raw >> 56) & 1;
            std::uint64_t top6 = (raw >> 57) & 0x3F;
            std::uint64_t expected = sign ? 0x3Full : 0x00;
            if (top6 != expected) {
                pos_ = start; return DecodeStatus::IntegerOverflow;
            }
        }

        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            int sig_bits = bytes_read * 7;

            // Sign-extend from the sign bit to 64 bits
            if (sig_bits < 64) {
                std::uint64_t sign_bit = 1ull << (sig_bits - 1);
                if (raw & sign_bit) {
                    std::uint64_t mask = ~((1ull << sig_bits) - 1);
                    raw |= mask;
                }
            }
            // For sig_bits >= 64 (10 bytes): the value is already correct in
            // the low 64 bits thanks to the pre-shift sign-extension check.

            // Canonical encoding: value must not fit in fewer bytes (unsigned logic)
            if (bytes_read > 1) {
                int prev_bits = (bytes_read - 1) * 7;
                if (prev_bits >= 1 && prev_bits < 64) {
                    std::uint64_t mask = (1ULL << prev_bits) - 1;
                    std::uint64_t truncated = raw & mask;
                    std::uint64_t trunc_sign = 1ULL << (prev_bits - 1);
                    if (truncated & trunc_sign) {
                        truncated |= ~mask;
                    }
                    if (truncated == raw) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                }
            }

            // Safe conversion: avoid implementation-defined unsigned-to-signed cast
            if (raw <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                out = static_cast<std::int64_t>(raw);
            } else {
                out = -static_cast<std::int64_t>(~raw) - 1;
            }
            return DecodeStatus::Ok;
        }
    }
    pos_ = start; return DecodeStatus::InvalidEncoding;
}

// --- Nullable unsigned 32 (FIX FAST 1.1, section 6.3.2) ---
// NULL wire is stop-bit 0 ([0x80]), not literal byte 0x00.
// Dedicated widened 35-bit unsigned decoding (max 5 bytes).
// Raw domain 0..2^32: raw 0 = NULL; raw 1..2^32 -> value = raw - 1.
// Canonical: multi-byte encoding must not fit in fewer bytes.
// Cursor fully restored on any error; out/is_null untouched until success.
DecodeStatus WireCursor::read_nullable_u32(std::uint32_t& out, bool& is_null) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;
    int bytes_read = 0;

    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);
        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            // Stop bit found
            // Canonical: multi-byte must not fit in fewer bytes
            if (bytes_read > 1) {
                std::size_t prev_bits = static_cast<std::size_t>(bytes_read - 1) * 7;
                if (raw <= (1ull << prev_bits) - 1) {
                    pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                }
            }

            // NULL: raw == 0
            if (raw == 0) {
                is_null = true;
                return DecodeStatus::Ok;
            }

            // Overflow: raw > 2^32
            if (raw > 0x100000000ull) {
                pos_ = start; return DecodeStatus::IntegerOverflow;
            }

            // Value = raw - 1
            out = static_cast<std::uint32_t>(raw - 1);
            is_null = false;
            return DecodeStatus::Ok;
        }
    }

    pos_ = start; return DecodeStatus::InvalidEncoding;  // no stop bit in 5 bytes
}

// --- Nullable unsigned 64 (FIX FAST 1.1, section 6.3.2) ---
// NULL wire is stop-bit 0 ([0x80]), not literal byte 0x00.
// Dedicated widened 70-bit unsigned decoding (max 10 bytes).
// Raw domain 0..2^64: raw 0 = NULL; raw 1..2^64 -> value = raw - 1.
// Canonical: multi-byte encoding must not fit in fewer bytes.
// Cursor fully restored on any error; out/is_null untouched until success.
DecodeStatus WireCursor::read_nullable_u64(std::uint64_t& out, bool& is_null) {
    std::size_t start = pos_;
    std::uint8_t groups[10];
    int bytes_read = 0;

    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        groups[bytes_read++] = b & 0x7F;

        if (b & 0x80u) {
            // Stop bit found
            if (bytes_read <= 9) {
                // Accumulate into uint64_t (max 2^63 - 1)
                std::uint64_t raw = 0;
                for (int g = 0; g < bytes_read; ++g) {
                    raw = (raw << 7) | groups[g];
                }

                // Canonical: multi-byte must not fit in fewer bytes
                if (bytes_read > 1) {
                    int prev_bits = 7 * (bytes_read - 1);
                    if (raw < (1ULL << prev_bits)) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                }

                // NULL: raw == 0
                if (raw == 0) {
                    is_null = true;
                    return DecodeStatus::Ok;
                }

                // Value = raw - 1
                out = raw - 1;
                is_null = false;
                return DecodeStatus::Ok;
            } else {
                // bytes_read == 10: use top/low63 split for 70-bit value
                std::uint8_t top = groups[0];
                std::uint64_t low63 = 0;
                for (int g = 1; g < 10; ++g) {
                    low63 = (low63 << 7) | groups[g];
                }

                // Overflow: raw > 2^64
                if (top > 2 || (top == 2 && low63 > 0)) {
                    pos_ = start; return DecodeStatus::IntegerOverflow;
                }

                // Canonical: top == 0 means raw = low63 < 2^63, fits in 9 bytes
                if (top == 0) {
                    pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                }

                // NULL: raw = 0 is unreachable here (top > 0 after canonical check)
                // but keep the guard for safety
                if (top == 0 && low63 == 0) {
                    is_null = true;
                    return DecodeStatus::Ok;
                }

                // Compute value = raw - 1
                // top == 1: raw = 2^63 + low63, value = 2^63 - 1 + low63
                // top == 2 (low63 == 0): raw = 2^64, value = UINT64_MAX
                std::uint64_t value;
                if (top == 1) {
                    value = ((1ULL << 63) - 1) + low63;
                } else {
                    value = std::numeric_limits<std::uint64_t>::max();
                }
                out = value;
                is_null = false;
                return DecodeStatus::Ok;
            }
        }
    }

    pos_ = start; return DecodeStatus::InvalidEncoding;  // no stop bit in 10 bytes
}

// --- Nullable signed 32 (FIX FAST 1.1, section 6.3.2) ---
// NULL wire is stop-bit 0 ([0x80]), not literal byte 0x00.
// Dedicated widened 35-bit signed decoding (max 5 bytes).
// Raw domain [-2^31, 2^31]: raw 0 = NULL; raw < 0 unchanged; raw > 0 -> raw - 1.
// Only non-negative signed values use the +1 offset.
// Canonical: multi-byte encoding must not fit in fewer bytes.
// Cursor fully restored on any error; out/is_null untouched until success.
DecodeStatus WireCursor::read_nullable_i32(std::int32_t& out, bool& is_null) {
    std::size_t start = pos_;
    std::uint64_t raw = 0;
    int bytes_read = 0;

    for (int i = 0; i < 5; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        std::uint64_t data_bits = static_cast<std::uint64_t>(b & 0x7Fu);
        raw = (raw << 7) | data_bits;

        if (b & 0x80u) {
            // Stop bit found
            int sig_bits = bytes_read * 7;

            // Sign extend to 35 bits
            if (sig_bits < 35) {
                std::uint64_t sign_bit = 1ull << (sig_bits - 1);
                if (raw & sign_bit) {
                    raw |= ~((1ull << sig_bits) - 1);
                }
            }
            static constexpr std::uint64_t mask35 = (1ull << 35) - 1;
            raw &= mask35;

            // Canonical: value must not fit in fewer bytes
            if (bytes_read > 1) {
                int prev_bits = (bytes_read - 1) * 7;
                if (prev_bits >= 1) {
                    std::uint64_t truncated = raw & ((1ull << prev_bits) - 1);
                    std::uint64_t sign_prev = 1ull << (prev_bits - 1);
                    if (truncated & sign_prev) {
                        truncated |= ~((1ull << prev_bits) - 1);
                    }
                    truncated &= mask35;
                    if (truncated == raw) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                }
            }

            // Interpret as 35-bit signed
            std::int64_t signed_raw;
            if (raw & (1ull << 34)) {
                signed_raw = static_cast<std::int64_t>(raw | ~mask35);
            } else {
                signed_raw = static_cast<std::int64_t>(raw);
            }

            // NULL: raw == 0
            if (signed_raw == 0) {
                is_null = true;
                return DecodeStatus::Ok;
            }

            // Domain check: [-2^31, 2^31]
            std::int64_t value;
            if (signed_raw < 0) {
                if (signed_raw < -2147483648LL) {
                    pos_ = start; return DecodeStatus::IntegerOverflow;
                }
                value = signed_raw;
            } else {
                if (signed_raw > 2147483648LL) {
                    pos_ = start; return DecodeStatus::IntegerOverflow;
                }
                value = signed_raw - 1;
            }

            out = static_cast<std::int32_t>(value);
            is_null = false;
            return DecodeStatus::Ok;
        }
    }

    pos_ = start; return DecodeStatus::InvalidEncoding;  // no stop bit in 5 bytes
}

// --- Nullable signed 64 (FIX FAST 1.1, section 6.3.2) ---
// NULL wire is stop-bit 0 ([0x80]), not literal byte 0x00.
// Dedicated widened 70-bit signed decoding (max 10 bytes).
// Raw domain [-2^63, 2^63]: raw 0 = NULL; raw < 0 unchanged; raw > 0 -> raw - 1.
// Only non-negative signed values use the +1 offset.
// Cursor fully restored on any error; out/is_null untouched until success.
DecodeStatus WireCursor::read_nullable_i64(std::int64_t& out, bool& is_null) {
    std::size_t start = pos_;
    std::uint8_t groups[10];
    int bytes_read = 0;

    for (int i = 0; i < 10; ++i) {
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t b = data_[pos_++];
        groups[bytes_read++] = b & 0x7F;

        if (b & 0x80u) {
            // Stop bit found
            if (bytes_read <= 9) {
                // Accumulate into uint64_t then sign-extend (max 63 bits)
                std::uint64_t raw = 0;
                for (int g = 0; g < bytes_read; ++g) {
                    raw = (raw << 7) | groups[g];
                }
                int sig_bits = bytes_read * 7;
                if (sig_bits < 64) {
                    std::uint64_t sign_bit = 1ull << (sig_bits - 1);
                    if (raw & sign_bit) {
                        raw |= ~((1ull << sig_bits) - 1);
                    }
                }

                // Canonical: value must not fit in fewer bytes (unsigned logic)
                if (bytes_read > 1) {
                    int prev_bits = (bytes_read - 1) * 7;
                    if (prev_bits >= 1 && prev_bits < 64) {
                        std::uint64_t mask = (1ULL << prev_bits) - 1;
                        std::uint64_t truncated = raw & mask;
                        std::uint64_t trunc_sign = 1ULL << (prev_bits - 1);
                        if (truncated & trunc_sign) {
                            truncated |= ~mask;
                        }
                        if (truncated == raw) {
                            pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                        }
                    }
                }

                // NULL: raw == 0
                if (raw == 0) {
                    is_null = true;
                    return DecodeStatus::Ok;
                }

                // Nullable mapping using sign bit of sign-extended raw
                if (raw >> 63) {
                    // Negative: safe conversion without implementation-defined cast
                    out = -static_cast<std::int64_t>(~raw) - 1;
                } else {
                    // Non-negative: raw-1, safe since raw fits in int64_t
                    out = static_cast<std::int64_t>(raw - 1);
                }
                is_null = false;
                return DecodeStatus::Ok;
            } else {
                // bytes_read == 10: use top/low63 split for 70-bit signed value
                std::uint8_t top = groups[0];
                std::uint64_t low63 = 0;
                for (int g = 1; g < 10; ++g) {
                    low63 = (low63 << 7) | groups[g];
                }

                bool is_negative = (top >= 64);

                if (!is_negative) {
                    // Positive: raw = top * 2^63 + low63
                    // Overflow if raw > 2^63
                    if (top > 1 || (top == 1 && low63 > 0)) {
                        pos_ = start; return DecodeStatus::IntegerOverflow;
                    }
                    // Canonical: top == 0 means raw = low63 < 2^63
                    // If low63 < 2^62, value fits in 9 signed bytes -> non-canonical
                    // This also catches raw == 0 (NULL), which is overlong in 10 bytes
                    if (top == 0 && low63 < (1ULL << 62)) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                    // top == 1, low63 == 0: raw = 2^63 -> value = 2^63-1 = INT64_MAX
                    // top == 0, low63 >= 2^62: raw = low63 -> value = low63-1
                    std::int64_t value;
                    if (top == 1) {
                        value = std::numeric_limits<std::int64_t>::max();
                    } else {
                        value = static_cast<std::int64_t>(low63 - 1);
                    }
                    out = value;
                    is_null = false;
                    return DecodeStatus::Ok;
                } else {
                    // Negative: top >= 64
                    // Overflow if raw < -2^63, i.e. top < 127
                    if (top < 127) {
                        pos_ = start; return DecodeStatus::IntegerOverflow;
                    }
                    // top == 127: raw = low63 - 2^63
                    // Canonical: if low63 >= 2^62 then raw >= -2^62, fits in 9 bytes
                    if (low63 >= (1ULL << 62)) {
                        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
                    }
                    // raw = low63 - 2^63 < -2^62 -> canonical, needs 10 bytes
                    // As uint64_t two's complement: 2^63 + low63
                    std::uint64_t raw64 = (1ULL << 63) + low63;
                    out = -static_cast<std::int64_t>(~raw64) - 1;
                    is_null = false;
                    return DecodeStatus::Ok;
                }
            }
        }
    }

    pos_ = start; return DecodeStatus::InvalidEncoding;  // no stop bit in 10 bytes
}

// --- Presence map (FIX FAST 1.1, section 6.3.1) ---
// Stop-bit terminated. MUST find stop bit even if requested bits already filled.
// Minimal encoding: trailing all-zero 7-bit groups are implicit.
// max_pmap_bytes limits transmitted wire bytes, not logical pmap_bits.
// Cursor and output are atomic: on any failure both remain unchanged.
// FAST 1.1 ERR R7: multi-byte map whose terminating 7-bit group data bits
// are zero is overlong (non-canonical). Canonical all-zero is [80].
DecodeStatus WireCursor::read_presence_map(std::size_t pmap_bits, std::vector<bool>& out_bits,
                                            std::size_t max_pmap_bytes) {
    std::size_t start = pos_;

    // Zero-byte limit: cannot read any wire bytes.
    if (max_pmap_bytes == 0) return DecodeStatus::LimitExceeded;

    std::vector<bool> tmp;
    tmp.reserve(pmap_bits);
    std::size_t bytes_read = 0;
    std::uint8_t terminating_byte = 0;
    bool terminated = false;

    while (!terminated) {
        // Check limit before reading: if we already consumed max_pmap_bytes
        // continuation bytes without a stop bit, completing the map would
        // require exceeding the limit.
        if (bytes_read >= max_pmap_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }

        std::uint8_t b = data_[pos_++];
        ++bytes_read;

        // Extract 7 data bits, MSB first (bit 6 down to bit 0)
        for (int i = 6; i >= 0; --i) {
            if (tmp.size() < pmap_bits) {
                tmp.push_back((b >> i) & 1);
            }
        }

        if (b & 0x80u) {
            terminating_byte = b;
            terminated = true;
        }
    }

    // Pad with implicit zeros if terminated before all bits consumed
    while (tmp.size() < pmap_bits) {
        tmp.push_back(false);
    }

    // FAST 1.1 ERR R7: multi-byte map whose terminating 7-bit group data
    // bits are all zero is overlong.  Canonical all-zero is single byte [80].
    // Check the actual terminating wire byte, not tmp (which may be truncated
    // by pmap_bits and miss non-zero bits beyond the requested output prefix).
    if (bytes_read > 1 && (terminating_byte & 0x7F) == 0) {
        pos_ = start; return DecodeStatus::NonCanonicalEncoding;
    }

    // Success: commit result.
    out_bits = std::move(tmp);
    return DecodeStatus::Ok;
}

// --- ASCII string (FIX FAST 1.1, section 6.3.6) ---
// Stop-bit encoded: each byte bit 7 = stop (1 = last), bits 6-0 = character.
// Valid characters: 0x01..0x7F (7-bit ASCII without NUL).
// Empty string: exactly [80] (stop bit, data=0).
// Zero payload is not a character and is valid only as the single-byte
// mandatory empty representation. [00 80], [00 C1], [41 80] are InvalidEncoding.
// max_bytes limits decoded characters, not wire bytes.
// Cursor and output are atomic: on any failure both remain unchanged.
DecodeStatus WireCursor::read_ascii_string(std::string& out, std::size_t max_bytes) {
    std::size_t start = pos_;
    std::string tmp;
    bool has_chars = false;
    while (pos_ < size_) {
        std::uint8_t b = data_[pos_++];
        std::uint8_t data_bits = b & 0x7Fu;
        bool stop = (b & 0x80u) != 0;

        if (stop) {
            if (data_bits == 0) {
                if (has_chars) {
                    // [41 80]: zero payload after characters is invalid
                    pos_ = start; return DecodeStatus::InvalidEncoding;
                }
                // Empty string: [80]
                out.clear();
                return DecodeStatus::Ok;
            }
            // Final character
            if (tmp.size() >= max_bytes) {
                pos_ = start; return DecodeStatus::LimitExceeded;
            }
            tmp.push_back(static_cast<char>(data_bits));
            out = std::move(tmp);
            return DecodeStatus::Ok;
        }

        // Continuation byte: zero data is not a valid character
        if (data_bits == 0) { pos_ = start; return DecodeStatus::InvalidEncoding; }
        if (tmp.size() >= max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }
        tmp.push_back(static_cast<char>(data_bits));
        has_chars = true;
    }
    pos_ = start; return DecodeStatus::NeedMoreData;
}

// --- Nullable ASCII (FIX FAST 1.1, section 6.3.6) ---
// NULL=[80], empty=[00 80], non-empty same wire form as mandatory.
// [00] is truncated nullable-empty preamble (NeedMoreData).
// [00 C1] and [00 00 80] are InvalidEncoding.
// NULL/empty consume zero characters; max_bytes=0 allows both.
// On NULL: is_null=true, caller string unchanged.
// On failure: cursor, out, and is_null unchanged.
DecodeStatus WireCursor::read_nullable_ascii(std::string& out, bool& is_null, std::size_t max_bytes) {
    std::size_t start = pos_;

    if (pos_ >= size_) return DecodeStatus::NeedMoreData;

    std::uint8_t first = data_[pos_++];

    if (first == 0x80u) {
        // NULL
        is_null = true;
        return DecodeStatus::Ok;
    }

    if (first == 0x00u) {
        // Preamble for empty: need [00 80]
        if (pos_ >= size_) { pos_ = start; return DecodeStatus::NeedMoreData; }
        std::uint8_t second = data_[pos_++];
        if (second == 0x80u) {
            // Empty
            out.clear();
            is_null = false;
            return DecodeStatus::Ok;
        }
        // [00 C1] or [00 00 80]: invalid
        pos_ = start; return DecodeStatus::InvalidEncoding;
    }

    // Non-empty: rewind to start and delegate to mandatory reader
    pos_ = start;
    std::string tmp;
    DecodeStatus st = read_ascii_string(tmp, max_bytes);
    if (st != DecodeStatus::Ok) return st;
    out = std::move(tmp);
    is_null = false;
    return DecodeStatus::Ok;
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
// Strict UTF-8 validation (no overlong, no surrogates, no > U+10FFFF).
// max_bytes limits UTF-8 wire bytes, not Unicode scalar count.
// Limit checked immediately after prefix decode, before body-availability.
// On failure cursor is unchanged (position restored); out unchanged.
DecodeStatus WireCursor::read_unicode_string(std::string& out, std::size_t max_bytes) {
    std::size_t start = pos_;
    std::uint32_t len = 0;
    auto st = read_stopbit_u32(len);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    // Limit before body-availability: declared length above limit is
    // LimitExceeded even when the supplied body is truncated.
    if (len > max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }
    if (len > remaining()) { pos_ = start; return DecodeStatus::NeedMoreData; }

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    if (len > 0 && !validate_utf8(ptr, len)) {
        pos_ = start; return DecodeStatus::InvalidEncoding;
    }

    out.assign(reinterpret_cast<const char*>(ptr), len);
    return DecodeStatus::Ok;
}

// --- Nullable Unicode (FIX FAST 1.1) ---
// Length encoded as nullable uInt32: [80] => NULL, [81] => empty, etc.
// Strict UTF-8 validation (no overlong, no surrogates, no > U+10FFFF).
// max_bytes limits UTF-8 wire bytes, not Unicode scalar count.
// Limit checked immediately after prefix decode, before body-availability.
// Cursor, out, and is_null are atomic: on any error all remain unchanged.
DecodeStatus WireCursor::read_nullable_unicode(std::string& out, bool& is_null, std::size_t max_bytes) {
    std::size_t start = pos_;
    std::uint32_t len = 0;
    bool len_null = false;
    auto st = read_nullable_u32(len, len_null);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    if (len_null) {
        is_null = true;
        return DecodeStatus::Ok;
    }

    // Limit before body-availability: declared length above limit is
    // LimitExceeded even when the supplied body is truncated.
    if (len > max_bytes) { pos_ = start; return DecodeStatus::LimitExceeded; }
    if (len > remaining()) { pos_ = start; return DecodeStatus::NeedMoreData; }

    const std::uint8_t* ptr = nullptr;
    st = read_bytes(len, ptr);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    if (len > 0 && !validate_utf8(ptr, len)) {
        pos_ = start; return DecodeStatus::InvalidEncoding;
    }

    // Publish result only after all validation succeeds.
    out.assign(reinterpret_cast<const char*>(ptr), len);
    is_null = false;
    return DecodeStatus::Ok;
}

// --- Decimal (FIX FAST 1.1, section 6.3.7) ---
// Exponent (i32) then mantissa (i64), each with own operator.
// If exponent nullable and null => whole decimal null, mantissa NOT consumed.
// mantissa_nullable=true is non-normative: UnsupportedTemplateFeature before reading input.
// All decoding uses local temporaries; caller outputs published only after full success.
// On any error, cursor restored and caller exponent/mantissa/is_null unchanged.
DecodeStatus WireCursor::read_decimal(std::int32_t& exponent, std::int64_t& mantissa, bool& is_null,
                                       bool exponent_nullable, bool mantissa_nullable) {
    // Non-normative: reject mantissa_nullable before consuming any input
    if (mantissa_nullable) {
        return DecodeStatus::UnsupportedTemplateFeature;
    }

    std::size_t start = pos_;

    // Decode exponent into local temporaries
    std::int32_t tmp_exp = 0;
    bool tmp_null = false;

    if (exponent_nullable) {
        DecodeStatus st = read_nullable_i32(tmp_exp, tmp_null);
        if (st != DecodeStatus::Ok) { pos_ = start; return st; }
        if (tmp_null) {
            // Null exponent => null decimal, mantissa NOT consumed.
            // Publish only is_null; leave caller exponent/mantissa unchanged.
            is_null = true;
            return DecodeStatus::Ok;
        }
    } else {
        DecodeStatus st = read_stopbit_i32(tmp_exp);
        if (st != DecodeStatus::Ok) { pos_ = start; return st; }
    }

    // Decode mantissa into local temporary (always ordinary non-nullable i64)
    std::int64_t tmp_man = 0;
    DecodeStatus st = read_stopbit_i64(tmp_man);
    if (st != DecodeStatus::Ok) { pos_ = start; return st; }

    // Full success: publish all outputs
    exponent = tmp_exp;
    mantissa = tmp_man;
    is_null = false;
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
