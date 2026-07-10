#ifdef _MSC_VER
#pragma warning(disable : 4189 4101 4309)
#endif

#include "test_check.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <utility>
#include <cstring>

namespace fs = std::filesystem;

// Portable null device redirect for stderr suppression
#ifdef _WIN32
static const char* kNullRedirect = " 2>NUL";
#else
static const char* kNullRedirect = " 2>/dev/null";
#endif

// ============================================================
// Minimal strict JSON parser for CLI output validation
// ============================================================

namespace cli_json {

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

}  // namespace cli_json

static std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    return std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
}

static std::string run_cmd(const std::string& cmd) {
    std::string result;
#ifdef _MSC_VER
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
#ifdef _MSC_VER
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

static int run_cmd_exit(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("cli_" + std::to_string(counter++));
    // Clean up if exists from previous run
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir.string();
}

int main() {
#ifdef MOEX_RAW_PATH
    std::string exe = MOEX_RAW_PATH;
#else
    std::string exe = "moex-raw";
#endif

    // --help
    {
        auto out = run_cmd(exe + " --help");
        CHECK(out.find("synth") != std::string::npos);
        CHECK(out.find("inspect") != std::string::npos);
        CHECK(out.find("replay") != std::string::npos);
    }

    // no args => non-zero
    {
        int rc = run_cmd_exit(exe + kNullRedirect);
        CHECK(rc != 0);
    }

    // unknown command => non-zero
    {
        int rc = run_cmd_exit(exe + " unknown" + kNullRedirect);
        CHECK(rc != 0);
    }

    // synth --help
    {
        auto out = run_cmd(exe + " synth --help");
        CHECK(out.find("--out") != std::string::npos);
        CHECK(out.find("--session") != std::string::npos);
    }

    // synth: missing --out
    {
        int rc = run_cmd_exit(exe + " synth" + kNullRedirect);
        CHECK(rc != 0);
    }

    // synth: valid
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/segments";
        int rc = run_cmd_exit(exe + " synth --out " + out_dir +
                              " --records 5 --payload-size 32");
        CHECK(rc == 0);
        CHECK(fs::exists(out_dir));

        // Check that .mxraw files were created
        int mxraw_count = 0;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") mxraw_count++;
        }
        CHECK(mxraw_count == 1);
    }

    // synth: multi-segment with rotation
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/segments_rot";
        int rc = run_cmd_exit(exe + " synth --out " + out_dir +
                              " --records 10 --max-records 3");
        CHECK(rc == 0);

        int mxraw_count = 0;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") mxraw_count++;
        }
        CHECK(mxraw_count >= 3);
    }

    // inspect --help
    {
        auto out = run_cmd(exe + " inspect --help");
        CHECK(out.find("--input") != std::string::npos);
    }

    // inspect: missing --input
    {
        int rc = run_cmd_exit(exe + " inspect" + kNullRedirect);
        CHECK(rc != 0);
    }

    // inspect: valid single file
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_seg";

        // Create segment first
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Find the .mxraw file
        std::string mxraw_path;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                mxraw_path = entry.path().string();
                break;
            }
        }

        int rc = run_cmd_exit(exe + " inspect --input " + mxraw_path);
        CHECK(rc == 0);
    }

    // inspect: directory
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_dir";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 5");

        int rc = run_cmd_exit(exe + " inspect --input " + out_dir);
        CHECK(rc == 0);
    }

    // inspect: JSON output
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_json";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        std::string mxraw_path;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                mxraw_path = entry.path().string();
                break;
            }
        }

        auto json_path = dir + "/report.json";
        int rc = run_cmd_exit(exe + " inspect --input " + mxraw_path + " --json-out " + json_path);
        CHECK(rc == 0);
        CHECK(fs::exists(json_path));

        // Verify JSON is valid (has expected fields)
        std::ifstream ifs(json_path);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        CHECK(content.find("schema_version") != std::string::npos);
        CHECK(content.find("overall_status") != std::string::npos);
    }

    // replay --help
    {
        auto out = run_cmd(exe + " replay --help");
        CHECK(out.find("--input") != std::string::npos);
        CHECK(out.find("--source") != std::string::npos);
    }

    // replay: missing --input
    {
        int rc = run_cmd_exit(exe + " replay" + kNullRedirect);
        CHECK(rc != 0);
    }

    // replay: valid stream
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/replay_seg";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 5");

        int rc = run_cmd_exit(exe + " replay --input " + out_dir);
        CHECK(rc == 0);
    }

    // replay: corrupt segment rejected
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/replay_corrupt";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Corrupt a segment
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                auto path = entry.path().string();
                std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
                f.seekp(100);
                unsigned char corrupt = 0xFF;
                f.write(reinterpret_cast<char*>(&corrupt), 1);
                f.close();
                break;
            }
        }

        int rc = run_cmd_exit(exe + " replay --input " + out_dir + kNullRedirect);
        CHECK(rc != 0);
    }

    // inspect: strict mode returns non-zero for invalid
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/strict_test";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Corrupt
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                auto path = entry.path().string();
                std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
                f.seekp(100);
                unsigned char corrupt = 0xFF;
                f.write(reinterpret_cast<char*>(&corrupt), 1);
                f.close();
                break;
            }
        }

        int rc = run_cmd_exit(exe + " inspect --input " + out_dir + " --strict" + kNullRedirect);
        CHECK(rc != 0);
    }

    // inspect: valid + corrupt .mxraw in directory — non-zero result
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/valid_corrupt";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Add a corrupt .mxraw file
        auto corrupt_path = out_dir + "/corrupt.mxraw";
        {
            std::ofstream ofs(corrupt_path, std::ios::binary);
            ofs << "NOT_A_VALID_MXRAW_FILE";
        }

        int rc = run_cmd_exit(exe + " inspect --input " + out_dir + kNullRedirect);
        CHECK(rc != 0);
    }

    // inspect: valid + .mxraw.partial in directory — reports partial warning
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/valid_partial";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Add a .mxraw.partial file
        auto partial_path = out_dir + "/incomplete.mxraw.partial";
        {
            std::ofstream ofs(partial_path, std::ios::binary);
            ofs << "partial_data";
        }

        // Should still succeed (partial is a warning, not error) but report the issue
        auto output = run_cmd(exe + " inspect --input " + out_dir);
        CHECK(output.find("PARTIAL_FILE") != std::string::npos);
    }

    // replay: valid + corrupt .mxraw in directory — non-zero result
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/replay_valid_corrupt";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Add a corrupt .mxraw file
        auto corrupt_path = out_dir + "/corrupt.mxraw";
        {
            std::ofstream ofs(corrupt_path, std::ios::binary);
            ofs << "NOT_A_VALID_MXRAW_FILE";
        }

        int rc = run_cmd_exit(exe + " replay --input " + out_dir + kNullRedirect);
        CHECK(rc != 0);
    }

    // replay: two sessions with same source/channel — ambiguity rejected
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/replay_ambig";

        // Create session A
        run_cmd_exit(exe + " synth --out " + out_dir +
                     " --session 0123456789abcdef0123456789abcdef --records 3");

        // Create session B (different session, same source/channel)
        run_cmd_exit(exe + " synth --out " + out_dir +
                     " --session fedcba9876543210fedcba9876543210 --records 3");

        int rc = run_cmd_exit(exe + " replay --input " + out_dir + kNullRedirect);
        CHECK(rc != 0);
    }

    // ============================================================
    // Round 7: Real CLI — single-file inspect JSON with strict parser
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_single";
        int rc = run_cmd_exit(exe + " synth --out " + out_dir + " --records 5");
        CHECK(rc == 0);

        std::string mxraw_path;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                mxraw_path = entry.path().string();
                break;
            }
        }
        CHECK(!mxraw_path.empty());

        auto json_path = dir + "/single_report.json";
        rc = run_cmd_exit(exe + " inspect --input " + mxraw_path + " --json-out " + json_path);
        CHECK(rc == 0);
        CHECK(fs::exists(json_path));

        auto content = read_file(json_path);
        CHECK(!content.empty());

        // Parse with strict parser — rejects malformed JSON
        auto root = cli_json::parse(content);
        CHECK(root.type == cli_json::Value::Object);

        // Verify key fields (single-file: no stream_sets, only singular top-level)
        CHECK(root["schema_version"].string_val == "1.0");
        CHECK(root["overall_status"].string_val == "valid");
        CHECK(root["record_count"].number_val == 5.0);
        CHECK(root["content_sha256"].string_val.size() == 64);
        CHECK(root["file_sha256"].string_val.size() == 64);
        CHECK(root["segment_indexes"].size() == 1);
        CHECK(root["segment_sizes"].size() == 1);
    }

    // ============================================================
    // Round 7: Real CLI — one-stream directory inspect JSON
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_dir_stream";
        int rc = run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");
        CHECK(rc == 0);

        auto json_path = dir + "/dir_report.json";
        rc = run_cmd_exit(exe + " inspect --input " + out_dir + " --json-out " + json_path);
        CHECK(rc == 0);
        CHECK(fs::exists(json_path));

        auto content = read_file(json_path);
        auto root = cli_json::parse(content);
        CHECK(root.type == cli_json::Value::Object);
        CHECK(root["overall_status"].string_val == "valid");
        CHECK(root["stream_sets"].type == cli_json::Value::Array);
        CHECK(root["stream_sets"].size() == 1);
        CHECK(root["stream_sets"][0]["record_count"].number_val == 3.0);
    }

    // ============================================================
    // Round 7: Real CLI — two-stream directory inspect JSON
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_two_streams";

        // Stream A: source=1, channel=1
        run_cmd_exit(exe + " synth --out " + out_dir +
                     " --source 0000000000000001 --channel 0000000000000001 --records 3");
        // Stream B: source=2, channel=2
        run_cmd_exit(exe + " synth --out " + out_dir +
                     " --source 0000000000000002 --channel 0000000000000002 --records 3");

        auto json_path = dir + "/two_stream_report.json";
        int rc = run_cmd_exit(exe + " inspect --input " + out_dir + " --json-out " + json_path);
        CHECK(rc == 0);
        CHECK(fs::exists(json_path));

        auto content = read_file(json_path);
        auto root = cli_json::parse(content);
        CHECK(root.type == cli_json::Value::Object);
        CHECK(root["overall_status"].string_val == "valid");

        // Multi-stream: singular top-level fields empty/zero
        CHECK(root["session_id"].string_val.empty());
        CHECK(root["source_id"].number_val == 0.0);
        CHECK(root["channel_id"].number_val == 0.0);
        CHECK(root["record_count"].number_val == 0.0);

        // stream_sets has authoritative data
        CHECK(root["stream_sets"].type == cli_json::Value::Array);
        CHECK(root["stream_sets"].size() == 2);
        CHECK(root["stream_sets"][0]["record_count"].number_val == 3.0);
        CHECK(root["stream_sets"][1]["record_count"].number_val == 3.0);
        CHECK(root["stream_sets"][0]["source_id"].number_val != root["stream_sets"][1]["source_id"].number_val);
    }

    // ============================================================
    // Round 7: Real CLI — strict partial/corrupt exit codes
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/strict_exit";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        // Corrupt a segment
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                auto path = entry.path().string();
                std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
                f.seekp(100);
                unsigned char corrupt = 0xFF;
                f.write(reinterpret_cast<char*>(&corrupt), 1);
                break;
            }
        }

        // Strict mode: corrupt → non-zero exit
        int rc = run_cmd_exit(exe + " inspect --input " + out_dir + " --strict" + kNullRedirect);
        CHECK(rc != 0);
    }

    // ============================================================
    // Round 7: Real CLI — synth records*segments overflow, output dir empty
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/synth_overflow";

        // UINT64_MAX records * 2 segments → overflow
        int rc = run_cmd_exit(exe + " synth --out " + out_dir +
                              " --records 18446744073709551615 --segments 2" + kNullRedirect);
        CHECK(rc != 0);

        // Output directory must be empty (no .mxraw or .partial created)
        bool found_file = false;
        if (fs::exists(out_dir)) {
            for (const auto& entry : fs::directory_iterator(out_dir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".mxraw" || entry.path().string().find(".mxraw.partial") != std::string::npos) {
                    found_file = true;
                }
            }
        }
        CHECK(!found_file);
    }

    // ============================================================
    // Round 7: Real CLI — synth timestamp overflow, output dir empty
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/synth_ts_overflow";

        // (UINT64_MAX - 1) * 1000000 → timestamp overflow
        int rc = run_cmd_exit(exe + " synth --out " + out_dir +
                              " --records 18446744073709551614" + kNullRedirect);
        CHECK(rc != 0);

        bool found_file = false;
        if (fs::exists(out_dir)) {
            for (const auto& entry : fs::directory_iterator(out_dir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".mxraw" || entry.path().string().find(".mxraw.partial") != std::string::npos) {
                    found_file = true;
                }
            }
        }
        CHECK(!found_file);
    }

    // ============================================================
    // Round 7: Real CLI — invalid JSON output path
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_json_invalid";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 3");

        std::string mxraw_path;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                mxraw_path = entry.path().string();
                break;
            }
        }

        // Write to a non-existent nested path (should fail)
        auto bad_json_path = dir + "/nonexistent/deep/path/report.json";
        int rc = run_cmd_exit(exe + " inspect --input " + mxraw_path +
                              " --json-out " + bad_json_path + kNullRedirect);
        CHECK(rc != 0);
    }

    // ============================================================
    // Round 7: Real CLI — deterministic repeated JSON output
    // ============================================================
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_deterministic";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 5");

        std::string mxraw_path;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") {
                mxraw_path = entry.path().string();
                break;
            }
        }

        auto json1_path = dir + "/report1.json";
        auto json2_path = dir + "/report2.json";

        int rc1 = run_cmd_exit(exe + " inspect --input " + mxraw_path + " --json-out " + json1_path);
        int rc2 = run_cmd_exit(exe + " inspect --input " + mxraw_path + " --json-out " + json2_path);
        CHECK(rc1 == 0);
        CHECK(rc2 == 0);

        auto content1 = read_file(json1_path);
        auto content2 = read_file(json2_path);
        CHECK(content1 == content2);

        // Verify strict parser accepts it
        auto root = cli_json::parse(content1);
        CHECK(root.type == cli_json::Value::Object);
        CHECK(root["overall_status"].string_val == "valid");
    }

    std::cout << "test_cli: ALL PASSED\n";
    return 0;
}
