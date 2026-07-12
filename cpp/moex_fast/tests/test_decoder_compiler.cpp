#include "moex_fast/fast_compiler.hpp"
#include "test_check.hpp"
#include <iostream>
#include <type_traits>
#include <string>

using namespace moex_fast;

static bool has_issue(const CompileResult& r, const std::string& code) {
    for (const auto& issue : r.issues) {
        if (issue.code == code) return true;
    }
    return false;
}

static void check_invalid_compile_9B1(const CompileResult& r, const std::string& expected_code) {
    CHECK(!r.ok);
    CHECK(has_issue(r, expected_code));
    CHECK(!r.compiled.valid());
    CHECK(r.compiled.empty());
    CHECK_EQ(r.compiled.size(), 0u);
    CHECK(r.compiled.templates().empty());
    CHECK(r.compiled.find(0) == nullptr);
    CHECK(r.compiled.find(1) == nullptr);
}

static void check_invalid_compile_9B2(const CompileResult& r, const std::string& expected_code) {
    CHECK(!r.ok);
    CHECK(has_issue(r, expected_code));
    CHECK(!r.compiled.valid());
    CHECK(r.compiled.empty());
    CHECK_EQ(r.compiled.size(), 0u);
    CHECK(r.compiled.templates().empty());
    CHECK(r.compiled.find(0) == nullptr);
    CHECK(r.compiled.find(1) == nullptr);
}

// --- static_assert: template collection returns const ref only ---
static_assert(std::is_same_v<
    decltype(std::declval<const CompiledTemplateSet&>().templates()),
    const std::vector<CompiledTemplate>&>,
    "templates() must return const reference");

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

    // Immutable handle checks
    CHECK(result.compiled.valid());
    CHECK(!result.compiled.empty());
    CHECK_EQ(result.compiled.size(), 1u);
    CHECK_EQ(result.compiled.templates().size(), 1u);
    CHECK_EQ(result.compiled.templates()[0].id, 100u);
    CHECK_EQ(result.compiled.templates()[0].name, "TestMsg");
    CHECK_EQ(result.compiled.templates()[0].fields.size(), 6u);
    CHECK(!result.compiled.templates_sha256().empty());

    // Find
    auto* found = result.compiled.find(100);
    CHECK(found != nullptr);
    CHECK_EQ(found->id, 100u);
    CHECK(result.compiled.find(999) == nullptr);

    // Check field details
    const auto& f0 = result.compiled.templates()[0].fields[0];
    CHECK_EQ(f0.name, "ConstField");
    CHECK(f0.wire_type == DecWireType::AsciiString);
    CHECK(f0.op.kind == OpKind::Constant);
    CHECK_EQ(f0.op.constant_str, "HELLO");

    const auto& f1 = result.compiled.templates()[0].fields[1];
    CHECK_EQ(f1.name, "SeqNum");
    CHECK(f1.wire_type == DecWireType::uInt32);
    CHECK(f1.is_mandatory);
    CHECK(!f1.has_pmap_bit);  // mandatory none -> no pmap bit

    const auto& f3 = result.compiled.templates()[0].fields[3];
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

    // Failed compile: handle must be invalid/empty
    CHECK(!result.compiled.valid());
    CHECK(result.compiled.empty());
    CHECK_EQ(result.compiled.size(), 0u);
    CHECK(result.compiled.templates().empty());
    CHECK(result.compiled.find(1) == nullptr);

    TEST_PASS("duplicate_template_id");
}

static void test_missing_template_id() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template name="NoId"><uInt32 name="F1"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);
    CHECK(!result.compiled.valid());
    CHECK_EQ(result.compiled.size(), 0u);
    TEST_PASS("missing_template_id");
}

static void test_invalid_xml() {
    auto result = compile_templates_from_string("<not valid xml");
    CHECK(!result.ok);
    CHECK(!result.compiled.valid());
    CHECK_EQ(result.compiled.size(), 0u);
    TEST_PASS("invalid_xml");
}

static void test_empty_templates() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates></templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);  // no templates compiled
    CHECK(!result.compiled.valid());
    CHECK_EQ(result.compiled.size(), 0u);
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
    CHECK_EQ(result.compiled.templates()[0].fields.size(), 2u);
    const auto& seq = result.compiled.templates()[0].fields[1];
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
    const auto& f = result.compiled.templates()[0].fields[0];
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
    CHECK(!result.compiled.valid());
    CHECK_EQ(result.compiled.size(), 0u);
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
    CHECK(!result.compiled.valid());
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
    CHECK(!result.compiled.valid());
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
    CHECK(!result.compiled.valid());
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
    const auto& f1 = result.compiled.templates()[0].fields[0];
    CHECK(f1.op.kind == OpKind::Copy);
    CHECK(f1.op.scope == DictScope::Global);
    const auto& f2 = result.compiled.templates()[0].fields[1];
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
    CHECK(!result.compiled.templates_sha256().empty());
    CHECK_EQ(result.compiled.templates_sha256().size(), 64u);  // hex sha256
    CHECK(result.compiled.templates_file_size() > 0);
    CHECK(!result.compiled.compiler_version().empty());
    CHECK(!result.compiled.schema_version().empty());
    TEST_PASS("sha256_provenance");
}

// --- New tests for round 9A ---

// Test 1: successful compile returns exact order, find, SHA, size, versions
static void test_compile_provenance_and_order() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="200" name="MsgA"><uInt32 name="F1"/></template>
  <template id="100" name="MsgB"><uInt32 name="F2"/></template>
  <template id="300" name="MsgC"><uInt32 name="F3"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK(result.compiled.valid());
    CHECK(!result.compiled.empty());
    CHECK_EQ(result.compiled.size(), 3u);

    // Exact order preserved
    CHECK_EQ(result.compiled.templates()[0].id, 200u);
    CHECK_EQ(result.compiled.templates()[0].name, "MsgA");
    CHECK_EQ(result.compiled.templates()[1].id, 100u);
    CHECK_EQ(result.compiled.templates()[1].name, "MsgB");
    CHECK_EQ(result.compiled.templates()[2].id, 300u);
    CHECK_EQ(result.compiled.templates()[2].name, "MsgC");

    // Find works for all
    CHECK(result.compiled.find(200) != nullptr);
    CHECK(result.compiled.find(100) != nullptr);
    CHECK(result.compiled.find(300) != nullptr);
    CHECK(result.compiled.find(400) == nullptr);
    CHECK_EQ(result.compiled.find(200)->name, "MsgA");
    CHECK_EQ(result.compiled.find(100)->name, "MsgB");

    // Provenance
    CHECK_EQ(result.compiled.templates_sha256().size(), 64u);
    CHECK(result.compiled.templates_file_size() > 0);
    CHECK(!result.compiled.compiler_version().empty());
    CHECK(!result.compiled.schema_version().empty());

    TEST_PASS("compile_provenance_and_order");
}

// Test 2: first valid template + subsequent duplicate template id → invalid empty handle, no partial lookup
static void test_fatal_path_duplicate_after_valid() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Good"><uInt32 name="F1"/></template>
  <template id="10" name="Dup"><uInt32 name="F2"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);

    // Issues preserved
    bool found_dup = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "duplicate_template_id") found_dup = true;
    }
    CHECK(found_dup);

    // Handle is completely invalid; no partial lookup
    CHECK(!result.compiled.valid());
    CHECK(result.compiled.empty());
    CHECK_EQ(result.compiled.size(), 0u);
    CHECK(result.compiled.templates().empty());
    CHECK(result.compiled.find(10) == nullptr);

    TEST_PASS("fatal_path_duplicate_after_valid");
}

// Test 3: second fatal path — unknown wire-affecting element after valid template
static void test_fatal_path_unknown_element() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Good"><uInt32 name="F1"/></template>
  <template id="20" name="Bad"><unknownWidget name="X"/></template>
</templates>)";

    auto result = compile_templates_from_string(xml);
    CHECK(!result.ok);

    bool found_unknown = false;
    for (const auto& issue : result.issues) {
        if (issue.code == "unknown_element") found_unknown = true;
    }
    CHECK(found_unknown);

    // Same isolation: invalid empty handle, no partial lookup
    CHECK(!result.compiled.valid());
    CHECK(result.compiled.empty());
    CHECK_EQ(result.compiled.size(), 0u);
    CHECK(result.compiled.find(10) == nullptr);
    CHECK(result.compiled.find(20) == nullptr);

    TEST_PASS("fatal_path_unknown_element");
}

// Test 4: copy/move lifetime — copied handle usable after source destroyed
static void test_copy_lifetime() {
    CompiledTemplateSet copy;
    std::string sha;
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg"><uInt32 name="Val"/></template>
</templates>)";
        auto result = compile_templates_from_string(xml);
        CHECK(result.ok);

        // Copy the handle
        copy = result.compiled;
        sha = copy.templates_sha256();

        // Verify copy is valid while source is alive
        CHECK(copy.valid());
        CHECK_EQ(copy.size(), 1u);
        CHECK(copy.find(10) != nullptr);
    }
    // Source (CompileResult) destroyed; copy must still be valid
    CHECK(copy.valid());
    CHECK_EQ(copy.size(), 1u);
    CHECK(copy.find(10) != nullptr);
    CHECK_EQ(copy.find(10)->name, "Msg");
    CHECK_EQ(copy.templates_sha256(), sha);
    CHECK_EQ(copy.templates_sha256().size(), 64u);

    TEST_PASS("copy_lifetime");
}

// Test 5: move lifetime
static void test_move_lifetime() {
    CompiledTemplateSet moved;
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="5" name="M"><uInt32 name="X"/></template>
</templates>)";
        auto result = compile_templates_from_string(xml);
        CHECK(result.ok);

        CompiledTemplateSet tmp = result.compiled;
        moved = std::move(tmp);

        // tmp should be in a valid but empty state after move
        // (shared_ptr move sets source to nullptr)
    }
    // Source destroyed; moved must still work
    CHECK(moved.valid());
    CHECK_EQ(moved.size(), 1u);
    CHECK(moved.find(5) != nullptr);
    CHECK_EQ(moved.find(5)->name, "M");

    TEST_PASS("move_lifetime");
}

// Test 6: default-constructed handle is invalid/empty
static void test_default_handle_invalid() {
    CompiledTemplateSet h;
    CHECK(!h.valid());
    CHECK(h.empty());
    CHECK_EQ(h.size(), 0u);
    CHECK(h.templates().empty());
    CHECK(h.find(0) == nullptr);
    CHECK(h.find(1) == nullptr);
    CHECK(h.templates_sha256().empty());
    CHECK(h.compiler_version().empty());
    CHECK(h.schema_version().empty());
    CHECK_EQ(h.templates_file_size(), 0u);

    TEST_PASS("default_handle_invalid");
}

// --- 9B1 tests ---

// Template ID UINT32_MAX succeeds and is findable
static void test_template_id_uint32_max() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="4294967295" name="MaxTid"><uInt32 name="F1"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK(result.compiled.valid());
    auto* t = result.compiled.find(4294967295u);
    CHECK(t != nullptr);
    CHECK_EQ(t->id, 4294967295u);
    TEST_PASS("template_id_uint32_max");
}

// Template id zero fails
static void test_template_id_zero() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="0" name="Zero"><uInt32 name="F1"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "invalid_template_id");
    TEST_PASS("template_id_zero");
}

// Template id non-decimal fails
static void test_template_id_non_decimal() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10abc" name="Bad"><uInt32 name="F1"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "non_numeric_template_id");
    TEST_PASS("template_id_non_decimal");
}

// Template id UINT32_MAX+1 fails
static void test_template_id_out_of_range() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="4294967296" name="BigTid"><uInt32 name="F1"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "template_id_out_of_range");
    TEST_PASS("template_id_out_of_range");
}

// FIX tag INT32_MAX succeeds on scalar
static void test_fix_tag_scalar_ok() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F" id="2147483647"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK_EQ(result.compiled.templates()[0].fields[0].fix_tag, 2147483647);
    CHECK(result.compiled.templates()[0].fields[0].has_fix_tag);
    TEST_PASS("fix_tag_scalar_ok");
}

// FIX tag INT32_MAX succeeds on decimal
static void test_fix_tag_decimal_ok() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><decimal name="D" id="2147483647"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK_EQ(result.compiled.templates()[0].fields[0].fix_tag, 2147483647);
    TEST_PASS("fix_tag_decimal_ok");
}

// FIX tag INT32_MAX succeeds on sequence length
static void test_fix_tag_sequence_length_ok() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L" id="2147483647"/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    CHECK_EQ(result.compiled.templates()[0].fields[0].children[0].fix_tag, 2147483647);
    CHECK(result.compiled.templates()[0].fields[0].children[0].has_fix_tag);
    TEST_PASS("fix_tag_sequence_length_ok");
}

// FIX tag zero fails
static void test_fix_tag_zero() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F" id="0"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "invalid_fix_tag");
    TEST_PASS("fix_tag_zero");
}

// FIX tag negative fails
static void test_fix_tag_negative() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F" id="-5"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "invalid_fix_tag");
    TEST_PASS("fix_tag_negative");
}

// FIX tag non-decimal fails
static void test_fix_tag_non_decimal() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F" id="abc"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "invalid_fix_tag");
    TEST_PASS("fix_tag_non_decimal");
}

// FIX tag INT32_MAX+1 fails
static void test_fix_tag_out_of_range() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F" id="2147483648"/></template>
</templates>)";
    auto result = compile_templates_from_string(xml);
    check_invalid_compile_9B1(result, "invalid_fix_tag");
    TEST_PASS("fix_tag_out_of_range");
}

// uInt32 0 and MAX succeed, -1 and MAX+1 fail
static void test_uint32_constant_range() {
    // 0 succeeds
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>0</constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 0u);
    }
    // MAX succeeds
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>4294967295</constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 4294967295u);
    }
    // -1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>-1</constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    // MAX+1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>4294967296</constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    TEST_PASS("uint32_constant_range");
}

// uInt64 0 and MAX succeed, -1 and MAX+1 fail
static void test_uint64_constant_range() {
    // 0
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><constant>0</constant></uInt64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 0u);
    }
    // MAX = 18446744073709551615
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><constant>18446744073709551615</constant></uInt64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, UINT64_MAX);
    }
    // -1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><constant>-1</constant></uInt64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    // 18446744073709551616 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><constant>18446744073709551616</constant></uInt64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    TEST_PASS("uint64_constant_range");
}

// int32 MIN and MAX succeed; adjacent values fail
static void test_int32_constant_range() {
    // MAX = 2147483647
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int32 name="F"><constant>2147483647</constant></int32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_int, INT32_MAX);
    }
    // MIN = -2147483648
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int32 name="F"><constant>-2147483648</constant></int32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_int, INT32_MIN);
    }
    // MAX+1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int32 name="F"><constant>2147483648</constant></int32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    // MIN-1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int32 name="F"><constant>-2147483649</constant></int32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    TEST_PASS("int32_constant_range");
}

// int64 MIN and MAX succeed; adjacent values fail
static void test_int64_constant_range() {
    // MAX = 9223372036854775807
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int64 name="F"><constant>9223372036854775807</constant></int64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_int, INT64_MAX);
    }
    // MIN = -9223372036854775808
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int64 name="F"><constant>-9223372036854775808</constant></int64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_int, INT64_MIN);
    }
    // MAX+1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int64 name="F"><constant>9223372036854775808</constant></int64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    // MIN-1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><int64 name="F"><constant>-9223372036854775809</constant></int64></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    TEST_PASS("int64_constant_range");
}

// Decimal exponent MIN/MAX and mantissa MIN/MAX exact metadata; adjacent failures
static void test_decimal_component_range() {
    // Exponent MAX (INT32_MAX) and mantissa MAX (INT64_MAX) succeed with exact metadata
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>2147483647</constant></exponent>
      <mantissa><constant>9223372036854775807</constant></mantissa>
    </decimal>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        const auto& f = r.compiled.templates()[0].fields[0];
        CHECK(f.is_decimal);
        CHECK_EQ(f.exponent_op.constant_int, INT32_MAX);
        CHECK_EQ(f.mantissa_op.constant_int, INT64_MAX);
    }
    // Exponent MIN and mantissa MIN succeed
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>-2147483648</constant></exponent>
      <mantissa><constant>-9223372036854775808</constant></mantissa>
    </decimal>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        const auto& f = r.compiled.templates()[0].fields[0];
        CHECK_EQ(f.exponent_op.constant_int, INT32_MIN);
        CHECK_EQ(f.mantissa_op.constant_int, INT64_MIN);
    }
    // Exponent MAX+1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>2147483648</constant></exponent>
      <mantissa><constant>0</constant></mantissa>
    </decimal>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    // Mantissa MAX+1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>0</constant></exponent>
      <mantissa><constant>9223372036854775808</constant></mantissa>
    </decimal>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    TEST_PASS("decimal_component_range");
}

// Sequence length initial 0/UINT32_MAX exact metadata; negative and MAX+1 fail
static void test_sequence_length_initial_range() {
    // 0 succeeds
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"><default>0</default></length>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].length_op.has_initial);
        CHECK_EQ(r.compiled.templates()[0].fields[0].length_op.initial_uint, 0u);
    }
    // UINT32_MAX succeeds
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"><default>4294967295</default></length>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].length_op.initial_uint, UINT32_MAX);
    }
    // Negative fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"><default>-1</default></length>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_default_value");
    }
    // MAX+1 fails
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"><default>4294967296</default></length>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_default_value");
    }
    TEST_PASS("sequence_length_initial_range");
}

// Reject hexadecimal numeric literal
static void test_reject_hex_literal() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>0xFF</constant></uInt32></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "invalid_constant_value");
    TEST_PASS("reject_hex_literal");
}

// Valid empty and non-empty ASCII constant values
static void test_valid_ascii_static_values() {
    // Empty ASCII
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F"><constant></constant></string></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_str, "");
    }
    // Non-empty ASCII
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F"><constant>Hello</constant></string></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_str, "Hello");
    }
    TEST_PASS("valid_ascii_static_values");
}

// Valid 1/2/3/4-byte Unicode static values
static void test_valid_unicode_static_values() {
    // 1-byte (ASCII)
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><unicode name="F"><constant>A</constant></unicode></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
    }
    // 2-byte (U+00A2 = C2 A2)
    {
        const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<templates><template id=\"1\" name=\"Msg\">"
            "<unicode name=\"F\"><constant>\xC2\xA2</constant></unicode>"
            "</template></templates>";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
    }
    // 3-byte (U+20AC = E2 82 AC)
    {
        const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<templates><template id=\"1\" name=\"Msg\">"
            "<unicode name=\"F\"><constant>\xE2\x82\xAC</constant></unicode>"
            "</template></templates>";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
    }
    // 4-byte (U+1F600 = F0 9F 98 80)
    {
        const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<templates><template id=\"1\" name=\"Msg\">"
            "<unicode name=\"F\"><constant>\xF0\x9F\x98\x80</constant></unicode>"
            "</template></templates>";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
    }
    TEST_PASS("valid_unicode_static_values");
}

// Non-ASCII in ASCII field fails
static void test_non_ascii_in_ascii_field() {
    // Non-ASCII byte (0xC2 0xA2 = U+00A2 in UTF-8) in ASCII field
    {
        const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<templates><template id=\"1\" name=\"Msg\">"
            "<string name=\"F\"><constant>\xC2\xA2</constant></string>"
            "</template></templates>";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B1(r, "invalid_constant_value");
    }
    TEST_PASS("non_ascii_in_ascii_field");
}

// Invalid Unicode (surrogate U+D800 = ED A0 80) fails
static void test_invalid_unicode_surrogate() {
    // Surrogate U+D800
    const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<templates><template id=\"1\" name=\"Msg\">"
        "<unicode name=\"F\"><constant>\xED\xA0\x80</constant></unicode>"
        "</template></templates>";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "invalid_constant_value");
    TEST_PASS("invalid_unicode_surrogate");
}

// Oversized static value (1 MiB + 1) fails
static void test_oversized_static_value() {
    // 1 MiB + 1 bytes of ASCII 'A'
    std::string big(1024 * 1024 + 1, 'A');
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<templates><template id=\"1\" name=\"Msg\">"
        "<string name=\"F\"><constant>" + big + "</constant></string>"
        "</template></templates>";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "invalid_constant_value");
    TEST_PASS("oversized_static_value");
}

// Exact 1 MiB static value succeeds
static void test_exact_1mib_static_value() {
    std::string big(1024 * 1024, 'A');
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<templates><template id=\"1\" name=\"Msg\">"
        "<string name=\"F\"><constant>" + big + "</constant></string>"
        "</template></templates>";
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_str.size(), 1024u * 1024u);
    TEST_PASS("exact_1mib_static_value");
}

// Default operator initial value: uInt64 MAX
static void test_uint64_default_initial() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><default>18446744073709551615</default></uInt64></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK_EQ(r.compiled.templates()[0].fields[0].op.initial_uint, UINT64_MAX);
    CHECK(r.compiled.templates()[0].fields[0].op.has_initial);
    TEST_PASS("uint64_default_initial");
}

// Copy operator initial value: uInt64 MAX
static void test_uint64_copy_initial() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><copy>18446744073709551615</copy></uInt64></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK_EQ(r.compiled.templates()[0].fields[0].op.initial_uint, UINT64_MAX);
    TEST_PASS("uint64_copy_initial");
}

// Increment operator initial value: uInt64 MAX
static void test_uint64_increment_initial() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt64 name="F"><increment>18446744073709551615</increment></uInt64></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK_EQ(r.compiled.templates()[0].fields[0].op.initial_uint, UINT64_MAX);
    TEST_PASS("uint64_increment_initial");
}

// --- 9B2 phase 1 structural validation tests ---

// Comments, whitespace, processing instructions are ignored; compile succeeds
static void test_structural_comments_whitespace_pi() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!-- comment -->
<templates>
  <?processing-instruction data?>
  <template id="1" name="Msg">
    <!-- field comment -->
    <uInt32 name="F1"/>
    <uInt64 name="F2"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    CHECK_EQ(r.compiled.size(), 1u);
    TEST_PASS("structural_comments_whitespace_pi");
}

// Unknown root child (not <template>) returns unknown_element
static void test_structural_unknown_root_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <unknownWidget name="X"/>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_unknown_root_child");
}

// Misplaced operator in template returns unknown_element
static void test_structural_misplaced_operator_in_template() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <constant>42</constant>
    <uInt32 name="F1"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_misplaced_operator_in_template");
}

// Misplaced length at template level returns unknown_element
static void test_structural_length_at_template_level() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <length name="L"/>
    <uInt32 name="F1"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_length_at_template_level");
}

// Misplaced length at group level returns unknown_element
static void test_structural_length_at_group_level() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <length name="L"/>
      <uInt32 name="F1"/>
    </group>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_length_at_group_level");
}

// Misplaced exponent in template returns unknown_element
static void test_structural_misplaced_exponent_in_template() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <exponent/>
    <uInt32 name="F1"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_misplaced_exponent_in_template");
}

// Misplaced mantissa in template returns unknown_element
static void test_structural_misplaced_mantissa_in_template() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <mantissa/>
    <uInt32 name="F1"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_misplaced_mantissa_in_template");
}

// Misplaced operator in group returns unknown_element
static void test_structural_misplaced_operator_in_group() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <copy/>
      <uInt32 name="F1"/>
    </group>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_misplaced_operator_in_group");
}

// Misplaced exponent in group returns unknown_element
static void test_structural_misplaced_exponent_in_group() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <exponent/>
      <uInt32 name="F1"/>
    </group>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_misplaced_exponent_in_group");
}

// Scalar with two operators returns multiple_operators
static void test_structural_scalar_two_operators() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"><constant>1</constant><copy/></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "multiple_operators");
    TEST_PASS("structural_scalar_two_operators");
}

// Scalar with unknown child returns unknown_element
static void test_structural_scalar_unknown_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"><bogus/></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_scalar_unknown_child");
}

// Operator with nested element returns unknown_element
static void test_structural_operator_nested_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"><constant><value>42</value></constant></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_operator_nested_child");
}

// Duplicate decimal exponent returns duplicate_decimal_exponent
static void test_structural_duplicate_decimal_exponent() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>1</constant></exponent>
      <exponent><constant>2</constant></exponent>
      <mantissa><constant>0</constant></mantissa>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "duplicate_decimal_exponent");
    TEST_PASS("structural_duplicate_decimal_exponent");
}

// Duplicate decimal mantissa returns duplicate_decimal_mantissa
static void test_structural_duplicate_decimal_mantissa() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>0</constant></exponent>
      <mantissa><constant>1</constant></mantissa>
      <mantissa><constant>2</constant></mantissa>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "duplicate_decimal_mantissa");
    TEST_PASS("structural_duplicate_decimal_mantissa");
}

// Unknown child in decimal returns unknown_element
static void test_structural_decimal_unknown_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <bogus/>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_decimal_unknown_child");
}

// Decimal unknown presence returns unknown_presence
static void test_structural_decimal_unknown_presence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D" presence="conditional"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_presence");
    TEST_PASS("structural_decimal_unknown_presence");
}

// Two operators in exponent returns multiple_operators
static void test_structural_exponent_two_operators() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>1</constant><copy/></exponent>
      <mantissa><constant>0</constant></mantissa>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "multiple_operators");
    TEST_PASS("structural_exponent_two_operators");
}

// Unknown child in exponent returns unknown_element
static void test_structural_exponent_unknown_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><bogus/></exponent>
      <mantissa><constant>0</constant></mantissa>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_exponent_unknown_child");
}

// Two operators in mantissa returns multiple_operators
static void test_structural_mantissa_two_operators() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>0</constant></exponent>
      <mantissa><constant>1</constant><copy/></mantissa>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "multiple_operators");
    TEST_PASS("structural_mantissa_two_operators");
}

// Unknown child in mantissa returns unknown_element
static void test_structural_mantissa_unknown_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>0</constant></exponent>
      <mantissa><bogus/></mantissa>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_mantissa_unknown_child");
}

// Sequence missing length returns missing_sequence_length
static void test_structural_sequence_missing_length() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "missing_sequence_length");
    TEST_PASS("structural_sequence_missing_length");
}

// Duplicate sequence length returns duplicate_sequence_length
static void test_structural_duplicate_sequence_length() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L1"/>
      <length name="L2"/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "duplicate_sequence_length");
    TEST_PASS("structural_duplicate_sequence_length");
}

// Misplaced operator in sequence returns unknown_element
static void test_structural_sequence_misplaced_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"/>
      <copy/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_sequence_misplaced_operator");
}

// Misplaced exponent in sequence returns unknown_element
static void test_structural_sequence_misplaced_exponent() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"/>
      <exponent/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_sequence_misplaced_exponent");
}

// Misplaced mantissa in sequence returns unknown_element
static void test_structural_sequence_misplaced_mantissa() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"/>
      <mantissa/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_sequence_misplaced_mantissa");
}

// Two operators in length returns multiple_operators
static void test_structural_length_two_operators() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"><constant>1</constant><copy/></length>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "multiple_operators");
    TEST_PASS("structural_length_two_operators");
}

// Unknown child in length returns unknown_element
static void test_structural_length_unknown_child() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"><bogus/></length>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_length_unknown_child");
}

// Reference at allowed level (template) remains unsupported_reference
static void test_structural_reference_at_allowed_level() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <typeRef name="SomeType"/>
    <uInt32 name="F1"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    CHECK(!r.ok);
    CHECK(has_issue(r, "unsupported_reference"));
    CHECK(!r.compiled.valid());
    TEST_PASS("structural_reference_at_allowed_level");
}

// Misplaced reference in scalar returns unknown_element (not unsupported_reference)
static void test_structural_misplaced_reference_in_scalar() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"><typeRef name="X"/></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B1(r, "unknown_element");
    TEST_PASS("structural_misplaced_reference_in_scalar");
}

// === 9B2 phase 2 tests ===

// Test 1: XML namespace declarations do not trigger unknown_attribute; official-style default-namespace snippet compiles
static void test_9B2_ns_decls_no_unknown_attribute() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates xmlns="http://www.fixprotocol.org/ns/fast/td/1.1">
  <template id="100" name="TestMsg">
    <string name="F1" id="1" xmlns:custom="http://example.com"/>
    <uInt32 name="F2" id="2"/>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    CHECK_EQ(r.compiled.size(), 1u);
    CHECK_EQ(r.compiled.templates()[0].fields.size(), 2u);
    TEST_PASS("9B2_ns_decls_no_unknown_attribute");
}

// Test 2: MOEX-observed attribute combinations compile, including string charset='unicode' and constant value='...'
static void test_9B2_moex_attr_combinations() {
    // string charset='unicode' compiles
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="F1" id="1" charset="unicode"/>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
        CHECK(r.compiled.templates()[0].fields[0].wire_type == DecWireType::UnicodeString);
    }
    // constant value='...' compiles
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1" id="1"><constant value="42"/></uInt32>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 42u);
    }
    TEST_PASS("9B2_moex_attr_combinations");
}

// Test 3: string charset='unicode' produces UnicodeString; absent charset remains AsciiString
static void test_9B2_string_charset_unicode_wire_type() {
    // charset='unicode' -> UnicodeString
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="F1" id="1" charset="unicode"><constant>\xC2\xA2</constant></string>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].wire_type == DecWireType::UnicodeString);
    }
    // absent charset -> AsciiString
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="F1" id="1"><constant>ABC</constant></string>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].wire_type == DecWireType::AsciiString);
    }
    // charset='unicode' accepts valid non-ASCII UTF-8 static constant
    {
        const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<templates><template id=\"1\" name=\"Msg\">"
            "<string name=\"F1\" id=\"1\" charset=\"unicode\"><constant>\xE2\x82\xAC</constant></string>"
            "</template></templates>";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].wire_type == DecWireType::UnicodeString);
    }
    TEST_PASS("9B2_string_charset_unicode_wire_type");
}

// Test 4: empty, unknown and case-mismatched charset return unknown_charset
static void test_9B2_unknown_charset() {
    // empty charset
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1" id="1" charset=""/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "unknown_charset");
    }
    // unknown charset value
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1" id="1" charset="utf-8"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "unknown_charset");
    }
    // case-mismatched: Unicode (capital U)
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1" id="1" charset="Unicode"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "unknown_charset");
    }
    // case-mismatched: UNICODE
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1" id="1" charset="UNICODE"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "unknown_charset");
    }
    TEST_PASS("9B2_unknown_charset");
}

// Test 5: Unknown attribute on root returns unknown_attribute
static void test_9B2_unknown_attr_root() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates version="1">
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_root");
}

// Test 5b: Unknown attribute on template returns unknown_attribute
static void test_9B2_unknown_attr_template() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg" version="2"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_template");
}

// Test 5c: Unknown attribute on scalar/string returns unknown_attribute
static void test_9B2_unknown_attr_scalar_string() {
    // uInt32 with unknown attribute
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1" unknown="x"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "unknown_attribute");
    }
    // string with unknown attribute
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1" unknown="x"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "unknown_attribute");
    }
    TEST_PASS("9B2_unknown_attr_scalar_string");
}

// Test 5d: Unknown attribute on decimal returns unknown_attribute
static void test_9B2_unknown_attr_decimal() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><decimal name="D" unknown="x"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_decimal");
}

// Test 5e: Unknown attribute on sequence returns unknown_attribute
static void test_9B2_unknown_attr_sequence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S" unknown="x">
      <length name="L"/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_sequence");
}

// Test 5f: Unknown attribute on length returns unknown_attribute
static void test_9B2_unknown_attr_length() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L" unknown="x"/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_length");
}

// Test 5g: Unknown attribute on exponent returns unknown_attribute
static void test_9B2_unknown_attr_exponent() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent unknown="x"><constant>0</constant></exponent>
      <mantissa><constant>0</constant></mantissa>
    </decimal>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_exponent");
}

// Test 5h: Unknown attribute on mantissa returns unknown_attribute
static void test_9B2_unknown_attr_mantissa() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D">
      <exponent><constant>0</constant></exponent>
      <mantissa unknown="x"><constant>0</constant></mantissa>
    </decimal>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_mantissa");
}

// Test 5i: Unknown attribute on operator returns unknown_attribute
static void test_9B2_unknown_attr_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"><constant unknown="x">42</constant></uInt32>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_unknown_attr_operator");
}

// Test 5j: Unknown attribute on reference returns unknown_attribute
static void test_9B2_unknown_attr_reference() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <typeRef name="SomeType" unknown="x"/>
    <uInt32 name="F1"/>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    // Should have both unknown_attribute and unsupported_reference
    CHECK(!r.ok);
    CHECK(has_issue(r, "unknown_attribute"));
    CHECK(!r.compiled.valid());
    CHECK(r.compiled.empty());
    CHECK_EQ(r.compiled.size(), 0u);
    CHECK(r.compiled.templates().empty());
    CHECK(r.compiled.find(0) == nullptr);
    CHECK(r.compiled.find(1) == nullptr);
    TEST_PASS("9B2_unknown_attr_reference");
}

// Test 6: Allowed synthetic operator attributes compile on corresponding operator families
static void test_9B2_operator_attr_allowed_families() {
    // value on constant
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"><constant value="42"/></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 42u);
    }
    // value,key,dictionary on default
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"><default value="10" key="k1" dictionary="global"/></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].op.has_initial);
    }
    // value,key,dictionary on copy
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"><copy value="5" key="k2" dictionary="template"/></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].op.has_initial);
    }
    // value,key,dictionary on increment
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"><increment value="1" key="k3" dictionary="global"/></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].op.has_initial);
    }
    // key,dictionary on delta
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1"><delta key="k4" dictionary="global"/></string></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].op.kind == OpKind::Delta);
    }
    // key,dictionary on tail
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F1"><tail key="k5" dictionary="global"/></string></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.templates()[0].fields[0].op.kind == OpKind::Tail);
    }
    TEST_PASS("9B2_operator_attr_allowed_families");
}

// Test 7: Text-only and value-only forms produce equivalent metadata for constant, default, copy, increment
static void test_9B2_text_value_equivalent_constant() {
    const char* xml_text = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>42</constant></uInt32></template>
</templates>)";
    const char* xml_value = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant value="42"/></uInt32></template>
</templates>)";
    auto r1 = compile_templates_from_string(xml_text);
    auto r2 = compile_templates_from_string(xml_value);
    CHECK(r1.ok);
    CHECK(r2.ok);
    const auto& f1 = r1.compiled.templates()[0].fields[0];
    const auto& f2 = r2.compiled.templates()[0].fields[0];
    CHECK(f1.op.kind == f2.op.kind);
    CHECK(f1.op.has_constant == f2.op.has_constant);
    CHECK(f1.op.constant_uint == f2.op.constant_uint);
    CHECK(f1.op.constant_str == f2.op.constant_str);
    TEST_PASS("9B2_text_value_equivalent_constant");
}

static void test_9B2_text_value_equivalent_default() {
    const char* xml_text = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><default>10</default></uInt32></template>
</templates>)";
    const char* xml_value = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><default value="10"/></uInt32></template>
</templates>)";
    auto r1 = compile_templates_from_string(xml_text);
    auto r2 = compile_templates_from_string(xml_value);
    CHECK(r1.ok);
    CHECK(r2.ok);
    const auto& f1 = r1.compiled.templates()[0].fields[0];
    const auto& f2 = r2.compiled.templates()[0].fields[0];
    CHECK(f1.op.kind == f2.op.kind);
    CHECK(f1.op.has_initial == f2.op.has_initial);
    CHECK(f1.op.initial_uint == f2.op.initial_uint);
    TEST_PASS("9B2_text_value_equivalent_default");
}

static void test_9B2_text_value_equivalent_copy() {
    const char* xml_text = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><copy>7</copy></uInt32></template>
</templates>)";
    const char* xml_value = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><copy value="7"/></uInt32></template>
</templates>)";
    auto r1 = compile_templates_from_string(xml_text);
    auto r2 = compile_templates_from_string(xml_value);
    CHECK(r1.ok);
    CHECK(r2.ok);
    const auto& f1 = r1.compiled.templates()[0].fields[0];
    const auto& f2 = r2.compiled.templates()[0].fields[0];
    CHECK(f1.op.kind == f2.op.kind);
    CHECK(f1.op.has_initial == f2.op.has_initial);
    CHECK(f1.op.initial_uint == f2.op.initial_uint);
    TEST_PASS("9B2_text_value_equivalent_copy");
}

static void test_9B2_text_value_equivalent_increment() {
    const char* xml_text = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><increment>3</increment></uInt32></template>
</templates>)";
    const char* xml_value = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><increment value="3"/></uInt32></template>
</templates>)";
    auto r1 = compile_templates_from_string(xml_text);
    auto r2 = compile_templates_from_string(xml_value);
    CHECK(r1.ok);
    CHECK(r2.ok);
    const auto& f1 = r1.compiled.templates()[0].fields[0];
    const auto& f2 = r2.compiled.templates()[0].fields[0];
    CHECK(f1.op.kind == f2.op.kind);
    CHECK(f1.op.has_initial == f2.op.has_initial);
    CHECK(f1.op.initial_uint == f2.op.initial_uint);
    TEST_PASS("9B2_text_value_equivalent_increment");
}

// Test 8: Simultaneous direct text and value returns ambiguous_operator_value for constant, default, copy, increment
static void test_9B2_ambiguous_value_constant() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant value="42">42</constant></uInt32></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "ambiguous_operator_value");
    TEST_PASS("9B2_ambiguous_value_constant");
}

static void test_9B2_ambiguous_value_default() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><default value="10">10</default></uInt32></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "ambiguous_operator_value");
    TEST_PASS("9B2_ambiguous_value_default");
}

static void test_9B2_ambiguous_value_copy() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><copy value="7">7</copy></uInt32></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "ambiguous_operator_value");
    TEST_PASS("9B2_ambiguous_value_copy");
}

static void test_9B2_ambiguous_value_increment() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><increment value="3">3</increment></uInt32></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "ambiguous_operator_value");
    TEST_PASS("9B2_ambiguous_value_increment");
}

// Test 9: value on delta and tail returns unknown_attribute
static void test_9B2_value_on_delta_unknown_attribute() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F"><delta value="x"/></string></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_value_on_delta_unknown_attribute");
}

static void test_9B2_value_on_tail_unknown_attribute() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><string name="F"><tail value="x"/></string></template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    check_invalid_compile_9B2(r, "unknown_attribute");
    TEST_PASS("9B2_value_on_tail_unknown_attribute");
}

// Test 10a: Split direct CDATA/text content is concatenated in document order
static void test_9B2_split_cdata_concatenation() {
    // Two adjacent CDATA segments should be concatenated: 42, not 4
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant><![CDATA[4]]><![CDATA[2]]></constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 42u);
    }
    // CDATA followed by PCDATA in document order
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant><![CDATA[1]]>23</constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 123u);
    }
    // PCDATA followed by CDATA
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant>9<![CDATA[9]]></constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK_EQ(r.compiled.templates()[0].fields[0].op.constant_uint, 99u);
    }
    TEST_PASS("9B2_split_cdata_concatenation");
}

// Test 10b: value attribute with non-empty content in a later PCDATA/CDATA segment returns ambiguous_operator_value
static void test_9B2_value_with_later_cdata_ambiguous() {
    // value attribute present, but there is also direct CDATA content (in a later segment)
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant value="42"><![CDATA[10]]></constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "ambiguous_operator_value");
    }
    // value attribute with trailing PCDATA
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F"><constant value="5">5</constant></uInt32></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        check_invalid_compile_9B2(r, "ambiguous_operator_value");
    }
    TEST_PASS("9B2_value_with_later_cdata_ambiguous");
}

// Test 10: xmlns and xmlns:prefix are not ordinary attributes
static void test_9B2_xmlns_not_ordinary_attribute() {
    // xmlns on root
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates xmlns="http://www.fixprotocol.org/ns/fast/td/1.1">
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
    }
    // xmlns:prefix on root
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates xmlns:fast="http://www.fixprotocol.org/ns/fast/td/1.1">
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
    }
    // xmlns on field
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1" xmlns="http://example.com"/>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
    }
    TEST_PASS("9B2_xmlns_not_ordinary_attribute");
}

// === 9B2 phase 3 tests ===

// Test 1: Each caller limit above its hard ceiling returns compile_limit_exceeds_hard_ceiling
static void test_hard_ceiling_max_templates() {
    CompileLimits lim;
    lim.max_templates = 4097;
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    check_invalid_compile_9B2(r, "compile_limit_exceeds_hard_ceiling");
    TEST_PASS("hard_ceiling_max_templates");
}

static void test_hard_ceiling_max_fields() {
    CompileLimits lim;
    lim.max_fields_per_template = 65536;
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    check_invalid_compile_9B2(r, "compile_limit_exceeds_hard_ceiling");
    TEST_PASS("hard_ceiling_max_fields");
}

static void test_hard_ceiling_max_nesting() {
    CompileLimits lim;
    lim.max_nesting_depth = 33;
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    check_invalid_compile_9B2(r, "compile_limit_exceeds_hard_ceiling");
    TEST_PASS("hard_ceiling_max_nesting");
}

// Test 2: Exact hard-ceiling values are accepted using small XML
static void test_hard_ceiling_exact_templates() {
    CompileLimits lim;
    lim.max_templates = 4096;
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    TEST_PASS("hard_ceiling_exact_templates");
}

static void test_hard_ceiling_exact_fields() {
    CompileLimits lim;
    lim.max_fields_per_template = 65535;
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    TEST_PASS("hard_ceiling_exact_fields");
}

static void test_hard_ceiling_exact_nesting() {
    CompileLimits lim;
    lim.max_nesting_depth = 32;
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <uInt32 name="F1"/>
    </group>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    TEST_PASS("hard_ceiling_exact_nesting");
}

// Test 3: max_templates boundary succeeds exactly at limit and fails on next
static void test_max_templates_boundary() {
    CompileLimits lim;
    lim.max_templates = 1;
    // Exactly 1 template succeeds
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
        CHECK_EQ(r.compiled.size(), 1u);
    }
    // 2 templates fails with excessive_templates
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg1"><uInt32 name="F1"/></template>
  <template id="2" name="Msg2"><uInt32 name="F2"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        check_invalid_compile_9B2(r, "excessive_templates");
    }
    TEST_PASS("max_templates_boundary");
}

// Test 3b: max_templates counts encountered templates including invalid ones
static void test_max_templates_includes_invalid() {
    CompileLimits lim;
    lim.max_templates = 1;
    // First template is invalid (missing id), second exceeds budget
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template name="NoId"><uInt32 name="F1"/></template>
  <template id="2" name="Msg"><uInt32 name="F2"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(!r.ok);
        CHECK(has_issue(r, "excessive_templates"));
    }
    // First template invalid (duplicate id), second exceeds budget
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="A"><uInt32 name="F1"/></template>
  <template id="1" name="B"><uInt32 name="F2"/></template>
  <template id="3" name="C"><uInt32 name="F3"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(!r.ok);
        CHECK(has_issue(r, "excessive_templates"));
    }
    TEST_PASS("max_templates_includes_invalid");
}

// Test 4: max_fields_per_template boundary succeeds at limit, fails on next scalar
static void test_max_fields_boundary_scalar() {
    CompileLimits lim;
    lim.max_fields_per_template = 1;
    // Exactly 1 field succeeds
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg"><uInt32 name="F1"/></template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
        CHECK_EQ(r.compiled.templates()[0].fields.size(), 1u);
    }
    // 2 fields fails with excessive_fields
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"/>
    <uInt32 name="F2"/>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        check_invalid_compile_9B2(r, "excessive_fields");
    }
    TEST_PASS("max_fields_boundary_scalar");
}

// Test 5: Field accounting includes nested group/sequence descendants and sequence <length>
static void test_field_accounting_nested() {
    CompileLimits lim;
    lim.max_fields_per_template = 3;
    // sequence + length + 1 entry = 3 fields exactly
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"/>
      <uInt32 name="E"/>
    </sequence>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
    }
    // sequence + length + 2 entries = 4 fields exceeds limit
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <sequence name="S">
      <length name="L"/>
      <uInt32 name="E1"/>
      <uInt32 name="E2"/>
    </sequence>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        check_invalid_compile_9B2(r, "excessive_fields");
    }
    // group + 1 child = 2 fields, plus scalar = 3 exactly
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"/>
    <group name="G">
      <uInt32 name="G1"/>
    </group>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
    }
    // group + 2 children + scalar = 4 exceeds limit
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="F1"/>
    <group name="G">
      <uInt32 name="G1"/>
      <uInt32 name="G2"/>
    </group>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        check_invalid_compile_9B2(r, "excessive_fields");
    }
    TEST_PASS("field_accounting_nested");
}

// Test 6: Prohibited composite node rejected before descending into subtree
static void test_composite_rejected_before_subtree() {
    CompileLimits lim;
    lim.max_nesting_depth = 1;
    // Group at depth 1 (inside template at depth 0) with invalid child inside
    // excessive_nesting should fire, NOT the unknown_element from the subtree
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <group name="Inner">
        <bogusWidget name="X"/>
      </group>
    </group>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml, lim);
    CHECK(!r.ok);
    CHECK(has_issue(r, "excessive_nesting"));
    CHECK(!r.compiled.valid());
    CHECK(r.compiled.empty());
    CHECK_EQ(r.compiled.size(), 0u);
    CHECK(r.compiled.templates().empty());
    CHECK(r.compiled.find(0) == nullptr);
    CHECK(r.compiled.find(1) == nullptr);
    TEST_PASS("composite_rejected_before_subtree");
}

// Test 7: Nesting succeeds at configured boundary, rejects first descendant beyond
static void test_nesting_boundary() {
    CompileLimits lim;
    lim.max_nesting_depth = 1;
    // depth 0 (template) + depth 1 (group/sequence) = ok
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <uInt32 name="F1"/>
    </group>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
    }
    // depth 0 + depth 1 + depth 2 (nested group) = excessive_nesting
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <group name="G">
      <group name="Inner">
        <uInt32 name="F1"/>
      </group>
    </group>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        check_invalid_compile_9B2(r, "excessive_nesting");
    }
    TEST_PASS("nesting_boundary");
}

// Test 8: Decimal counts as exactly one field for the budget
static void test_decimal_field_budget_count() {
    CompileLimits lim;
    lim.max_fields_per_template = 2;
    // decimal + scalar = 2 fields exactly
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D"/>
    <uInt32 name="F1"/>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        CHECK(r.ok);
        CHECK(r.compiled.valid());
        CHECK_EQ(r.compiled.templates()[0].fields.size(), 2u);
        CHECK(r.compiled.templates()[0].fields[0].is_decimal);
        CHECK_EQ(r.compiled.templates()[0].fields[1].name, "F1");
    }
    // decimal + 2 scalars = 3 fields exceeds limit
    {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <decimal name="D"/>
    <uInt32 name="F1"/>
    <uInt32 name="F2"/>
  </template>
</templates>)";
        auto r = compile_templates_from_string(xml, lim);
        check_invalid_compile_9B2(r, "excessive_fields");
    }
    TEST_PASS("decimal_field_budget_count");
}

// Test 9: Mixed scalar + decimal + group + sequence/length indices contiguous, no decimal gap
static void test_mixed_field_index_contiguous() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <uInt32 name="A"/>
    <decimal name="B"/>
    <group name="C">
      <uInt32 name="C1"/>
    </group>
    <sequence name="D">
      <length name="DL"/>
      <uInt32 name="D1"/>
    </sequence>
  </template>
</templates>)";
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    CHECK(r.compiled.valid());
    const auto& fields = r.compiled.templates()[0].fields;
    CHECK_EQ(fields.size(), 4u);
    // Decimal now counts as 1 (no gap), indices are contiguous
    CHECK_EQ(fields[0].index, 0u);  // uInt32 A
    CHECK_EQ(fields[1].index, 1u);  // decimal B
    CHECK_EQ(fields[2].index, 2u);  // group C
    CHECK_EQ(fields[3].index, 4u);  // sequence D (after group child C1 at 3)
    CHECK(fields[1].is_decimal);
    CHECK(fields[2].has_children);
    CHECK_EQ(fields[2].children.size(), 1u);
    CHECK_EQ(fields[2].children[0].index, 3u);  // C1
    CHECK(fields[3].is_sequence);
    CHECK_EQ(fields[3].children.size(), 2u); // length + entry
    CHECK_EQ(fields[3].children[0].index, 5u);  // DL (length)
    CHECK_EQ(fields[3].children[1].index, 6u);  // D1 (entry)
    TEST_PASS("mixed_field_index_contiguous");
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
    test_compile_provenance_and_order();
    test_fatal_path_duplicate_after_valid();
    test_fatal_path_unknown_element();
    test_copy_lifetime();
    test_move_lifetime();
    test_default_handle_invalid();
    // 9B1 tests
    test_template_id_uint32_max();
    test_template_id_zero();
    test_template_id_non_decimal();
    test_template_id_out_of_range();
    test_fix_tag_scalar_ok();
    test_fix_tag_decimal_ok();
    test_fix_tag_sequence_length_ok();
    test_fix_tag_zero();
    test_fix_tag_negative();
    test_fix_tag_non_decimal();
    test_fix_tag_out_of_range();
    test_uint32_constant_range();
    test_uint64_constant_range();
    test_int32_constant_range();
    test_int64_constant_range();
    test_decimal_component_range();
    test_sequence_length_initial_range();
    test_reject_hex_literal();
    test_valid_ascii_static_values();
    test_valid_unicode_static_values();
    test_non_ascii_in_ascii_field();
    test_invalid_unicode_surrogate();
    test_oversized_static_value();
    test_exact_1mib_static_value();
    test_uint64_default_initial();
    test_uint64_copy_initial();
    test_uint64_increment_initial();
    // 9B2 phase 1 structural validation tests
    test_structural_comments_whitespace_pi();
    test_structural_unknown_root_child();
    test_structural_misplaced_operator_in_template();
    test_structural_length_at_template_level();
    test_structural_length_at_group_level();
    test_structural_misplaced_exponent_in_template();
    test_structural_misplaced_mantissa_in_template();
    test_structural_misplaced_operator_in_group();
    test_structural_misplaced_exponent_in_group();
    test_structural_scalar_two_operators();
    test_structural_scalar_unknown_child();
    test_structural_operator_nested_child();
    test_structural_duplicate_decimal_exponent();
    test_structural_duplicate_decimal_mantissa();
    test_structural_decimal_unknown_child();
    test_structural_decimal_unknown_presence();
    test_structural_exponent_two_operators();
    test_structural_exponent_unknown_child();
    test_structural_mantissa_two_operators();
    test_structural_mantissa_unknown_child();
    test_structural_sequence_missing_length();
    test_structural_duplicate_sequence_length();
    test_structural_sequence_misplaced_operator();
    test_structural_sequence_misplaced_exponent();
    test_structural_sequence_misplaced_mantissa();
    test_structural_length_two_operators();
    test_structural_length_unknown_child();
    test_structural_reference_at_allowed_level();
    test_structural_misplaced_reference_in_scalar();
    // 9B2 phase 2 attribute/charset/operator-value tests
    test_9B2_ns_decls_no_unknown_attribute();
    test_9B2_moex_attr_combinations();
    test_9B2_string_charset_unicode_wire_type();
    test_9B2_unknown_charset();
    test_9B2_unknown_attr_root();
    test_9B2_unknown_attr_template();
    test_9B2_unknown_attr_scalar_string();
    test_9B2_unknown_attr_decimal();
    test_9B2_unknown_attr_sequence();
    test_9B2_unknown_attr_length();
    test_9B2_unknown_attr_exponent();
    test_9B2_unknown_attr_mantissa();
    test_9B2_unknown_attr_operator();
    test_9B2_unknown_attr_reference();
    test_9B2_operator_attr_allowed_families();
    test_9B2_text_value_equivalent_constant();
    test_9B2_text_value_equivalent_default();
    test_9B2_text_value_equivalent_copy();
    test_9B2_text_value_equivalent_increment();
    test_9B2_ambiguous_value_constant();
    test_9B2_ambiguous_value_default();
    test_9B2_ambiguous_value_copy();
    test_9B2_ambiguous_value_increment();
    test_9B2_value_on_delta_unknown_attribute();
    test_9B2_value_on_tail_unknown_attribute();
    test_9B2_split_cdata_concatenation();
    test_9B2_value_with_later_cdata_ambiguous();
    test_9B2_xmlns_not_ordinary_attribute();
    // 9B2 phase 3 compiler accounting tests
    test_hard_ceiling_max_templates();
    test_hard_ceiling_max_fields();
    test_hard_ceiling_max_nesting();
    test_hard_ceiling_exact_templates();
    test_hard_ceiling_exact_fields();
    test_hard_ceiling_exact_nesting();
    test_max_templates_boundary();
    test_max_templates_includes_invalid();
    test_max_fields_boundary_scalar();
    test_field_accounting_nested();
    test_composite_rejected_before_subtree();
    test_nesting_boundary();
    test_decimal_field_budget_count();
    test_mixed_field_index_contiguous();
    std::cout << "All decoder compiler tests passed.\n";
    return 0;
}
