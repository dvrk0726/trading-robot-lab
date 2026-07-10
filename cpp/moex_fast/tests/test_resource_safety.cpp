#include "moex_fast/inspector.hpp"
#include "moex_fast/xml_parser.hpp"
#include "moex_fast/report.hpp"
#include "test_helpers.hpp"
#include <iostream>
#include <fstream>
#include <string>

namespace {

void test_empty_file() {
    write_temp_file("empty_file.xml", "");

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("empty_file.xml").c_str(), templates, issues);
    CHECK(!ok);
    CHECK(!issues.empty());

    TEST_PASS("empty file");
}

void test_truncated_file() {
    write_temp_file("truncated.xml",
        "<?xml version=\"1.0\"?><templates><template id='1' name='X'>");

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("truncated.xml").c_str(), templates, issues);
    (void)ok;  // pugixml may partially parse or fail; no crash is the test

    TEST_PASS("truncated file (no crash)");
}

void test_large_template_count() {
    std::string xml = "<templates>";
    for (int i = 1; i <= 100; ++i) {
        xml += "<template id='" + std::to_string(i) + "' name='T" + std::to_string(i) + "'>";
        xml += "<uInt32 name='F1'/>";
        xml += "</template>";
    }
    xml += "</templates>";
    write_temp_file("large_templates.xml", xml.c_str());

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("large_templates.xml").c_str(), templates, issues);
    CHECK(ok);
    CHECK(templates.size() == 100);

    TEST_PASS("large template count");
}

void test_large_field_count() {
    std::string xml = "<templates><template id='1' name='Big'>";
    for (int i = 0; i < 50; ++i) {
        xml += "<uInt32 name='Field" + std::to_string(i) + "'/>";
    }
    xml += "</template></templates>";
    write_temp_file("large_fields.xml", xml.c_str());

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("large_fields.xml").c_str(), templates, issues);
    CHECK(ok);
    CHECK(templates.size() == 1);
    CHECK(templates[0].fields.size() == 50);

    TEST_PASS("large field count");
}

void test_output_write_failure() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.json_out_path = "nonexistent_dir/report.json";

    auto report = moex_fast::run_inspector(opts);
    auto err = moex_fast::write_json_report(report, opts.json_out_path);
    CHECK(!err.empty());

    TEST_PASS("output write failure");
}

void test_no_crash_on_invalid_xml() {
    const char* cases[] = {
        "<",
        "<templates><template/></templates>",
        "<templates><template id=''/></templates>",
        "<?xml version='1.0'?>\n<templates>\n</templates>",
        "<templates><!-- comment --></templates>",
    };
    for (int i = 0; i < 5; ++i) {
        write_temp_file("invalid_case.xml", cases[i]);
        std::vector<moex_fast::FastTemplateDescriptor> templates;
        std::vector<moex_fast::InspectionIssue> issues;
        moex_fast::parse_templates_xml(temp_path("invalid_case.xml").c_str(), templates, issues);
    }

    TEST_PASS("no crash on invalid XML variants");
}

void test_report_json_escape() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    bool in_string = false;
    bool escaped = false;
    for (char c : json) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && in_string) {
            escaped = true;
            continue;
        }
        if (c == '"') in_string = !in_string;
    }
    CHECK(!in_string);

    TEST_PASS("JSON escape handling");
}

void test_wire_type_names() {
    CHECK(std::string(moex_fast::wire_type_name(moex_fast::WireType::uInt32)) == "uInt32");
    CHECK(std::string(moex_fast::wire_type_name(moex_fast::WireType::Sequence)) == "Sequence");
    CHECK(std::string(moex_fast::wire_type_name(moex_fast::WireType::Unknown)) == "Unknown");

    CHECK(moex_fast::parse_wire_type("uInt32") == moex_fast::WireType::uInt32);
    CHECK(moex_fast::parse_wire_type("sequence") == moex_fast::WireType::Sequence);
    CHECK(moex_fast::parse_wire_type("unknown_type") == moex_fast::WireType::Unknown);

    // Round 5: "length" must parse as uInt32
    CHECK(moex_fast::parse_wire_type("length") == moex_fast::WireType::uInt32);

    TEST_PASS("wire type names (including length as uInt32)");
}

}  // namespace

int main() {
    test_empty_file();
    test_truncated_file();
    test_large_template_count();
    test_large_field_count();
    test_output_write_failure();
    test_no_crash_on_invalid_xml();
    test_report_json_escape();
    test_wire_type_names();

    std::cout << "\nAll resource safety tests PASSED.\n";
    return 0;
}
