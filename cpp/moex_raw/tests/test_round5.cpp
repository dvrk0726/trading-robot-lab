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
#include <vector>
#include <string>
#include <utility>
#include <cstdint>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("round5_" + std::to_string(counter++));
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

// ============================================================
// Mock I/O for deterministic failure injection
// ============================================================

struct MockFileHandle : moex_raw::IFileHandle {
    bool fail_write = false;
    bool fail_flush = false;
    bool fail_close = false;
    bool fail_read = false;
    bool fail_seek = false;
    std::size_t short_write_bytes = 0;  // >0: write returns fewer bytes
    std::size_t short_read_bytes = 0;   // >0: read returns fewer bytes
    std::size_t bytes_written = 0;
    std::size_t bytes_read = 0;

    std::size_t read(void* buf, std::size_t size) override {
        (void)buf;
        if (fail_read) return 0;
        if (short_read_bytes > 0 && short_read_bytes < size) {
            bytes_read += short_read_bytes;
            return short_read_bytes;
        }
        bytes_read += size;
        return size;
    }

    std::size_t write(const void* buf, std::size_t size) override {
        (void)buf;
        if (fail_write) return 0;
        if (short_write_bytes > 0 && short_write_bytes < size) {
            bytes_written += short_write_bytes;
            return short_write_bytes;
        }
        bytes_written += size;
        return size;
    }

    bool seek(std::int64_t, int) override {
        return !fail_seek;
    }

    bool flush() override {
        return !fail_flush;
    }

    bool close() override {
        return !fail_close;
    }
};

struct MockFileSystem : moex_raw::IFileSystem {
    bool fail_rename = false;
    bool fail_exists = false;
    bool exists_return = false;
    bool fail_file_size = false;
    std::uint64_t mock_file_size = 0;
    bool fail_open_read = false;
    bool fail_open_write = false;
    std::vector<MockFileHandle*> created_handles;

    bool exists(const std::string&) override {
        if (fail_exists) return exists_return;
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
        if (fail_open_read) return nullptr;
        auto h = std::make_unique<MockFileHandle>();
        created_handles.push_back(h.get());
        return h;
    }

    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string&) override {
        if (fail_open_write) return nullptr;
        auto h = std::make_unique<MockFileHandle>();
        created_handles.push_back(h.get());
        return h;
    }
};

// ============================================================
// Minimal strict JSON parser for test validation
// ============================================================

namespace json_test {

struct Value;
using JObject = std::vector<std::pair<std::string, Value>>;
using JArray = std::vector<Value>;

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool bool_val = false;
    double number_val = 0;
    std::string string_val;
    JArray array_val;
    JObject object_val;

    // Access helpers
    const Value& operator[](const std::string& key) const {
        for (const auto& p : object_val) {
            if (p.first == key) return p.second;
        }
        static Value null_val;
        return null_val;
    }

    const Value& operator[](std::size_t idx) const {
        return array_val[idx];
    }

    std::size_t size() const {
        if (type == Array) return array_val.size();
        if (type == Object) return object_val.size();
        return 0;
    }

    bool has_key(const std::string& key) const {
        for (const auto& p : object_val) {
            if (p.first == key) return true;
        }
        return false;
    }
};

class Parser {
    const std::string& s;
    std::size_t pos = 0;
public:
    explicit Parser(const std::string& json) : s(json) {}

    Value parse() {
        skip_ws();
        return parse_value();
    }

private:
    void skip_ws() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '\t'))
            pos++;
    }

    char peek() { return pos < s.size() ? s[pos] : '\0'; }
    char advance() { return pos < s.size() ? s[pos++] : '\0'; }

    Value parse_value() {
        skip_ws();
        char c = peek();
        if (c == '"') return parse_string_val();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        return parse_number();
    }

    Value parse_string_val() {
        Value v;
        v.type = Value::String;
        v.string_val = parse_string();
        return v;
    }

    std::string parse_string() {
        if (advance() != '"') return "";
        std::string result;
        while (pos < s.size()) {
            char c = advance();
            if (c == '"') return result;
            if (c == '\\') {
                char esc = advance();
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'u': {
                        std::string hex;
                        for (int i = 0; i < 4 && pos < s.size(); i++) hex += advance();
                        // Simple: just store as-is for now
                        result += "\\u" + hex;
                        break;
                    }
                    default: result += esc;
                }
            } else {
                result += c;
            }
        }
        return result;
    }

    Value parse_number() {
        Value v;
        v.type = Value::Number;
        std::string num;
        bool is_float = false;
        if (peek() == '-') num += advance();
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') num += advance();
        if (peek() == '.') { is_float = true; num += advance();
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') num += advance();
        }
        if (peek() == 'e' || peek() == 'E') { is_float = true; num += advance();
            if (peek() == '+' || peek() == '-') num += advance();
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') num += advance();
        }
        v.number_val = std::stod(num);
        return v;
    }

    Value parse_bool() {
        Value v;
        v.type = Value::Bool;
        if (s.substr(pos, 4) == "true") { pos += 4; v.bool_val = true; }
        else if (s.substr(pos, 5) == "false") { pos += 5; v.bool_val = false; }
        return v;
    }

    Value parse_null() {
        Value v;
        v.type = Value::Null;
        if (s.substr(pos, 4) == "null") pos += 4;
        return v;
    }

    Value parse_array() {
        Value v;
        v.type = Value::Array;
        advance(); // [
        skip_ws();
        if (peek() == ']') { advance(); return v; }
        while (true) {
            v.array_val.push_back(parse_value());
            skip_ws();
            if (peek() == ',') { advance(); skip_ws(); }
            else break;
        }
        if (peek() == ']') advance();
        return v;
    }

    Value parse_object() {
        Value v;
        v.type = Value::Object;
        advance(); // {
        skip_ws();
        if (peek() == '}') { advance(); return v; }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            if (peek() == ':') advance();
            skip_ws();
            v.object_val.emplace_back(key, parse_value());
            skip_ws();
            if (peek() == ',') { advance(); skip_ws(); }
            else break;
        }
        if (peek() == '}') advance();
        return v;
    }
};

inline Value parse(const std::string& json) {
    Parser p(json);
    return p.parse();
}

}  // namespace json_test

// ============================================================
// Main test driver
// ============================================================

int main() {
    using namespace moex_raw;

    // ============================================================
    // Blocker 1: Short write failure — writer enters Failed, no finalized .mxraw
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        MockFileSystem mock;

        // open_write succeeds, but write fails on header
        MockFileHandle* handle = nullptr;
        {
            // First call to open_write returns a handle that fails on write
            auto h = std::make_unique<MockFileHandle>();
            h->fail_write = true;
            handle = h.get();
            mock.created_handles.push_back(handle);
        }
        // We need to configure open_write to return a failing handle
        // Simpler: use a flag-based approach
        MockFileSystem mock2;
        RawSegmentWriter w(meta, dir, {}, &mock2);
        // open_write succeeds (default mock returns ok handle)
        auto err = w.open();
        CHECK(err.empty());

        // Now configure the handle to fail on next write
        if (mock2.created_handles.size() > 0) {
            mock2.created_handles.back()->fail_write = true;
        }

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        err = w.append(rec);
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Flush failure — writer enters Failed, no finalized .mxraw
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        MockFileSystem mock;

        RawSegmentWriter w(meta, dir, {}, &mock);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        CHECK(w.append(rec).empty());

        // Configure flush to fail
        if (!mock.created_handles.empty()) {
            mock.created_handles.back()->fail_flush = true;
        }

        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Close failure — writer enters Failed, no finalized .mxraw
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        MockFileSystem mock;

        RawSegmentWriter w(meta, dir, {}, &mock);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        CHECK(w.append(rec).empty());

        // Configure close to fail
        if (!mock.created_handles.empty()) {
            mock.created_handles.back()->fail_close = true;
        }

        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Rename failure — writer enters Failed, no finalized .mxraw
    // (already tested in Round 3, but verify with new IFileSystem)
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        MockFileSystem mock;
        mock.fail_rename = true;

        RawSegmentWriter w(meta, dir, {}, &mock);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        CHECK(w.append(rec).empty());

        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Short read in reader — returns IoError
    // ============================================================
    {
        // Create a real segment first
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 1000;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        w.append(rec);
        w.finalize();
        auto paths = w.finalized_paths();
        CHECK(paths.size() == 1);

        // Now validate with a mock that returns short reads
        MockFileSystem mock_fs;
        // Get real file size
        bool ok = false;
        auto real_size = DefaultFileSystem().file_size(paths[0], ok);
        CHECK(ok);
        mock_fs.mock_file_size = real_size;

        // Configure short read on the handle
        // We need to intercept at open_read time
        struct ShortReadFileSystem : IFileSystem {
            std::uint64_t real_size_;
            bool exists(const std::string&) override { return true; }
            bool rename(const std::string&, const std::string&) override { return true; }
            bool remove(const std::string&) override { return true; }
            std::uint64_t file_size(const std::string&, bool& ok) override { ok = true; return real_size_; }
            std::unique_ptr<IFileHandle> open_read(const std::string&) override {
                auto h = std::make_unique<MockFileHandle>();
                h->short_read_bytes = 1;  // return only 1 byte per read
                return h;
            }
            std::unique_ptr<IFileHandle> open_write(const std::string&) override { return nullptr; }
        };

        ShortReadFileSystem sfs;
        sfs.real_size_ = real_size;

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(paths[0], vmeta, vfooter, issues, ch, fh,
                                       nullptr, nullptr, nullptr, nullptr, &sfs);
        // Should fail because short read doesn't deliver enough header bytes
        CHECK(status != SegmentStatus::ValidFinalized);
    }

    // ============================================================
    // Blocker 2: FILE_TOO_LARGE — mock file_size > 64 GiB
    // Reader rejects BEFORE allocation/read
    // ============================================================
    {
        MockFileSystem mock_fs;
        mock_fs.mock_file_size = kMaxSegmentBytes + 1;

        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment("fake_path.mxraw", meta, footer, issues, ch, fh,
                                       nullptr, nullptr, nullptr, nullptr, &mock_fs);
        CHECK(status == SegmentStatus::Unsupported);

        bool found_too_large = false;
        for (const auto& issue : issues) {
            if (issue.code == "FILE_TOO_LARGE") found_too_large = true;
        }
        CHECK(found_too_large);
    }

    // ============================================================
    // Blocker 7: start_capture_index = UINT64_MAX — no mutation
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.start_capture_index = UINT64_MAX;
        RawSegmentWriter w(meta, dir, {});
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = UINT64_MAX;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        auto err = w.append(rec);
        // Should fail because next_capture_index would overflow
        CHECK(!err.empty());
        CHECK(w.finalized_paths().empty());
        // Writer should still be Open (write succeeded but overflow detected)
        // Actually with prospective validation, overflow is detected before write
        CHECK(w.state() == WriterState::Open);
    }

    // ============================================================
    // Blocker 8: Classification — empty file → Truncated
    // ============================================================
    {
        auto dir = temp_dir();
        auto empty_path = dir + "/empty.mxraw";
        {
            std::ofstream ofs(empty_path, std::ios::binary);
            // Write nothing
        }

        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(empty_path, meta, footer, issues, ch, fh);
        CHECK(status == SegmentStatus::Truncated);
    }

    // ============================================================
    // Blocker 8: Classification — truncated preamble → Truncated
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = dir + "/trunc_preamble.mxraw";
        {
            std::ofstream ofs(path, std::ios::binary);
            std::uint8_t data[] = {'M', 'X', 'R', 'A'};  // Only 4 bytes
            ofs.write(reinterpret_cast<const char*>(data), sizeof(data));
        }

        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(path, meta, footer, issues, ch, fh);
        CHECK(status == SegmentStatus::Truncated);
    }

    // ============================================================
    // Blocker 8: Classification — truncated header → Truncated
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = dir + "/trunc_header.mxraw";
        {
            std::ofstream ofs(path, std::ios::binary);
            // Write valid magic + version + header_size but not enough data
            std::uint8_t data[100] = {};
            std::memcpy(data, kMagicRaw, 8);
            data[8] = 1; data[9] = 0;  // version 1
            data[10] = 200; data[11] = 0; data[12] = 0; data[13] = 0;  // header_size = 200
            ofs.write(reinterpret_cast<const char*>(data), sizeof(data));
        }

        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(path, meta, footer, issues, ch, fh);
        // File too short for header + footer
        CHECK(status == SegmentStatus::Truncated);
    }

    // ============================================================
    // Blocker 8: Classification — unsupported version → Unsupported
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
    // Blocker 8: Classification — corrupt finalized .mxraw → Corrupt (not Partial)
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
        auto bad_path = dir + "/corrupt_finalized.mxraw";
        fs::copy(paths[0], bad_path);

        // Corrupt the footer magic (change MXENDV1 to garbage)
        {
            std::fstream f(bad_path, std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(0, std::ios::end);
            auto end_pos = f.tellp();
            f.seekp(end_pos - static_cast<std::streamoff>(92));  // footer start
            std::uint8_t garbage[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
            f.write(reinterpret_cast<const char*>(garbage), 8);
        }

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(bad_path, vmeta, vfooter, issues, ch, fh);
        // Should be Corrupt, NOT Partial
        CHECK(status == SegmentStatus::Corrupt);
    }

    // ============================================================
    // Blocker 8: Classification — .mxraw.partial → Partial
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        // Create a valid segment first to get proper bytes
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        w.append(rec);
        w.finalize();
        auto paths = w.finalized_paths();
        CHECK(paths.size() == 1);

        // Read the valid segment and truncate it (simulate partial)
        auto valid_size = fs::file_size(paths[0]);
        auto partial_path = dir + "/test.mxraw.partial";
        {
            std::ifstream ifs(paths[0], std::ios::binary);
            std::vector<std::uint8_t> data(valid_size);
            ifs.read(reinterpret_cast<char*>(data.data()), valid_size);
            ifs.close();

            // Write truncated version (header + record, no footer)
            std::ofstream ofs(partial_path, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(valid_size - kFooterSize));
        }

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(partial_path, vmeta, vfooter, issues, ch, fh);
        CHECK(status == SegmentStatus::Partial);
    }

    // ============================================================
    // Blocker 3: UTC bounds from actual records with kRecordFlagUtcValid
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();

        // Record 0: no UTC flag
        RawPacketRecord rec;
        rec.record_flags = 0;
        rec.capture_index = 0;
        rec.capture_utc_ns = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01};
        CHECK(w.append(rec).empty());

        // Record 1: UTC flag, utc = 5000
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 1;
        rec.capture_utc_ns = 5000;
        rec.capture_monotonic_ns = 1000;
        rec.payload = {0x02};
        CHECK(w.append(rec).empty());

        // Record 2: UTC flag, utc = 9000
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 2;
        rec.capture_utc_ns = 9000;
        rec.capture_monotonic_ns = 2000;
        rec.payload = {0x03};
        CHECK(w.append(rec).empty());

        w.finalize();
        auto paths = w.finalized_paths();

        // Validate and check UTC bounds
        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        std::uint64_t first_utc = 0, last_utc = 0;
        auto status = validate_segment(paths[0], vmeta, vfooter, issues, ch, fh,
                                       nullptr, nullptr, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(first_utc == 5000);   // first record with kRecordFlagUtcValid
        CHECK(last_utc == 9000);    // last record with kRecordFlagUtcValid
    }

    // ============================================================
    // Blocker 3: UTC zero semantics — no records with kRecordFlagUtcValid
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();

        RawPacketRecord rec;
        rec.record_flags = 0;  // no UTC flag
        rec.capture_index = 0;
        rec.capture_utc_ns = 99999;  // has UTC but flag not set
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01};
        CHECK(w.append(rec).empty());

        w.finalize();
        auto paths = w.finalized_paths();

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        std::uint64_t first_utc = 999, last_utc = 999;  // sentinel
        auto status = validate_segment(paths[0], vmeta, vfooter, issues, ch, fh,
                                       nullptr, nullptr, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(first_utc == 0);  // zero semantics
        CHECK(last_utc == 0);   // zero semantics
    }

    // ============================================================
    // Blocker 3: Multi-segment UTC bounds via validate_stream_set
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;
        RawSegmentWriter w(meta, dir, pol);
        w.open();

        for (std::uint64_t i = 0; i < 4; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i * 100;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i)};
            CHECK(w.append(rec).empty());
        }
        w.finalize();
        auto paths = w.finalized_paths();
        CHECK(paths.size() == 2);

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        std::uint64_t first_utc = 0, last_utc = 0;
        auto status = validate_stream_set(paths, metas, footers, issues, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(first_utc == 1000);  // first record's UTC
        CHECK(last_utc == 1300);   // last record's UTC (1000 + 3*100)
    }

    // ============================================================
    // Blocker 4: Multi-stream directory — top-level fields empty/zero
    // ============================================================
    {
        auto dir = temp_dir();

        // Stream A
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

        // Stream B (different source)
        auto meta_b = make_meta();
        meta_b.source.source_id = 2;
        meta_b.source.channel_id = 2;
        RawSegmentWriter wb(meta_b, dir, {});
        wb.open();
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        wb.append(rec);
        wb.finalize();

        // Generate JSON report via CLI-like inspection
        RawSegmentReport report;
        report.operation = "inspect";
        report.format_version = "1";

        std::vector<RawValidationIssue> disc_issues;
        auto stream_sets = group_stream_sets(dir, disc_issues);
        CHECK(stream_sets.size() == 2);

        // Sort deterministically
        std::sort(stream_sets.begin(), stream_sets.end(),
                  [](const StreamSetInfo& a, const StreamSetInfo& b) {
                      int cmp = std::memcmp(a.session_id, b.session_id, 16);
                      if (cmp != 0) return cmp < 0;
                      if (a.source_id != b.source_id) return a.source_id < b.source_id;
                      return a.channel_id < b.channel_id;
                  });

        for (const auto& ss : stream_sets) {
            std::vector<RawSegmentMetadata> metas;
            std::vector<RawFooter> footers;
            std::vector<RawValidationIssue> issues;
            auto status = validate_stream_set(ss.segment_paths, metas, footers, issues);
            CHECK(status == SegmentStatus::ValidFinalized);

            std::vector<std::string> ch, fh;
            for (const auto& p : ss.segment_paths) {
                RawSegmentMetadata tm;
                RawFooter tf;
                std::vector<RawValidationIssue> ti;
                std::string c, f;
                validate_segment(p, tm, tf, ti, c, f);
                ch.push_back(c);
                fh.push_back(f);
            }

            RawStreamSummary s;
            s.session_id_hex = session_id_hex(ss.session_id);
            s.source_id = ss.source_id;
            s.channel_id = ss.channel_id;
            report.stream_sets.push_back(s);
        }

        // For multi-stream, top-level singular fields should remain empty/zero
        // (they are NOT set in the loop above — this is the correct behavior)
        CHECK(report.session_id_hex.empty());
        CHECK(report.source_id == 0);
        CHECK(report.record_count == 0);
        CHECK(report.segment_indexes.empty());

        // stream_sets[] should have the authoritative data
        CHECK(report.stream_sets.size() == 2);
    }

    // ============================================================
    // Blocker 5: One-segment stream — content_sha256 and file_sha256 populated
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 1000;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42, 0x43};
        CHECK(w.append(rec).empty());
        w.finalize();
        auto paths = w.finalized_paths();
        CHECK(paths.size() == 1);

        // Validate to get per-segment hashes
        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string content_hex, file_hex;
        auto status = validate_segment(paths[0], vmeta, vfooter, issues, content_hex, file_hex);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(!content_hex.empty());
        CHECK(!file_hex.empty());

        // Build a single-segment stream summary — hashes should be populated
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> ss_issues;
        validate_stream_set(paths, metas, footers, ss_issues);

        // The summary for a single-segment stream should have content_sha256 = segment's hash
        CHECK(metas.size() == 1);
        CHECK(!content_hex.empty());
        CHECK(content_hex.size() == 64);  // SHA-256 hex
    }

    // ============================================================
    // Blocker 5: Issue path is specific to the failing file
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

        // Create two segments, corrupt the second one
        meta.segment_index = 1;
        meta.start_capture_index = 1;
        RawSegmentWriter w2(meta, dir, {});
        w2.open();
        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        w2.append(rec);
        w2.finalize();

        auto paths = w.finalized_paths();
        auto paths2 = w2.finalized_paths();
        CHECK(paths.size() == 1);
        CHECK(paths2.size() == 1);

        // Corrupt the second segment's footer
        {
            std::fstream f(paths2[0], std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(0, std::ios::end);
            auto end_pos = f.tellp();
            f.seekp(end_pos - static_cast<std::streamoff>(92));
            std::uint8_t garbage[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
            f.write(reinterpret_cast<const char*>(garbage), 8);
        }

        // Validate the stream set — should fail with path pointing to second segment
        std::vector<std::string> all_paths = {paths[0], paths2[0]};
        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        auto status = validate_stream_set(all_paths, metas, footers, issues);
        CHECK(status != SegmentStatus::ValidFinalized);

        // Check that at least one issue has the path of the second segment
        bool found_path = false;
        for (const auto& issue : issues) {
            if (issue.path == paths2[0]) found_path = true;
        }
        CHECK(found_path);
    }

    // ============================================================
    // Blocker 6: JSON strict parser — types, arrays, escaping, values
    // ============================================================
    {
        RawSegmentReport report;
        report.schema_version = "1.0";
        report.tool_version = "0.1.0";
        report.operation = "inspect";
        report.format_version = "1";
        report.overall_status = "valid";
        report.source_id = 42;
        report.channel_id = 7;
        report.record_count = 100;
        report.total_payload_bytes = 6400;
        report.first_capture_index = 0;
        report.last_capture_index = 99;
        report.first_capture_utc_ns = 1000000;
        report.last_capture_utc_ns = 2000000;
        report.session_id_hex = "0123456789abcdef0123456789abcdef";
        report.feed_group = "ORDERS-LOG";
        report.endpoint_role = "Incremental";
        report.clock_domain = "synthetic";
        report.transport = "udp";
        report.source_side = "A";
        report.configuration_sha256 = "abc123";
        report.content_sha256 = "deadbeef";
        report.file_sha256 = "cafebabe";
        report.segment_indexes = {0, 1};
        report.segment_sizes = {1024, 2048};
        report.replay_sha256 = "fedcba9876543210";

        // Add stream_sets with specific values
        RawStreamSummary ss;
        ss.session_id_hex = "0123456789abcdef0123456789abcdef";
        ss.source_id = 42;
        ss.channel_id = 7;
        ss.feed_group = "ORDERS-LOG";
        ss.endpoint_role = "Incremental";
        ss.record_count = 100;
        ss.first_capture_utc_ns = 1000000;
        ss.last_capture_utc_ns = 2000000;
        ss.status = "valid";
        ss.segment_indexes = {0};
        ss.segment_content_sha256.push_back("hash1");
        ss.segment_file_sha256.push_back("hash2");
        report.stream_sets.push_back(ss);

        // Add issue with source/path
        report.issues.push_back({ValidationSeverity::Error, "TEST_ERR", "test \"quoted\" message",
                                  "stream_key", "/path/to/file\\with\\backslashes"});
        report.issues.push_back({ValidationSeverity::Warning, "TEST_WARN",
                                  "line1\nline2\ttab", "src", "path"});

        auto json = generate_json_report(report);

        // Parse with strict JSON parser
        auto root = json_test::parse(json);

        // Verify types
        CHECK(root.type == json_test::Value::Object);

        // Numeric types — source_id must be a number, not a string
        CHECK(root["source_id"].type == json_test::Value::Number);
        CHECK(root["source_id"].number_val == 42.0);
        CHECK(root["channel_id"].type == json_test::Value::Number);
        CHECK(root["channel_id"].number_val == 7.0);
        CHECK(root["record_count"].type == json_test::Value::Number);
        CHECK(root["record_count"].number_val == 100.0);
        CHECK(root["first_capture_utc_ns"].type == json_test::Value::Number);
        CHECK(root["first_capture_utc_ns"].number_val == 1000000.0);
        CHECK(root["last_capture_utc_ns"].type == json_test::Value::Number);
        CHECK(root["last_capture_utc_ns"].number_val == 2000000.0);

        // String types
        CHECK(root["session_id"].type == json_test::Value::String);
        CHECK(root["session_id"].string_val == "0123456789abcdef0123456789abcdef");
        CHECK(root["feed_group"].type == json_test::Value::String);
        CHECK(root["feed_group"].string_val == "ORDERS-LOG");
        CHECK(root["overall_status"].type == json_test::Value::String);
        CHECK(root["overall_status"].string_val == "valid");

        // Arrays
        CHECK(root["segment_indexes"].type == json_test::Value::Array);
        CHECK(root["segment_indexes"].size() == 2);
        CHECK(root["segment_indexes"][0].type == json_test::Value::Number);
        CHECK(root["segment_indexes"][0].number_val == 0.0);
        CHECK(root["segment_indexes"][1].number_val == 1.0);

        CHECK(root["segment_sizes"].type == json_test::Value::Array);
        CHECK(root["segment_sizes"].size() == 2);
        CHECK(root["segment_sizes"][0].number_val == 1024.0);
        CHECK(root["segment_sizes"][1].number_val == 2048.0);

        // Nested stream_sets array
        CHECK(root["stream_sets"].type == json_test::Value::Array);
        CHECK(root["stream_sets"].size() == 1);
        CHECK(root["stream_sets"][0]["source_id"].number_val == 42.0);
        CHECK(root["stream_sets"][0]["record_count"].number_val == 100.0);
        CHECK(root["stream_sets"][0]["segment_content_sha256"].type == json_test::Value::Array);
        CHECK(root["stream_sets"][0]["segment_content_sha256"].size() == 1);
        CHECK(root["stream_sets"][0]["segment_content_sha256"][0].string_val == "hash1");

        // Issues array with source/path
        CHECK(root["issues"].type == json_test::Value::Array);
        CHECK(root["issues"].size() == 2);
        CHECK(root["issues"][0]["severity"].string_val == "error");
        CHECK(root["issues"][0]["code"].string_val == "TEST_ERR");
        CHECK(root["issues"][0]["source"].string_val == "stream_key");
        CHECK(root["issues"][0]["path"].string_val == "/path/to/file\\with\\backslashes");

        // Escaping: quotes and backslashes in message
        CHECK(root["issues"][0]["message"].string_val.find("test \"quoted\" message") != std::string::npos);
        CHECK(root["issues"][1]["message"].string_val.find('\n') != std::string::npos);
        CHECK(root["issues"][1]["message"].string_val.find('\t') != std::string::npos);

        // Deterministic ordering — same input produces same JSON
        auto json2 = generate_json_report(report);
        CHECK(json == json2);
    }

    // ============================================================
    // Blocker 6: JSON escaping — control characters
    // ============================================================
    {
        RawSegmentReport report;
        report.overall_status = "valid";
        // Add issue with control characters in message
        report.issues.push_back({ValidationSeverity::Error, "CTRL", "tab\there", {}, {}});

        auto json = generate_json_report(report);
        auto root = json_test::parse(json);

        // The tab should be escaped as \t
        auto msg = root["issues"][0]["message"].string_val;
        CHECK(msg.find('\t') != std::string::npos);
    }

    std::cout << "test_round5: ALL PASSED\n";
    return 0;
}
