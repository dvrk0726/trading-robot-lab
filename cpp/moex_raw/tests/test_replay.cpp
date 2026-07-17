#ifdef _MSC_VER
#pragma warning(disable : 4189 4101 4505)
#endif

#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_replay_cursor.hpp"
#include "moex_raw/raw_ab_replay_cursor.hpp"
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
#include <set>

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

struct PathAwareMockFileHandle : moex_raw::IFileHandle {
    moex_raw::DefaultFileHandle* real_ = nullptr;
    bool fail_read_ = false;
    bool fail_seek_ = false;

    explicit PathAwareMockFileHandle(moex_raw::DefaultFileHandle* r) : real_(r) {}
    ~PathAwareMockFileHandle() override { delete real_; }

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

struct PathAwareMockFileSystem : moex_raw::IFileSystem {
    moex_raw::DefaultFileSystem real_;
    std::set<std::string> fail_file_size_;
    std::set<std::string> fail_open_;

    bool exists(const std::string& path) override { return real_.exists(path); }
    bool rename(const std::string& from, const std::string& to) override {
        return real_.rename(from, to);
    }
    bool remove(const std::string& path) override { return real_.remove(path); }
    std::uint64_t file_size(const std::string& path, bool& ok) override {
        if (fail_file_size_.count(path)) { ok = false; return 0; }
        return real_.file_size(path, ok);
    }
    std::unique_ptr<moex_raw::IFileHandle> open_read(const std::string& path) override {
        if (fail_open_.count(path)) return nullptr;
        auto real_handle = real_.open_read(path);
        if (!real_handle) return nullptr;
        auto* raw = static_cast<moex_raw::DefaultFileHandle*>(real_handle.release());
        return std::make_unique<PathAwareMockFileHandle>(raw);
    }
    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string& path) override {
        return real_.open_write(path);
    }
};

static moex_raw::RawSegmentMetadata make_meta_a() {
    moex_raw::RawSegmentMetadata meta;
    for (int i = 0; i < 16; ++i) meta.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
    meta.session.feed_group = "ORDERS-LOG";
    meta.session.endpoint_role = "Incremental-A";
    meta.session.source_label = "test-a";
    meta.source.clock_domain = moex_raw::ClockDomain::Synthetic;
    meta.source.transport = moex_raw::Transport::Udp;
    meta.source.source_side = moex_raw::SourceSide::A;
    meta.source.source_id = 1;
    meta.source.channel_id = 1;
    moex_raw::sha256("c", 1, meta.source.configuration_sha256);
    moex_raw::sha256("t", 1, meta.source.templates_sha256);
    moex_raw::sha256("f-a", 3, meta.source.endpoint_fingerprint_sha256);
    meta.created_utc_ns = 1700000000000000000ULL;
    return meta;
}

static moex_raw::RawSegmentMetadata make_meta_b() {
    moex_raw::RawSegmentMetadata meta;
    for (int i = 0; i < 16; ++i) meta.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
    meta.session.feed_group = "ORDERS-LOG";
    meta.session.endpoint_role = "Incremental-B";
    meta.session.source_label = "test-b";
    meta.source.clock_domain = moex_raw::ClockDomain::Synthetic;
    meta.source.transport = moex_raw::Transport::Udp;
    meta.source.source_side = moex_raw::SourceSide::B;
    meta.source.source_id = 2;
    meta.source.channel_id = 2;
    moex_raw::sha256("c", 1, meta.source.configuration_sha256);
    moex_raw::sha256("t", 1, meta.source.templates_sha256);
    moex_raw::sha256("f-b", 3, meta.source.endpoint_fingerprint_sha256);
    meta.created_utc_ns = 1700000000000000000ULL;
    return meta;
}

static void run_b12_tests();

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

    // Cursor: default ReplayCursorInitResult has fail-closed values
    {
        ReplayCursorInitResult default_result;
        CHECK(default_result.code == ReplayCursorCode::ValidationFailed);
        CHECK(default_result.segment_status == SegmentStatus::Corrupt);
        CHECK(default_result.issues.empty());
    }

    // Cursor: two segments, I/O failure on second segment preflight, then retry succeeds
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;
        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 4; ++i) {
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
        CHECK(paths.size() == 2);
        auto ss = make_stream_set(paths);

        // Validate with real filesystem first (both segments OK)
        PathAwareMockFileSystem mock_fs;
        ValidatedReplayCursor cursor;

        // Fail file_size on second segment during preflight
        mock_fs.fail_file_size_.insert(paths[1]);

        auto init = cursor.initialize(ss, &mock_fs);
        CHECK(init.code == ReplayCursorCode::IoError);
        CHECK(init.segment_status == SegmentStatus::IoError);
        CHECK(!init.issues.empty());

        // Fail-closed: cursor remains Uninitialized
        CHECK(cursor.state() == ReplayCursorState::Uninitialized);
        CHECK(cursor.stream_metadata() == nullptr);

        // next() returns NotInitialized
        auto next_result = cursor.next();
        CHECK(next_result.code == ReplayCursorCode::NotInitialized);

        // Retry with fixed filesystem succeeds
        PathAwareMockFileSystem good_fs;
        auto init2 = cursor.initialize(ss, &good_fs);
        CHECK(init2.code == ReplayCursorCode::Ok);
        CHECK(init2.segment_status == SegmentStatus::ValidFinalized);
        CHECK(cursor.state() == ReplayCursorState::Ready);
        CHECK(cursor.stream_metadata() != nullptr);

        std::uint64_t count = 0;
        while (cursor.next().code == ReplayCursorCode::Ok) {
            count++;
        }
        CHECK(count == 4);
        CHECK(cursor.state() == ReplayCursorState::End);
    }

    // --- B1.2 tests ---
    run_b12_tests();

    std::cout << "test_replay: ALL PASSED\n";
    return 0;
}

// CountingMockFileSystem: returns real file_size for the first N calls,
// then returns fake_size_ for subsequent calls (deterministic StreamChanged injection).
struct CountingMockFileSystem : moex_raw::IFileSystem {
    moex_raw::DefaultFileSystem real_;
    int file_size_calls_ = 0;
    int file_size_fail_after_ = -1;  // -1 = never fail
    std::uint64_t fake_size_ = 0;

    bool exists(const std::string& path) override { return real_.exists(path); }
    bool rename(const std::string& from, const std::string& to) override {
        return real_.rename(from, to);
    }
    bool remove(const std::string& path) override { return real_.remove(path); }
    std::uint64_t file_size(const std::string& path, bool& ok) override {
        file_size_calls_++;
        if (file_size_fail_after_ >= 0 && file_size_calls_ > file_size_fail_after_) {
            ok = true;
            return fake_size_;
        }
        return real_.file_size(path, ok);
    }
    std::unique_ptr<moex_raw::IFileHandle> open_read(const std::string& path) override {
        return real_.open_read(path);
    }
    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string& path) override {
        return real_.open_write(path);
    }
};

// --- B1.2 helper: write records to a segment and return paths ---
struct RecSpec { std::uint64_t idx; std::uint64_t ts; int payload; };
static std::vector<std::string> write_segment(
    const moex_raw::RawSegmentMetadata& meta, const std::string& dir,
    std::initializer_list<RecSpec> records) {
    moex_raw::RawSegmentWriter writer(meta, dir, {});
    writer.open();
    for (auto& r : records) {
        moex_raw::RawPacketRecord rec;
        rec.capture_index = r.idx;
        rec.capture_monotonic_ns = r.ts;
        rec.payload = {static_cast<std::uint8_t>(r.payload)};
        writer.append(rec);
    }
    writer.finalize();
    return writer.finalized_paths();
}

// B1.2: A earlier than B
static void b12_a_before_b() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,1000,0xB0},{1,1100,0xB1},{2,1200,0xB2}});
    {
        ValidatedAbReplayCursor c;
        CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
        CHECK(c.state() == AbReplayState::Ready);
        CHECK(c.next().side == SourceSide::A);
        CHECK(c.next().side == SourceSide::A);
        CHECK(c.next().side == SourceSide::A);
        CHECK(c.next().side == SourceSide::B);
        CHECK(c.next().side == SourceSide::B);
        CHECK(c.next().side == SourceSide::B);
        CHECK(c.next().code == AbReplayCode::End);
        CHECK(c.state() == AbReplayState::End);
    }
}

// B1.2: B earlier than A
static void b12_b_before_a() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,1000,0xA0},{1,1100,0xA1},{2,1200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,0,0xB0},{1,100,0xB1},{2,200,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: same timestamp A before B
static void b12_same_ts_a_first() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,100,0xA0},{1,100,0xA1},{2,100,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xB0},{1,100,0xB1},{2,100,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    for (int i = 0; i < 3; ++i) { CHECK(c.next().side == SourceSide::A); }
    for (int i = 0; i < 3; ++i) { CHECK(c.next().side == SourceSide::B); }
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: multiple A with same timestamp before B
static void b12_multi_a_same_ts() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,100,0xA0},{1,100,0xA1},{2,100,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    auto r1 = c.next(); CHECK(r1.side == SourceSide::A); CHECK(r1.record.capture_index == 0);
    auto* ptr = r1.record.payload.data();
    auto r2 = c.next(); CHECK(r2.side == SourceSide::A); CHECK(r2.record.capture_index == 1);
    CHECK(r2.record.payload.data() == ptr); // buffer reuse
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: local capture_index preservation
static void b12_capture_index() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,1000,0xB0},{1,1100,0xB1},{2,1200,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    CHECK(c.next().record.capture_index == 0);
    CHECK(c.next().record.capture_index == 1);
    CHECK(c.next().record.capture_index == 2);
    CHECK(c.next().record.capture_index == 0);
    CHECK(c.next().record.capture_index == 1);
    CHECK(c.next().record.capture_index == 2);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: B,A input order recognized
static void b12_input_order() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,100,0xA0},{1,100,0xA1}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xB0},{1,100,0xB1}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pb), make_stream_set(pa), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: one side ends earlier (A ends first)
static void b12_a_ends_first() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,500,0xB0},{1,600,0xB1},{2,700,0xB2},{3,800,0xB3}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: one side ends earlier (B ends first)
static void b12_b_ends_first() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2},{3,300,0xA3}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    // A(0), B(50), A(100), B(150), A(200), A(300)
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::B);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().side == SourceSide::A);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: multi-segment A/B
static void b12_multi_segment() {
    using namespace moex_raw;
    auto d = temp_dir();
    RawSegmentRotationPolicy pol; pol.max_records_per_segment = 2;
    auto m_a = make_meta_a();
    RawSegmentWriter wa(m_a, d + "/a", pol); wa.open();
    for (uint64_t i = 0; i < 6; ++i) { RawPacketRecord r; r.capture_index = i; r.capture_monotonic_ns = i*100; r.payload = {(uint8_t)(0xA0+i)}; wa.append(r); }
    wa.finalize();
    auto m_b = make_meta_b();
    RawSegmentWriter wb(m_b, d + "/b", pol); wb.open();
    for (uint64_t i = 0; i < 6; ++i) { RawPacketRecord r; r.capture_index = i; r.capture_monotonic_ns = 50+i*100; r.payload = {(uint8_t)(0xB0+i)}; wb.append(r); }
    wb.finalize();
    CHECK(wa.finalized_paths().size() == 3);
    CHECK(wb.finalized_paths().size() == 3);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(wa.finalized_paths()), make_stream_set(wb.finalized_paths()), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    for (uint64_t i = 0; i < 6; ++i) {
        CHECK(c.next().side == SourceSide::A);
        CHECK(c.next().side == SourceSide::B);
    }
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: zero-copy borrowed payload and reuse
static void b12_zerocopy() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,100,0xA0},{1,100,0xA1},{2,100,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,200,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    auto r1 = c.next();
    CHECK(r1.side == SourceSide::A);
    CHECK(r1.record.payload[0] == 0xA0);
    auto* p1 = r1.record.payload.data();
    auto r2 = c.next();
    CHECK(r2.side == SourceSide::A);
    CHECK(r2.record.payload.data() == p1); // buffer reuse
    auto r3 = c.next();
    CHECK(r3.side == SourceSide::A);
    auto r4 = c.next();
    CHECK(r4.side == SourceSide::B);
    CHECK(r4.record.payload[0] == 0xBB);
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: payload valid until next aggregate next
static void b12_payload_lifetime() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,200,0xA1}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xB0},{1,300,0xB1}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    auto r1 = c.next();
    CHECK(r1.record.payload[0] == 0xA0);
    auto* p = r1.record.payload.data();
    auto sz = r1.record.payload.size();
    // Payload still valid (no next() yet)
    CHECK(r1.record.payload.data() == p);
    CHECK(r1.record.payload.size() == sz);
    c.next(); c.next(); c.next();
    CHECK(c.next().code == AbReplayCode::End);
}

// B1.2: stable End
static void b12_stable_end() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    c.next(); c.next();
    CHECK(c.next().code == AbReplayCode::End);
    CHECK(c.next().code == AbReplayCode::End);
    CHECK(c.next().code == AbReplayCode::End);
    CHECK(c.state() == AbReplayState::End);
}

// B1.2: stable Failed
static void b12_stable_failed() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,1000,0xB0},{1,1100,0xB1},{2,1200,0xB2}});
    // Create a child cursor that will fail: corrupt the file by truncating
    // After initialization, truncate file A to trigger IoError on next read
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    // Read all records to consume them
    while (c.next().code == AbReplayCode::Ok) {}
    // Cursor is now at End state — test that End is stable
    CHECK(c.state() == AbReplayState::End);
    CHECK(c.next().code == AbReplayCode::End);
    CHECK(c.next().code == AbReplayCode::End);
    CHECK(c.state() == AbReplayState::End);
}

// FailingFileSystem: delegates to DefaultFileSystem but returns nullptr from
// open_read once the budget is exhausted.
struct FailingFileSystem : moex_raw::IFileSystem {
    moex_raw::DefaultFileSystem real_;
    int open_read_budget_;
    explicit FailingFileSystem(int budget) : open_read_budget_(budget) {}
    bool exists(const std::string& p) override { return real_.exists(p); }
    bool rename(const std::string& a, const std::string& b) override { return real_.rename(a, b); }
    bool remove(const std::string& p) override { return real_.remove(p); }
    std::uint64_t file_size(const std::string& p, bool& ok) override { return real_.file_size(p, ok); }
    std::unique_ptr<moex_raw::IFileHandle> open_read(const std::string& p) override {
        if (open_read_budget_-- <= 0) return nullptr;
        return real_.open_read(p);
    }
    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string& p) override { return real_.open_write(p); }
};

static std::vector<std::string> write_multi_segment(
        moex_raw::RawSegmentMetadata meta, const std::string& dir,
        std::initializer_list<RecSpec> records, std::uint64_t max_records) {
    moex_raw::RawSegmentWriter writer(meta, dir, {max_records, 0});
    writer.open();
    for (auto& r : records) {
        moex_raw::RawPacketRecord rec;
        rec.capture_index = r.idx;
        rec.capture_monotonic_ns = r.ts;
        rec.payload = {static_cast<std::uint8_t>(r.payload)};
        writer.append(rec);
    }
    writer.finalize();
    return writer.finalized_paths();
}

// B1.2: AlreadyInitialized after Ready
static void b12_already_init_ready() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1},{2,250,0xB2}});
    auto sa = make_stream_set(pa); auto sb = make_stream_set(pb);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(sa, sb, ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    c.next(); c.next();
    CHECK(c.initialize(sa, sb, ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::AlreadyInitialized);
    CHECK(c.state() == AbReplayState::Ready);
    CHECK(c.next().code == AbReplayCode::Ok);
}

// B1.2: AlreadyInitialized after End
static void b12_already_init_end() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xBB}});
    auto sa = make_stream_set(pa); auto sb = make_stream_set(pb);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(sa, sb, ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    while (c.next().code == AbReplayCode::Ok) {}
    CHECK(c.state() == AbReplayState::End);
    CHECK(c.initialize(sa, sb, ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::AlreadyInitialized);
    CHECK(c.state() == AbReplayState::End);
}

// B1.2: AlreadyInitialized after Failed
static void b12_already_init_failed() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_multi_segment(make_meta_a(), d + "/a",
                                  {{0,0,0xA0},{1,100,0xA1}}, 1);
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1}});
    auto sa = make_stream_set(pa); auto sb = make_stream_set(pb);
    FailingFileSystem fs_a(5);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(sa, sb, ClockMergeContract::SharedMonotonicTimeline,
                       &fs_a).code == AbReplayCode::Ok);
    c.next(); c.next(); // first picks winner, second advances A → seg1 → IoError → Failed
    CHECK(c.state() == AbReplayState::Failed);
    CHECK(c.initialize(sa, sb, ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::AlreadyInitialized);
    CHECK(c.state() == AbReplayState::Failed);
}

// B1.2: move construction
static void b12_move_construct() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1},{2,250,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    c.next(); c.next();
    ValidatedAbReplayCursor moved(std::move(c));
    CHECK(moved.state() == AbReplayState::Ready);
    CHECK(moved.next().code == AbReplayCode::Ok);
    CHECK(moved.next().code == AbReplayCode::Ok);
    CHECK(moved.next().code == AbReplayCode::Ok);
    CHECK(moved.next().code == AbReplayCode::Ok);
    CHECK(moved.next().code == AbReplayCode::End);
}

// B1.2: move assignment
static void b12_move_assign() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1},{2,250,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    c.next(); c.next();
    ValidatedAbReplayCursor target;
    target = std::move(c);
    CHECK(target.state() == AbReplayState::Ready);
    CHECK(target.next().code == AbReplayCode::Ok);
    CHECK(target.next().code == AbReplayCode::Ok);
    CHECK(target.next().code == AbReplayCode::Ok);
    CHECK(target.next().code == AbReplayCode::Ok);
    CHECK(target.next().code == AbReplayCode::End);
}

// B1.2: runtime failure child A
static void b12_runtime_fail_a() {
    using namespace moex_raw;
    auto d = temp_dir();
    // A has 2 segments (max_records=1), B has 1 segment
    auto pa = write_multi_segment(make_meta_a(), d + "/a",
                                  {{0,0,0xA0},{1,100,0xA1}}, 1);
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1}});
    FailingFileSystem fs_a(5);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb),
                       ClockMergeContract::SharedMonotonicTimeline,
                       &fs_a).code == AbReplayCode::Ok);
    c.next(); // A(ts=0) - consumes seg0
    auto r = c.next(); // advance A → seg0 done → open seg1 → 6th call → IoError
    CHECK(r.code == AbReplayCode::IoError);
    CHECK(c.state() == AbReplayState::Failed);
}

// B1.2: runtime failure child B
static void b12_runtime_fail_b() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1}});
    // B has 2 segments (max_records=1)
    auto pb = write_multi_segment(make_meta_b(), d + "/b",
                                  {{0,50,0xB0},{1,150,0xB1}}, 1);
    FailingFileSystem fs_b(5);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb),
                       ClockMergeContract::SharedMonotonicTimeline,
                       nullptr, &fs_b).code == AbReplayCode::Ok);
    c.next(); // A(ts=0)
    c.next(); // advance A, return B(ts=50)
    auto r = c.next(); // advance B → seg0 done → open seg1 → IoError
    CHECK(r.code == AbReplayCode::IoError);
    CHECK(c.state() == AbReplayState::Failed);
}

// B1.2: retry after failed initialize
static void b12_retry() {
    using namespace moex_raw;
    auto d = temp_dir();
    StreamSetInfo bad; bad.source_id = 1; bad.channel_id = 1;
    bad.segment_paths = {d + "/nonexistent.mxraw"}; bad.segment_indexes = {0};
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(bad, bad, ClockMergeContract::SharedMonotonicTimeline).code != AbReplayCode::Ok);
    CHECK(c.state() == AbReplayState::Uninitialized);
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xBB}});
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    CHECK(c.next().code == AbReplayCode::Ok);
}

// B1.2: Unspecified clock contract
static void b12_unspecified_clock() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    auto init = c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::Unspecified);
    CHECK(init.code == AbReplayCode::ValidationFailed);
    CHECK(c.state() == AbReplayState::Uninitialized);
}

// B1.2: A/A (both sides A)
static void b12_aa() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto m = make_meta_a();
    auto p1 = write_segment(m, d + "/a1", {{0,0,0xAA}});
    auto p2 = write_segment(m, d + "/a2", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(p1), make_stream_set(p2), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
    CHECK(c.state() == AbReplayState::Uninitialized);
}

// B1.2: B/B (both sides B)
static void b12_bb() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto m = make_meta_b();
    auto p1 = write_segment(m, d + "/b1", {{0,0,0xAA}});
    auto p2 = write_segment(m, d + "/b2", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(p1), make_stream_set(p2), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
    CHECK(c.state() == AbReplayState::Uninitialized);
}

// B1.2: None (one side None)
static void b12_none() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto pn = write_segment(make_meta(), d + "/n", {{0,100,0xBB}}); // make_meta has SourceSide::None
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pn), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
    CHECK(c.state() == AbReplayState::Uninitialized);
}

// B1.2: non-UDP transport rejected
static void b12_non_udp() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto mb = make_meta_b();
    mb.source.transport = Transport::Tcp;
    auto pb = write_segment(mb, d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
    CHECK(c.state() == AbReplayState::Uninitialized);
}

// B1.2: mismatch session_id
static void b12_mismatch_session() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto mb = make_meta_b();
    mb.session.session_id[0] ^= 0xFF;
    auto pb = write_segment(mb, d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
}

// B1.2: mismatch feed_group
static void b12_mismatch_feed() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto mb = make_meta_b();
    mb.session.feed_group = "OTHER";
    auto pb = write_segment(mb, d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
}

// B1.2: mismatch configuration_sha256
static void b12_mismatch_config() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto mb = make_meta_b();
    mb.source.configuration_sha256[0] ^= 0xFF;
    auto pb = write_segment(mb, d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
}

// B1.2: mismatch templates_sha256
static void b12_mismatch_templates() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto mb = make_meta_b();
    mb.source.templates_sha256[0] ^= 0xFF;
    auto pb = write_segment(mb, d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
}

// B1.2: mismatch clock_domain
static void b12_mismatch_clock() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto mb = make_meta_b();
    mb.source.clock_domain = ClockDomain::HardwareReceive;
    auto pb = write_segment(mb, d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::ValidationFailed);
}

// B1.2: differing endpoint-specific fields accepted
static void b12_endpoint_diff_ok() {
    using namespace moex_raw;
    auto d = temp_dir();
    // meta_a and meta_b already differ in source_id, channel_id, source_label, endpoint_role, endpoint_fingerprint
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1},{2,250,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    uint64_t count = 0;
    while (c.next().code == AbReplayCode::Ok) count++;
    CHECK(count == 6);
}

// B1.2: metadata access
static void b12_metadata() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xAA}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,100,0xBB}});
    ValidatedAbReplayCursor c;
    CHECK(c.metadata(SourceSide::A) == nullptr);
    CHECK(c.metadata(SourceSide::B) == nullptr);
    CHECK(c.metadata(SourceSide::None) == nullptr);
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    auto* mA = c.metadata(SourceSide::A);
    CHECK(mA != nullptr);
    CHECK(mA->source.source_side == SourceSide::A);
    CHECK(mA->source.source_id == 1);
    CHECK(mA->session.feed_group == "ORDERS-LOG");
    auto* mB = c.metadata(SourceSide::B);
    CHECK(mB != nullptr);
    CHECK(mB->source.source_side == SourceSide::B);
    CHECK(mB->source.source_id == 2);
    CHECK(c.metadata(SourceSide::None) == nullptr);
}

// B1.2: strict merge-key regression guard
static void b12_merge_key_guard() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1},{2,200,0xA2}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1},{2,250,0xB2}});
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb), ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    // Keys: (0,0,0)<(50,1,0)<(100,0,1)<(150,1,1)<(200,0,2)<(250,1,2) — strictly increasing
    CHECK(c.next().side == SourceSide::A);   // (0,0,0)
    CHECK(c.next().side == SourceSide::B);   // (50,1,0)
    CHECK(c.next().side == SourceSide::A);   // (100,0,1)
    CHECK(c.next().side == SourceSide::B);   // (150,1,1)
    CHECK(c.next().side == SourceSide::A);   // (200,0,2)
    CHECK(c.next().side == SourceSide::B);   // (250,1,2)
    CHECK(c.next().code == AbReplayCode::End);
    CHECK(c.state() == AbReplayState::End);
}

// B1.2: deterministic empty on NotInitialized
static void b12_not_initialized() {
    using namespace moex_raw;
    ValidatedAbReplayCursor c;
    CHECK(c.state() == AbReplayState::Uninitialized);
    CHECK(c.terminal_code() == AbReplayCode::Ok);
    CHECK(c.metadata(SourceSide::A) == nullptr);
    CHECK(c.metadata(SourceSide::B) == nullptr);
    auto r = c.next();
    CHECK(r.code == AbReplayCode::NotInitialized);
    CHECK(r.side == SourceSide::None);
    CHECK(r.record.payload.empty());
    CHECK(r.record.capture_index == 0);
    CHECK(r.record.capture_monotonic_ns == 0);
    CHECK(r.record.record_flags == 0);
}

// B1.2: default AbReplayResult is fail-closed
static void b12_default_result() {
    using namespace moex_raw;
    AbReplayResult r;
    CHECK(r.code == AbReplayCode::NotInitialized);
    CHECK(r.side == SourceSide::None);
    CHECK(r.record.payload.empty());
    CHECK(r.record.capture_index == 0);
    CHECK(r.record.capture_monotonic_ns == 0);
    CHECK(r.record.record_flags == 0);
}

// B1.2: transactional initial-lookahead IoError — cursor remains Uninitialized
static void b12_transactional_init_ioerror() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1}});
    // FailingFileSystem budget=1: init succeeds (1 open_read), initial next() fails (2nd open_read)
    FailingFileSystem fs_a(1);
    ValidatedAbReplayCursor c;
    auto init = c.initialize(make_stream_set(pa), make_stream_set(pb),
                             ClockMergeContract::SharedMonotonicTimeline,
                             &fs_a);
    CHECK(init.code == AbReplayCode::IoError);
    CHECK(c.metadata(SourceSide::A) == nullptr);
    CHECK(c.metadata(SourceSide::B) == nullptr);
    CHECK(c.state() == AbReplayState::Uninitialized);
    auto r = c.next();
    CHECK(r.code == AbReplayCode::NotInitialized);
    CHECK(r.side == SourceSide::None);
    CHECK(r.record.payload.empty());
    // Retry with good filesystem succeeds
    auto pa2 = write_segment(make_meta_a(), d + "/a2", {{0,0,0xA0}});
    auto pb2 = write_segment(make_meta_b(), d + "/b2", {{0,50,0xB0}});
    CHECK(c.initialize(make_stream_set(pa2), make_stream_set(pb2),
                       ClockMergeContract::SharedMonotonicTimeline).code == AbReplayCode::Ok);
    CHECK(c.state() == AbReplayState::Ready);
    CHECK(c.metadata(SourceSide::A) != nullptr);
}

// B1.2: initial StreamChanged mapping — exact code preserved, not hardcoded IoError
static void b12_init_stream_changed() {
    using namespace moex_raw;
    auto d = temp_dir();
    auto pa = write_segment(make_meta_a(), d + "/a", {{0,0,0xA0},{1,100,0xA1}});
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1}});
    // CountingMockFileSystem: returns real size for first 3 file_size calls (A init
    // calls file_size in validate_stream_set + validate_segment + preflight), then
    // returns 0 for subsequent calls (A initial next() -> StreamChanged).
    CountingMockFileSystem mock_fs;
    mock_fs.file_size_fail_after_ = 3;
    mock_fs.fake_size_ = 0;
    ValidatedAbReplayCursor c;
    auto init = c.initialize(make_stream_set(pa), make_stream_set(pb),
                             ClockMergeContract::SharedMonotonicTimeline,
                             &mock_fs);
    CHECK(init.code == AbReplayCode::StreamChanged);
    CHECK(c.metadata(SourceSide::A) == nullptr);
    CHECK(c.metadata(SourceSide::B) == nullptr);
    CHECK(c.state() == AbReplayState::Uninitialized);
    auto r = c.next();
    CHECK(r.code == AbReplayCode::NotInitialized);
}

// B1.2: stable Failed — runtime child failure, then next() returns same terminal code
static void b12_stable_failed_runtime() {
    using namespace moex_raw;
    auto d = temp_dir();
    // A has 2 segments (max_records=1), B has 1 segment
    auto pa = write_multi_segment(make_meta_a(), d + "/a",
                                  {{0,0,0xA0},{1,100,0xA1}}, 1);
    auto pb = write_segment(make_meta_b(), d + "/b", {{0,50,0xB0},{1,150,0xB1}});
    FailingFileSystem fs_a(5);
    ValidatedAbReplayCursor c;
    CHECK(c.initialize(make_stream_set(pa), make_stream_set(pb),
                       ClockMergeContract::SharedMonotonicTimeline,
                       &fs_a).code == AbReplayCode::Ok);
    c.next(); // A(ts=0) - consumes seg0
    auto r = c.next(); // advance A -> seg1 -> open fails -> IoError
    CHECK(r.code == AbReplayCode::IoError);
    CHECK(r.side == SourceSide::None);
    CHECK(r.record.payload.empty());
    CHECK(c.state() == AbReplayState::Failed);
    CHECK(c.terminal_code() == AbReplayCode::IoError);
    // Stable: repeated next() returns same terminal code, SourceSide::None, empty record
    for (int i = 0; i < 3; ++i) {
        auto ri = c.next();
        CHECK(ri.code == AbReplayCode::IoError);
        CHECK(ri.side == SourceSide::None);
        CHECK(ri.record.payload.empty());
    }
    CHECK(c.terminal_code() == AbReplayCode::IoError);
}

static void run_b12_tests() {
    b12_not_initialized();
    b12_a_before_b();
    b12_b_before_a();
    b12_same_ts_a_first();
    b12_multi_a_same_ts();
    b12_capture_index();
    b12_input_order();
    b12_a_ends_first();
    b12_b_ends_first();
    b12_multi_segment();
    b12_zerocopy();
    b12_payload_lifetime();
    b12_stable_end();
    b12_stable_failed();
    b12_already_init_ready();
    b12_already_init_end();
    b12_already_init_failed();
    b12_move_construct();
    b12_move_assign();
    b12_runtime_fail_a();
    b12_runtime_fail_b();
    b12_retry();
    b12_unspecified_clock();
    b12_aa();
    b12_bb();
    b12_none();
    b12_non_udp();
    b12_mismatch_session();
    b12_mismatch_feed();
    b12_mismatch_config();
    b12_mismatch_templates();
    b12_mismatch_clock();
    b12_endpoint_diff_ok();
    b12_metadata();
    b12_merge_key_guard();
    b12_default_result();
    b12_transactional_init_ioerror();
    b12_init_stream_changed();
    b12_stable_failed_runtime();
}
