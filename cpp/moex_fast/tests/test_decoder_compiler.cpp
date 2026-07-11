#include "moex_fast/fast_compiler.hpp"
#include "test_check.hpp"
#include <iostream>

using namespace moex_fast;

static void test_valid_compile() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="100" name="TestMsg">
    <string name="ConstField" id="1"><constant>HELLO</constant></string>
    <uInt32 name="SeqNum" id="34"/>
    <uInt64 name="Timestamp" id="52"/>
    <int32 name="IntField" id="100" presence="optional"/>
    <string name="StrField" id="101" presence="optional"/>
    <decimal name="Price" id="270" presence="optional"/>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK_EQ(result.compiled.templates.size(), 1u);
    CHECK_EQ(result.compiled.templates[0].id, 100u);
    CHECK_EQ(result.compiled.templates[0].name, "TestMsg");
    CHECK_EQ(result.compiled.templates[0].fields.size(), 6u);
    CHECK(!result.compiled.templates_sha256.empty());

    // Check field details
    const auto& f0 = result.compiled.templates[0].fields[0];
    CHECK_EQ(f0.name, "ConstField");
    CHECK(f0.wire_type == DecWireType::AsciiString);
    CHECK(f0.op.kind == OpKind::Constant);
    CHECK_EQ(f0.op.constant_str, "HELLO");

    const auto& f1 = result.compiled.templates[0].fields[1];
    CHECK_EQ(f1.name, "SeqNum");
    CHECK(f1.wire_type == DecWireType::uInt32);
    CHECK(f1.is_mandatory);
    CHECK(!f1.has_pmap_bit);  // mandatory none -> no pmap bit

    const auto& f3 = result.compiled.templates[0].fields[3];
    CHECK_EQ(f3.name, "IntField");
    CHECK(!f3.is_mandatory);
    CHECK(f3.has_pmap_bit);  // optional -> pmap bit

    TEST_PASS("valid_compile");
}

static void test_duplicate_template_id() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="A"><uInt32 name="F1"/></template>
  <template id="1" name="B"><uInt32 name="F2"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    bool found = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "duplicate_template_id") found = true;
    }
    CHECK(found);
    TEST_PASS("duplicate_template_id");
}

static void test_missing_template_id() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template name="NoId"><uInt32 name="F1"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    TEST_PASS("missing_template_id");
}

static void test_invalid_xml() {
    auto result = compile_templates_from_string("<not valid xml");
    CHECK(!result.ok);
    TEST_PASS("invalid_xml");
}

static void test_empty_templates() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates></templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);  // no templates compiled
    TEST_PASS("empty_templates");
}

static void test_sequence_compile() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="SeqMsg">
    <uInt32 name="Header"/>
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Action" id="279"/>
      <string name="Type" id="269" presence="optional"/>
    </sequence>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK_EQ(result.compiled.templates[0].fields.size(), 2u);
    const auto& seq = result.compiled.templates[0].fields[1];
    CHECK(seq.is_sequence);
    CHECK(seq.has_children);
    // Children: length + 2 entry fields
    CHECK_EQ(seq.children.size(), 3u);
    CHECK_EQ(seq.children[0].name, "NoEntries");
    CHECK_EQ(seq.children[1].name, "Action");
    CHECK_EQ(seq.children[2].name, "Type");
    TEST_PASS("sequence_compile");
}

static void test_decimal_compile() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="20" name="DecMsg">
    <decimal name="Price" id="270" presence="optional"/>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    const auto& f = result.compiled.templates[0].fields[0];
    CHECK(f.is_decimal);
    CHECK(f.wire_type == DecWireType::Decimal);
    TEST_PASS("decimal_compile");
}

int main() {
    test_valid_compile();
    test_duplicate_template_id();
    test_missing_template_id();
    test_invalid_xml();
    test_empty_templates();
    test_sequence_compile();
    test_decimal_compile();
    std::cout << "All decoder compiler tests passed.\n";
    return 0;
}
