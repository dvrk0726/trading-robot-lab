#include "moex_fast/inspector.hpp"
#include "moex_fast/xml_parser.hpp"
#include "moex_fast/report.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

namespace {

void write_file(const char* path, const char* content) {
    std::ofstream ofs(path);
    ofs << content;
}

void test_empty_file() {
    write_file("fixtures/empty_file.xml", "");

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/empty_file.xml", templates, issues);
    assert(!ok);
    (void)ok;
    assert(!issues.empty());

    std::cout << "PASS: empty file\n";
}

void test_truncated_file() {
    write_file("fixtures/truncated.xml",
        "<?xml version=\"1.0\"?><templates><template id='1' name='X'>");

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/truncated.xml", templates, issues);
    // pugixml may partially parse or fail; either way no crash
    (void)ok;

    std::cout << "PASS: truncated file (no crash)\n";
}

void test_large_template_count() {
    std::string xml = "<templates>";
    for (int i = 1; i <= 100; ++i) {
        xml += "<template id='" + std::to_string(i) + "' name='T" + std::to_string(i) + "'>";
        xml += "<uInt32 name='F1'/>";
        xml += "</template>";
    }
    xml += "</templates>";
    write_file("fixtures/large_templates.xml", xml.c_str());

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/large_templates.xml", templates, issues);
    assert(ok);
    (void)ok;
    assert(templates.size() == 100);

    std::cout << "PASS: large template count\n";
}

void test_large_field_count() {
    std::string xml = "<templates><template id='1' name='Big'>";
    for (int i = 0; i < 50; ++i) {
        xml += "<uInt32 name='Field" + std::to_string(i) + "'/>";
    }
    xml += "</template></templates>";
    write_file("fixtures/large_fields.xml", xml.c_str());

    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/large_fields.xml", templates, issues);
    assert(ok);
    (void)ok;
    assert(templates.size() == 1);
    assert(templates[0].fields.size() == 50);

    std::cout << "PASS: large field count\n";
}

void test_output_write_failure() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";
    opts.json_out_path = "nonexistent_dir/report.json";

    auto report = moex_fast::run_inspector(opts);
    auto err = moex_fast::write_json_report(report, opts.json_out_path);
    assert(!err.empty());

    std::cout << "PASS: output write failure\n";
}

void test_no_crash_on_invalid_xml() {
    // Various malformed XML inputs
    const char* cases[] = {
        "<",
        "<templates><template/></templates>",
        "<templates><template id=''/></templates>",
        "<?xml version='1.0'?>\n<templates>\n</templates>",
        "<templates><!-- comment --></templates>",
    };
    for (const auto* c : cases) {
        write_file("fixtures/invalid_case.xml", c);
        std::vector<moex_fast::FastTemplateDescriptor> templates;
        std::vector<moex_fast::InspectionIssue> issues;
        moex_fast::parse_templates_xml("fixtures/invalid_case.xml", templates, issues);
        // No crash is the test
    }

    std::cout << "PASS: no crash on invalid XML variants\n";
}

void test_report_json_escape() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // JSON should not have unescaped special characters in string values
    // Check that the JSON is well-formed by verifying string boundaries
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
    // Should end with strings properly closed
    assert(!in_string);

    std::cout << "PASS: JSON escape handling\n";
}

void test_wire_type_names() {
    assert(std::string(moex_fast::wire_type_name(moex_fast::WireType::uInt32)) == "uInt32");
    assert(std::string(moex_fast::wire_type_name(moex_fast::WireType::Sequence)) == "Sequence");
    assert(std::string(moex_fast::wire_type_name(moex_fast::WireType::Unknown)) == "Unknown");

    assert(moex_fast::parse_wire_type("uInt32") == moex_fast::WireType::uInt32);
    assert(moex_fast::parse_wire_type("sequence") == moex_fast::WireType::Sequence);
    assert(moex_fast::parse_wire_type("unknown_type") == moex_fast::WireType::Unknown);

    std::cout << "PASS: wire type names\n";
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
