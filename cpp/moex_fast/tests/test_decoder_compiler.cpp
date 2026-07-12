#include "moex_fast/fast_compiler.hpp"
#include "test_check.hpp"
#include <iostream>
#include <type_traits>

using namespace moex_fast;

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
    std::cout << "All decoder compiler tests passed.\n";
    return 0;
}
