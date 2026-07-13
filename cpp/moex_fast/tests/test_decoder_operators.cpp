#include "moex_fast/fast_compiler.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "test_check.hpp"
#include <iostream>
#include <concepts>
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

// Assert the complete invalid/empty CompiledTemplateSet invariant for single-issue rejection.
static void check_single_issue_rejection(const CompileResult& r,
                                          const std::string& expected_code,
                                          const std::string& expected_field_path,
                                          const std::string& expected_message) {
    CHECK(!r.ok);
    CHECK(!r.compiled.valid());
    CHECK(r.compiled.empty());
    CHECK_EQ(r.compiled.size(), 0u);
    CHECK(r.compiled.templates().empty());
    CHECK(r.compiled.find(0) == nullptr);
    CHECK(r.compiled.find(1) == nullptr);
    // Exactly one issue
    CHECK_EQ(r.issues.size(), 1u);
    CHECK_EQ(r.issues[0].code, expected_code);
    CHECK_EQ(r.issues[0].field_path, expected_field_path);
    CHECK_EQ(r.issues[0].message, expected_message);
    // No secondary diagnostics
    CHECK(!has_issue(r, "unknown_attribute"));
    CHECK(!has_issue(r, "unknown_element"));
    CHECK(!has_issue(r, "multiple_operators"));
    CHECK(!has_issue(r, "unsupported_operator"));
    CHECK(!has_issue(r, "unsupported_operator_type"));
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

// Compile-time absence concepts: decimal component operator metadata removed
template<typename T>
concept has_is_decimal_component = requires { &T::is_decimal_component; };

template<typename T>
concept has_exponent_op = requires { &T::exponent_op; };

template<typename T>
concept has_mantissa_op = requires { &T::mantissa_op; };

static_assert(!has_is_decimal_component<OpInstruction>,
              "OpInstruction must not have is_decimal_component member");
static_assert(!has_exponent_op<CompiledField>,
              "CompiledField must not have exponent_op member");
static_assert(!has_mantissa_op<CompiledField>,
              "CompiledField must not have mantissa_op member");

// Test: optional decimal non-null - no field pmap bit, ordinary mantissa
static void test_optional_decimal_non_null_no_field_bit() {
    // Decimal fields never consume a field-level pmap bit.
    // pmap: [tmpl-id=1] -> 0xC0
    // tmpl-id=10 -> 0x8A
    // optional decimal: nullable exponent [81] = non-null 0, ordinary mantissa [81] = 1
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1" presence="optional"><exponent/><mantissa/></decimal>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify compiler metadata: optional decimal has NO field pmap bit
    {
        const auto& f = ts.find(10)->fields[0];
        CHECK(f.is_decimal);
        CHECK(!f.is_mandatory);
        CHECK(!f.has_pmap_bit);  // decimal: no field-level pmap bit
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

// Test: compile-time rejection of <constant> in top-level decimal exponent
static void test_decimal_compile_reject_constant_in_exponent() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1" presence="optional">
      <exponent bad_attr="x"><constant>5</constant><nested/></exponent>
      <mantissa/>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_single_issue_rejection(r, "unsupported_decimal_component_operator",
                                 "Price.exponent",
                                 "Operator <constant> on decimal component <exponent> "
                                 "is outside the accepted MOEX SPECTRA T0/T1 profile (Price.exponent)");

    TEST_PASS("decimal_compile_reject_constant_in_exponent");
}

// Test: compile-time rejection of <copy> in nested decimal mantissa (inside sequence)
static void test_decimal_compile_reject_copy_in_mantissa() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="SeqMsg">
    <sequence name="Entries">
      <length name="Count"/>
      <decimal name="Price" id="1" presence="optional">
        <exponent/>
        <mantissa bad_attr="y"><copy/><nested/></mantissa>
      </decimal>
    </sequence>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_single_issue_rejection(r, "unsupported_decimal_component_operator",
                                 "Entries.Price.mantissa",
                                 "Operator <copy> on decimal component <mantissa> "
                                 "is outside the accepted MOEX SPECTRA T0/T1 profile (Entries.Price.mantissa)");

    TEST_PASS("decimal_compile_reject_copy_in_mantissa");
}

// Test: mantissa-before-exponent XML order — proves actual document-order scan
static void test_decimal_mantissa_before_exponent_order() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1">
      <mantissa bad_attr="x"><copy/><nested/></mantissa>
      <exponent bad_attr="y"><constant>5</constant><nested/></exponent>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_single_issue_rejection(r, "unsupported_decimal_component_operator",
                                 "Price.mantissa",
                                 "Operator <copy> on decimal component <mantissa> "
                                 "is outside the accepted MOEX SPECTRA T0/T1 profile (Price.mantissa)");

    TEST_PASS("decimal_mantissa_before_exponent_order");
}

// Test: operator in second duplicate exponent — proves scanning of later duplicate nodes
static void test_decimal_operator_in_second_duplicate_exponent() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DecimalMsg">
    <decimal name="Price" id="1">
      <exponent/>
      <exponent bad_attr="x"><delta/><nested/></exponent>
      <mantissa/>
    </decimal>
  </template>
</templates>)";

    auto r = compile_templates_from_string(xml);
    check_single_issue_rejection(r, "unsupported_decimal_component_operator",
                                 "Price.exponent",
                                 "Operator <delta> on decimal component <exponent> "
                                 "is outside the accepted MOEX SPECTRA T0/T1 profile (Price.exponent)");

    TEST_PASS("decimal_operator_in_second_duplicate_exponent");
}

int main() {
    test_constant_operator();
    test_excluded_operator_default();
    test_excluded_operator_copy();
    test_excluded_operator_increment();
    test_excluded_operator_multiple();
    test_optional_decimal_non_null_no_field_bit();
    test_optional_decimal_null_no_mantissa();
    test_decimal_compile_reject_constant_in_exponent();
    test_decimal_compile_reject_copy_in_mantissa();
    test_decimal_mantissa_before_exponent_order();
    test_decimal_operator_in_second_duplicate_exponent();
    std::cout << "All decoder operator tests passed.\n";
    return 0;
}
