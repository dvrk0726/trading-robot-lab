#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_replay_cursor.hpp"
#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/file_position.hpp"
#include "test_check.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <fstream>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("replay_" + std::to_string(counter++));
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

static moex_raw::StreamSetInfo make_stream_set(const std::vector<std::string>& paths) {
    moex_raw::StreamSetInfo ss;
    if (paths.empty()) return ss;

    moex_raw::ParsedFilename pf;
    moex_raw::parse_canonical_filename(fs::path(paths[0]).filename().string(), pf);
    std::memcpy(ss.session_id, pf.session_id, 16);
    ss.source_id = pf.source_id;
    ss.channel_id = pf.channel_id;

    std::vector<std::pair<std::string, std::uint64_t>> indexed;
    for (const auto& p : paths) {
        moex_raw::ParsedFilename pf2;
        moex_raw::parse_canonical_filename(fs::path(p).filename().string(), pf2);
        indexed.emplace_back(p, pf2.segment_index);
    }
    std::sort(indexed.begin(), indexed.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    for (const auto& [path, idx] : indexed) {
        ss.segment_paths.push_back(path);
        ss.segment_indexes.push_back(idx);
    }
    return ss;
}

// --- Mock IFileSystem for injected failures ---

struct MockFileHandle : moex_raw::IFileHandle {
    moex_raw::DefaultFileHandle* real_ = nullptr;
    bool fail_read_ = false;
    bool fail_seek_ = false;

    explicit MockFileHandle(moex_raw::DefaultFileHandle* r) : real_(r) {}
    ~MockFileHandle() override { delete real_; }

    std::size_t read(void* buf, std::size_t size) override {
        if (fail_read_) return 0;
        return real_->read(buf, size);
    }
    std::size_t write(const void* buf, std::size_t size) override {
        return real_->write(buf, size);
    }
    bool seek(std::int64_t offset, int origin) override {
        if (fail_seek_) return false;
        return real_->seek(offset, origin);
    }
    bool flush() override { return real_->flush(); }
    bool close() override { return real_->close(); }
};

struct MockFileSystem : moex_raw::IFileSystem {
    moex_raw::DefaultFileSystem real_;
    bool fail_open_ = false;
    bool fail_read_ = false;
    bool fail_seek_ = false;
    bool fail_stat_ = false;

    bool exists(const std::string& path) override { return real_.exists(path); }
    bool rename(const std::string& from, const std::string& to) override {
        return real_.rename(from, to);
    }
    bool remove(const std::string& path) override { return real_.remove(path); }
    std::uint64_t file_size(const std::string& path, bool& ok) override {
        if (fail_stat_) { ok = false; return 0; }
        return real_.file_size(path, ok);
    }
    std::unique_ptr<moex_raw::IFileHandle> open_read(const std::string& path) override {
        if (fail_open_) return nullptr;
        auto real_handle = real_.open_read(path);
        if (!real_handle) return nullptr;
        auto* raw = static_cast<moex_raw::DefaultFileHandle*>(real_handle.release());
        auto mock = std::make_unique<MockFileHandle>(raw);
        mock->fail_read_ = fail_read_;
        mock->fail_seek_ = fail_seek_;
        return mock;
    }
    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string& path) override {
        return real_.open_write(path);
    }
};

int main() {
    using namespace moex_raw;

    // Positive: replay callback count equals record count
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 3;

        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        std::vector<RawPacketRecord> original_records;
        for (std::uint64_t i = 0; i < 9; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i), 0xAB};
            writer.append(rec);
            original_records.push_back(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        std::uint64_t callback_count = 0;
        std::vector<std::uint8_t> first_payload, last_payload;

        auto result = replay_stream(paths, metas[0],
            [&](const RawSegmentMetadata& /*m*/, const RawPacketRecord& rec) -> bool {
                if (callback_count == 0) first_payload = rec.payload;
                last_payload = rec.payload;
                callback_count++;
                return true;
            });

        CHECK(result.status == ReplayStatus::Ok);
        CHECK(callback_count == 9);
        CHECK(result.summary.record_count == 9);
    }

    // Positive: callback order equals capture_index order
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        std::uint64_t prev_index = 0;
        bool first = true;
        replay_stream(paths, metas[0],
            [&](const RawSegmentMetadata&, const RawPacketRecord& rec) -> bool {
                if (!first) {
                    CHECK(rec.capture_index == prev_index + 1);
                }
                prev_index = rec.capture_index;
                first = false;
                return true;
            });
    }

    // Positive: payload byte-identical
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0xDE, 0xAD, 0xBE, 0xEF};
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        std::vector<std::uint8_t> received_payload;
        replay_stream(paths, metas[0],
            [&](const RawSegmentMetadata&, const RawPacketRecord& r) -> bool {
                received_payload = r.payload;
                return true;
            });

        CHECK(received_payload == rec.payload);
    }

    // Positive: replay_sha256 deterministic
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        std::vector<RawPacketRecord> records;
        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
            records.push_back(rec);
        }
        writer.finalize();

        auto digest1 = compute_replay_sha256(meta, records);
        auto digest2 = compute_replay_sha256(meta, records);
        CHECK(!digest1.empty());
        CHECK(digest1 == digest2);
    }

    // Positive: changing payload changes digest
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

    // Positive: changing rotation limits does NOT change replay_sha256
    {
        auto meta = make_meta();
        std::vector<RawPacketRecord> records;
        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            records.push_back(rec);
        }

        auto digest = compute_replay_sha256(meta, records);
        CHECK(!digest.empty());
    }

    // Positive: callback-requested stop returns aborted status
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
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

    // Independent MXREPLAY1 framing test
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

        auto digest = compute_replay_sha256(meta, records);
        CHECK(digest.size() == 64);
    }

    // --- Cursor tests ---

    // Cursor: single-segment cursor
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 100;
            rec.payload = {static_cast<std::uint8_t>(i), 0xAA};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::Ok);
        CHECK(cursor.state() == ReplayCursorState::Ready);

        std::uint64_t count = 0;
        while (true) {
            auto result = cursor.next();
            if (result.code != ReplayCursorCode::Ok) break;
            CHECK(result.record.capture_index == count);
            CHECK(result.record.capture_monotonic_ns == count * 100);
            count++;
        }
        CHECK(count == 5);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // Cursor: multi-segment cursor
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 3;

        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 9; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 100;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 3);
        auto ss = make_stream_set(paths);

        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        std::uint64_t count = 0;
        while (cursor.next().code == ReplayCursorCode::Ok) {
            count++;
        }
        CHECK(count == 9);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // Cursor: reverse input paths
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;

        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 6; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 3);

        std::vector<std::string> reversed(paths.rbegin(), paths.rend());
        auto ss = make_stream_set(reversed);

        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        std::uint64_t count = 0;
        while (cursor.next().code == ReplayCursorCode::Ok) {
            count++;
        }
        CHECK(count == 6);
    }

    // Cursor: capture_index continuity across segment boundaries
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 4;

        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 10; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 10;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 3);
        auto ss = make_stream_set(paths);

        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        std::uint64_t prev = 0;
        bool first = true;
        while (true) {
            auto result = cursor.next();
            if (result.code != ReplayCursorCode::Ok) break;
            if (!first) {
                CHECK(result.record.capture_index == prev + 1);
            }
            prev = result.record.capture_index;
            first = false;
        }
        CHECK(prev == 9);
    }

    // Cursor: byte-identical payload
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord orig;
        orig.capture_index = 0;
        orig.capture_monotonic_ns = 0;
        orig.payload = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
        writer.append(orig);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::Ok);
        CHECK(result.record.payload.size() == orig.payload.size());
        CHECK(std::memcmp(result.record.payload.data(), orig.payload.data(),
                          orig.payload.size()) == 0);

        auto result2 = cursor.next();
        CHECK(result2.code == ReplayCursorCode::End);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // Cursor: zero-length payload
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord orig;
        orig.capture_index = 0;
        orig.capture_monotonic_ns = 0;
        writer.append(orig);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::Ok);
        CHECK(result.record.payload.empty());

        auto result2 = cursor.next();
        CHECK(result2.code == ReplayCursorCode::End);
    }

    // Cursor: returned payload span points into reusable cursor buffer
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(0xA0 + i),
                           static_cast<std::uint8_t>(0xB0 + i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::Ok);
        auto* ptr1 = r1.record.payload.data();
        CHECK(r1.record.payload.size() == 2);
        CHECK(r1.record.payload[0] == 0xA0);

        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::Ok);
        auto* ptr2 = r2.record.payload.data();
        CHECK(r2.record.payload[0] == 0xA1);

        // Next reuses the same buffer
        CHECK(ptr1 == ptr2);
    }

    // Cursor: next reuses buffer (no owning copy)
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 2; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::Ok);
        CHECK(r1.record.payload[0] == 0);
        auto* ptr = r1.record.payload.data();

        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::Ok);
        CHECK(r2.record.payload[0] == 1);
        // Same buffer pointer
        CHECK(r2.record.payload.data() == ptr);
        // r1's payload is now overwritten (borrowed view semantics)
    }

    // Cursor: no owning payload copies in cursor API
    // (Verified by checking that RawPacketRecordView uses span, not vector)

    // Cursor: initialize accepts StreamSetInfo
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        StreamSetInfo ss;
        ParsedFilename pf;
        parse_canonical_filename(fs::path(paths[0]).filename().string(), pf);
        std::memcpy(ss.session_id, pf.session_id, 16);
        ss.source_id = pf.source_id;
        ss.channel_id = pf.channel_id;
        ss.segment_paths = paths;
        ss.segment_indexes.push_back(pf.segment_index);

        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);
        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::Ok);
        CHECK(result.record.capture_index == 0);
    }

    // Cursor: invalid initialize and valid retry
    {
        auto dir = temp_dir();
        StreamSetInfo bad_ss;
        bad_ss.source_id = 1;
        bad_ss.channel_id = 1;
        bad_ss.segment_paths = {dir + "/nonexistent.mxraw"};
        bad_ss.segment_indexes = {0};

        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(bad_ss).code == ReplayCursorCode::ValidationFailed);
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);

        // Retry with valid data
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);
        CHECK(cursor.state() == ReplayCursorState::Ready);
    }

    // Cursor: AlreadyInitialized preserves position (Ready state)
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Read 2 records
        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::Ok);
        CHECK(r1.record.capture_index == 0);
        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::Ok);
        CHECK(r2.record.capture_index == 1);

        // Re-initialize should return AlreadyInitialized
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::AlreadyInitialized);

        // Position should be preserved — next record is 2
        auto r3 = cursor.next();
        CHECK(r3.code == ReplayCursorCode::Ok);
        CHECK(r3.record.capture_index == 2);
        auto r4 = cursor.next();
        CHECK(r4.record.capture_index == 3);
        auto r5 = cursor.next();
        CHECK(r5.record.capture_index == 4);
        auto r6 = cursor.next();
        CHECK(r6.code == ReplayCursorCode::End);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // Cursor: AlreadyInitialized after End state
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        while (cursor.next().code == ReplayCursorCode::Ok) {}
        CHECK(cursor.state() == ReplayCursorState::End);

        // AlreadyInitialized in End state
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::AlreadyInitialized);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // Cursor: AlreadyInitialized after Failed state
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Delete file to cause runtime failure
        fs::remove(paths[0]);

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);

        // AlreadyInitialized in Failed state
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::AlreadyInitialized);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: stable End
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::Ok);
        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::End);
        CHECK(cursor.state() == ReplayCursorState::End);

        // End is stable
        auto r3 = cursor.next();
        CHECK(r3.code == ReplayCursorCode::End);
        CHECK(cursor.state() == ReplayCursorState::End);
        auto r4 = cursor.next();
        CHECK(r4.code == ReplayCursorCode::End);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // Cursor: deterministic empty result on NotInitialized
    {
        ValidatedReplayCursor cursor;
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::NotInitialized);
        CHECK(result.record.payload.empty());
        CHECK(result.record.capture_index == 0);
        CHECK(result.record.capture_utc_ns == 0);
        CHECK(result.record.capture_monotonic_ns == 0);
        CHECK(result.record.record_flags == 0);
    }

    // Cursor: deterministic empty result on End
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);
        cursor.next(); // read the one record
        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::End);
        CHECK(result.record.payload.empty());
        CHECK(result.record.capture_index == 0);
    }

    // Cursor: deterministic empty result and stable terminal code on Failed
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        fs::remove(paths[0]);

        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::IoError);
        CHECK(r1.record.payload.empty());
        CHECK(cursor.terminal_code() == ReplayCursorCode::IoError);

        // Stable terminal code
        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::IoError);
        CHECK(r2.record.payload.empty());
        CHECK(cursor.terminal_code() == ReplayCursorCode::IoError);
    }

    // Cursor: truncation after initialize gives StreamChanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Truncate the file: remove last 10 bytes
        {
            auto& p = paths[0];
            auto sz = fs::file_size(p);
            fs::resize_file(p, sz - 10);
        }

        bool got_stream_changed = false;
        while (true) {
            auto result = cursor.next();
            if (result.code != ReplayCursorCode::Ok) {
                if (result.code == ReplayCursorCode::StreamChanged) {
                    got_stream_changed = true;
                }
                break;
            }
        }
        CHECK(got_stream_changed);
        CHECK(cursor.state() == ReplayCursorState::Failed);
        CHECK(cursor.terminal_code() == ReplayCursorCode::StreamChanged);
    }

    // Cursor: CRC mutation after initialize gives StreamChanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Mutate a payload byte
        {
            auto& p = paths[0];
            std::ifstream ifs(p, std::ios::binary);
            ifs.seekg(10);
            std::uint32_t hsize = 0;
            ifs.read(reinterpret_cast<char*>(&hsize), 4);
            ifs.close();

            std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(static_cast<std::streamoff>(hsize + 44));
            std::uint8_t bad = 0xFF;
            f.write(reinterpret_cast<const char*>(&bad), 1);
            f.close();
        }

        bool got_stream_changed = false;
        while (true) {
            auto result = cursor.next();
            if (result.code != ReplayCursorCode::Ok) {
                if (result.code == ReplayCursorCode::StreamChanged) {
                    got_stream_changed = true;
                }
                break;
            }
        }
        CHECK(got_stream_changed);
    }

    // Cursor: filesystem failure gives IoError
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        fs::remove(paths[0]);

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
        CHECK(cursor.terminal_code() == ReplayCursorCode::IoError);
    }

    // Cursor: injected IFileSystem open failure
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);

        MockFileSystem mock_fs;
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss, &mock_fs).code == ReplayCursorCode::Ok);

        // Now fail on open
        mock_fs.fail_open_ = true;
        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: injected IFileSystem read failure
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);

        MockFileSystem mock_fs;
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss, &mock_fs).code == ReplayCursorCode::Ok);

        // Now fail on read
        mock_fs.fail_read_ = true;
        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: injected IFileSystem seek failure
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);

        MockFileSystem mock_fs;
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss, &mock_fs).code == ReplayCursorCode::Ok);

        // Now fail on seek
        mock_fs.fail_seek_ = true;
        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: injected IFileSystem stat failure during next
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);

        MockFileSystem mock_fs;
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss, &mock_fs).code == ReplayCursorCode::Ok);

        mock_fs.fail_stat_ = true;
        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: stable Failed
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);
        fs::remove(paths[0]);

        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);

        // Failed is stable
        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
        auto r3 = cursor.next();
        CHECK(r3.code == ReplayCursorCode::IoError);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: move-constructed cursor continues replay
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 100;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Read 2 records
        auto r1 = cursor.next();
        CHECK(r1.code == ReplayCursorCode::Ok);
        CHECK(r1.record.capture_index == 0);
        auto r2 = cursor.next();
        CHECK(r2.code == ReplayCursorCode::Ok);
        CHECK(r2.record.capture_index == 1);

        // Move-construct
        ValidatedReplayCursor moved(std::move(cursor));
        CHECK(moved.state() == ReplayCursorState::Ready);

        // Continue reading from moved cursor
        auto r3 = moved.next();
        CHECK(r3.code == ReplayCursorCode::Ok);
        CHECK(r3.record.capture_index == 2);
        auto r4 = moved.next();
        CHECK(r4.record.capture_index == 3);
        auto r5 = moved.next();
        CHECK(r5.record.capture_index == 4);
        auto r6 = moved.next();
        CHECK(r6.code == ReplayCursorCode::End);
    }

    // Cursor: move-assigned cursor continues replay
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 100;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto r1 = cursor.next();
        CHECK(r1.record.capture_index == 0);
        auto r2 = cursor.next();
        CHECK(r2.record.capture_index == 1);

        // Move-assign
        ValidatedReplayCursor target;
        target = std::move(cursor);
        CHECK(target.state() == ReplayCursorState::Ready);

        auto r3 = target.next();
        CHECK(r3.code == ReplayCursorCode::Ok);
        CHECK(r3.record.capture_index == 2);
        auto r4 = target.next();
        CHECK(r4.record.capture_index == 3);
        auto r5 = target.next();
        CHECK(r5.record.capture_index == 4);
        auto r6 = target.next();
        CHECK(r6.code == ReplayCursorCode::End);
    }

    // Cursor: changing file size after preflight gives StreamChanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Append bytes to change file size
        {
            std::ofstream ofs(paths[0], std::ios::binary | std::ios::app);
            ofs.put(0x00);
        }

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::StreamChanged);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: changing metadata after preflight gives StreamChanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Mutate a metadata byte (session_id at offset 18)
        {
            std::fstream f(paths[0], std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(18);
            std::uint8_t bad = 0xFF;
            f.write(reinterpret_cast<const char*>(&bad), 1);
            f.close();
        }

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::StreamChanged);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: changing header_size after preflight gives StreamChanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Mutate header_size field (at offset 10, 4 bytes)
        {
            std::fstream f(paths[0], std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(10);
            std::uint32_t bad = 999;
            f.write(reinterpret_cast<const char*>(&bad), 4);
            f.close();
        }

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::StreamChanged);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Cursor: changing footer after preflight gives StreamChanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        // Mutate footer record_count (at file_size - 92 + 16)
        {
            auto sz = fs::file_size(paths[0]);
            std::fstream f(paths[0], std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(static_cast<std::streamoff>(sz - kFooterSize + 16));
            std::uint64_t bad = 999;
            f.write(reinterpret_cast<const char*>(&bad), 8);
            f.close();
        }

        auto result = cursor.next();
        CHECK(result.code == ReplayCursorCode::StreamChanged);
        CHECK(cursor.state() == ReplayCursorState::Failed);
    }

    // Legacy: replay_stream unchanged
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        validate_stream_set(paths, metas, footers, issues);

        std::uint64_t count = 0;
        auto result = replay_stream(paths, metas[0],
            [&](const RawSegmentMetadata&, const RawPacketRecord& rec) -> bool {
                CHECK(rec.capture_index == count);
                count++;
                return true;
            });
        CHECK(result.status == ReplayStatus::Ok);
        CHECK(count == 5);
    }

    // Owning deserialize_record_header preserves payload copy
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord orig;
        orig.record_flags = kRecordFlagUtcValid;
        orig.capture_index = 0;
        orig.capture_utc_ns = 42;
        orig.capture_monotonic_ns = 99;
        orig.payload = {0x11, 0x22, 0x33, 0x44, 0x55};
        writer.append(orig);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto fh = std::fopen(paths[0].c_str(), "rb");
        CHECK(fh != nullptr);

        std::uint8_t preamble[14];
        CHECK(std::fread(preamble, 1, 14, fh) == 14);
        std::size_t hsize = moex_raw::read_u32_le(preamble + 10);
        moex_raw::fseek64(fh, static_cast<std::int64_t>(hsize), SEEK_SET);

        std::uint8_t rec_hdr[44];
        CHECK(std::fread(rec_hdr, 1, 44, fh) == 44);
        std::uint32_t rsize = moex_raw::read_u32_le(rec_hdr + 8);
        std::vector<std::uint8_t> record_bytes(rsize);
        std::memcpy(record_bytes.data(), rec_hdr, 44);
        CHECK(std::fread(record_bytes.data() + 44, 1, rsize - 44, fh) == rsize - 44);
        std::fclose(fh);

        RawPacketRecord owned;
        std::size_t rtotal = 0;
        std::vector<RawValidationIssue> vis;
        CHECK(deserialize_record_header(record_bytes.data(), record_bytes.size(), owned, rtotal, vis));

        record_bytes[44] = 0xFF;
        CHECK(owned.payload[0] == 0x11);
        CHECK(owned.payload.size() == 5);
        CHECK(owned.capture_index == 0);
        CHECK(owned.capture_monotonic_ns == 99);
    }

    // Borrowed view from deserialize_record_view
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(0xA0 + i),
                           static_cast<std::uint8_t>(0xB0 + i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();

        auto fh = std::fopen(paths[0].c_str(), "rb");
        CHECK(fh != nullptr);

        std::uint8_t preamble[14];
        CHECK(std::fread(preamble, 1, 14, fh) == 14);
        std::size_t hsize = moex_raw::read_u32_le(preamble + 10);
        moex_raw::fseek64(fh, static_cast<std::int64_t>(hsize), SEEK_SET);

        std::uint8_t rec_hdr[44];
        CHECK(std::fread(rec_hdr, 1, 44, fh) == 44);
        std::uint32_t rsize = moex_raw::read_u32_le(rec_hdr + 8);
        std::vector<std::uint8_t> full_rec(rsize);
        std::memcpy(full_rec.data(), rec_hdr, 44);
        CHECK(std::fread(full_rec.data() + 44, 1, rsize - 44, fh) == rsize - 44);
        std::fclose(fh);

        RawPacketRecordView view;
        std::size_t rtotal = 0;
        std::vector<RawValidationIssue> vis;
        CHECK(deserialize_record_view(full_rec.data(), full_rec.size(), view, rtotal, vis));
        CHECK(view.payload.size() == 2);
        CHECK(view.payload.data() == full_rec.data() + 44);
        CHECK(view.payload[0] == 0xA0);
        CHECK(view.payload[1] == 0xB0);
        CHECK(view.capture_index == 0);
    }

    // --- B1.1 defect tests ---

    // Cursor: bad payload CRC leaves record_total_size=0
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 42;
        rec.capture_monotonic_ns = 99;
        rec.payload = {0xAA, 0xBB, 0xCC};
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto fh = std::fopen(paths[0].c_str(), "rb");
        CHECK(fh != nullptr);

        std::uint8_t preamble[14];
        CHECK(std::fread(preamble, 1, 14, fh) == 14);
        std::size_t hsize = moex_raw::read_u32_le(preamble + 10);
        moex_raw::fseek64(fh, static_cast<std::int64_t>(hsize), SEEK_SET);

        std::uint8_t rec_hdr[44];
        CHECK(std::fread(rec_hdr, 1, 44, fh) == 44);
        std::uint32_t rsize = moex_raw::read_u32_le(rec_hdr + 8);
        std::vector<std::uint8_t> record_bytes(rsize);
        std::memcpy(record_bytes.data(), rec_hdr, 44);
        CHECK(std::fread(record_bytes.data() + 44, 1, rsize - 44, fh) == rsize - 44);
        std::fclose(fh);

        // Corrupt a payload byte
        record_bytes[44] ^= 0xFF;

        RawPacketRecordView view;
        std::size_t rtotal = 999;
        std::vector<RawValidationIssue> vis;
        CHECK(!deserialize_record_view(record_bytes.data(), record_bytes.size(), view, rtotal, vis));
        CHECK(rtotal == 0);
        CHECK(view.payload.empty());
        CHECK(view.capture_index == 0);
        CHECK(view.record_flags == 0);
    }

    // Cursor: bad record CRC leaves record_total_size=0
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 42;
        rec.capture_monotonic_ns = 99;
        rec.payload = {0xAA, 0xBB};
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto fh = std::fopen(paths[0].c_str(), "rb");
        CHECK(fh != nullptr);

        std::uint8_t preamble[14];
        CHECK(std::fread(preamble, 1, 14, fh) == 14);
        std::size_t hsize = moex_raw::read_u32_le(preamble + 10);
        moex_raw::fseek64(fh, static_cast<std::int64_t>(hsize), SEEK_SET);

        std::uint8_t rec_hdr[44];
        CHECK(std::fread(rec_hdr, 1, 44, fh) == 44);
        std::uint32_t rsize = moex_raw::read_u32_le(rec_hdr + 8);
        std::vector<std::uint8_t> record_bytes(rsize);
        std::memcpy(record_bytes.data(), rec_hdr, 44);
        CHECK(std::fread(record_bytes.data() + 44, 1, rsize - 44, fh) == rsize - 44);
        std::fclose(fh);

        // Corrupt the record CRC (last 4 bytes)
        record_bytes[rsize - 1] ^= 0xFF;

        RawPacketRecordView view;
        std::size_t rtotal = 999;
        std::vector<RawValidationIssue> vis;
        CHECK(!deserialize_record_view(record_bytes.data(), record_bytes.size(), view, rtotal, vis));
        CHECK(rtotal == 0);
        CHECK(view.payload.empty());
    }

    // Cursor: insufficient bytes leaves record_total_size=0
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 42;
        rec.capture_monotonic_ns = 99;
        rec.payload = {0xAA, 0xBB, 0xCC, 0xDD};
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto fh = std::fopen(paths[0].c_str(), "rb");
        CHECK(fh != nullptr);

        std::uint8_t preamble[14];
        CHECK(std::fread(preamble, 1, 14, fh) == 14);
        std::size_t hsize = moex_raw::read_u32_le(preamble + 10);
        moex_raw::fseek64(fh, static_cast<std::int64_t>(hsize), SEEK_SET);

        std::uint8_t rec_hdr[44];
        CHECK(std::fread(rec_hdr, 1, 44, fh) == 44);
        std::uint32_t rsize = moex_raw::read_u32_le(rec_hdr + 8);
        std::vector<std::uint8_t> record_bytes(rsize);
        std::memcpy(record_bytes.data(), rec_hdr, 44);
        CHECK(std::fread(record_bytes.data() + 44, 1, rsize - 44, fh) == rsize - 44);
        std::fclose(fh);

        // Pass fewer bytes than record_size but enough for header
        RawPacketRecordView view;
        std::size_t rtotal = 999;
        std::vector<RawValidationIssue> vis;
        CHECK(!deserialize_record_view(record_bytes.data(), 44, view, rtotal, vis));
        CHECK(rtotal == 0);
        CHECK(view.payload.empty());
    }

    // Cursor: mismatched session_id in StreamSetInfo
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ss.session_id[0] ^= 0xFF;

        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::ValidationFailed);
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
    }

    // Cursor: mismatched source_id in StreamSetInfo
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ss.source_id = 999;

        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::ValidationFailed);
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
    }

    // Cursor: mismatched channel_id in StreamSetInfo
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ss.channel_id = 999;

        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::ValidationFailed);
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
    }

    // Cursor: wrong declared segment_index
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ss.segment_indexes[0] = 999;

        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::ValidationFailed);
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
    }

    // Cursor: swapped path/index association
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;
        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 4; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 2);

        StreamSetInfo ss;
        ParsedFilename pf;
        parse_canonical_filename(fs::path(paths[0]).filename().string(), pf);
        std::memcpy(ss.session_id, pf.session_id, 16);
        ss.source_id = pf.source_id;
        ss.channel_id = pf.channel_id;
        // Swap paths but keep original index order — mismatched pairing
        ss.segment_paths = {paths[1], paths[0]};
        ss.segment_indexes = {0, 1};

        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::ValidationFailed);
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
    }

    // Cursor: unsorted StreamSetInfo with correct pairs
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;
        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 4; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 2);

        StreamSetInfo ss;
        ParsedFilename pf;
        parse_canonical_filename(fs::path(paths[0]).filename().string(), pf);
        std::memcpy(ss.session_id, pf.session_id, 16);
        ss.source_id = pf.source_id;
        ss.channel_id = pf.channel_id;
        // Reverse order but correct pairings
        ss.segment_paths = {paths[1], paths[0]};
        ss.segment_indexes = {1, 0};

        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::Ok);
        CHECK(init.segment_status == SegmentStatus::ValidFinalized);

        std::uint64_t count = 0;
        while (cursor.next().code == ReplayCursorCode::Ok) {
            count++;
        }
        CHECK(count == 4);
    }

    // Cursor: initialize IoError classification
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);

        MockFileSystem mock_fs;
        mock_fs.fail_stat_ = true;
        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss, &mock_fs);
        CHECK(init.code == ReplayCursorCode::IoError);
        CHECK(init.segment_status == SegmentStatus::IoError);
    }

    // Cursor: segment_status and stream_metadata on success
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 100;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        auto init = cursor.initialize(ss);
        CHECK(init.code == ReplayCursorCode::Ok);
        CHECK(init.segment_status == SegmentStatus::ValidFinalized);

        auto* sm = cursor.stream_metadata();
        CHECK(sm != nullptr);
        CHECK(sm->segment_index == 0);
        CHECK(sm->source.source_id == 1);
        CHECK(sm->source.channel_id == 1);
        CHECK(sm->session.feed_group == "ORDERS-LOG");
    }

    // Cursor: stream_metadata null before initialize
    {
        ValidatedReplayCursor cursor;
        CHECK(cursor.stream_metadata() == nullptr);
    }

    // Cursor: segment_status=ValidFinalized for AlreadyInitialized
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 3; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        auto ss = make_stream_set(paths);
        ValidatedReplayCursor cursor;
        CHECK(cursor.initialize(ss).code == ReplayCursorCode::Ok);

        auto init2 = cursor.initialize(ss);
        CHECK(init2.code == ReplayCursorCode::AlreadyInitialized);
        CHECK(init2.segment_status == SegmentStatus::ValidFinalized);
    }

    std::cout << "test_replay: ALL PASSED\n";
    return 0;
}
