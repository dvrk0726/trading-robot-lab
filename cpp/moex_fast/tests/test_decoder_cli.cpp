#include "test_check.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <sstream>

// CLI test using the moex-fast-decode binary
// Defined via CMake: MOEX_FAST_DECODE_PATH

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static int run_cmd(const std::string& cmd, std::string& stdout_out, std::string& /*stderr_out*/) {
    stdout_out.clear();

    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) return -1;

    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        stdout_out += buf;
    }
    int ret = pclose(pipe);
#ifdef _WIN32
    return ret;
#else
    return WEXITSTATUS(ret);
#endif
}

static void test_valid_hex_decode() {
    // Create a minimal templates file
    std::string tmpl_path = "test_decode_cli_templates.xml";
    {
        std::ofstream ofs(tmpl_path);
        ofs << R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <string name="Const" id="1"><constant>X</constant></string>
    <uInt32 name="Val" id="2"/>
  </template>
</templates>)";
    }

    // pmap: [tmpl-id=1] -> 0xC0, tmpl=10->0x8A, Const "X" (no wire bytes), Val=42->0xAA
    std::string cmd = std::string(MOEX_FAST_DECODE_PATH) +
                      " --templates " + tmpl_path +
                      " --hex C08AAA --exact";

    std::string out, err;
    int ret = run_cmd(cmd, out, err);
    CHECK_EQ(ret, 0);
    CHECK(out.find("Status: ok") != std::string::npos);
    CHECK(out.find("Template: 10") != std::string::npos);

    std::filesystem::remove(tmpl_path);
    TEST_PASS("valid_hex_decode");
}

static void test_invalid_hex() {
    std::string tmpl_path = "test_decode_cli_templates.xml";
    {
        std::ofstream ofs(tmpl_path);
        ofs << R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";
    }

    std::string cmd = std::string(MOEX_FAST_DECODE_PATH) +
                      " --templates " + tmpl_path +
                      " --hex ZZZZ";

    std::string out, err;
    int ret = run_cmd(cmd, out, err);
    CHECK_NE(ret, 0);

    std::filesystem::remove(tmpl_path);
    TEST_PASS("invalid_hex");
}

static void test_missing_templates() {
    std::string cmd = std::string(MOEX_FAST_DECODE_PATH) +
                      " --templates nonexistent.xml --hex 80";

    std::string out, err;
    int ret = run_cmd(cmd, out, err);
    CHECK_NE(ret, 0);

    TEST_PASS("missing_templates");
}

int main() {
    test_valid_hex_decode();
    test_invalid_hex();
    test_missing_templates();
    std::cout << "All decoder CLI tests passed.\n";
    return 0;
}
