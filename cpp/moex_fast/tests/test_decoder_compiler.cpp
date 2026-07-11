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

static void test_dictionary_collision() {
    // Two fields with same dictionary scope, template, name and type collide
    // Same explicit key means same dictionary entry — which is ambiguous
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val1" id="1"><copy key="sharedKey"/></uInt32>
    <uInt32 name="Val2" id="2"><copy key="sharedKey"/></uInt32>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    bool found = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "duplicate_dictionary_key") found = true;
    }
    CHECK(found);
    TEST_PASS("dictionary_collision");
}

static void test_reference_rejection() {
    // typeRef should be rejected as unsupported
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <typeRef name="SomeType"/>
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    bool found = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "unsupported_reference") found = true;
    }
    CHECK(found);
    TEST_PASS("reference_rejection");
}

static void test_template_ref_rejection() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <templateRef name="OtherTemplate"/>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    bool found = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "unsupported_reference") found = true;
    }
    CHECK(found);
    TEST_PASS("template_ref_rejection");
}

static void test_unsupported_operator_type() {
    // tail on uInt32 is unsupported
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val" id="1"><tail/></uInt32>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    bool found = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "unsupported_operator_type") found = true;
    }
    CHECK(found);
    TEST_PASS("unsupported_operator_type");
}

static void test_copy_with_dictionary_scope() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val" id="1"><copy dictionary="global"/></uInt32>
    <uInt32 name="Val2" id="2"><copy dictionary="template"/></uInt32>
  </template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    const auto& f1 = result.compiled.templates[0].fields[0];
    CHECK(f1.op.kind == OpKind::Copy);
    CHECK(f1.op.scope == DictScope::Global);
    const auto& f2 = result.compiled.templates[0].fields[1];
    CHECK(f2.op.kind == OpKind::Copy);
    CHECK(f2.op.scope == DictScope::TemplateType);
    TEST_PASS("copy_with_dictionary_scope");
}

static void test_sha256_provenance() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg"><uInt32 name="Val"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK(!result.compiled.templates_sha256.empty());
    CHECK_EQ(result.compiled.templates_sha256.size(), 64u);  // hex sha256
    CHECK(result.compiled.templates_file_size > 0);
    TEST_PASS("sha256_provenance");
}

int main() {
    test_valid_compile();
    test_duplicate_template_id();
    test_missing_template_id();
    test_invalid_xml();
    test_empty_templates();
    test_sequence_compile();
    test_decimal_compile();
    test_dictionary_collision();
    test_reference_rejection();
    test_template_ref_rejection();
    test_unsupported_operator_type();
    test_copy_with_dictionary_scope();
    test_sha256_provenance();
    std::cout << "All decoder compiler tests passed.\n";
    return 0;
}
