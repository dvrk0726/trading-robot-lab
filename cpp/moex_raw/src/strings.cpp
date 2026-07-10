#include "moex_raw/strings.hpp"
#include "moex_raw/endian.hpp"
#include <cstring>

namespace moex_raw {

bool validate_utf8_string(const std::string& s) {
    if (s.size() > kMaxStringBytes) return false;

    // Check for embedded NUL
    if (s.find('\0') != std::string::npos) return false;

    // Validate UTF-8 encoding
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint8_t c = static_cast<std::uint8_t>(s[i]);
        std::size_t seq_len = 0;

        if (c <= 0x7F) {
            seq_len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            seq_len = 2;
            if (c < 0xC2) return false;  // overlong
        } else if ((c & 0xF0) == 0xE0) {
            seq_len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            seq_len = 4;
            if (c > 0xF4) return false;  // beyond Unicode
        } else {
            return false;  // invalid lead byte
        }

        if (i + seq_len > s.size()) return false;

        for (std::size_t j = 1; j < seq_len; ++j) {
            if ((static_cast<std::uint8_t>(s[i + j]) & 0xC0) != 0x80)
                return false;
        }

        // Decode codepoint and check ranges
        std::uint32_t cp = 0;
        if (seq_len == 1) cp = c;
        else if (seq_len == 2) cp = ((c & 0x1F) << 6) | (static_cast<std::uint8_t>(s[i+1]) & 0x3F);
        else if (seq_len == 3) cp = ((c & 0x0F) << 12) | ((static_cast<std::uint8_t>(s[i+1]) & 0x3F) << 6) | (static_cast<std::uint8_t>(s[i+2]) & 0x3F);
        else cp = ((c & 0x07) << 18) | ((static_cast<std::uint8_t>(s[i+1]) & 0x3F) << 12) | ((static_cast<std::uint8_t>(s[i+2]) & 0x3F) << 6) | (static_cast<std::uint8_t>(s[i+3]) & 0x3F);

        // Reject surrogates
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        // Reject overlong for 3-byte
        if (seq_len == 3 && cp < 0x0800) return false;
        // Reject overlong for 4-byte
        if (seq_len == 4 && cp < 0x10000) return false;

        i += seq_len;
    }

    return true;
}

void write_length_string(std::vector<std::uint8_t>& buf, const std::string& s) {
    write_u16_le(buf, static_cast<std::uint16_t>(s.size()));
    write_bytes(buf, s.data(), s.size());
}

bool read_length_string(const std::uint8_t* data, std::size_t available,
                        std::string& out, std::size_t& bytes_consumed) {
    if (available < 2) return false;

    std::uint16_t len = read_u16_le(data);
    if (len > kMaxStringBytes) return false;
    if (available < static_cast<std::size_t>(2) + len) return false;

    out.assign(reinterpret_cast<const char*>(data + 2), len);
    bytes_consumed = static_cast<std::size_t>(2) + len;

    // Validate UTF-8
    if (!validate_utf8_string(out)) return false;

    return true;
}

}  // namespace moex_raw
