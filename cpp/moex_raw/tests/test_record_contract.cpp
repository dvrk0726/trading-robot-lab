#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "test_check.hpp"
#include <iostream>
#include <cstring>

int main() {
    using namespace moex_raw;

    // Positive: zero-length payload
    {
        RawPacketRecord rec;
        rec.record_flags = 0;
        rec.capture_index = 0;
        rec.capture_utc_ns = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {};

        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);
        CHECK(buf.size() == kRecordHeaderSize + 4);

        CHECK(std::memcmp(buf.data(), kMagicRec, 4) == 0);
        CHECK(read_u16_le(buf.data() + 4) == kRecordHeaderSize);
        CHECK(read_u32_le(buf.data() + 8) == kRecordHeaderSize + 4);
        CHECK(read_u32_le(buf.data() + 36) == 0);
        CHECK(read_u32_le(buf.data() + 40) == 0);
    }

    // Positive: normal payload round-trip
    {
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 42;
        rec.capture_utc_ns = 1700000000000000000ULL;
        rec.capture_monotonic_ns = 1000000;
        rec.payload = {0x01, 0x02, 0x03, 0xFF};

        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);

        RawPacketRecord out;
        std::size_t total_size = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(deserialize_record_header(buf.data(), buf.size(), out, total_size, issues));
        CHECK(out.capture_index == 42);
        CHECK(out.capture_utc_ns == 1700000000000000000ULL);
        CHECK(out.capture_monotonic_ns == 1000000);
        CHECK(out.payload == rec.payload);
        CHECK(total_size == buf.size());
    }

    // Positive: exact payload bytes preserved
    {
        RawPacketRecord rec;
        rec.record_flags = 0;
        rec.capture_index = 0;
        rec.capture_utc_ns = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x00, 0xFF, 0x00, 0xFF};

        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);

        RawPacketRecord out;
        std::size_t total_size = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(deserialize_record_header(buf.data(), buf.size(), out, total_size, issues));
        CHECK(out.payload[0] == 0x00);
        CHECK(out.payload[1] == 0xFF);
        CHECK(out.payload[2] == 0x00);
        CHECK(out.payload[3] == 0xFF);
    }

    // Positive: record_size = 44 + payload_size + 4
    {
        RawPacketRecord rec;
        rec.record_flags = 0;
        rec.capture_index = 0;
        rec.capture_utc_ns = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload.resize(100, 0xAB);

        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);
        CHECK(read_u32_le(buf.data() + 8) == 44 + 100 + 4);
    }

    // Negative: wrong record magic
    {
        RawPacketRecord rec;
        rec.payload = {1, 2, 3};
        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);
        buf[0] = 'X';

        RawPacketRecord out;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), out, ts, issues));
        (void)ts;
    }

    // Negative: wrong record_header_size
    {
        RawPacketRecord rec;
        rec.payload = {1, 2, 3};
        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);
        buf[4] = 50;

        RawPacketRecord out;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), out, ts, issues));
        (void)ts;
    }

    // Negative: unknown record flag bit
    {
        RawPacketRecord rec;
        rec.record_flags = 0x0002;
        rec.payload = {1, 2, 3};
        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);

        RawPacketRecord out;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), out, ts, issues));
        (void)ts;
    }

    // Negative: truncated header
    {
        std::vector<std::uint8_t> buf(20, 0);
        RawPacketRecord out;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), out, ts, issues));
        (void)ts;
    }

    // Negative: payload CRC mismatch
    {
        RawPacketRecord rec;
        rec.payload = {1, 2, 3};
        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);
        buf[41] ^= 0xFF;

        RawPacketRecord out;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), out, ts, issues));
        (void)ts;
    }

    // Negative: record CRC mismatch
    {
        RawPacketRecord rec;
        rec.payload = {1, 2, 3};
        std::vector<std::uint8_t> buf;
        serialize_record(buf, rec);
        buf.back() ^= 0xFF;

        RawPacketRecord out;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), out, ts, issues));
        (void)ts;
    }

    // Max payload constant (compile-time check)
    static_assert(kMaxPayloadSize == 1024 * 1024, "kMaxPayloadSize must be 1 MiB");

    std::cout << "test_record_contract: ALL PASSED\n";
    return 0;
}
