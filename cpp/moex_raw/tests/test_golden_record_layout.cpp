#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "test_check.hpp"
#include <iostream>
#include <cstring>
#include <vector>

// Independent exact-byte golden-vector test for record layout.
// Verifies that capture_index, capture_utc_ns, capture_monotonic_ns
// are at the exact byte offsets specified by the v1 contract.

int main() {
    using namespace moex_raw;

    // Build a record with known values
    RawPacketRecord rec;
    rec.record_flags = kRecordFlagUtcValid;  // 0x0001
    rec.capture_index = 0x0102030405060708ULL;
    rec.capture_utc_ns = 0x1112131415161718ULL;
    rec.capture_monotonic_ns = 0x2122232425262728ULL;
    rec.payload = {0xAA, 0xBB, 0xCC, 0xDD};

    std::vector<std::uint8_t> buf;
    serialize_record(buf, rec);

    // Expected layout:
    // Offset 0-3:   magic "REC1"
    // Offset 4-5:   record_header_size (44) = 0x2C, 0x00
    // Offset 6-7:   record_flags (0x0001) = 0x01, 0x00
    // Offset 8-11:  record_size (44 + 4 + 4 = 52) = 0x34, 0x00, 0x00, 0x00
    // Offset 12-19: capture_index = 0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01
    // Offset 20-27: capture_utc_ns = 0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11
    // Offset 28-35: capture_monotonic_ns = 0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21
    // Offset 36-39: payload_size (4) = 0x04, 0x00, 0x00, 0x00
    // Offset 40-43: payload_crc32c
    // Offset 44-47: payload bytes
    // Offset 48-51: record_crc32c

    CHECK(buf.size() == 52);  // 44 + 4 + 4

    // Magic
    CHECK(buf[0] == 'R');
    CHECK(buf[1] == 'E');
    CHECK(buf[2] == 'C');
    CHECK(buf[3] == '1');

    // record_header_size = 44
    CHECK(read_u16_le(buf.data() + 4) == kRecordHeaderSize);

    // record_flags = 0x0001
    CHECK(read_u16_le(buf.data() + 6) == 0x0001);

    // record_size = 52
    CHECK(read_u32_le(buf.data() + 8) == 52);

    // capture_index at offset 12 (NOT 16)
    CHECK(buf[12] == 0x08);
    CHECK(buf[13] == 0x07);
    CHECK(buf[14] == 0x06);
    CHECK(buf[15] == 0x05);
    CHECK(buf[16] == 0x04);
    CHECK(buf[17] == 0x03);
    CHECK(buf[18] == 0x02);
    CHECK(buf[19] == 0x01);
    CHECK(read_u64_le(buf.data() + 12) == 0x0102030405060708ULL);

    // capture_utc_ns at offset 20 (NOT 24)
    CHECK(buf[20] == 0x18);
    CHECK(buf[21] == 0x17);
    CHECK(buf[22] == 0x16);
    CHECK(buf[23] == 0x15);
    CHECK(buf[24] == 0x14);
    CHECK(buf[25] == 0x13);
    CHECK(buf[26] == 0x12);
    CHECK(buf[27] == 0x11);
    CHECK(read_u64_le(buf.data() + 20) == 0x1112131415161718ULL);

    // capture_monotonic_ns at offset 28 (NOT 32)
    CHECK(buf[28] == 0x28);
    CHECK(buf[29] == 0x27);
    CHECK(buf[30] == 0x26);
    CHECK(buf[31] == 0x25);
    CHECK(buf[32] == 0x24);
    CHECK(buf[33] == 0x23);
    CHECK(buf[34] == 0x22);
    CHECK(buf[35] == 0x21);
    CHECK(read_u64_le(buf.data() + 28) == 0x2122232425262728ULL);

    // payload_size at offset 36
    CHECK(read_u32_le(buf.data() + 36) == 4);

    // payload_crc32c at offset 40
    std::uint32_t expected_payload_crc = crc32c(rec.payload.data(), rec.payload.size());
    CHECK(read_u32_le(buf.data() + 40) == expected_payload_crc);

    // payload bytes at offset 44
    CHECK(buf[44] == 0xAA);
    CHECK(buf[45] == 0xBB);
    CHECK(buf[46] == 0xCC);
    CHECK(buf[47] == 0xDD);

    // record_crc32c at offset 48 (over bytes 0-47)
    std::uint32_t expected_record_crc = crc32c(buf.data(), 48);
    CHECK(read_u32_le(buf.data() + 48) == expected_record_crc);

    // Now verify deserialization reads from the correct offsets
    RawPacketRecord out;
    std::size_t total_size = 0;
    std::vector<RawValidationIssue> issues;
    CHECK(deserialize_record_header(buf.data(), buf.size(), out, total_size, issues));
    CHECK(out.capture_index == 0x0102030405060708ULL);
    CHECK(out.capture_utc_ns == 0x1112131415161718ULL);
    CHECK(out.capture_monotonic_ns == 0x2122232425262728ULL);
    CHECK(out.record_flags == kRecordFlagUtcValid);
    CHECK(out.payload.size() == 4);
    CHECK(out.payload[0] == 0xAA);
    CHECK(out.payload[1] == 0xBB);
    CHECK(out.payload[2] == 0xCC);
    CHECK(out.payload[3] == 0xDD);
    CHECK(total_size == 52);

    // Cross-check: if offsets were wrong (16/24/32), the values would be different
    CHECK(read_u64_le(buf.data() + 16) != 0x0102030405060708ULL);
    CHECK(read_u64_le(buf.data() + 24) != 0x1112131415161718ULL);
    CHECK(read_u64_le(buf.data() + 32) != 0x2122232425262728ULL);

    std::cout << "test_golden_record_layout: ALL PASSED\n";
    return 0;
}
