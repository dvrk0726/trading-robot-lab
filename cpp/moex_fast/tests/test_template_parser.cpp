#include "moex_fast/xml_parser.hpp"
#include "test_helpers.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <set>

namespace {

const char* VALID_TEMPLATES = "fixtures/synthetic_templates.xml";

void test_valid_templates_parse() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);
    CHECK(ok);
    CHECK(templates.size() == 7);

    std::set<unsigned> ids;
    for (const auto& t : templates) ids.insert(t.id);
    CHECK(ids.count(29));
    CHECK(ids.count(30));
    CHECK(ids.count(31));
    CHECK(ids.count(32));
    CHECK(ids.count(40));
    CHECK(ids.count(45));
    CHECK(ids.count(46));

    TEST_PASS("valid templates parse");
}

void test_template_names_match_specification() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    std::map<unsigned, std::string> id_name;
    for (const auto& t : templates) id_name[t.id] = t.name;

    CHECK(id_name[29] == "OrdersLogMessage");
    CHECK(id_name[30] == "BookMessage");
    CHECK(id_name[31] == "DefaultIncrementalRefreshMessage");
    CHECK(id_name[32] == "DefaultSnapshotMessage");
    CHECK(id_name[40] == "SecurityDefinition");
    CHECK(id_name[45] == "SecurityGroupStatus");
    CHECK(id_name[46] == "TradingSessionStatus");

    TEST_PASS("template names match specification");
}

void test_template_fields() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    CHECK(ol != nullptr);
    CHECK(ol->name == "OrdersLogMessage");

    CHECK(!ol->fields.empty());
    CHECK(ol->fields[0].name == "ApplVerID");
    CHECK(ol->fields[0].is_constant);
    CHECK(ol->fields[0].constant_value == "9");
    CHECK(ol->fields[0].has_fix_tag);
    CHECK(ol->fields[0].fix_tag == 1128);

    TEST_PASS("template fields");
}

void test_mandatory_optional() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    CHECK(ol != nullptr);

    // In real MOEX templates, mandatory fields omit the presence attribute.
    // Absent presence => mandatory; presence="optional" => optional.
    bool found_mandatory_no_presence = false;
    bool found_optional = false;
    for (const auto& f : ol->fields) {
        if (f.name == "MsgSeqNum") {
            // MsgSeqNum has no presence attribute in the fixture => mandatory
            CHECK(f.is_mandatory);
            found_mandatory_no_presence = true;
        }
        if (f.name == "MDEntryID") {
            // MDEntryID has presence="optional" in the fixture
            CHECK(!f.is_mandatory);
            found_optional = true;
        }
    }
    CHECK(found_mandatory_no_presence);
    CHECK(found_optional);

    TEST_PASS("mandatory/optional presence (absent=mandatory, optional=optional)");
}

void test_sequence_fields() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    CHECK(ol != nullptr);

    bool has_sequence = false;
    for (const auto& f : ol->fields) {
        if (f.wire_type == moex_fast::WireType::Sequence) {
            has_sequence = true;
            break;
        }
    }
    CHECK(has_sequence);

    bool has_length = false;
    for (const auto& f : ol->fields) {
        if (f.is_sequence_length) {
            has_length = true;
            CHECK(f.name == "NoMDEntries");
            // Round 5: <length> must be WireType::uInt32, not Unknown
            CHECK(f.wire_type == moex_fast::WireType::uInt32);
            // FIX id must be preserved
            CHECK(f.has_fix_tag);
            CHECK(f.fix_tag == 268);
            // parent_sequence must be set
            CHECK(f.parent_sequence == "MDEntries");
            break;
        }
    }
    CHECK(has_length);

    TEST_PASS("sequence fields (length as uInt32, FIX id, parent_sequence)");
}

void test_sequence_nesting_preserved() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    CHECK(ol != nullptr);

    // Nested sequence fields should have parent_sequence set
    bool found_nested = false;
    for (const auto& f : ol->fields) {
        if (f.parent_sequence == "MDEntries") {
            found_nested = true;
            break;
        }
    }
    CHECK(found_nested);

    // Top-level fields should have empty parent_sequence
    CHECK(ol->fields[0].parent_sequence.empty());

    TEST_PASS("sequence nesting preserved");
}

void test_field_order_monotonic() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(VALID_TEMPLATES, templates, issues);

    const moex_fast::FastTemplateDescriptor* ol = nullptr;
    for (const auto& t : templates) {
        if (t.id == 29) { ol = &t; break; }
    }
    CHECK(ol != nullptr);
    CHECK(ol->fields.size() > 1);

    // Field order must be strictly monotonic (no resets at sequence boundaries)
    for (std::size_t i = 1; i < ol->fields.size(); ++i) {
        CHECK_MSG(ol->fields[i].order > ol->fields[i - 1].order,
            "Field order not monotonic at index " + std::to_string(i));
    }

    TEST_PASS("field order monotonic");
}

void test_malformed_xml() {
    write_temp_file("bad_templates.xml", "not xml at all");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("bad_templates.xml").c_str(), templates, issues);
    CHECK(!ok);
    CHECK(!issues.empty());
    CHECK(issues[0].severity == moex_fast::Severity::Error);

    TEST_PASS("malformed XML");
}

void test_missing_root() {
    write_temp_file("no_root_templates.xml", "<foo><bar/></foo>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("no_root_templates.xml").c_str(), templates, issues);
    CHECK(!ok);
    CHECK(!issues.empty());

    TEST_PASS("missing root element");
}

void test_duplicate_template_id() {
    write_temp_file("dup_templates.xml",
        "<templates>"
        "  <template id='1' name='A'><uInt32 name='X'/></template>"
        "  <template id='1' name='B'><uInt32 name='Y'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("dup_templates.xml").c_str(), templates, issues);
    CHECK(ok);
    CHECK(templates.size() == 1);
    bool found_dup = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Duplicate") != std::string::npos) found_dup = true;
    }
    CHECK(found_dup);

    TEST_PASS("duplicate template id");
}

void test_non_numeric_id() {
    write_temp_file("bad_id_templates.xml",
        "<templates>"
        "  <template id='abc' name='A'><uInt32 name='X'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(temp_path("bad_id_templates.xml").c_str(), templates, issues);
    CHECK(templates.empty());
    bool found_bad_id = false;
    for (const auto& iss : issues) {
        if (iss.message.find("non-numeric") != std::string::npos) found_bad_id = true;
    }
    CHECK(found_bad_id);

    TEST_PASS("non-numeric template id");
}

void test_missing_id() {
    write_temp_file("no_id_templates.xml",
        "<templates>"
        "  <template name='A'><uInt32 name='X'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(temp_path("no_id_templates.xml").c_str(), templates, issues);
    CHECK(templates.empty());

    TEST_PASS("missing template id");
}

void test_empty_templates() {
    write_temp_file("empty_templates.xml", "<templates/>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml(temp_path("empty_templates.xml").c_str(), templates, issues);
    CHECK(ok);
    CHECK(templates.empty());

    TEST_PASS("empty templates");
}

void test_file_not_found() {
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_templates_xml("nonexistent.xml", templates, issues);
    CHECK(!ok);
    CHECK(!issues.empty());

    TEST_PASS("file not found");
}

void test_unknown_element_reported() {
    write_temp_file("unknown_elem.xml",
        "<templates>"
        "  <template id='1' name='A'>"
        "    <uInt32 name='X'><tail/></uInt32>"
        "  </template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(temp_path("unknown_elem.xml").c_str(), templates, issues);

    // The parser should report the unsupported 'tail' operator in field 'X'
    bool found_issue = false;
    for (const auto& iss : issues) {
        if (iss.message.find("tail") != std::string::npos ||
            iss.message.find("Unsupported") != std::string::npos ||
            iss.message.find("operator") != std::string::npos) {
            found_issue = true;
        }
    }
    CHECK(found_issue);

    TEST_PASS("unsupported FAST operator reported");
}

void test_unknown_presence_reported() {
    write_temp_file("unknown_pres.xml",
        "<templates>"
        "  <template id='1' name='A'>"
        "    <uInt32 name='X' presence='constant'/>"
        "  </template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(temp_path("unknown_pres.xml").c_str(), templates, issues);

    bool found_issue = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Unsupported presence") != std::string::npos) {
            found_issue = true;
        }
    }
    CHECK(found_issue);

    TEST_PASS("unknown presence value reported");
}

void test_issue_source_template() {
    write_temp_file("src_test.xml",
        "<templates>"
        "  <template id='1' name='A'><uInt32 name='X'/></template>"
        "</templates>");
    std::vector<moex_fast::FastTemplateDescriptor> templates;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_templates_xml(temp_path("src_test.xml").c_str(), templates, issues);

    for (const auto& iss : issues) {
        CHECK(iss.source == moex_fast::IssueSource::Template);
    }

    TEST_PASS("issue source is Template");
}

}  // namespace

int main() {
    test_valid_templates_parse();
    test_template_names_match_specification();
    test_template_fields();
    test_mandatory_optional();
    test_sequence_fields();
    test_sequence_nesting_preserved();
    test_field_order_monotonic();
    test_malformed_xml();
    test_missing_root();
    test_duplicate_template_id();
    test_non_numeric_id();
    test_missing_id();
    test_empty_templates();
    test_file_not_found();
    test_unknown_element_reported();
    test_unknown_presence_reported();
    test_issue_source_template();

    std::cout << "\nAll template parser tests PASSED.\n";
    return 0;
}
