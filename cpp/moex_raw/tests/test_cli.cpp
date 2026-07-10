#ifdef _MSC_VER
#pragma warning(disable : 4189 4101 4309)
#endif

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

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
    auto dir = fs::temp_directory_path() / "moex_raw_test" / ("cli_" + std::to_string(counter++));
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
        assert(out.find("synth") != std::string::npos);
        assert(out.find("inspect") != std::string::npos);
        assert(out.find("replay") != std::string::npos);
    }

    // no args => non-zero
    {
        int rc = run_cmd_exit(exe + " 2>NUL");
        assert(rc != 0);
    }

    // unknown command => non-zero
    {
        int rc = run_cmd_exit(exe + " unknown 2>NUL");
        assert(rc != 0);
    }

    // synth --help
    {
        auto out = run_cmd(exe + " synth --help");
        assert(out.find("--out") != std::string::npos);
        assert(out.find("--session") != std::string::npos);
    }

    // synth: missing --out
    {
        int rc = run_cmd_exit(exe + " synth 2>NUL");
        assert(rc != 0);
    }

    // synth: valid
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/segments";
        int rc = run_cmd_exit(exe + " synth --out " + out_dir +
                              " --records 5 --payload-size 32");
        assert(rc == 0);
        assert(fs::exists(out_dir));

        // Check that .mxraw files were created
        int mxraw_count = 0;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") mxraw_count++;
        }
        assert(mxraw_count == 1);
    }

    // synth: multi-segment with rotation
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/segments_rot";
        int rc = run_cmd_exit(exe + " synth --out " + out_dir +
                              " --records 10 --max-records 3");
        assert(rc == 0);

        int mxraw_count = 0;
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            if (entry.path().extension() == ".mxraw") mxraw_count++;
        }
        assert(mxraw_count >= 3);
    }

    // inspect --help
    {
        auto out = run_cmd(exe + " inspect --help");
        assert(out.find("--input") != std::string::npos);
    }

    // inspect: missing --input
    {
        int rc = run_cmd_exit(exe + " inspect 2>NUL");
        assert(rc != 0);
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
        assert(rc == 0);
    }

    // inspect: directory
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/inspect_dir";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 5");

        int rc = run_cmd_exit(exe + " inspect --input " + out_dir);
        assert(rc == 0);
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
        assert(rc == 0);
        assert(fs::exists(json_path));

        // Verify JSON is valid (has expected fields)
        std::ifstream ifs(json_path);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        assert(content.find("schema_version") != std::string::npos);
        assert(content.find("overall_status") != std::string::npos);
    }

    // replay --help
    {
        auto out = run_cmd(exe + " replay --help");
        assert(out.find("--input") != std::string::npos);
        assert(out.find("--source") != std::string::npos);
    }

    // replay: missing --input
    {
        int rc = run_cmd_exit(exe + " replay 2>NUL");
        assert(rc != 0);
    }

    // replay: valid stream
    {
        auto dir = temp_dir();
        auto out_dir = dir + "/replay_seg";
        run_cmd_exit(exe + " synth --out " + out_dir + " --records 5");

        int rc = run_cmd_exit(exe + " replay --input " + out_dir);
        assert(rc == 0);
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

        int rc = run_cmd_exit(exe + " replay --input " + out_dir + " 2>NUL");
        assert(rc != 0);
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

        int rc = run_cmd_exit(exe + " inspect --input " + out_dir + " --strict 2>NUL");
        assert(rc != 0);
    }

    std::cout << "test_cli: ALL PASSED\n";
    return 0;
}
