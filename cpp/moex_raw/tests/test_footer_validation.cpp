#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

int main() {
    using namespace moex_raw;

    // Positive: footer serialization and round-trip
    {
        RawFooter footer;
        footer.record_count = 5;
        footer.first_capture_index = 0;
        footer.last_capture_index = 4;
        footer.total_payload_bytes = 320;
        footer.data_bytes_before_footer = 1024;
        sha256("test content", 12, footer.content_sha256);

        std::vector<std::uint8_t> buf;
        serialize_footer(buf, footer);

        assert(buf.size() == kFooterSize);
        assert(std::memcmp(buf.data(), kMagicEnd, 8) == 0);
        assert(read_u32_le(buf.data() + 8) == kFooterSize);
        assert(read_u32_le(buf.data() + 12) == 0);  // flags

        RawFooter out;
        std::vector<RawValidationIssue> issues;
        assert(deserialize_footer(buf.data(), buf.size(), out, issues));
        assert(out.record_count == 5);
        assert(out.first_capture_index == 0);
        assert(out.last_capture_index == 4);
        assert(out.total_payload_bytes == 320);
        assert(out.data_bytes_before_footer == 1024);
        assert(std::memcmp(out.content_sha256, footer.content_sha256, 32) == 0);
    }

    // Positive: footer CRC32C over first 88 bytes
    {
        RawFooter footer;
        footer.record_count = 1;
        std::vector<std::uint8_t> buf;
        serialize_footer(buf, footer);

        // Verify CRC is correct
        std::uint32_t computed = crc32c(buf.data(), 88);
        std::uint32_t stored = read_u32_le(buf.data() + 88);
        assert(computed == stored);
    }

    // Negative: wrong footer magic
    {
        RawFooter footer;
        std::vector<std::uint8_t> buf;
        serialize_footer(buf, footer);
        buf[0] = 'X';

        RawFooter out;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_footer(buf.data(), buf.size(), out, issues));
    }

    // Negative: wrong footer size
    {
        RawFooter footer;
        std::vector<std::uint8_t> buf;
        serialize_footer(buf, footer);
        buf[8] = 100;  // wrong size

        RawFooter out;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_footer(buf.data(), buf.size(), out, issues));
    }

    // Negative: non-zero footer flags
    {
        RawFooter footer;
        std::vector<std::uint8_t> buf;
        serialize_footer(buf, footer);
        buf[13] = 1;  // non-zero flags

        RawFooter out;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_footer(buf.data(), buf.size(), out, issues));
    }

    // Negative: wrong footer CRC
    {
        RawFooter footer;
        std::vector<std::uint8_t> buf;
        serialize_footer(buf, footer);
        buf[89] ^= 0xFF;  // corrupt CRC

        RawFooter out;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_footer(buf.data(), buf.size(), out, issues));
    }

    // Negative: truncated footer
    {
        std::vector<std::uint8_t> buf(50, 0);
        RawFooter out;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_footer(buf.data(), buf.size(), out, issues));
    }

    std::cout << "test_footer_validation: ALL PASSED\n";
    return 0;
}
