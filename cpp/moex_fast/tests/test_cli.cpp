// CLI integration tests. These tests run the moex-fast-inspect executable
// and verify exit codes and output behavior.
// Tests remain effective in Release mode (no assert dependency).

#include "test_helpers.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdio>

#ifdef _WIN32
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif

#ifndef MOEX_FAST_INSPECT_PATH
#define MOEX_FAST_INSPECT_PATH "moex-fast-inspect"
#endif

namespace {

std::string cli_path() {
    return MOEX_FAST_INSPECT_PATH;
}

int run_cmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

void test_help_flag() {
    int rc = run_cmd("\"" + cli_path() + "\" --help > " + NULL_DEVICE " 2>&1");
    CHECK(rc == 0);
    TEST_PASS("--help exits 0");
}

void test_no_args() {
    int rc = run_cmd("\"" + cli_path() + "\" > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("no arguments exits non-zero");
}

void test_missing_configuration() {
    int rc = run_cmd("\"" + cli_path() + "\" --templates fixtures/synthetic_templates.xml > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("missing --configuration exits non-zero");
}

void test_missing_templates() {
    int rc = run_cmd("\"" + cli_path() + "\" --configuration fixtures/synthetic_configuration.xml > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("missing --templates exits non-zero");
}

void test_missing_files() {
    int rc = run_cmd("\"" + cli_path() + "\" --configuration nonexistent.xml --templates nonexistent.xml > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("missing files exits non-zero");
}

void test_valid_input_no_json() {
    int rc = run_cmd("\"" + cli_path() + "\" --configuration fixtures/synthetic_configuration.xml --templates fixtures/synthetic_templates.xml > " + NULL_DEVICE " 2>&1");
    CHECK(rc == 0);
    TEST_PASS("valid input without --json-out exits 0");
}

void test_valid_input_with_json() {
    std::string json_out = "fixtures/test_cli_output.json";
    int rc = run_cmd("\"" + cli_path() + "\" --configuration fixtures/synthetic_configuration.xml --templates fixtures/synthetic_templates.xml --json-out " + json_out + " > " + NULL_DEVICE " 2>&1");
    CHECK(rc == 0);

    std::ifstream f(json_out);
    CHECK(f.good());
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    CHECK(!content.empty());
    CHECK(content[0] == '{');

    std::remove(json_out.c_str());
    TEST_PASS("valid input with --json-out exits 0");
}

void test_strict_mode() {
    int rc = run_cmd("\"" + cli_path() + "\" --configuration fixtures/synthetic_configuration.xml --templates fixtures/synthetic_templates.xml --strict > " + NULL_DEVICE " 2>&1");
    CHECK(rc == 0);
    TEST_PASS("--strict mode with valid input exits 0");
}

void test_nonstrict_mode() {
    int rc = run_cmd("\"" + cli_path() + "\" --configuration fixtures/synthetic_configuration.xml --templates fixtures/synthetic_templates.xml > " + NULL_DEVICE " 2>&1");
    CHECK(rc == 0);
    TEST_PASS("non-strict mode with valid input exits 0");
}

void test_invalid_output_path() {
    int rc = run_cmd("\"" + cli_path() + "\" --configuration fixtures/synthetic_configuration.xml --templates fixtures/synthetic_templates.xml --json-out nonexistent_dir/report.json > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("invalid output path exits non-zero");
}

void test_unknown_argument() {
    int rc = run_cmd("\"" + cli_path() + "\" --unknown-flag > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("unknown argument exits non-zero");
}

void test_invalid_profile_value() {
    int rc = run_cmd(
        "\"" + cli_path() + "\""
        " --configuration fixtures/synthetic_configuration.xml"
        " --templates fixtures/synthetic_templates.xml"
        " --profile bogus-profile > " + NULL_DEVICE " 2>&1");
    CHECK(rc != 0);
    TEST_PASS("unsupported --profile value exits non-zero");
}

}  // namespace

int main() {
    test_help_flag();
    test_no_args();
    test_missing_configuration();
    test_missing_templates();
    test_missing_files();
    test_valid_input_no_json();
    test_valid_input_with_json();
    test_strict_mode();
    test_nonstrict_mode();
    test_invalid_output_path();
    test_unknown_argument();
    test_invalid_profile_value();

    std::cout << "\nAll CLI tests PASSED.\n";
    return 0;
}
