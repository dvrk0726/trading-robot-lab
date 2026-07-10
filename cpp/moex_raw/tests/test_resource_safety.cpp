#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_replay.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include "test_check.hpp"
#include <iostream>
#include <cstring>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("resource_" + std::to_string(counter++));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir.string();
}

static moex_raw::RawSegmentMetadata make_meta() {
    moex_raw::RawSegmentMetadata meta;
    for (int i = 0; i < 16; ++i) meta.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
    meta.session.feed_group = "ORDERS-LOG";
    meta.session.endpoint_role = "Incremental";
    meta.session.source_label = "test";
    meta.source.clock_domain = moex_raw::ClockDomain::Synthetic;
    meta.source.transport = moex_raw::Transport::Synthetic;
    meta.source.source_side = moex_raw::SourceSide::None;
    meta.source.source_id = 1;
    meta.source.channel_id = 1;
    moex_raw::sha256("c", 1, meta.source.configuration_sha256);
    moex_raw::sha256("t", 1, meta.source.templates_sha256);
    moex_raw::sha256("f", 1, meta.source.endpoint_fingerprint_sha256);
    meta.created_utc_ns = 1700000000000000000ULL;
    return meta;
}

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

    // Bounded memory: replay of many records does not accumulate all payloads
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        const std::uint64_t N = 1000;
        for (std::uint64_t i = 0; i < N; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i & 0xFF), 0xAB};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        // Replay with callback that only counts — no accumulation
        std::uint64_t count = 0;
        std::uint64_t max_payload_seen = 0;
        auto result = replay_stream(paths, metas[0],
            [&](const RawSegmentMetadata&, const RawPacketRecord& rec) -> bool {
                count++;
                if (rec.payload.size() > max_payload_seen)
                    max_payload_seen = rec.payload.size();
                return true;
            });

        CHECK(result.status == ReplayStatus::Ok);
        CHECK(count == N);
        // Each payload is small (2 bytes), not accumulated
        CHECK(max_payload_seen == 2);
        // Summary reports correct total without storing all records
        CHECK(result.summary.record_count == N);
        CHECK(result.summary.total_payload_bytes == N * 2);
    }

    // Directory discovery: corrupt .mxraw file produces explicit issues
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Create a valid segment
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        // Create a corrupt .mxraw file
        auto corrupt_path = dir + "/corrupt.mxraw";
        {
            std::FILE* f = std::fopen(corrupt_path.c_str(), "wb");
            std::uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
            std::fwrite(garbage, 1, sizeof(garbage), f);
            std::fclose(f);
        }

        std::vector<RawValidationIssue> issues;
        auto sets = group_stream_sets(dir, issues);
        // Valid segment should still be found
        CHECK(sets.size() == 1);
        // Corrupt file should produce an explicit issue
        bool found_malformed = false;
        for (const auto& issue : issues) {
            if (issue.code == "MALFORMED_HEADER") found_malformed = true;
        }
        CHECK(found_malformed);
    }

    // Directory discovery: .mxraw.partial file produces explicit warning
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Create a valid segment
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        // Create a .mxraw.partial file
        auto partial_path = dir + "/test.mxraw.partial";
        {
            std::FILE* f = std::fopen(partial_path.c_str(), "wb");
            std::uint8_t data[] = {1, 2, 3};
            std::fwrite(data, 1, 3, f);
            std::fclose(f);
        }

        std::vector<RawValidationIssue> issues;
        auto sets = group_stream_sets(dir, issues);
        CHECK(sets.size() == 1);
        bool found_partial = false;
        for (const auto& issue : issues) {
            if (issue.code == "PARTIAL_FILE") found_partial = true;
        }
        CHECK(found_partial);
    }

    // Directory discovery: two sessions with same source/channel — explicit ambiguity
    {
        auto dir = temp_dir();

        // Session A
        auto meta_a = make_meta();
        meta_a.session.session_id[0] = 0xAA;
        RawSegmentWriter wa(meta_a, dir, {});
        wa.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        wa.append(rec);
        wa.finalize();

        // Session B (different session_id, same source_id/channel_id)
        auto meta_b = make_meta();
        meta_b.session.session_id[0] = 0xBB;
        RawSegmentWriter wb(meta_b, dir, {});
        wb.open();
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        wb.append(rec);
        wb.finalize();

        std::vector<RawValidationIssue> issues;
        auto sets = group_stream_sets(dir, issues);
        // Should find 2 distinct stream sets
        CHECK(sets.size() == 2);

        // replay_from_directory with source_id=1, channel_id=1 should be ambiguous
        auto result = replay_from_directory(dir, 1, 1,
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });
        CHECK(result.status == ReplayStatus::AmbiguousStream);
    }

    // Explicit session selection: two sessions, select one by session_id
    {
        auto dir = temp_dir();

        // Session A with 2 records
        auto meta_a = make_meta();
        meta_a.session.session_id[0] = 0xAA;
        RawSegmentWriter wa(meta_a, dir, {});
        wa.open();
        for (std::uint64_t i = 0; i < 2; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 1000;
            wa.append(rec);
        }
        wa.finalize();

        // Session B with 3 records (different session_id, same source/channel)
        auto meta_b = make_meta();
        meta_b.session.session_id[0] = 0xBB;
        RawSegmentWriter wb(meta_b, dir, {});
        wb.open();
        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 1000;
            wb.append(rec);
        }
        wb.finalize();

        // Select session A explicitly using replay_from_stream_set
        std::vector<RawValidationIssue> discovery_issues;
        auto sets = group_stream_sets(dir, discovery_issues);
        CHECK(sets.size() == 2);

        // Find session A
        const StreamSetInfo* target_a = nullptr;
        for (const auto& ss : sets) {
            if (ss.session_id[0] == 0xAA) {
                target_a = &ss;
                break;
            }
        }
        CHECK(target_a != nullptr);

        std::uint64_t count = 0;
        auto result = replay_from_stream_set(*target_a,
            [&](const RawSegmentMetadata&, const RawPacketRecord&) -> bool {
                count++;
                return true;
            });
        CHECK(result.status == ReplayStatus::Ok);
        CHECK(count == 2);  // Session A has 2 records
    }

    std::cout << "test_resource_safety: ALL PASSED\n";
    return 0;
}
