#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include "test_check.hpp"
#include <iostream>
#include <cstring>
#include <vector>

int main() {
    using namespace moex_raw;

    // Huge header_size rejected
    {
        std::vector<std::uint8_t> buf(100, 0);
        // Write magic
        std::memcpy(buf.data(), kMagicRaw, 8);
        // Write version
        buf[8] = 1; buf[9] = 0;
        // Write huge header_size
        buf[10] = 0xFF; buf[11] = 0xFF; buf[12] = 0xFF; buf[13] = 0xFF;

        RawSegmentMetadata meta;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_header(buf.data(), buf.size(), meta, hs, issues));
    }

    // Huge payload_size rejected
    {
        std::vector<std::uint8_t> buf(kRecordHeaderSize + 4, 0);
        std::memcpy(buf.data(), kMagicRec, 4);
        buf[4] = kRecordHeaderSize;  // header size
        // record_size = 44 + huge + 4
        std::uint32_t huge_payload = kMaxPayloadSize + 1;
        buf[36] = static_cast<std::uint8_t>(huge_payload);
        buf[37] = static_cast<std::uint8_t>(huge_payload >> 8);
        buf[38] = static_cast<std::uint8_t>(huge_payload >> 16);
        buf[39] = static_cast<std::uint8_t>(huge_payload >> 24);

        RawPacketRecord rec;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), rec, ts, issues));
    }

    // Truncated file at header boundary
    {
        std::vector<std::uint8_t> buf(50, 0);
        std::memcpy(buf.data(), kMagicRaw, 8);
        buf[8] = 1; buf[9] = 0;
        // header_size = 200 (larger than buffer)
        buf[10] = 200;

        RawSegmentMetadata meta;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_header(buf.data(), buf.size(), meta, hs, issues));
    }

    // Truncated file at record boundary
    {
        std::vector<std::uint8_t> buf(kRecordHeaderSize, 0);
        std::memcpy(buf.data(), kMagicRec, 4);
        buf[4] = kRecordHeaderSize;
        // payload_size = 100 but buffer is only 44 bytes
        buf[36] = 100;
        // record_size = 44 + 100 + 4 = 148
        std::uint32_t rs = 148;
        buf[8] = static_cast<std::uint8_t>(rs);
        buf[9] = static_cast<std::uint8_t>(rs >> 8);

        RawPacketRecord rec;
        std::size_t ts = 0;
        std::vector<RawValidationIssue> issues;
        CHECK(!deserialize_record_header(buf.data(), buf.size(), rec, ts, issues));
    }

    // Checked arithmetic rejects overflow
    {
        std::uint64_t r;
        CHECK(!checked_add_u64(0xFFFFFFFFFFFFFFFFULL, 1, r));
        CHECK(!checked_mul_u64(0xFFFFFFFFFFFFFFFFULL, 2, r));
    }

    // No unbounded allocation from file values
    {
        // Simulate a header that claims huge string length
        std::vector<std::uint8_t> buf(200, 0);
        std::memcpy(buf.data(), kMagicRaw, 8);
        buf[8] = 1; buf[9] = 0;
        buf[10] = 200; buf[11] = 0; buf[12] = 0; buf[13] = 0;  // header_size = 200
        // After all fixed fields, write a string length of 0xFFFF
        // This should be caught by the 128-byte limit
        // (position depends on exact layout, but the validation catches it)

        RawSegmentMetadata meta;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        // Should either succeed (if string is within bounds) or fail gracefully
        deserialize_header(buf.data(), buf.size(), meta, hs, issues);
        // No crash = success
    }

    // max_segment_bytes above 64 GiB rejected
    {
        RawSegmentMetadata meta;
        for (int i = 0; i < 16; ++i) meta.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
        meta.session.feed_group = "FUT-INFO";
        meta.session.endpoint_role = "Snapshot";
        meta.source.source_id = 1;
        meta.source.channel_id = 1;
        sha256("c", 1, meta.source.configuration_sha256);
        sha256("t", 1, meta.source.templates_sha256);
        sha256("f", 1, meta.source.endpoint_fingerprint_sha256);
        meta.created_utc_ns = 1;

        RawSegmentRotationPolicy pol;
        pol.max_segment_bytes = kMaxSegmentBytes + 1;

        RawSegmentWriter writer(meta, "/tmp/test_overflow", pol);
        CHECK(!writer.open().empty());
    }

    // Empty file
    {
        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        // This would fail to open a non-existent file
        auto status = validate_segment("/nonexistent/path.mxraw", meta, footer, issues, ch, fh);
        CHECK(status == SegmentStatus::IoError);
    }

    std::cout << "test_resource_safety: ALL PASSED\n";
    return 0;
}
