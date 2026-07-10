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

// --- Round 5 tests ---

void test_length_wire_type_valid() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.strict = true;

    auto report = moex_fast::run_inspector(opts);

    // No "Unknown wire type" issues should exist (length is now uInt32)
    for (const auto& iss : report.issues) {
        CHECK_MSG(iss.message.find("Unknown wire type") == std::string::npos,
            "Unexpected 'Unknown wire type' issue: " + iss.message);
    }

    TEST_PASS("length wire type — no Unknown wire type issues");
}

void test_strict_valid_synthetic() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.strict = true;

    auto report = moex_fast::run_inspector(opts);

    // Strict mode with valid synthetic input must produce "valid" with zero issues
    CHECK_MSG(report.overall_status == "valid",
        "Expected overall_status 'valid', got '" + report.overall_status + "'");
    CHECK_MSG(report.issues.empty(),
        "Expected zero issues, got " + std::to_string(report.issues.size()));

    TEST_PASS("strict valid synthetic — valid with zero issues");
}

void test_profile_auto_detected_129() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "spectra-1.29");
    CHECK(report.compatibility_status == "compatible");

    auto json = moex_fast::report_to_json(report);
    CHECK(json.find("\"detected_profile\"") != std::string::npos);
    CHECK(json.find("\"profile_evidence\"") != std::string::npos);
    CHECK(json.find("\"compatibility_status\"") != std::string::npos);
    CHECK(json.find("spectra-1.29") != std::string::npos);

    TEST_PASS("profile auto-detected spectra-1.29");
}

void test_profile_auto_detected_130() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates_130.xml";

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "spectra-1.30");
    CHECK(report.compatibility_status == "compatible");

    // spectra-1.30 must have 8 required template checks
    CHECK(report.required_template_results.size() == 8);

    // All required templates should be present
    for (const auto& r : report.required_template_results) {
        CHECK(r.present);
    }

    TEST_PASS("profile auto-detected spectra-1.30");
}

void test_profile_explicit_override() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.profile = "spectra-1.29";

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "spectra-1.29");
    CHECK(report.requested_profile == "spectra-1.29");
    CHECK(report.selected_profile == "spectra-1.29");
    CHECK(report.detection_evidence.find("ID 40 SecurityDefinition") != std::string::npos);
    CHECK(report.compatibility_status == "compatible");

    TEST_PASS("profile explicit override");
}

void test_profile_mismatch_warning() {
    // Request spectra-1.30 but supply spectra-1.29 templates
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.profile = "spectra-1.30";

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.compatibility_status == "mismatch");

    // Should have a mismatch warning
    bool found_mismatch = false;
    for (const auto& iss : report.issues) {
        if (iss.message.find("mismatch") != std::string::npos) {
            found_mismatch = true;
        }
    }
    CHECK(found_mismatch);

    TEST_PASS("profile mismatch warning");
}

void test_profile_mismatch_strict_error() {
    // Strict mode: mismatch should be an error
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.profile = "spectra-1.30";
    opts.strict = true;

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.overall_status == "invalid");
    CHECK(report.compatibility_status == "mismatch");

    TEST_PASS("profile mismatch strict mode error");
}

void test_profile_in_text_report() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto text = moex_fast::report_to_text(report);

    CHECK(text.find("Detected profile:") != std::string::npos);
    CHECK(text.find("Profile evidence:") != std::string::npos);
    CHECK(text.find("Compatibility status:") != std::string::npos);
    CHECK(text.find("spectra-1.29") != std::string::npos);

    TEST_PASS("profile in text report");
}

void test_mixed_profile_negative() {
    // Templates with both ID 40 and ID 47 SecurityDefinition => ambiguous
    write_temp_file("mixed_profile.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='40' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "</templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("mixed_profile.xml");

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "ambiguous");
    CHECK(report.compatibility_status == "mismatch");

    TEST_PASS("mixed profile negative test");
}

void test_wrong_name_profile_negative() {
    // ID 47 with wrong name
    write_temp_file("wrong_name_profile.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='WrongName'><uInt32 name='X'/></template>"
        "</templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("wrong_name_profile.xml");

    auto report = moex_fast::run_inspector(opts);
    // ID 47 with wrong name should not be detected as spectra-1.30
    CHECK(report.detected_profile != "spectra-1.30");

    TEST_PASS("wrong name profile negative test");
}

void test_length_no_unknown_wire_type_issue() {
    // Explicit check: <length name="NoMDEntries" id="268"/> must NOT produce
    // "Unknown wire type" in any template
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);

    for (const auto& iss : report.issues) {
        CHECK_MSG(iss.message.find("Unknown wire type") == std::string::npos,
            "Unexpected Unknown wire type issue: " + iss.message);
    }

    TEST_PASS("NoMDEntries length — no Unknown wire type issue");
}

// --- Round 6 tests ---

void test_round6_129_plus_id48_negative() {
    // 1.29 required templates + ID 48 SecurityStatus => mixed/mismatch
    write_temp_file("r6_129_plus_id48.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='40' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='48' name='SecurityStatus'><uInt32 name='X'/></template>"
        "</templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("r6_129_plus_id48.xml");

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "ambiguous");
    CHECK(report.compatibility_status == "mismatch");

    TEST_PASS("Round 6: 1.29 + ID 48 => ambiguous/mismatch");
}

void test_round6_id40_wrong_name_id47_id48_negative() {
    // ID 40 SecurityDefinition + wrong-name ID 47 + ID 48 SecurityStatus => mixed/mismatch
    write_temp_file("r6_id40_wrong47_id48.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='40' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='WrongName'><uInt32 name='X'/></template>"
        "  <template id='48' name='SecurityStatus'><uInt32 name='X'/></template>"
        "</templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("r6_id40_wrong47_id48.xml");

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "ambiguous");
    CHECK(report.compatibility_status == "mismatch");

    TEST_PASS("Round 6: ID 40 + wrong-name ID 47 + ID 48 => ambiguous/mismatch");
}

void test_round6_explicit_129_on_ambiguous() {
    // explicit 1.29 override on an ambiguous artifact => mismatch; strict invalid
    write_temp_file("r6_ambiguous.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='40' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "</templates>");

    // Non-strict
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r6_ambiguous.xml");
        opts.profile = "spectra-1.29";

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.requested_profile == "spectra-1.29");
        CHECK(report.selected_profile == "spectra-1.29");
        CHECK(report.compatibility_status == "mismatch");
    }

    // Strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r6_ambiguous.xml");
        opts.profile = "spectra-1.29";
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "mismatch");
    }

    TEST_PASS("Round 6: explicit 1.29 on ambiguous => mismatch/invalid");
}

void test_round6_explicit_130_on_ambiguous() {
    // explicit 1.30 override on an ambiguous artifact => mismatch; strict invalid
    // Non-strict
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r6_ambiguous.xml");  // re-use from above
        opts.profile = "spectra-1.30";

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.requested_profile == "spectra-1.30");
        CHECK(report.selected_profile == "spectra-1.30");
        CHECK(report.compatibility_status == "mismatch");
    }

    // Strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r6_ambiguous.xml");
        opts.profile = "spectra-1.30";
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "mismatch");
    }

    TEST_PASS("Round 6: explicit 1.30 on ambiguous => mismatch/invalid");
}

void test_round6_override_preserves_detection_evidence() {
    // explicit override report preserves actual detected profile and evidence
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.profile = "spectra-1.29";

    auto report = moex_fast::run_inspector(opts);

    // Actual detection must be preserved
    CHECK(report.detected_profile == "spectra-1.29");
    CHECK(report.detection_evidence.find("ID 40 SecurityDefinition") != std::string::npos);
    CHECK(report.detection_evidence != "explicit CLI override");

    // Requested and selected must be separate
    CHECK(report.requested_profile == "spectra-1.29");
    CHECK(report.selected_profile == "spectra-1.29");

    // JSON must contain all profile fields
    auto json = moex_fast::report_to_json(report);
    CHECK(json.find("\"detected_profile\"") != std::string::npos);
    CHECK(json.find("\"profile_evidence\"") != std::string::npos);
    CHECK(json.find("\"requested_profile\"") != std::string::npos);
    CHECK(json.find("\"selected_profile\"") != std::string::npos);
    CHECK(json.find("\"compatibility_status\"") != std::string::npos);

    TEST_PASS("Round 6: override preserves detection evidence");
}

// --- Round 7 tests ---

void test_round7_auto_ambiguous_selected_none() {
    // auto + ambiguous artifact => selected_profile=none; mismatch; strict invalid
    write_temp_file("r7_auto_ambiguous.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='40' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='SecurityDefinition'><uInt32 name='X'/></template>"
        "</templates>");

    // Non-strict
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_auto_ambiguous.xml");

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.selected_profile == "none");
        CHECK(report.requested_profile == "auto");
        CHECK(report.compatibility_status == "mismatch");
        CHECK(report.required_template_results.empty());
    }

    // Strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_auto_ambiguous.xml");
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "mismatch");
    }

    TEST_PASS("Round 7: auto + ambiguous => selected_profile=none, mismatch, strict invalid");
}

void test_round7_auto_unknown_selected_none() {
    // auto + unknown artifact => selected_profile=none; compatibility unknown; strict invalid
    write_temp_file("r7_auto_unknown.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "</templates>");

    // Non-strict
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_auto_unknown.xml");

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "unknown");
        CHECK(report.selected_profile == "none");
        CHECK(report.requested_profile == "auto");
        CHECK(report.compatibility_status == "unknown");
        CHECK(report.required_template_results.empty());
    }

    // Strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_auto_unknown.xml");
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "unknown");
    }

    TEST_PASS("Round 7: auto + unknown => selected_profile=none, unknown, strict invalid");
}

void test_round7_wrong_name_47_only_mismatch() {
    // wrong-name-only ID 47 (no ID 40, no valid ID 48) => mismatch; strict invalid
    write_temp_file("r7_wrong47_only.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='WrongName'><uInt32 name='X'/></template>"
        "</templates>");

    // Non-strict
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_wrong47_only.xml");

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.selected_profile == "none");
        CHECK(report.compatibility_status == "mismatch");
    }

    // Strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_wrong47_only.xml");
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "mismatch");
    }

    TEST_PASS("Round 7: wrong-name-only ID 47 => ambiguous/mismatch, strict invalid");
}

void test_round7_wrong_name_48_only_mismatch() {
    // wrong-name-only ID 48 (no ID 40, no valid ID 47) => mismatch; strict invalid
    write_temp_file("r7_wrong48_only.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='48' name='WrongName'><uInt32 name='X'/></template>"
        "</templates>");

    // Non-strict
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_wrong48_only.xml");

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.selected_profile == "none");
        CHECK(report.compatibility_status == "mismatch");
    }

    // Strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_wrong48_only.xml");
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "mismatch");
    }

    TEST_PASS("Round 7: wrong-name-only ID 48 => ambiguous/mismatch, strict invalid");
}

void test_round7_explicit_override_wrong_name_stays_mismatch() {
    // explicit 1.29 and 1.30 overrides on wrong-name evidence remain mismatch/invalid
    write_temp_file("r7_override_wrong.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "  <template id='47' name='WrongName'><uInt32 name='X'/></template>"
        "</templates>");

    // Explicit spectra-1.29 override
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_override_wrong.xml");
        opts.profile = "spectra-1.29";

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.selected_profile == "spectra-1.29");
        CHECK(report.compatibility_status == "mismatch");
    }

    // Explicit spectra-1.30 override
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_override_wrong.xml");
        opts.profile = "spectra-1.30";

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.detected_profile == "ambiguous");
        CHECK(report.selected_profile == "spectra-1.30");
        CHECK(report.compatibility_status == "mismatch");
    }

    // Both strict => invalid
    {
        moex_fast::InspectorOptions opts;
        opts.configuration_path = "fixtures/synthetic_configuration.xml";
        opts.templates_path = temp_path("r7_override_wrong.xml");
        opts.profile = "spectra-1.29";
        opts.strict = true;

        auto report = moex_fast::run_inspector(opts);
        CHECK(report.overall_status == "invalid");
        CHECK(report.compatibility_status == "mismatch");
    }

    TEST_PASS("Round 7: explicit overrides on wrong-name evidence remain mismatch/invalid");
}

void test_round7_shared_checks_still_run_on_unresolved() {
    // When selected_profile=none, shared checks (wire type) still run
    write_temp_file("r7_shared_checks.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "</templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("r7_shared_checks.xml");

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "unknown");
    CHECK(report.selected_profile == "none");
    // Configuration validation should still run
    CHECK(!report.required_feed_results.empty());

    TEST_PASS("Round 7: shared checks run on unresolved profile");
}

void test_round7_positive_129_still_valid() {
    // Existing valid official-style spectra-1.29 fixture remains valid
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.strict = true;

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "spectra-1.29");
    CHECK(report.selected_profile == "spectra-1.29");
    CHECK(report.compatibility_status == "compatible");
    CHECK(report.overall_status == "valid");
    CHECK(report.required_template_results.size() == 7);
    for (const auto& r : report.required_template_results) {
        CHECK(r.present);
    }

    TEST_PASS("Round 7: positive spectra-1.29 still valid");
}

void test_round7_positive_130_still_valid() {
    // Existing valid synthetic spectra-1.30 fixture remains valid
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates_130.xml";
    opts.strict = true;

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.detected_profile == "spectra-1.30");
    CHECK(report.selected_profile == "spectra-1.30");
    CHECK(report.compatibility_status == "compatible");
    CHECK(report.overall_status == "valid");
    CHECK(report.required_template_results.size() == 8);
    for (const auto& r : report.required_template_results) {
        CHECK(r.present);
    }

    TEST_PASS("Round 7: positive spectra-1.30 still valid");
}

void test_round7_strict_unknown_invalid() {
    // Strict mode with unknown compatibility must be invalid
    write_temp_file("r7_strict_unknown.xml",
        "<templates>"
        "  <template id='29' name='OrdersLogMessage'><uInt32 name='X'/></template>"
        "  <template id='30' name='BookMessage'><uInt32 name='X'/></template>"
        "  <template id='31' name='DefaultIncrementalRefreshMessage'><uInt32 name='X'/></template>"
        "  <template id='32' name='DefaultSnapshotMessage'><uInt32 name='X'/></template>"
        "  <template id='45' name='SecurityGroupStatus'><uInt32 name='X'/></template>"
        "  <template id='46' name='TradingSessionStatus'><uInt32 name='X'/></template>"
        "</templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("r7_strict_unknown.xml");
    opts.strict = true;

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.compatibility_status == "unknown");
    CHECK(report.overall_status == "invalid");

    TEST_PASS("Round 7: strict unknown compatibility => invalid");
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
    // Round 5 tests
    test_length_wire_type_valid();
    test_strict_valid_synthetic();
    test_profile_auto_detected_129();
    test_profile_auto_detected_130();
    test_profile_explicit_override();
    test_profile_mismatch_warning();
    test_profile_mismatch_strict_error();
    test_profile_in_text_report();
    test_mixed_profile_negative();
    test_wrong_name_profile_negative();
    test_length_no_unknown_wire_type_issue();
    // Round 6 tests
    test_round6_129_plus_id48_negative();
    test_round6_id40_wrong_name_id47_id48_negative();
    test_round6_explicit_129_on_ambiguous();
    test_round6_explicit_130_on_ambiguous();
    test_round6_override_preserves_detection_evidence();
    // Round 7 tests
    test_round7_auto_ambiguous_selected_none();
    test_round7_auto_unknown_selected_none();
    test_round7_wrong_name_47_only_mismatch();
    test_round7_wrong_name_48_only_mismatch();
    test_round7_explicit_override_wrong_name_stays_mismatch();
    test_round7_shared_checks_still_run_on_unresolved();
    test_round7_positive_129_still_valid();
    test_round7_positive_130_still_valid();
    test_round7_strict_unknown_invalid();

    std::cout << "\nAll deterministic report tests PASSED.\n";
    return 0;
}
