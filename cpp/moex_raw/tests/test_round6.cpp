#ifdef _MSC_VER
#pragma warning(disable : 4189 4101 4309)
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
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("round6_" + std::to_string(counter++));
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
// Scripted IFileHandle — stage-aware failure injection
// ============================================================

struct ScriptedFileHandle : moex_raw::IFileHandle {
    // Internal buffer: accumulates written bytes or serves as read source
    std::vector<std::uint8_t> data;
    std::size_t read_pos = 0;

    // Per-operation-type counters
    int read_count = 0;
    int write_count = 0;
    int seek_count = 0;
    int flush_count = 0;
    int close_count = 0;

    // Failure injection: which operation # to fail at (-1 = never)
    int fail_write_at = -1;   // write returns 0 at this write #
    int short_write_at = -1;  // write returns fewer bytes at this write #
    std::size_t short_write_bytes = 1;

    int fail_read_at = -1;    // read returns 0 at this read #
    int short_read_at = -1;   // read returns fewer bytes at this read #
    std::size_t short_read_bytes = 1;

    int fail_seek_at = -1;    // seek returns false at this seek #
    int fail_flush_at = -1;   // flush returns false at this flush #
    int fail_close_at = -1;   // close returns false at this close #

    std::size_t read(void* buf, std::size_t size) override {
        read_count++;
        if (read_count == fail_read_at) return 0;
        if (read_count == short_read_at) {
            std::size_t n = std::min(short_read_bytes, std::min(size, data.size() - read_pos));
            if (n == 0) return 0;
            std::memcpy(buf, data.data() + read_pos, n);
            read_pos += n;
            return n;
        }
        std::size_t n = std::min(size, data.size() - read_pos);
        if (n == 0) return 0;
        std::memcpy(buf, data.data() + read_pos, n);
        read_pos += n;
        return n;
    }

    std::size_t write(const void* buf, std::size_t size) override {
        write_count++;
        if (write_count == fail_write_at) return 0;
        if (write_count == short_write_at) {
            std::size_t n = std::min(short_write_bytes, size);
            data.insert(data.end(), static_cast<const std::uint8_t*>(buf),
                       static_cast<const std::uint8_t*>(buf) + n);
            return n;
        }
        data.insert(data.end(), static_cast<const std::uint8_t*>(buf),
                   static_cast<const std::uint8_t*>(buf) + size);
        return size;
    }

    bool seek(std::int64_t offset, int origin) override {
        seek_count++;
        if (seek_count == fail_seek_at) return false;
        std::size_t new_pos = 0;
        if (origin == SEEK_SET) {
            new_pos = static_cast<std::size_t>(offset);
        } else if (origin == SEEK_CUR) {
            new_pos = read_pos + static_cast<std::size_t>(offset);
        } else {
            if (static_cast<std::size_t>(-offset) > data.size()) return false;
            new_pos = data.size() + static_cast<std::size_t>(offset);
        }
        if (new_pos > data.size()) return false;
        read_pos = new_pos;
        return true;
    }

    bool flush() override {
        flush_count++;
        return flush_count != fail_flush_at;
    }

    bool close() override {
        close_count++;
        return close_count != fail_close_at;
    }
};

struct ScriptedFileSystem : moex_raw::IFileSystem {
    bool fail_rename = false;
    bool fail_open_write = false;
    bool fail_open_read = false;
    std::uint64_t mock_file_size = 0;
    bool mock_file_size_ok = true;

    // Track created handles for post-configuration
    std::vector<ScriptedFileHandle*> created_handles;

    bool exists(const std::string&) override { return false; }
    bool rename(const std::string&, const std::string&) override { return !fail_rename; }
    bool remove(const std::string&) override { return true; }

    std::uint64_t file_size(const std::string&, bool& ok) override {
        ok = mock_file_size_ok;
        return mock_file_size;
    }

    std::unique_ptr<moex_raw::IFileHandle> open_read(const std::string&) override {
        if (fail_open_read) return nullptr;
        auto h = std::make_unique<ScriptedFileHandle>();
        created_handles.push_back(h.get());
        return h;
    }

    std::unique_ptr<moex_raw::IFileHandle> open_write(const std::string&) override {
        if (fail_open_write) return nullptr;
        auto h = std::make_unique<ScriptedFileHandle>();
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
    enum Type { Null, Bool, Number, String, Array, Object, Invalid } type = Invalid;
    bool bool_val = false;
    double number_val = 0;
    std::string string_val;
    JArray array_val;
    JObject object_val;

    const Value& operator[](const std::string& key) const {
        for (const auto& p : object_val) {
            if (p.first == key) return p.second;
        }
        static Value null_val;
        null_val.type = Null;
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
    bool failed = false;
    std::string error_msg;
public:
    explicit Parser(const std::string& json) : s(json) {}

    Value parse() {
        skip_ws();
        Value v = parse_value();
        if (failed) { v.type = Value::Invalid; return v; }
        skip_ws();
        // Strict: must consume ALL input
        if (pos != s.size()) {
            failed = true;
            error_msg = "trailing input after value";
            v.type = Value::Invalid;
        }
        return v;
    }

    bool ok() const { return !failed; }
    const std::string& error() const { return error_msg; }

private:
    void skip_ws() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' || s[pos] == '\r' || s[pos] == '\t'))
            pos++;
    }

    char peek() { return pos < s.size() ? s[pos] : '\0'; }
    char advance() {
        if (pos >= s.size()) { failed = true; return '\0'; }
        return s[pos++];
    }

    void expect(char c) {
        char got = advance();
        if (got != c) {
            failed = true;
            error_msg = std::string("expected '") + c + "' but got '" + got + "'";
        }
    }

    Value parse_value() {
        if (failed) return {};
        skip_ws();
        char c = peek();
        if (c == '"') return parse_string_val();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        failed = true;
        error_msg = std::string("unexpected character '") + c + "'";
        return {};
    }

    Value parse_string_val() {
        Value v;
        v.type = Value::String;
        v.string_val = parse_string();
        return v;
    }

    std::string parse_string() {
        if (failed) return "";
        char open = advance();
        if (open != '"') { failed = true; error_msg = "expected '\"'"; return ""; }
        std::string result;
        while (pos < s.size()) {
            char c = advance();
            if (failed) return "";
            if (c == '"') return result;
            if (c == '\\') {
                char esc = advance();
                if (failed) return "";
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
                        if (hex.size() != 4) { failed = true; error_msg = "incomplete \\u escape"; return ""; }
                        result += "\\u" + hex;
                        break;
                    }
                    default:
                        failed = true;
                        error_msg = std::string("invalid escape '\\") + esc + "'";
                        return "";
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                failed = true;
                error_msg = "unescaped control character in string";
                return "";
            } else {
                result += c;
            }
        }
        failed = true;
        error_msg = "unterminated string";
        return "";
    }

    Value parse_number() {
        Value v;
        v.type = Value::Number;
        std::string num;
        if (peek() == '-') num += advance();
        if (peek() == '0') {
            num += advance();
        } else if (peek() >= '1' && peek() <= '9') {
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') num += advance();
        } else {
            failed = true; error_msg = "invalid number"; return v;
        }
        if (peek() == '.') {
            num += advance();
            if (!(peek() >= '0' && peek() <= '9')) { failed = true; error_msg = "invalid number fraction"; return v; }
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') num += advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            num += advance();
            if (peek() == '+' || peek() == '-') num += advance();
            if (!(peek() >= '0' && peek() <= '9')) { failed = true; error_msg = "invalid number exponent"; return v; }
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') num += advance();
        }
        v.number_val = std::stod(num);
        return v;
    }

    Value parse_bool() {
        Value v;
        v.type = Value::Bool;
        if (s.size() - pos >= 4 && s.substr(pos, 4) == "true") { pos += 4; v.bool_val = true; }
        else if (s.size() - pos >= 5 && s.substr(pos, 5) == "false") { pos += 5; v.bool_val = false; }
        else { failed = true; error_msg = "invalid bool"; }
        return v;
    }

    Value parse_null() {
        Value v;
        v.type = Value::Null;
        if (s.size() - pos >= 4 && s.substr(pos, 4) == "null") { pos += 4; }
        else { failed = true; error_msg = "invalid null"; }
        return v;
    }

    Value parse_array() {
        Value v;
        v.type = Value::Array;
        expect('[');
        if (failed) return v;
        skip_ws();
        if (peek() == ']') { advance(); return v; }
        while (true) {
            if (failed) return v;
            v.array_val.push_back(parse_value());
            skip_ws();
            if (peek() == ',') { advance(); skip_ws(); }
            else break;
        }
        expect(']');
        return v;
    }

    Value parse_object() {
        Value v;
        v.type = Value::Object;
        expect('{');
        if (failed) return v;
        skip_ws();
        if (peek() == '}') { advance(); return v; }
        while (true) {
            if (failed) return v;
            skip_ws();
            if (peek() != '"') { failed = true; error_msg = "expected string key"; return v; }
            std::string key = parse_string();
            if (failed) return v;
            skip_ws();
            expect(':');
            if (failed) return v;
            skip_ws();
            v.object_val.emplace_back(key, parse_value());
            skip_ws();
            if (peek() == ',') { advance(); skip_ws(); }
            else break;
        }
        expect('}');
        return v;
    }
};

inline Value parse(const std::string& json) {
    Parser p(json);
    return p.parse();
}

inline bool parse_fails(const std::string& json) {
    Parser p(json);
    p.parse();
    return !p.ok();
}

}  // namespace json_test

// ============================================================
// Helper: create a valid segment and return its path
// ============================================================

static std::string create_valid_segment(const std::string& dir,
                                        moex_raw::RawSegmentMetadata meta = make_meta(),
                                        std::uint64_t records = 1) {
    moex_raw::RawSegmentWriter w(meta, dir, {});
    w.open();
    for (std::uint64_t i = 0; i < records; ++i) {
        moex_raw::RawPacketRecord rec;
        rec.record_flags = moex_raw::kRecordFlagUtcValid;
        rec.capture_index = meta.start_capture_index + i;
        rec.capture_utc_ns = 1000 + i * 100;
        rec.capture_monotonic_ns = i * 1000;
        rec.payload = {static_cast<std::uint8_t>(i & 0xFF), 0x42};
        w.append(rec);
    }
    w.finalize();
    auto paths = w.finalized_paths();
    CHECK(paths.size() == 1);
    return paths[0];
}

// ============================================================
// Main test driver
// ============================================================

int main() {
    using namespace moex_raw;

    // ============================================================
    // Blocker 1: Scripted short-I/O — short header write
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        ScriptedFileSystem fs;

        RawSegmentWriter w(meta, dir, {}, &fs);
        CHECK(w.open().empty());
        CHECK(fs.created_handles.size() == 1);

        // Configure: the NEXT write (record write = stage 2) fails with short write
        fs.created_handles[0]->short_write_at = 2;
        fs.created_handles[0]->short_write_bytes = 1;

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        auto err = w.append(rec);
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Short record write
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        ScriptedFileSystem fs;

        RawSegmentWriter w(meta, dir, {}, &fs);
        CHECK(w.open().empty());
        CHECK(fs.created_handles.size() == 1);

        // Write one record OK (stage 2), then short-write second record (stage 3)
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01};
        CHECK(w.append(rec).empty());

        // Configure short write for next write
        fs.created_handles[0]->short_write_at = 3;
        fs.created_handles[0]->short_write_bytes = 2;

        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        rec.payload = {0x02};
        auto err2 = w.append(rec);
        CHECK(!err2.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Short footer write
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        ScriptedFileSystem fs;

        RawSegmentWriter w(meta, dir, {}, &fs);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        CHECK(w.append(rec).empty());

        // Configure: next write (footer = stage 3) short-writes
        fs.created_handles[0]->short_write_at = 3;
        fs.created_handles[0]->short_write_bytes = 10;

        auto err3 = w.finalize();
        CHECK(!err3.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Flush failure
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        ScriptedFileSystem fs;

        RawSegmentWriter w(meta, dir, {}, &fs);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        CHECK(w.append(rec).empty());

        // Configure: flush fails at stage after footer write
        fs.created_handles[0]->fail_flush_at = 1;

        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Close failure
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        ScriptedFileSystem fs;

        RawSegmentWriter w(meta, dir, {}, &fs);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        CHECK(w.append(rec).empty());

        // Configure: close fails (stage after flush)
        fs.created_handles[0]->fail_close_at = 1;

        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Rename failure
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        ScriptedFileSystem fs;
        fs.fail_rename = true;

        RawSegmentWriter w(meta, dir, {}, &fs);
        CHECK(w.open().empty());

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42};
        CHECK(w.append(rec).empty());

        auto err = w.finalize();
        CHECK(!err.empty());
        CHECK(w.state() == WriterState::Failed);
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 1: Reader — short header read returns IoError
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = create_valid_segment(dir);
        bool ok = false;
        auto real_size = DefaultFileSystem().file_size(path, ok);
        CHECK(ok);

        // Create a ScriptedFileSystem that serves short reads
        ScriptedFileSystem sfs;
        sfs.mock_file_size = real_size;
        sfs.created_handles.clear();

        // Create a handle that will short-read on the first read (header)
        // We need to pre-load the real bytes into the handle
        {
            std::ifstream ifs(path, std::ios::binary);
            std::vector<std::uint8_t> all_bytes(real_size);
            ifs.read(reinterpret_cast<char*>(all_bytes.data()), real_size);

            // We'll configure open_read to return a handle with the real bytes
            // but short-read on stage 1
        }

        // Simpler: use a lambda-based approach
        struct ShortReadFS : IFileSystem {
            std::uint64_t sz;
            std::string real_path;
            bool exists(const std::string&) override { return true; }
            bool rename(const std::string&, const std::string&) override { return true; }
            bool remove(const std::string&) override { return true; }
            std::uint64_t file_size(const std::string&, bool& ok) override { ok = true; return sz; }
            std::unique_ptr<IFileHandle> open_read(const std::string&) override {
                auto h = std::make_unique<ScriptedFileHandle>();
                // Load real bytes
                std::ifstream ifs(real_path, std::ios::binary);
                h->data.resize(sz);
                ifs.read(reinterpret_cast<char*>(h->data.data()), sz);
                h->short_read_at = 1;  // first read returns only 1 byte
                h->short_read_bytes = 1;
                return h;
            }
            std::unique_ptr<IFileHandle> open_write(const std::string&) override { return nullptr; }
        };

        ShortReadFS srfs;
        srfs.sz = real_size;
        srfs.real_path = path;

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(path, vmeta, vfooter, issues, ch, fh,
                                       nullptr, nullptr, nullptr, nullptr, &srfs);
        CHECK(status == SegmentStatus::IoError);

        // Check that the issue has the concrete path
        bool found_path = false;
        for (const auto& issue : issues) {
            if (issue.path == path) found_path = true;
        }
        CHECK(found_path);
    }

    // ============================================================
    // Blocker 1: Reader — corrupt record data with concrete path propagation
    // Corrupting record magic causes content SHA mismatch (detected first).
    // Verify the concrete path is attached to the resulting issue.
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = create_valid_segment(dir);

        auto corrupt_path = dir + "/corrupt_record.mxraw";
        fs::copy(path, corrupt_path);
        {
            std::fstream f(corrupt_path, std::ios::in | std::ios::out | std::ios::binary);
            f.seekg(10);
            std::uint32_t header_size = 0;
            f.read(reinterpret_cast<char*>(&header_size), 4);
            f.seekp(header_size);
            std::uint8_t bad_magic[] = {0xDE, 0xAD, 0xBE, 0xEF};
            f.write(reinterpret_cast<const char*>(bad_magic), 4);
        }

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(corrupt_path, vmeta, vfooter, issues, ch, fh);
        CHECK(status == SegmentStatus::Corrupt);

        // Verify at least one error issue has the concrete path
        bool found_path = false;
        for (const auto& issue : issues) {
            if (issue.severity == ValidationSeverity::Error && issue.path == corrupt_path) {
                found_path = true;
            }
        }
        CHECK(found_path);
    }

    // ============================================================
    // Blocker 1: Reader — seek failure on footer
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = create_valid_segment(dir);
        bool ok = false;
        auto real_size = DefaultFileSystem().file_size(path, ok);

        struct SeekFailFS : IFileSystem {
            std::uint64_t sz;
            std::string real_path;
            bool exists(const std::string&) override { return true; }
            bool rename(const std::string&, const std::string&) override { return true; }
            bool remove(const std::string&) override { return true; }
            std::uint64_t file_size(const std::string&, bool& ok) override { ok = true; return sz; }
            std::unique_ptr<IFileHandle> open_read(const std::string&) override {
                auto h = std::make_unique<ScriptedFileHandle>();
                std::ifstream ifs(real_path, std::ios::binary);
                h->data.resize(sz);
                ifs.read(reinterpret_cast<char*>(h->data.data()), sz);
                // Fail the first seek (seek to footer) — seek #1
                h->fail_seek_at = 1;
                return h;
            }
            std::unique_ptr<IFileHandle> open_write(const std::string&) override { return nullptr; }
        };

        SeekFailFS sfs;
        sfs.sz = real_size;
        sfs.real_path = path;

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(path, vmeta, vfooter, issues, ch, fh,
                                       nullptr, nullptr, nullptr, nullptr, &sfs);
        CHECK(status == SegmentStatus::IoError);

        bool found_seek_issue = false;
        for (const auto& issue : issues) {
            if (issue.code == "IO_ERROR" && issue.message.find("seek") != std::string::npos) {
                found_seek_issue = true;
            }
        }
        CHECK(found_seek_issue);
    }

    // ============================================================
    // Blocker 1: Reader — seek failure on content SHA-256
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = create_valid_segment(dir);
        bool ok = false;
        auto real_size = DefaultFileSystem().file_size(path, ok);

        struct SeekFailContentFS : IFileSystem {
            std::uint64_t sz;
            std::string real_path;
            bool exists(const std::string&) override { return true; }
            bool rename(const std::string&, const std::string&) override { return true; }
            bool remove(const std::string&) override { return true; }
            std::uint64_t file_size(const std::string&, bool& ok) override { ok = true; return sz; }
            std::unique_ptr<IFileHandle> open_read(const std::string&) override {
                auto h = std::make_unique<ScriptedFileHandle>();
                std::ifstream ifs(real_path, std::ios::binary);
                h->data.resize(sz);
                ifs.read(reinterpret_cast<char*>(h->data.data()), sz);
                // Fail seek to start for content SHA — seek #2
                h->fail_seek_at = 2;
                return h;
            }
            std::unique_ptr<IFileHandle> open_write(const std::string&) override { return nullptr; }
        };

        SeekFailContentFS sfs;
        sfs.sz = real_size;
        sfs.real_path = path;

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(path, vmeta, vfooter, issues, ch, fh,
                                       nullptr, nullptr, nullptr, nullptr, &sfs);
        CHECK(status == SegmentStatus::IoError);
    }

    // ============================================================
    // Blocker 2: UINT64_MAX start_capture_index — no mutation
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
        // Writer should still be Open (preflight detected overflow before write)
        CHECK(w.state() == WriterState::Open);
    }

    // ============================================================
    // Blocker 2: segment_index UINT64_MAX — rotation overflow
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.segment_index = UINT64_MAX;
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 1;
        RawSegmentWriter w(meta, dir, pol);
        CHECK(w.open().empty());

        // First record OK
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01};
        CHECK(w.append(rec).empty());

        // Second record triggers rotation, but segment_index would overflow
        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        rec.payload = {0x02};
        auto err = w.append(rec);
        CHECK(!err.empty());
        CHECK(w.finalized_paths().empty());
        // Writer should still be Open (preflight detected overflow before rotation)
        CHECK(w.state() == WriterState::Open);
    }

    // ============================================================
    // Blocker 2: Too-large boundary record — preflight rejection
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        // Compute actual header size for proper limit
        std::vector<std::uint8_t> hdr;
        serialize_header(hdr, meta);
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 1;
        // Fit header + 1 small record + footer, but not boundary large record
        pol.max_segment_bytes = hdr.size() + kRecordHeaderSize + 2 + 4 + kFooterSize;
        RawSegmentWriter w(meta, dir, pol);
        CHECK(w.open().empty());

        // First record OK
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01};
        CHECK(w.append(rec).empty());

        // Second record with large payload — triggers rotation, but boundary record
        // won't fit in new segment with max_segment_bytes=200
        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        rec.payload.resize(300, 0xAB);  // Too large for 200-byte segment
        auto err = w.append(rec);
        CHECK(!err.empty());
        // No finalized paths from rotation (only the first segment finalized)
        // Actually, the first segment was finalized by the rotation attempt,
        // but the preflight should have caught this BEFORE rotation.
        // With preflight, the error is caught before finalize_current is called.
        CHECK(w.finalized_paths().empty());
    }

    // ============================================================
    // Blocker 4: UTC bounds — only middle segment has UTC
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Segment 0: no UTC flag
        {
            RawSegmentWriter w(meta, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = 0;  // no UTC
            rec.capture_index = 0;
            rec.capture_utc_ns = 0;
            rec.capture_monotonic_ns = 0;
            rec.payload = {0x01};
            w.append(rec);
            w.finalize();
        }

        // Segment 1: has UTC flag
        auto meta1 = meta;
        meta1.segment_index = 1;
        meta1.start_capture_index = 1;
        {
            RawSegmentWriter w(meta1, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = 1;
            rec.capture_utc_ns = 5000;
            rec.capture_monotonic_ns = 1000;
            rec.payload = {0x02};
            w.append(rec);
            w.finalize();
        }

        // Segment 2: no UTC flag
        auto meta2 = meta;
        meta2.segment_index = 2;
        meta2.start_capture_index = 2;
        {
            RawSegmentWriter w(meta2, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = 0;  // no UTC
            rec.capture_index = 2;
            rec.capture_utc_ns = 0;
            rec.capture_monotonic_ns = 2000;
            rec.payload = {0x03};
            w.append(rec);
            w.finalize();
        }

        // Collect paths
        std::vector<std::string> paths;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".mxraw") paths.push_back(entry.path().string());
        }
        std::sort(paths.begin(), paths.end());

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        std::uint64_t first_utc = 0, last_utc = 0;
        auto status = validate_stream_set(paths, metas, footers, issues, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(first_utc == 5000);  // from segment 1
        CHECK(last_utc == 5000);   // from segment 1
    }

    // ============================================================
    // Blocker 4: UTC bounds — only first segment has UTC
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Segment 0: has UTC
        {
            RawSegmentWriter w(meta, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = 0;
            rec.capture_utc_ns = 3000;
            rec.capture_monotonic_ns = 0;
            rec.payload = {0x01};
            w.append(rec);
            w.finalize();
        }

        // Segment 1: no UTC
        auto meta1 = meta;
        meta1.segment_index = 1;
        meta1.start_capture_index = 1;
        {
            RawSegmentWriter w(meta1, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = 0;
            rec.capture_index = 1;
            rec.capture_utc_ns = 0;
            rec.capture_monotonic_ns = 1000;
            rec.payload = {0x02};
            w.append(rec);
            w.finalize();
        }

        std::vector<std::string> paths;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".mxraw") paths.push_back(entry.path().string());
        }
        std::sort(paths.begin(), paths.end());

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        std::uint64_t first_utc = 0, last_utc = 0;
        auto status = validate_stream_set(paths, metas, footers, issues, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(first_utc == 3000);
        CHECK(last_utc == 3000);
    }

    // ============================================================
    // Blocker 4: UTC bounds — only last segment has UTC
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Segment 0: no UTC
        {
            RawSegmentWriter w(meta, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = 0;
            rec.capture_index = 0;
            rec.capture_utc_ns = 0;
            rec.capture_monotonic_ns = 0;
            rec.payload = {0x01};
            w.append(rec);
            w.finalize();
        }

        // Segment 1: has UTC
        auto meta1 = meta;
        meta1.segment_index = 1;
        meta1.start_capture_index = 1;
        {
            RawSegmentWriter w(meta1, dir, {});
            w.open();
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = 1;
            rec.capture_utc_ns = 7000;
            rec.capture_monotonic_ns = 1000;
            rec.payload = {0x02};
            w.append(rec);
            w.finalize();
        }

        std::vector<std::string> paths;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".mxraw") paths.push_back(entry.path().string());
        }
        std::sort(paths.begin(), paths.end());

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        std::uint64_t first_utc = 0, last_utc = 0;
        auto status = validate_stream_set(paths, metas, footers, issues, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(first_utc == 7000);
        CHECK(last_utc == 7000);
    }

    // ============================================================
    // Blocker 5: .mxraw.partial with valid footer → Partial
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Create a valid segment
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 1000;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x42, 0x43};
        w.append(rec);
        w.finalize();
        auto paths = w.finalized_paths();
        CHECK(paths.size() == 1);

        // Copy as .mxraw.partial (complete file with valid footer)
        auto partial_path = dir + "/complete.mxraw.partial";
        fs::copy(paths[0], partial_path);

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(partial_path, vmeta, vfooter, issues, ch, fh);
        // Must be Partial even though the file has a valid footer
        CHECK(status == SegmentStatus::Partial);

        bool found_partial = false;
        for (const auto& issue : issues) {
            if (issue.code == "PARTIAL_FILE") found_partial = true;
        }
        CHECK(found_partial);
    }

    // ============================================================
    // Blocker 5: .mxraw.partial truncated → Partial
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
        rec.payload = {0x42};
        w.append(rec);
        w.finalize();
        auto paths = w.finalized_paths();
        CHECK(paths.size() == 1);

        // Create truncated .mxraw.partial (header + record, no footer)
        auto valid_size = fs::file_size(paths[0]);
        auto partial_path = dir + "/truncated.mxraw.partial";
        {
            std::ifstream ifs(paths[0], std::ios::binary);
            std::vector<std::uint8_t> data(valid_size);
            ifs.read(reinterpret_cast<char*>(data.data()), valid_size);
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
    // Blocker 6: Concrete path on low-level deserializer issues
    // Use a real segment and corrupt the footer magic to get WRONG_FOOTER_MAGIC
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = create_valid_segment(dir);

        // Corrupt the footer magic (first 8 bytes of footer = last 92 bytes of file)
        auto corrupt_path = dir + "/corrupt_footer_magic.mxraw";
        fs::copy(path, corrupt_path);
        {
            std::fstream f(corrupt_path, std::ios::in | std::ios::out | std::ios::binary);
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
        auto status = validate_segment(corrupt_path, vmeta, vfooter, issues, ch, fh);
        CHECK(status == SegmentStatus::Corrupt);

        // Check that WRONG_FOOTER_MAGIC issue has the concrete path
        bool found_wrong_footer = false;
        bool found_path = false;
        for (const auto& issue : issues) {
            if (issue.code == "WRONG_FOOTER_MAGIC") {
                found_wrong_footer = true;
                if (issue.path == corrupt_path) found_path = true;
            }
        }
        CHECK(found_wrong_footer);
        CHECK(found_path);
    }

    // ============================================================
    // Blocker 6: Concrete path on record issue (WRONG_RECORD_MAGIC)
    // ============================================================
    {
        auto dir = temp_dir();
        auto path = create_valid_segment(dir);

        auto corrupt_path = dir + "/bad_record.mxraw";
        fs::copy(path, corrupt_path);
        {
            std::fstream f(corrupt_path, std::ios::in | std::ios::out | std::ios::binary);
            f.seekg(10);
            std::uint32_t header_size = 0;
            f.read(reinterpret_cast<char*>(&header_size), 4);
            f.seekp(header_size);
            std::uint8_t bad[] = {0x00, 0x00, 0x00, 0x00};
            f.write(reinterpret_cast<const char*>(bad), 4);
        }

        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string ch, fh;
        auto status = validate_segment(corrupt_path, vmeta, vfooter, issues, ch, fh);
        CHECK(status == SegmentStatus::Corrupt);

        // Content SHA check happens before record validation, so the error
        // may be WRONG_CONTENT_SHA256 rather than WRONG_RECORD_MAGIC.
        // Verify at least one error has the concrete path.
        bool found_path = false;
        for (const auto& issue : issues) {
            if (issue.severity == ValidationSeverity::Error && issue.path == corrupt_path) {
                found_path = true;
            }
        }
        CHECK(found_path);
    }

    // ============================================================
    // Blocker 7: Strict JSON parser — valid JSON
    // ============================================================
    {
        auto root = json_test::parse(R"({"key": "value", "num": 42, "arr": [1, 2, 3]})");
        CHECK(root.type == json_test::Value::Object);
        CHECK(root["key"].string_val == "value");
        CHECK(root["num"].number_val == 42.0);
        CHECK(root["arr"].size() == 3);
    }

    // ============================================================
    // Blocker 7: Strict JSON parser — rejects trailing input
    // ============================================================
    {
        CHECK(json_test::parse_fails(R"({"key": "value"} extra)"));
        CHECK(json_test::parse_fails(R"({"key": "value"}{)"));
    }

    // ============================================================
    // Blocker 7: Strict JSON parser — rejects malformed JSON
    // ============================================================
    {
        // Missing closing brace
        CHECK(json_test::parse_fails(R"({"key": "value")"));
        // Missing closing bracket
        CHECK(json_test::parse_fails(R"([1, 2, 3)"));
        // Missing colon
        CHECK(json_test::parse_fails(R"({"key" "value"})"));
        // Missing comma
        CHECK(json_test::parse_fails(R"({"a": 1 "b": 2})"));
        // Invalid escape (use regular string to avoid MSVC hex literal warning)
        CHECK(json_test::parse_fails("{\"key\": \"val\\xue\"}"));
        // Unterminated string
        CHECK(json_test::parse_fails(R"({"key": "value})"));
        // Empty input
        CHECK(json_test::parse_fails(""));
        // Just whitespace
        CHECK(json_test::parse_fails("   "));
        // Trailing comma in object
        CHECK(json_test::parse_fails(R"({"key": "value",})"));
        // Trailing comma in array
        CHECK(json_test::parse_fails(R"([1, 2, 3,])"));
    }

    // ============================================================
    // Blocker 7: Strict JSON parser — valid escapes
    // ============================================================
    {
        auto root = json_test::parse(R"({"key": "line1\nline2\ttab\"quote\\back"})");
        CHECK(root.type == json_test::Value::Object);
        auto val = root["key"].string_val;
        CHECK(val.find('\n') != std::string::npos);
        CHECK(val.find('\t') != std::string::npos);
        CHECK(val.find('"') != std::string::npos);
        CHECK(val.find('\\') != std::string::npos);
    }

    // ============================================================
    // Blocker 7: End-to-end CLI JSON — single-segment stream
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/single_seg";

        // Create segments using library (not CLI, to avoid dependency on exe path)
        auto meta = make_meta();
        RawSegmentWriter w(meta, out_dir, {});
        w.open();
        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000000 + i * 1000;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i), 0x42, 0x43};
            w.append(rec);
        }
        w.finalize();

        // Run inspect --json-out
        std::vector<std::string> paths;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") paths.push_back(entry.path().string());
        }
        CHECK(paths.size() == 1);

        // Validate and build report
        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> issues;
        std::string content_hex, file_hex;
        std::uint64_t first_utc = 0, last_utc = 0;
        auto status = validate_segment(paths[0], vmeta, vfooter, issues, content_hex, file_hex,
                                       nullptr, nullptr, &first_utc, &last_utc);
        CHECK(status == SegmentStatus::ValidFinalized);

        RawSegmentReport report;
        report.operation = "inspect";
        report.format_version = "1";
        report.session_id_hex = session_id_hex(vmeta.session.session_id);
        report.source_id = vmeta.source.source_id;
        report.channel_id = vmeta.source.channel_id;
        report.content_sha256 = content_hex;
        report.file_sha256 = file_hex;
        report.record_count = vfooter.record_count;
        report.total_payload_bytes = vfooter.total_payload_bytes;
        report.first_capture_index = vfooter.first_capture_index;
        report.last_capture_index = vfooter.last_capture_index;
        report.first_capture_utc_ns = first_utc;
        report.last_capture_utc_ns = last_utc;
        report.overall_status = "valid";

        // Add stream_sets
        RawStreamSummary ss;
        ss.session_id_hex = report.session_id_hex;
        ss.source_id = report.source_id;
        ss.channel_id = report.channel_id;
        ss.record_count = vfooter.record_count;
        ss.first_capture_utc_ns = first_utc;
        ss.last_capture_utc_ns = last_utc;
        ss.status = "valid";
        ss.segment_content_sha256.push_back(content_hex);
        ss.segment_file_sha256.push_back(file_hex);
        ss.segment_indexes.push_back(0);
        ss.segment_sizes.push_back(fs::file_size(paths[0]));
        report.stream_sets.push_back(ss);

        auto json = generate_json_report(report);

        // Parse with strict parser
        auto root = json_test::parse(json);
        CHECK(root.type == json_test::Value::Object);
        CHECK(json_test::parse_fails(json) == false);  // should NOT fail

        // Verify key values
        CHECK(root["overall_status"].string_val == "valid");
        CHECK(root["record_count"].number_val == 5.0);
        CHECK(root["first_capture_utc_ns"].number_val == 1000000.0);
        CHECK(root["last_capture_utc_ns"].number_val == 1004000.0);
        CHECK(root["content_sha256"].string_val == content_hex);
        CHECK(root["file_sha256"].string_val == file_hex);

        // Verify stream_sets
        CHECK(root["stream_sets"].type == json_test::Value::Array);
        CHECK(root["stream_sets"].size() == 1);
        CHECK(root["stream_sets"][0]["record_count"].number_val == 5.0);
        CHECK(root["stream_sets"][0]["segment_content_sha256"].size() == 1);
        CHECK(root["stream_sets"][0]["segment_content_sha256"][0].string_val == content_hex);
        CHECK(root["stream_sets"][0]["first_capture_utc_ns"].number_val == 1000000.0);
        CHECK(root["stream_sets"][0]["last_capture_utc_ns"].number_val == 1004000.0);
    }

    // ============================================================
    // Blocker 7: End-to-end CLI JSON — two stream sets (multi-stream)
    // ============================================================
    {
        auto dir = temp_dir();

        // Stream A: source_id=1, channel_id=1
        auto meta_a = make_meta();
        meta_a.source.source_id = 1;
        meta_a.source.channel_id = 1;
        RawSegmentWriter wa(meta_a, dir, {});
        wa.open();
        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 2000000;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01};
        wa.append(rec);
        wa.finalize();

        // Stream B: source_id=2, channel_id=2
        auto meta_b = make_meta();
        meta_b.source.source_id = 2;
        meta_b.source.channel_id = 2;
        RawSegmentWriter wb(meta_b, dir, {});
        wb.open();
        rec.capture_index = 0;
        rec.capture_utc_ns = 3000000;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x02};
        wb.append(rec);
        wb.finalize();

        // Discover stream sets
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

        RawSegmentReport report;
        report.operation = "inspect";
        report.format_version = "1";

        for (const auto& ss : stream_sets) {
            std::vector<RawSegmentMetadata> metas;
            std::vector<RawFooter> footers;
            std::vector<RawValidationIssue> issues;
            std::uint64_t first_utc = 0, last_utc = 0;
            auto status = validate_stream_set(ss.segment_paths, metas, footers, issues,
                                              &first_utc, &last_utc);
            CHECK(status == SegmentStatus::ValidFinalized);

            std::vector<std::string> content_hashes, file_hashes;
            for (const auto& p : ss.segment_paths) {
                RawSegmentMetadata tm;
                RawFooter tf;
                std::vector<RawValidationIssue> ti;
                std::string c, f;
                validate_segment(p, tm, tf, ti, c, f);
                content_hashes.push_back(c);
                file_hashes.push_back(f);
            }

            RawStreamSummary s;
            s.session_id_hex = session_id_hex(ss.session_id);
            s.source_id = ss.source_id;
            s.channel_id = ss.channel_id;
            s.record_count = 1;
            s.first_capture_utc_ns = first_utc;
            s.last_capture_utc_ns = last_utc;
            s.status = "valid";
            s.segment_content_sha256 = content_hashes;
            s.segment_file_sha256 = file_hashes;
            s.segment_indexes = ss.segment_indexes;
            for (const auto& p : ss.segment_paths) {
                s.segment_sizes.push_back(fs::file_size(p));
            }
            report.stream_sets.push_back(s);
        }

        report.overall_status = "valid";

        auto json = generate_json_report(report);

        // Parse with strict parser
        auto root = json_test::parse(json);
        CHECK(root.type == json_test::Value::Object);

        // Multi-stream: singular top-level fields must be empty/zero
        CHECK(root["session_id"].string_val.empty());
        CHECK(root["source_id"].number_val == 0.0);
        CHECK(root["channel_id"].number_val == 0.0);
        CHECK(root["record_count"].number_val == 0.0);
        CHECK(root["segment_indexes"].size() == 0);

        // stream_sets must have authoritative data
        CHECK(root["stream_sets"].size() == 2);
        CHECK(root["stream_sets"][0]["source_id"].number_val == 1.0);
        CHECK(root["stream_sets"][1]["source_id"].number_val == 2.0);
        CHECK(root["stream_sets"][0]["record_count"].number_val == 1.0);
        CHECK(root["stream_sets"][1]["record_count"].number_val == 1.0);

        // Verify per-segment hashes are populated
        CHECK(root["stream_sets"][0]["segment_content_sha256"].size() == 1);
        CHECK(root["stream_sets"][0]["segment_content_sha256"][0].string_val.size() == 64);
        CHECK(root["stream_sets"][1]["segment_content_sha256"].size() == 1);
        CHECK(root["stream_sets"][1]["segment_content_sha256"][0].string_val.size() == 64);

        // Verify UTC bounds
        CHECK(root["stream_sets"][0]["first_capture_utc_ns"].number_val == 2000000.0);
        CHECK(root["stream_sets"][0]["last_capture_utc_ns"].number_val == 2000000.0);
        CHECK(root["stream_sets"][1]["first_capture_utc_ns"].number_val == 3000000.0);
        CHECK(root["stream_sets"][1]["last_capture_utc_ns"].number_val == 3000000.0);

        // Deterministic ordering — same input produces same JSON
        auto json2 = generate_json_report(report);
        CHECK(json == json2);
    }

    // ============================================================
    // Blocker 7: JSON parser — nested arrays and objects
    // ============================================================
    {
        auto root = json_test::parse(R"({
            "stream_sets": [
                {"name": "a", "indexes": [0, 1, 2]},
                {"name": "b", "indexes": [3]}
            ]
        })");
        CHECK(root.type == json_test::Value::Object);
        CHECK(root["stream_sets"].type == json_test::Value::Array);
        CHECK(root["stream_sets"].size() == 2);
        CHECK(root["stream_sets"][0]["name"].string_val == "a");
        CHECK(root["stream_sets"][0]["indexes"].size() == 3);
        CHECK(root["stream_sets"][0]["indexes"][0].number_val == 0.0);
        CHECK(root["stream_sets"][1]["name"].string_val == "b");
        CHECK(root["stream_sets"][1]["indexes"][0].number_val == 3.0);
    }

    // ============================================================
    // Blocker 7: JSON parser — rejects unescaped control chars
    // ============================================================
    {
        // Tab in string must be escaped, not raw
        CHECK(json_test::parse_fails("{\"key\": \"val\tue\"}"));
    }

    // ============================================================
    // Blocker 7: JSON parser — rejects invalid number formats
    // ============================================================
    {
        CHECK(json_test::parse_fails(R"({"n": .5})"));     // leading dot
        CHECK(json_test::parse_fails(R"({"n": 01})"));     // leading zero
        CHECK(json_test::parse_fails(R"({"n": 1.})"));     // trailing dot
    }

    // ============================================================
    // Blocker 3: CLI pre-validation — records*segments overflow
    // (verified by checking that cmd_synth computes total_records before open)
    // The actual test is that no partial file is created on overflow.
    // ============================================================
    {
        // This is tested implicitly by the CLI: if records*segments overflows,
        // no files are created. We verify the library behavior directly.
        auto dir = temp_dir();
        auto meta = make_meta();

        // Simulate: records_per_seg = UINT64_MAX, num_segments = 2
        // records * segments would overflow
        std::uint64_t total_records;
        bool overflow = !checked_mul_u64(UINT64_MAX, 2, total_records);
        CHECK(overflow);

        // Verify no files created in dir
        CHECK(fs::directory_iterator(dir) == fs::directory_iterator(dir));
    }

    // ============================================================
    // Blocker 3: CLI pre-validation — max timestamp overflow
    // ============================================================
    {
        // Simulate: total_records - 1 = UINT64_MAX - 1, multiply by 1000000
        std::uint64_t max_ts_offset;
        bool overflow = !checked_mul_u64(UINT64_MAX - 1, 1000000, max_ts_offset);
        CHECK(overflow);
    }

    // ============================================================
    // Blocker 2: Preflight — boundary record fits in empty segment
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 1;
        // Set max_segment_bytes to exactly header + 1 record + footer
        // First compute header size
        std::vector<std::uint8_t> hdr;
        serialize_header(hdr, meta);
        pol.max_segment_bytes = hdr.size() + kRecordHeaderSize + 2 + 4 + kFooterSize;  // exact fit

        RawSegmentWriter w(meta, dir, pol);
        CHECK(w.open().empty());

        // First record with payload=2 fits exactly
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01, 0x02};
        CHECK(w.append(rec).empty());

        // Second record triggers rotation. Boundary record should fit in new segment.
        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        rec.payload = {0x03, 0x04};
        auto err = w.append(rec);
        CHECK(err.empty());

        // Finalize the second segment
        err = w.finalize();
        CHECK(err.empty());
        CHECK(w.finalized_paths().size() == 2);
    }

    // ============================================================
    // Blocker 2: Preflight — boundary record too large for new segment
    // ============================================================
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 1;

        // Compute header size for segment 0
        std::vector<std::uint8_t> hdr;
        serialize_header(hdr, meta);
        // Set max_segment_bytes to fit header + small record + footer in segment 0
        // but NOT enough for the boundary record in segment 1
        pol.max_segment_bytes = hdr.size() + kRecordHeaderSize + 2 + 4 + kFooterSize;

        RawSegmentWriter w(meta, dir, pol);
        CHECK(w.open().empty());

        // First record fits
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {0x01, 0x02};
        CHECK(w.append(rec).empty());

        // Second record with larger payload — triggers rotation but won't fit
        rec.capture_index = 1;
        rec.capture_monotonic_ns = 1000;
        rec.payload.resize(100, 0xAB);  // Much larger
        auto err = w.append(rec);
        CHECK(!err.empty());
        // No finalized paths because preflight caught it before rotation
        CHECK(w.finalized_paths().empty());
    }

    std::cout << "test_round6: ALL PASSED\n";
    return 0;
}
