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

namespace fs = std::filesystem;

// Portable null device redirect for stderr suppression
#ifdef _WIN32
static const char* kNullRedirect = " 2>NUL";
#else
static const char* kNullRedirect = " 2>/dev/null";
#endif

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

    std::cout << "test_cli: ALL PASSED\n";
    return 0;
}
