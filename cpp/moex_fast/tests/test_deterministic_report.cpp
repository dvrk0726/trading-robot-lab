#include "moex_fast/inspector.hpp"
#include "moex_fast/report.hpp"
#include <cassert>
#include <iostream>
#include <string>

namespace {

void test_deterministic_json() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report1 = moex_fast::run_inspector(opts);
    auto report2 = moex_fast::run_inspector(opts);

    auto json1 = moex_fast::report_to_json(report1);
    auto json2 = moex_fast::report_to_json(report2);

    assert(json1 == json2);

    std::cout << "PASS: deterministic JSON output\n";
}

void test_schema_version_present() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    assert(!report.schema_version.empty());

    auto json = moex_fast::report_to_json(report);
    assert(json.find("schema_version") != std::string::npos);

    std::cout << "PASS: schema version present\n";
}

void test_overall_status_valid() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    // With valid synthetic fixtures, status should be valid or warning
    assert(report.overall_status == "valid" || report.overall_status == "warning");

    std::cout << "PASS: overall status for valid input\n";
}

void test_overall_status_invalid() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/nonexistent.xml";
    opts.templates_path = "fixtures/nonexistent.xml";

    auto report = moex_fast::run_inspector(opts);
    assert(report.overall_status == "invalid");

    std::cout << "PASS: overall status for invalid input\n";
}

void test_strict_vs_nonstrict() {
    // With missing files, strict and non-strict should both report errors
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/nonexistent.xml";
    opts.templates_path = "fixtures/nonexistent.xml";

    opts.strict = true;
    auto r1 = moex_fast::run_inspector(opts);

    opts.strict = false;
    auto r2 = moex_fast::run_inspector(opts);

    // Both should report errors for missing files
    assert(r1.overall_status == "invalid");
    assert(r2.overall_status == "invalid");

    std::cout << "PASS: strict vs non-strict mode\n";
}

void test_template_ordering() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // Templates in JSON should be sorted by ID for determinism
    // Find positions of template IDs in the JSON
    auto pos29 = json.find("\"id\": 29");
    auto pos30 = json.find("\"id\": 30");
    auto pos31 = json.find("\"id\": 31");

    if (pos29 != std::string::npos && pos30 != std::string::npos) {
        assert(pos29 < pos30);
    }
    if (pos30 != std::string::npos && pos31 != std::string::npos) {
        assert(pos30 < pos31);
    }

    std::cout << "PASS: template ordering in JSON\n";
}

void test_json_valid_syntax() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // Basic JSON syntax checks
    assert(json[0] == '{');
    assert(json.find("overall_status") != std::string::npos);
    // Check balanced braces
    int depth = 0;
    for (char c : json) {
        if (c == '{' || c == '[') ++depth;
        if (c == '}' || c == ']') --depth;
        assert(depth >= 0);
    }
    assert(depth == 0);

    std::cout << "PASS: JSON valid syntax\n";
}

void test_text_output() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto text = moex_fast::report_to_text(report);

    assert(!text.empty());
    assert(text.find("MOEX FAST") != std::string::npos);
    assert(text.find("Overall status") != std::string::npos);

    std::cout << "PASS: text output\n";
}

}  // namespace

int main() {
    test_deterministic_json();
    test_schema_version_present();
    test_overall_status_valid();
    test_overall_status_invalid();
    test_strict_vs_nonstrict();
    test_template_ordering();
    test_json_valid_syntax();
    test_text_output();

    std::cout << "\nAll deterministic report tests PASSED.\n";
    return 0;
}
