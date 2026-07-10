#include "moex_fast/inspector.hpp"
#include "moex_fast/report.hpp"
#include "test_helpers.hpp"
#include <iostream>
#include <fstream>
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
    // Create a fixture with FUT-INFO but missing ORDERS-LOG
    write_temp_file("strict_test_config.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<configuration>\n"
        "  <MarketDataGroup feedType='FUT-INFO' marketID='D' label='Futures defintion'>\n"
        "    <connections>\n"
        "      <connection>\n"
        "        <type>Instrument Replay</type>\n"
        "        <protocol>UDP/IP</protocol>\n"
        "        <src-ip>192.0.2.1</src-ip>\n"
        "        <ip>233.0.0.11</ip>\n"
        "        <port>48011</port>\n"
        "        <feed>A</feed>\n"
        "      </connection>\n"
        "      <connection>\n"
        "        <type>Instrument Replay</type>\n"
        "        <protocol>UDP/IP</protocol>\n"
        "        <src-ip>192.0.2.2</src-ip>\n"
        "        <ip>233.0.0.12</ip>\n"
        "        <port>49011</port>\n"
        "        <feed>B</feed>\n"
        "      </connection>\n"
        "    </connections>\n"
        "  </MarketDataGroup>\n"
        "  <MarketDataGroup feedType='ORDERS-LOG' marketID='D' label='Full orders log'>\n"
        "    <connections>\n"
        "      <connection>\n"
        "        <type>Incremental</type>\n"
        "        <protocol>UDP/IP</protocol>\n"
        "        <src-ip>192.0.2.1</src-ip>\n"
        "        <ip>233.0.0.40</ip>\n"
        "        <port>48040</port>\n"
        "        <feed>A</feed>\n"
        "      </connection>\n"
        "      <connection>\n"
        "        <type>Incremental</type>\n"
        "        <protocol>UDP/IP</protocol>\n"
        "        <src-ip>192.0.2.2</src-ip>\n"
        "        <ip>233.0.0.41</ip>\n"
        "        <port>49040</port>\n"
        "        <feed>B</feed>\n"
        "      </connection>\n"
        "    </connections>\n"
        "  </MarketDataGroup>\n"
        "</configuration>\n");

    std::string config_path = temp_path("strict_test_config.xml");

    // Non-strict: missing Snapshot A/B is a warning
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = config_path;
        opts.templates_path = "fixtures/synthetic_templates.xml";
        opts.strict = false;

        auto r = moex_fast::run_inspector(opts);
        CHECK(r.overall_status == "warning");

        bool found_snap_warning = false;
        for (const auto& fr : r.required_feed_results) {
            if (fr.name == "ORDERS-LOG-Snap-A") {
                CHECK(!fr.present);
                CHECK(fr.severity == moex_fast::Severity::Warning);
                found_snap_warning = true;
            }
        }
        CHECK(found_snap_warning);
    }

    // Strict: missing Snapshot A/B is an error
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = config_path;
        opts.templates_path = "fixtures/synthetic_templates.xml";
        opts.strict = true;

        auto r = moex_fast::run_inspector(opts);
        CHECK(r.overall_status == "invalid");

        bool found_snap_error = false;
        for (const auto& fr : r.required_feed_results) {
            if (fr.name == "ORDERS-LOG-Snap-A") {
                CHECK(!fr.present);
                CHECK(fr.severity == moex_fast::Severity::Error);
                found_snap_error = true;
            }
        }
        CHECK(found_snap_error);
    }

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

    // Should have 7 required template checks (ID+name pairs)
    CHECK(report.required_template_results.size() == 7);
    for (const auto& r : report.required_template_results) {
        CHECK(r.present);  // All required templates are in the fixture
    }

    // Should have 7 required feed checks
    CHECK(report.required_feed_results.size() == 7);

    TEST_PASS("required check results populated");
}

void test_required_template_pair_names() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);

    // Verify the required template result names include both ID and name
    bool found_29 = false;
    for (const auto& r : report.required_template_results) {
        if (r.name.find("29") != std::string::npos &&
            r.name.find("OrdersLogMessage") != std::string::npos) {
            found_29 = true;
            CHECK(r.present);
        }
    }
    CHECK(found_29);

    TEST_PASS("required template pair names include ID and name");
}

void test_endpoint_role_in_json() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // endpoint_role should appear in endpoint objects
    CHECK(json.find("\"endpoint_role\"") != std::string::npos);
    // Check that all roles appear
    CHECK(json.find("Incremental") != std::string::npos);
    CHECK(json.find("Snapshot") != std::string::npos);
    CHECK(json.find("Historical Replay") != std::string::npos);

    TEST_PASS("endpoint_role in JSON");
}

void test_label_in_json() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // label should appear in feed group objects
    CHECK(json.find("\"label\"") != std::string::npos);
    // feedType should also appear
    CHECK(json.find("\"feedType\"") != std::string::npos);

    TEST_PASS("label and feedType in JSON");
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
    test_required_template_pair_names();
    test_endpoint_role_in_json();
    test_label_in_json();
    test_parent_sequence_in_json();

    std::cout << "\nAll deterministic report tests PASSED.\n";
    return 0;
}
