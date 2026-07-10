#include "moex_fast/inspector.hpp"
#include "moex_fast/report.hpp"
#include "test_helpers.hpp"
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

    CHECK(json1 == json2);

    TEST_PASS("deterministic JSON output");
}

void test_schema_version_present() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    CHECK(!report.schema_version.empty());

    auto json = moex_fast::report_to_json(report);
    CHECK(json.find("schema_version") != std::string::npos);

    TEST_PASS("schema version present");
}

void test_overall_status_valid() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.overall_status == "valid" || report.overall_status == "warning");

    TEST_PASS("overall status for valid input");
}

void test_overall_status_invalid() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/nonexistent.xml";
    opts.templates_path = "fixtures/nonexistent.xml";

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.overall_status == "invalid");

    TEST_PASS("overall status for invalid input");
}

void test_strict_vs_nonstrict() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    opts.strict = true;
    auto r1 = moex_fast::run_inspector(opts);

    opts.strict = false;
    auto r2 = moex_fast::run_inspector(opts);

    // With valid fixtures, both should be valid or warning
    CHECK(r1.overall_status == "valid" || r1.overall_status == "warning");
    CHECK(r2.overall_status == "valid" || r2.overall_status == "warning");

    TEST_PASS("strict vs non-strict mode");
}

void test_template_ordering() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    auto pos29 = json.find("\"id\": 29");
    auto pos30 = json.find("\"id\": 30");
    auto pos31 = json.find("\"id\": 31");

    if (pos29 != std::string::npos && pos30 != std::string::npos) {
        CHECK(pos29 < pos30);
    }
    if (pos30 != std::string::npos && pos31 != std::string::npos) {
        CHECK(pos30 < pos31);
    }

    TEST_PASS("template ordering in JSON");
}

void test_json_valid_syntax() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    CHECK(json[0] == '{');
    CHECK(json.find("overall_status") != std::string::npos);
    int depth = 0;
    for (char c : json) {
        if (c == '{' || c == '[') ++depth;
        if (c == '}' || c == ']') --depth;
        CHECK(depth >= 0);
    }
    CHECK(depth == 0);

    TEST_PASS("JSON valid syntax");
}

void test_text_output() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto text = moex_fast::report_to_text(report);

    CHECK(!text.empty());
    CHECK(text.find("MOEX FAST") != std::string::npos);
    CHECK(text.find("Overall status") != std::string::npos);

    TEST_PASS("text output");
}

void test_required_templates_in_json() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    CHECK(json.find("required_templates") != std::string::npos);
    CHECK(json.find("required_feeds") != std::string::npos);
    CHECK(json.find("template-29") != std::string::npos);
    CHECK(json.find("ORDERS-LOG") != std::string::npos);

    TEST_PASS("required templates and feeds in JSON");
}

void test_required_checks_present() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);

    // Should have 7 required template checks
    CHECK(report.required_template_results.size() == 7);
    for (const auto& r : report.required_template_results) {
        CHECK(r.present);  // All required templates are in the fixture
    }

    // Should have 7 required feed checks
    CHECK(report.required_feed_results.size() == 7);

    TEST_PASS("required check results populated");
}

void test_feed_type_in_endpoint_json() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // feed_type should be inside endpoint objects, not at group level
    CHECK(json.find("\"feed_type\"") != std::string::npos);
    // Check that Incremental, Snapshot, Historical Replay all appear
    CHECK(json.find("Incremental") != std::string::npos);
    CHECK(json.find("Snapshot") != std::string::npos);
    CHECK(json.find("Historical Replay") != std::string::npos);

    TEST_PASS("feed_type in endpoint JSON");
}

void test_parent_sequence_in_json() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // parent_sequence should appear for nested fields
    CHECK(json.find("\"parent_sequence\"") != std::string::npos);

    TEST_PASS("parent_sequence in JSON");
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
    test_required_templates_in_json();
    test_required_checks_present();
    test_feed_type_in_endpoint_json();
    test_parent_sequence_in_json();

    std::cout << "\nAll deterministic report tests PASSED.\n";
    return 0;
}
