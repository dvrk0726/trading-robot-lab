#include "moex_fast/fast_compiler.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "test_check.hpp"
#include <iostream>
#include <vector>

using namespace moex_fast;

static std::vector<std::uint8_t> hex(const char* h) {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; h[i] && h[i + 1]; i += 2) {
        auto hv = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        bytes.push_back(static_cast<std::uint8_t>(hv(h[i]) * 16 + hv(h[i + 1])));
    }
    return bytes;
}

static CompiledTemplateSet compile(const char* xml) {
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    return r.compiled;
}

static bool has_issue(const CompileResult& r, const std::string& code) {
    for (const auto& issue : r.issues) {
        if (issue.code == code) return true;
    }
    return false;
}

static void check_invalid_compile(const CompileResult& r, const std::string& expected_code) {
    CHECK(!r.ok);
    CHECK(has_issue(r, expected_code));
    CHECK(!r.compiled.valid());
    CHECK(r.compiled.empty());
    CHECK_EQ(r.compiled.size(), 0u);
    CHECK(r.compiled.templates().empty());
    CHECK(r.compiled.find(0) == nullptr);
    CHECK(r.compiled.find(1) == nullptr);
}

// Test: constant operator - no wire bytes consumed
static void test_constant_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="ConstMsg">
    <string name="Type" id="1"><constant>X</constant></string>
    <uInt32 name="Val" id="2"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    // pmap: [tmpl-id=1] -> 0xC0
    // tmpl-id=10 -> 0x8A
    // Constant "X" consumes NO wire bytes
    // Val = 7 -> 0x87
    auto data = hex("C0" "8A" "87");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK(r.message.fields[0].source == ValueSource::Constant);
    CHECK_EQ(std::get<std::string>(r.message.fields[0].value), "X");
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].value), 7u);

    TEST_PASS("constant_operator");
}

// Test: excluded operator <default> fails compilation with unsupported_operator
static void test_excluded_operator_default() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DefaultMsg">
    <uInt32 name="Val" id="1"><default>42</default></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile(r, "unsupported_operator");
    TEST_PASS("excluded_operator_default");
}

// Test: excluded operator <copy> fails compilation with unsupported_operator
static void test_excluded_operator_copy() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="CopyMsg">
    <uInt32 name="Val" id="1"><copy/></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile(r, "unsupported_operator");
    TEST_PASS("excluded_operator_copy");
}

// Test: excluded operator <increment> fails compilation with unsupported_operator
static void test_excluded_operator_increment() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="IncrMsg">
    <uInt32 name="Seq" id="1"><increment>1</increment></uInt32>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile(r, "unsupported_operator");
    TEST_PASS("excluded_operator_increment");
}

// Test: template with multiple excluded operators fails on first encountered
static void test_excluded_operator_multiple() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="UInt64MaxMsg">
    <uInt64 name="ConstField" id="1"><constant>18446744073709551615</constant></uInt64>
    <uInt64 name="DefaultField" id="2"><default>18446744073709551615</default></uInt64>
    <uInt64 name="CopyField" id="3"><copy>18446744073709551615</copy></uInt64>
    <uInt64 name="IncrField" id="4"><increment>18446744073709551615</increment></uInt64>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_invalid_compile(r, "unsupported_operator");
    TEST_PASS("excluded_operator_multiple");
}

// Test: optional decimal non-null - no field pmap bit, ordinary mantissa
static void test_optional_decimal_non_null_no_field_bit() {
    // Decimal fields never consume a field-level pmap bit.
    // pmap: [tmpl-id=1] -> 0xC0
    // tmpl-id=10 -> 0x8A
    // optional decimal: nullable exponent [81] = non-null 0, ordinary mantissa [81] = 1
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify compiler metadata: optional decimal has NO field pmap bit
    {
        const auto& f = ts.find(10)->fields[0];
        CHECK(f.is_decimal);
        CHECK(!f.is_mandatory);
        CHECK(!f.has_pmap_bit);  // decimal: no field-level pmap bit
        CHECK(f.exponent_op.kind == OpKind::None);
        CHECK(f.mantissa_op.kind == OpKind::None);
    }

    auto msg = hex("C0" "8A" "81" "81");

    DecoderSession session(ts);
    auto r = session.decode_exact(msg.data(), msg.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.template_id, 10u);
    CHECK_EQ(r.message.fields.size(), 1u);

    auto& field = r.message.fields[0];
    CHECK(!field.is_null);
    CHECK(field.is_present);
    auto& dd = std::get<DecodedDecimal>(field.value);
    CHECK_EQ(dd.exponent, 0);
    CHECK_EQ(dd.mantissa, 1);

    TEST_PASS("optional_decimal_non_null_no_field_bit");
}

// Test: optional decimal NULL - no field pmap bit, no mantissa consumed
static void test_optional_decimal_null_no_mantissa() {
    // Decimal NULL: nullable exponent NULL [80], mantissa NOT consumed.
    // pmap: [tmpl-id=1] -> 0xC0
    // tmpl-id=10 -> 0x8A
    // nullable exponent NULL -> [80]
    // trailing byte FF is NOT consumed (mantissa omitted)
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile(xml);
    auto msg = hex("C0" "8A" "80" "FF");

    DecoderSession session(ts);
    auto r = session.decode_one(msg.data(), msg.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.template_id, 10u);
    CHECK_EQ(r.message.fields.size(), 1u);

    auto& field = r.message.fields[0];
    CHECK(field.is_null);
    CHECK(!field.is_present);
    CHECK(std::holds_alternative<std::monostate>(field.value));

    // Verify mantissa was NOT consumed: 2 bytes consumed (pmap + tmpl-id + exponent)
    CHECK_EQ(r.bytes_consumed, 3u);  // C0 + 8A + 80

    TEST_PASS("optional_decimal_null_no_mantissa");
}

// Test: decimal runtime preflight rejects non-None component operator
static void test_decimal_preflight_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify valid no-operator decimal compiles correctly
    {
        const auto* tmpl = ts.find(10);
        CHECK(tmpl != nullptr);
        CHECK_EQ(tmpl->fields.size(), 1u);
        const auto& f = tmpl->fields[0];
        CHECK(f.is_decimal);
        CHECK(!f.is_mandatory);
        CHECK(f.exponent_op.kind == OpKind::None);
        CHECK(f.mantissa_op.kind == OpKind::None);
    }

    // Test-only: simulate corrupted compiled state by setting mantissa_op.kind to Copy
    {
        const auto* tmpl = ts.find(10);
        auto& f = const_cast<CompiledField&>(tmpl->fields[0]);
        f.mantissa_op.kind = OpKind::Copy;
    }

    DecoderSession session(ts);

    // Snapshot fingerprint before decode
    auto fp_before = session.fingerprint();

    // pmap: [tmpl-id=1] -> 0xC0
    // tmpl-id 10 -> 0x8A
    // nullable exponent NULL -> 0x80
    auto msg = hex("C0" "8A" "80");
    auto r = session.decode_exact(msg.data(), msg.size());

    // Must reject with UnsupportedTemplateFeature
    CHECK(r.status == DecodeStatus::UnsupportedTemplateFeature);

    // Issue must carry unsupported_operator_runtime with exact decimal field path
    CHECK(r.issues.size() >= 1u);
    bool found_issue = false;
    for (const auto& issue : r.issues) {
        if (issue.code == "unsupported_operator_runtime" &&
            issue.field_path == "DecimalMsg.Price") {
            found_issue = true;
            break;
        }
    }
    CHECK(found_issue);

    // Fingerprint must be unchanged (transaction rolled back)
    auto fp_after = session.fingerprint();
    CHECK(fp_before.has_template_id == fp_after.has_template_id);
    CHECK_EQ(fp_before.template_id, fp_after.template_id);
    CHECK_EQ(fp_before.dict_entry_count, fp_after.dict_entry_count);
    CHECK_EQ(fp_before.dict_hash, fp_after.dict_hash);

    TEST_PASS("decimal_preflight_operator");
}

int main() {
    test_constant_operator();
    test_excluded_operator_default();
    test_excluded_operator_copy();
    test_excluded_operator_increment();
    test_excluded_operator_multiple();
    test_optional_decimal_non_null_no_field_bit();
    test_optional_decimal_null_no_mantissa();
    test_decimal_preflight_operator();
    std::cout << "All decoder operator tests passed.\n";
    return 0;
}
