#include "moex_fast/xml_parser.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <set>

namespace {

const char* VALID_TEMPLATES = "fixtures/synthetic_templates.xml";

void write_file(const char* path, const char* content) {
    std::ofstream ofs(path);
    ofs << content;
}

void test_valid_templates_parse() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);
    assert(ok);
    (void)ok;
    assert(templates.size() == 7);

    // Check required template IDs are present
    std::set<unsigned> ids;
    for (const auto& t : templates) ids.insert(t.id);
    assert(ids.count(29));
    assert(ids.count(30));
    assert(ids.count(31));
    assert(ids.count(32));
    assert(ids.count(40));
    assert(ids.count(45));
    assert(ids.count(46));

    std::cout << "PASS: valid templates parse\n";
}

void test_template_fields() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    // Find OrdersLogMessage (id=29)
    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    assert(ol != nullptr);
    assert(ol->name == "OrdersLogMessage");

    // First field should be ApplVerID
    assert(!ol->fields.empty());
    assert(ol->fields[0].name == "ApplVerID");
    assert(ol->fields[0].is_constant);
    assert(ol->fields[0].constant_value == "9");
    assert(ol->fields[0].has_fix_tag);
    assert(ol->fields[0].fix_tag == 1128);

    std::cout << "PASS: template fields\n";
}

void test_mandatory_optional() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    assert(ol != nullptr);

    // MsgSeqNum should be mandatory
    bool found_mandatory = false;
    for (const auto& f : ol->fields) {
        if (f.name == "MsgSeqNum") {
            assert(f.is_mandatory);
            found_mandatory = true;
            break;
        }
    }
    assert(found_mandatory);

    std::cout << "PASS: mandatory/optional presence\n";
}

void test_sequence_fields() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    assert(ol != nullptr);

    // Should have at least one sequence field
    bool has_sequence = false;
    for (const auto& f : ol->fields) {
        if (f.wire_type == moex_fast::WireType::Sequence) {
            has_sequence = true;
            break;
        }
    }
    assert(has_sequence);

    // Should have a sequence length field
    bool has_length = false;
    for (const auto& f : ol->fields) {
        if (f.is_sequence_length) {
            has_length = true;
            assert(f.name == "NoMDEntries");
            break;
        }
    }
    assert(has_length);

    std::cout << "PASS: sequence fields\n";
}

void test_malformed_xml() {
    write_file("fixtures/bad_templates.xml", "not xml at all");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/bad_templates.xml", templates, issues);
    assert(!ok);
    (void)ok;
    assert(!issues.empty());
    assert(issues[0].severity == moex_fast::Severity::Error);

    std::cout << "PASS: malformed XML\n";
}

void test_missing_root() {
    write_file("fixtures/no_root_templates.xml", "<foo><bar/></foo>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/no_root_templates.xml", templates, issues);
    assert(!ok);
    (void)ok;
    assert(!issues.empty());

    std::cout << "PASS: missing root element\n";
}

void test_duplicate_template_id() {
    write_file("fixtures/dup_templates.xml",
        "<templates>"
        "  <template id='1' name='A'><uInt32 name='X'/></template>"
        "  <template id='1' name='B'><uInt32 name='Y'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/dup_templates.xml", templates, issues);
    assert(ok);
    (void)ok;
    assert(templates.size() == 1);  // Duplicate is skipped
    bool found_dup = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Duplicate") != std::string::npos) found_dup = true;
    }
    assert(found_dup);

    std::cout << "PASS: duplicate template id\n";
}

void test_non_numeric_id() {
    write_file("fixtures/bad_id_templates.xml",
        "<templates>"
        "  <template id='abc' name='A'><uInt32 name='X'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml("fixtures/bad_id_templates.xml", templates, issues);
    assert(templates.empty());
    bool found_bad_id = false;
    for (const auto& iss : issues) {
        if (iss.message.find("non-numeric") != std::string::npos) found_bad_id = true;
    }
    assert(found_bad_id);

    std::cout << "PASS: non-numeric template id\n";
}

void test_missing_id() {
    write_file("fixtures/no_id_templates.xml",
        "<templates>"
        "  <template name='A'><uInt32 name='X'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml("fixtures/no_id_templates.xml", templates, issues);
    assert(templates.empty());

    std::cout << "PASS: missing template id\n";
}

void test_empty_templates() {
    write_file("fixtures/empty_templates.xml", "<templates/>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("fixtures/empty_templates.xml", templates, issues);
    assert(ok);
    (void)ok;
    assert(templates.empty());

    std::cout << "PASS: empty templates\n";
}

void test_file_not_found() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("nonexistent.xml", templates, issues);
    assert(!ok);
    (void)ok;
    assert(!issues.empty());

    std::cout << "PASS: file not found\n";
}

}  // namespace

int main() {
    test_valid_templates_parse();
    test_template_fields();
    test_mandatory_optional();
    test_sequence_fields();
    test_malformed_xml();
    test_missing_root();
    test_duplicate_template_id();
    test_non_numeric_id();
    test_missing_id();
    test_empty_templates();
    test_file_not_found();

    std::cout << "\nAll template parser tests PASSED.\n";
    return 0;
}
