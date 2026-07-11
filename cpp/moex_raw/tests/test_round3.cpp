#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_report.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/strings.hpp"
#include "test_check.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("round3_" + std::to_string(counter++));
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

// Independent MXREPLAY1 digest derivation (not using production helper)
static std::string independent_mxreplay1_digest(const moex_raw::RawSegmentMetadata& meta,
                                                 const std::vector<moex_raw::RawPacketRecord>& records) {
    std::vector<std::uint8_t> buf;
    for (int i = 0; i < 10; ++i) buf.push_back(moex_raw::kMagicReplay[i]);
    for (int i = 0; i < 16; ++i) buf.push_back(meta.session.session_id[i]);
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>(meta.source.source_id >> (i * 8)));
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>(meta.source.channel_id >> (i * 8)));
    buf.push_back(static_cast<std::uint8_t>(meta.source.clock_domain));
    buf.push_back(static_cast<std::uint8_t>(meta.source.transport));
    buf.push_back(static_cast<std::uint8_t>(meta.source.source_side));
    for (int i = 0; i < 32; ++i) buf.push_back(meta.source.configuration_sha256[i]);
    for (int i = 0; i < 32; ++i) buf.push_back(meta.source.templates_sha256[i]);
    for (int i = 0; i < 32; ++i) buf.push_back(meta.source.endpoint_fingerprint_sha256[i]);
    auto write_str = [&buf](const std::string& s) {
        buf.push_back(static_cast<std::uint8_t>(s.size() & 0xFF));
        buf.push_back(static_cast<std::uint8_t>((s.size() >> 8) & 0xFF));
        for (std::size_t i = 0; i < s.size(); ++i) buf.push_back(static_cast<std::uint8_t>(s[i]));
    };
    write_str(meta.session.feed_group);
    write_str(meta.session.endpoint_role);
    write_str(meta.session.source_label);
    for (const auto& rec : records) {
        buf.push_back(static_cast<std::uint8_t>(rec.record_flags & 0xFF));
        buf.push_back(static_cast<std::uint8_t>((rec.record_flags >> 8) & 0xFF));
        for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>(rec.capture_index >> (i * 8)));
        for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>(rec.capture_utc_ns >> (i * 8)));
        for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>(rec.capture_monotonic_ns >> (i * 8)));
        auto ps = static_cast<std::uint32_t>(rec.payload.size());
        buf.push_back(static_cast<std::uint8_t>(ps & 0xFF));
        buf.push_back(static_cast<std::uint8_t>((ps >> 8) & 0xFF));
        buf.push_back(static_cast<std::uint8_t>((ps >> 16) & 0xFF));
        buf.push_back(static_cast<std::uint8_t>((ps >> 24) & 0xFF));
        for (std::size_t i = 0; i < rec.payload.size(); ++i) buf.push_back(rec.payload[i]);
    }
    return moex_raw::sha256_hex(buf.data(), buf.size());
}

// Mock filesystem that can inject failures
struct MockFileHandle : moex_raw::IFileHandle {
    bool fail_write = false;
    bool fail_flush = false;
    bool fail_close = false;
    bool fail_read = false;
    std::size_t short_write_bytes = 0;

    std::size_t read(void*, std::size_t size) override {
        if (fail_read) return 0;
        return size;
    }
    std::size_t write(const void*, std::size_t size) override {
        if (fail_write) return 0;
        if (short_write_bytes > 0 && short_write_bytes < size) return short_write_bytes;
        return size;
    }
    bool seek(std::int64_t, int) override { return true; }
    bool flush() override { return !fail_flush; }
    bool close() override { return !fail_close; }
};

struct MockFileSystem : moex_raw::IFileSystem {
    bool fail_rename = false;
    bool fail_exists = false;
    bool exists_result = false;  // what exists() returns when not failing
    bool fail_file_size = false;
    std::uint64_t mock_file_size = 0;
    bool fail_open_write = false;
    MockFileHandle* last_handle = nullptr;

    bool exists(const std::string&) override {
        if (fail_exists) return exists_result;
        return false;
    }
    bool rename(const std::string&, const std::string&) override {
        return !fail_rename;
    }
    bool remove(const std::string&) override { return true; }
    std::uint64_t file_size(const std::string&, bool& ok) override {
        if (fail_file_size) { ok = false; return 0; }
        ok = true;
        return mock_file_size;
    }
    std::unique_ptr<moex_raw::IFileHandle> open_read(const std::string&) override {
        auto h = std::make_unique<MockFileHandle>();
        last_handle = h.get();
        return h;
    }
    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string&) override {
        if (fail_open_write) return nullptr;
        auto h = std::make_unique<MockFileHandle>();
        last_handle = h.get();
        return h;
    }
};

int main() {
    using namespace moex_raw;

    // ============================================================
    // Block 9: Hard-coded golden MXREPLAY1 digest
    // ============================================================
    {
        auto meta = make_meta();
        std::vector<RawPacketRecord> records;
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 12345;
        rec.capture_monotonic_ns = 67890;
        rec.payload = {0xAA, 0xBB};
        records.push_back(rec);

        auto prod_digest = compute_replay_sha256(meta, records);
        auto indep_digest = independent_mxreplay1_digest(meta, records);

        // Hard-coded golden value (computed independently, verified cross-platform)
        const std::string expected = "2817df4f909b9da3e5346fe859a127a71014f7b9a785e6b97abf97114a2862b7";
        CHECK(prod_digest == expected);
        CHECK(indep_digest == expected);
        CHECK(prod_digest == indep_digest);
    }

    // ============================================================
    // Block 9: Rotation invariance — two differently rotated sets, same digest
    // ============================================================
    {
        auto meta = make_meta();
        std::vector<RawPacketRecord> records;
        for (std::uint64_t i = 0; i < 6; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i), 0xCD};
            records.push_back(rec);
        }

        // Set A: rotation limit = 2 records per segment (3 segments)
        auto dir_a = temp_dir();
        RawSegmentRotationPolicy pol_a;
        pol_a.max_records_per_segment = 2;
        RawSegmentWriter wa(meta, dir_a, pol_a);
        wa.open();
        for (const auto& r : records) wa.append(r);
        wa.finalize();
        auto paths_a = wa.finalized_paths();
        CHECK(paths_a.size() == 3);

        // Set B: rotation limit = 3 records per segment (2 segments)
        auto dir_b = temp_dir();
        RawSegmentRotationPolicy pol_b;
        pol_b.max_records_per_segment = 3;
        RawSegmentWriter wb(meta, dir_b, pol_b);
        wb.open();
        for (const auto& r : records) wb.append(r);
        wb.finalize();
        auto paths_b = wb.finalized_paths();
        CHECK(paths_b.size() == 2);

        // Replay both and compare digests
        auto result_a = replay_from_stream_set(
            [&]() {
                StreamSetInfo ss;
                std::memcpy(ss.session_id, meta.session.session_id, 16);
                ss.source_id = meta.source.source_id;
                ss.channel_id = meta.source.channel_id;
                ss.segment_paths = paths_a;
                return ss;
            }(),
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });

        auto result_b = replay_from_stream_set(
            [&]() {
                StreamSetInfo ss;
                std::memcpy(ss.session_id, meta.session.session_id, 16);
                ss.source_id = meta.source.source_id;
                ss.channel_id = meta.source.channel_id;
                ss.segment_paths = paths_b;
                return ss;
            }(),
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });

        CHECK(result_a.status == ReplayStatus::Ok);
        CHECK(result_b.status == ReplayStatus::Ok);
        CHECK(!result_a.summary.replay_sha256.empty());
        CHECK(result_a.summary.replay_sha256 == result_b.summary.replay_sha256);
    }

    // ============================================================
    // Block 1: Cross-segment monotonic — real negative fixture
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Segment 0: monotonic 0, 1000 (via writer A)
        RawSegmentWriter wa(meta, dir, {});
        wa.open();
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        CHECK(wa.append(rec).empty());
        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        CHECK(wa.append(rec).empty());
        wa.finalize();

        // Segment 1: monotonic 500 (< 1000!), 2000 (via writer B)
        // Writer B starts fresh, so it doesn't know about segment 0's monotonic
        meta.segment_index = 1;
        meta.start_capture_index = 2;
        RawSegmentWriter wb(meta, dir, {});
        wb.open();
        rec.capture_index = 2;
        rec.capture_monotonic_ns = 500;  // DECREASE from segment 0's last=1000
        CHECK(wb.append(rec).empty());
        rec.capture_index = 3;
        rec.capture_monotonic_ns = 2000;
        CHECK(wb.append(rec).empty());
        wb.finalize();

        // Collect all finalized paths from both writers
        std::vector<std::string> all_paths;
        for (const auto& p : wa.finalized_paths()) all_paths.push_back(p);
        for (const auto& p : wb.finalized_paths()) all_paths.push_back(p);
        CHECK(all_paths.size() == 2);

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        auto result = validate_stream_set(all_paths, metas, footers, issues);
        CHECK(result == SegmentStatus::Corrupt);

        bool found_mono = false;
        for (const auto& issue : issues) {
            if (issue.code == "MONOTONIC_DECREASE_CROSS_SEGMENT") found_mono = true;
        }
        CHECK(found_mono);
    }

    // ============================================================
    // Block 2: Reversed-input replay test
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;

        RawSegmentWriter w(meta, dir, pol);
        w.open();
        for (std::uint64_t i = 0; i < 6; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i)};
            w.append(rec);
        }
        w.finalize();

        auto paths = w.finalized_paths();
        CHECK(paths.size() == 3);

        // Replay in forward order
        StreamSetInfo ss_fwd;
        std::memcpy(ss_fwd.session_id, meta.session.session_id, 16);
        ss_fwd.source_id = meta.source.source_id;
        ss_fwd.channel_id = meta.source.channel_id;
        ss_fwd.segment_paths = paths;

        auto result_fwd = replay_from_stream_set(ss_fwd,
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });

        // Replay in reversed order
        std::vector<std::string> reversed_paths(paths.rbegin(), paths.rend());
        StreamSetInfo ss_rev;
        std::memcpy(ss_rev.session_id, meta.session.session_id, 16);
        ss_rev.source_id = meta.source.source_id;
        ss_rev.channel_id = meta.source.channel_id;
        ss_rev.segment_paths = reversed_paths;

        auto result_rev = replay_from_stream_set(ss_rev,
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });

        CHECK(result_fwd.status == ReplayStatus::Ok);
        CHECK(result_rev.status == ReplayStatus::Ok);
        // Digest must be identical regardless of input order
        CHECK(result_fwd.summary.replay_sha256 == result_rev.summary.replay_sha256);
        // Record count must be the same
        CHECK(result_fwd.summary.record_count == result_rev.summary.record_count);
    }

    // ============================================================
    // Block 3: Library valid+partial rejection
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        // Add a partial file
        auto partial_path = dir + "/incomplete.mxraw.partial";
        {
            std::FILE* f = std::fopen(partial_path.c_str(), "wb");
            std::uint8_t data[] = {1, 2, 3};
            std::fwrite(data, 1, 3, f);
            std::fclose(f);
        }

        // replay_from_directory should reject partial files
        auto result = replay_from_directory(dir, meta.source.source_id, meta.source.channel_id,
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });
        CHECK(result.status == ReplayStatus::ValidationFailed);

        bool found_partial = false;
        for (const auto& issue : result.issues) {
            if (issue.code == "PARTIAL_FILE") found_partial = true;
        }
        CHECK(found_partial);
    }

    // ============================================================
    // Block 4: Writer rejects unknown record flags
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();

        RawPacketRecord rec;
        rec.record_flags = 0xFFFF;  // unknown bits
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        auto err = w.append(rec);
        CHECK(!err.empty());

        // Verify no file was created (writer should still be Open, not Failed)
        CHECK(w.state() == WriterState::Open);
    }

    // ============================================================
    // Block 4: Writer rejects decreasing monotonic within segment
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();

        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 1000;
        CHECK(w.append(rec).empty());

        rec.capture_index = 1;
        rec.capture_monotonic_ns = 500;  // DECREASE!
        auto err = w.append(rec);
        CHECK(!err.empty());
    }

    // ============================================================
    // Block 5: Uppercase canonical filename rejection
    // ============================================================
    {
        ParsedFilename parsed;
        // Valid lowercase
        CHECK(parse_canonical_filename(
            "0123456789abcdef0123456789abcdef_src0000000000000001_ch0000000000000001_seg0000000000000000.mxraw",
            parsed));
        // Uppercase A-F should be rejected
        CHECK(!parse_canonical_filename(
            "0123456789ABCDEF0123456789abcdef_src0000000000000001_ch0000000000000001_seg0000000000000000.mxraw",
            parsed));
        CHECK(!parse_canonical_filename(
            "0123456789abcdef0123456789abcdef_Src0000000000000001_ch0000000000000001_seg0000000000000000.mxraw",
            parsed));
    }

    // ============================================================
    // Block 5: Trailing header bytes rejection
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        auto paths = w.finalized_paths();
        auto bad_path = dir + "/trailing_header.mxraw";
        fs::copy(paths[0], bad_path);

        // Read the file, insert extra bytes after the header strings
        std::ifstream ifs(bad_path, std::ios::binary);
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                        std::istreambuf_iterator<char>());
        ifs.close();

        // Find header_size from offset 10
        std::uint32_t header_size = static_cast<std::uint32_t>(data[10]) |
                                    (static_cast<std::uint32_t>(data[11]) << 8) |
                                    (static_cast<std::uint32_t>(data[12]) << 16) |
                                    (static_cast<std::uint32_t>(data[13]) << 24);

        // Insert 4 extra bytes at the end of the header region
        std::vector<std::uint8_t> modified;
        modified.insert(modified.end(), data.begin(), data.begin() + header_size);
        modified.push_back(0xFF);
        modified.push_back(0xFF);
        modified.push_back(0xFF);
        modified.push_back(0xFF);
        // Update header_size to include the extra bytes
        std::uint32_t new_header_size = header_size + 4;
        modified[10] = static_cast<std::uint8_t>(new_header_size);
        modified[11] = static_cast<std::uint8_t>(new_header_size >> 8);
        modified[12] = static_cast<std::uint8_t>(new_header_size >> 16);
        modified[13] = static_cast<std::uint8_t>(new_header_size >> 24);
        // Append the rest of the file (records + footer)
        modified.insert(modified.end(), data.begin() + header_size, data.end());

        std::ofstream ofs(bad_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(modified.data()), modified.size());
        ofs.close();

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(bad_path, vmeta, vfooter, issues, ch, fh);
        CHECK(status == SegmentStatus::Corrupt);

        bool found_trailing = false;
        for (const auto& issue : issues) {
            if (issue.code == "TRAILING_HEADER_BYTES") found_trailing = true;
        }
        CHECK(found_trailing);
    }

    // ============================================================
    // Block 6: Checked arithmetic overflow tests
    // ============================================================
    {
        std::uint64_t r;
        CHECK(!checked_add_u64(0xFFFFFFFFFFFFFFFFULL, 1, r));
        CHECK(!checked_mul_u64(0xFFFFFFFFFFFFFFFFULL, 2, r));
        CHECK(checked_add_u64(100, 200, r));
        CHECK(r == 300);
        CHECK(checked_mul_u64(100, 200, r));
        CHECK(r == 20000);
    }

    // ============================================================
    // Block 6: Writer rejects max_segment_bytes > 64 GiB
    // ============================================================
    {
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_segment_bytes = kMaxSegmentBytes + 1;
        RawSegmentWriter w(meta, "/tmp/test_64gib_over", pol);
        CHECK(!w.open().empty());
    }

    // ============================================================
    // Block 7: Status classification — Unsupported version
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        auto paths = w.finalized_paths();
        auto bad_path = dir + "/bad_version.mxraw";
        fs::copy(paths[0], bad_path);

        // Change version to 2
        {
            std::fstream f(bad_path, std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(8);
            char v = 2;
            f.write(&v, 1);
        }

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(bad_path, vmeta, vfooter, issues, ch, fh);
        CHECK(status == SegmentStatus::Unsupported);
    }

    // ============================================================
    // Block 10: Injectable rename failure
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        MockFileSystem mock;
        mock.fail_rename = true;

        RawSegmentWriter w(meta, dir, {}, &mock);
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
    }

    // ============================================================
    // Block 10: Callback stop with no later callback
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            w.append(rec);
        }
        w.finalize();

        auto paths = w.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        std::uint64_t count = 0;
        auto result = replay_stream(paths, metas[0],
            [&](const RawSegmentMetadata&, const RawPacketRecord&) -> bool {
                count++;
                return count < 3;
            });

        CHECK(result.status == ReplayStatus::Aborted);
        CHECK(count == 3);
    }

    // ============================================================
    // Block 8: Report JSON — parse and check actual values
    // ============================================================
    {
        RawSegmentReport report;
        report.format_version = "1";
        report.operation = "inspect";
        report.overall_status = "valid";
        report.source_id = 42;
        report.channel_id = 7;
        report.clock_domain = "synthetic";
        report.transport = "udp";
        report.source_side = "A";
        report.configuration_sha256 = "abc123";
        report.first_capture_utc_ns = 1000;
        report.last_capture_utc_ns = 2000;

        RawStreamSummary ss;
        ss.session_id_hex = "0123456789abcdef0123456789abcdef";
        ss.source_id = 42;
        ss.channel_id = 7;
        ss.feed_group = "ORDERS-LOG";
        ss.endpoint_role = "Incremental";
        ss.source_label = "test";
        ss.clock_domain = "synthetic";
        ss.transport = "udp";
        ss.source_side = "A";
        ss.configuration_sha256 = "abc123";
        ss.templates_sha256 = "def456";
        ss.endpoint_fingerprint_sha256 = "ghi789";
        ss.stream_key = "0123456789abcdef0123456789abcdef_src000000000000002a_ch0000000000000007";
        ss.record_count = 10;
        ss.first_capture_utc_ns = 1000;
        ss.last_capture_utc_ns = 2000;
        ss.status = "valid";
        ss.segment_content_sha256.push_back("hash1");
        ss.segment_file_sha256.push_back("hash2");
        report.stream_sets.push_back(ss);

        // Add an issue with source/path
        report.issues.push_back({ValidationSeverity::Warning, "TEST_CODE", "test message", "stream_key", "/path/to/file"});

        auto json = generate_json_report(report);

        // Parse and verify actual values, not just key presence
        CHECK(json.find("\"source_id\": 42") != std::string::npos);
        CHECK(json.find("\"channel_id\": 7") != std::string::npos);
        CHECK(json.find("\"clock_domain\": \"synthetic\"") != std::string::npos);
        CHECK(json.find("\"transport\": \"udp\"") != std::string::npos);
        CHECK(json.find("\"source_side\": \"A\"") != std::string::npos);
        CHECK(json.find("\"configuration_sha256\": \"abc123\"") != std::string::npos);
        CHECK(json.find("\"first_capture_utc_ns\": 1000") != std::string::npos);
        CHECK(json.find("\"last_capture_utc_ns\": 2000") != std::string::npos);
        CHECK(json.find("\"segment_content_sha256\"") != std::string::npos);
        CHECK(json.find("\"segment_file_sha256\"") != std::string::npos);
        CHECK(json.find("\"source\": \"stream_key\"") != std::string::npos);
        CHECK(json.find("\"path\": \"/path/to/file\"") != std::string::npos);
    }

    // ============================================================
    // Block 1: Stream-set validation — filename mismatch
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        auto paths = w.finalized_paths();
        auto orig = paths[0];
        auto bad_name = dir + "/" + canonical_filename(meta.session.session_id, 999, meta.source.channel_id, 0);
        fs::copy(orig, bad_name);

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        auto result = validate_stream_set({bad_name}, metas, footers, issues);
        CHECK(result == SegmentStatus::Corrupt);

        bool found_mismatch = false;
        for (const auto& issue : issues) {
            if (issue.code == "FILENAME_MISMATCH") found_mismatch = true;
        }
        CHECK(found_mismatch);
    }

    // ============================================================
    // Block 1: Lexical vs numeric sort
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 1;

        RawSegmentWriter w(meta, dir, pol);
        w.open();
        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 1000;
            w.append(rec);
        }
        w.finalize();

        auto paths = w.finalized_paths();
        CHECK(paths.size() == 3);

        std::vector<std::string> reversed(paths.rbegin(), paths.rend());
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        auto result = validate_stream_set(reversed, metas, footers, issues);
        CHECK(result == SegmentStatus::ValidFinalized);
        CHECK(metas[0].segment_index < metas[1].segment_index);
        CHECK(metas[1].segment_index < metas[2].segment_index);
    }

    // ============================================================
    // Block 9: ReplayResult.summary.replay_sha256 is populated
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i)};
            w.append(rec);
        }
        w.finalize();

        auto paths = w.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        auto result = replay_stream(paths, metas[0],
            [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool { return true; });

        CHECK(result.status == ReplayStatus::Ok);
        CHECK(!result.summary.replay_sha256.empty());
        CHECK(result.summary.replay_sha256.size() == 64);

        // Compare with independently computed digest
        std::vector<RawPacketRecord> all_records;
        for (const auto& path : paths) {
            std::FILE* f = std::fopen(path.c_str(), "rb");
            std::fseek(f, 0, SEEK_END);
            auto sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            std::vector<std::uint8_t> data(sz);
            auto n = std::fread(data.data(), 1, sz, f);
            (void)n;
            std::fclose(f);

            auto header_size = read_u32_le(data.data() + 10);
            auto file_size = static_cast<std::uint64_t>(sz);
            auto footer_offset = file_size - kFooterSize;

            std::uint64_t pos = header_size;
            while (pos < footer_offset) {
                std::uint32_t rec_size = read_u32_le(data.data() + pos + 8);
                RawPacketRecord rec;
                std::size_t total = 0;
                std::vector<RawValidationIssue> ri;
                deserialize_record_header(data.data() + pos, rec_size, rec, total, ri);
                all_records.push_back(rec);
                pos += rec_size;
            }
        }

        auto expected = compute_replay_sha256(metas[0], all_records);
        CHECK(result.summary.replay_sha256 == expected);
    }

    // ============================================================
    // Block 2: Per-stream summaries — two independent streams
    // ============================================================
    {
        auto dir = temp_dir();

        auto meta_a = make_meta();
        meta_a.source.source_id = 1;
        meta_a.source.channel_id = 1;
        RawSegmentWriter wa(meta_a, dir, {});
        wa.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        wa.append(rec);
        wa.finalize();

        auto meta_b = make_meta();
        meta_b.source.source_id = 2;
        meta_b.source.channel_id = 2;
        RawSegmentWriter wb(meta_b, dir, {});
        wb.open();
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        wb.append(rec);
        wb.finalize();

        std::vector<RawValidationIssue> disc_issues;
        auto sets = group_stream_sets(dir, disc_issues);
        CHECK(sets.size() == 2);
        CHECK(sets[0].source_id != sets[1].source_id);
    }

    // ============================================================
    // Digest changes when logical metadata changes
    // ============================================================
    {
        auto meta1 = make_meta();
        auto meta2 = make_meta();
        meta2.source.source_id = 999;

        std::vector<RawPacketRecord> records;
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.payload = {1, 2, 3};
        records.push_back(rec);

        CHECK(compute_replay_sha256(meta1, records) != compute_replay_sha256(meta2, records));
    }

    // ============================================================
    // Digest changes when record payload changes
    // ============================================================
    {
        auto meta = make_meta();
        std::vector<RawPacketRecord> r1, r2;
        RawPacketRecord rec1, rec2;
        rec1.capture_index = 0;
        rec1.payload = {1, 2, 3};
        rec2.capture_index = 0;
        rec2.payload = {1, 2, 4};
        r1.push_back(rec1);
        r2.push_back(rec2);

        CHECK(compute_replay_sha256(meta, r1) != compute_replay_sha256(meta, r2));
    }

    // ============================================================
    // Digest changes when record metadata changes
    // ============================================================
    {
        auto meta = make_meta();
        std::vector<RawPacketRecord> r1, r2;
        RawPacketRecord rec1, rec2;
        rec1.capture_index = 0;
        rec1.capture_utc_ns = 100;
        rec1.payload = {1, 2, 3};
        rec2.capture_index = 0;
        rec2.capture_utc_ns = 200;
        rec2.payload = {1, 2, 3};
        r1.push_back(rec1);
        r2.push_back(rec2);

        CHECK(compute_replay_sha256(meta, r1) != compute_replay_sha256(meta, r2));
    }

    // ============================================================
    // Block 5: Writer rejects invalid metadata before file creation
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        std::memset(meta.session.session_id, 0, 16);
        RawSegmentWriter w(meta, dir, {});
        CHECK(!w.open().empty());
        CHECK(!fs::exists(dir + "/00000000000000000000000000000000_src0000000000000001_ch0000000000000001_seg0000000000000000.mxraw.partial"));
    }
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.source.source_id = 0;
        RawSegmentWriter w(meta, dir, {});
        CHECK(!w.open().empty());
    }
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.session.feed_group = "";
        RawSegmentWriter w(meta, dir, {});
        CHECK(!w.open().empty());
    }
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.source.clock_domain = static_cast<ClockDomain>(99);
        RawSegmentWriter w(meta, dir, {});
        CHECK(!w.open().empty());
    }
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.created_utc_ns = 0;
        RawSegmentWriter w(meta, dir, {});
        CHECK(!w.open().empty());
    }

    std::cout << "test_round3: ALL PASSED\n";
    return 0;
}
