#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/sha256.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

static moex_raw::RawSegmentMetadata make_test_meta() {
    moex_raw::RawSegmentMetadata meta;
    // Non-zero session ID
    for (int i = 0; i < 16; ++i) meta.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
    meta.session.feed_group = "ORDERS-LOG";
    meta.session.endpoint_role = "Incremental";
    meta.session.source_label = "test-label";

    meta.source.clock_domain = moex_raw::ClockDomain::Synthetic;
    meta.source.transport = moex_raw::Transport::Synthetic;
    meta.source.source_side = moex_raw::SourceSide::None;
    meta.source.source_id = 42;
    meta.source.channel_id = 7;

    // Non-zero SHA-256 values
    moex_raw::sha256("config", 6, meta.source.configuration_sha256);
    moex_raw::sha256("templates", 9, meta.source.templates_sha256);
    moex_raw::sha256("fingerprint", 11, meta.source.endpoint_fingerprint_sha256);

    meta.segment_index = 0;
    meta.start_capture_index = 0;
    meta.created_utc_ns = 1700000000000000000ULL;
    return meta;
}

int main() {
    using namespace moex_raw;

    // Positive: valid header serialization
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        // Magic
        assert(buf.size() >= 8);
        assert(std::memcmp(buf.data(), kMagicRaw, 8) == 0);

        // Version
        assert(read_u16_le(buf.data() + 8) == 1);

        // Header size
        std::uint32_t header_size = read_u32_le(buf.data() + 10);
        assert(header_size == buf.size());
        assert(header_size <= kMaxHeaderSize);

        // Format flags
        assert(read_u32_le(buf.data() + 14) == 0);
    }

    // Positive: deserialize round-trip
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t header_size = 0;
        std::vector<RawValidationIssue> issues;
        assert(deserialize_header(buf.data(), buf.size(), out, header_size, issues));

        assert(std::memcmp(out.session.session_id, meta.session.session_id, 16) == 0);
        assert(out.segment_index == meta.segment_index);
        assert(out.start_capture_index == meta.start_capture_index);
        assert(out.created_utc_ns == meta.created_utc_ns);
        assert(out.source.clock_domain == meta.source.clock_domain);
        assert(out.source.transport == meta.source.transport);
        assert(out.source.source_side == meta.source.source_side);
        assert(out.source.source_id == meta.source.source_id);
        assert(out.source.channel_id == meta.source.channel_id);
        assert(std::memcmp(out.source.configuration_sha256, meta.source.configuration_sha256, 32) == 0);
        assert(out.session.feed_group == meta.session.feed_group);
        assert(out.session.endpoint_role == meta.session.endpoint_role);
        assert(out.session.source_label == meta.session.source_label);
    }

    // Positive: repeated serialization is byte-identical
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf1, buf2;
        serialize_header(buf1, meta);
        serialize_header(buf2, meta);
        assert(buf1 == buf2);
    }

    // Negative: all-zero session ID
    {
        auto meta = make_test_meta();
        std::memset(meta.session.session_id, 0, 16);
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: zero created_utc_ns
    {
        auto meta = make_test_meta();
        meta.created_utc_ns = 0;
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: zero source_id
    {
        auto meta = make_test_meta();
        meta.source.source_id = 0;
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: zero channel_id
    {
        auto meta = make_test_meta();
        meta.source.channel_id = 0;
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: zero SHA-256
    {
        auto meta = make_test_meta();
        std::memset(meta.source.configuration_sha256, 0, 32);
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: empty feed_group
    {
        auto meta = make_test_meta();
        meta.session.feed_group = "";
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: wrong magic
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);
        buf[0] = 'X';

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: wrong version
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);
        buf[8] = 2;  // version = 2

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: unsupported enum
    {
        auto meta = make_test_meta();
        meta.source.clock_domain = static_cast<ClockDomain>(99);
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Negative: non-zero reserved byte
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);
        // Reserved byte is at offset 18+16+8+8+8+2 = 60 (after session_id, seg, start, created, clock, transport)
        buf[60 + 2] = 1;  // reserved byte

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), buf.size(), out, hs, issues));
    }

    // Truncated header
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);

        RawSegmentMetadata out;
        std::size_t hs = 0;
        std::vector<RawValidationIssue> issues;
        assert(!deserialize_header(buf.data(), 10, out, hs, issues));
    }

    // Header SHA-256 deterministic cross-platform check
    {
        auto meta = make_test_meta();
        std::vector<std::uint8_t> buf;
        serialize_header(buf, meta);
        std::string hex = sha256_hex(buf.data(), buf.size());
        assert(!hex.empty());
        assert(hex.size() == 64);

        // Same metadata gives same hash
        std::vector<std::uint8_t> buf2;
        serialize_header(buf2, meta);
        assert(sha256_hex(buf2.data(), buf2.size()) == hex);
    }

    std::cout << "test_header_contract: ALL PASSED\n";
    return 0;
}
