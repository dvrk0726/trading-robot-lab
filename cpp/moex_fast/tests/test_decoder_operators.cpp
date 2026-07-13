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

// Test: default operator - absent uses initial, present overrides
static void test_default_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="DefaultMsg">
    <uInt32 name="Val" id="1"><default>42</default></uInt32>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Message 1: pmap bit for Val = 1 (present on wire), value = 100
    // pmap: [tmpl-id=1, Val=1] -> 0xE0
    // tmpl-id=10 -> 0x8A
    // Val=100 -> 0x64 | 0x80 = 0xE4
    auto msg1 = hex("E0" "8A" "E4");

    DecoderSession session(ts);
    auto r1 = session.decode_exact(msg1.data(), msg1.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK(r1.message.fields[0].source == ValueSource::Wire);
    CHECK_EQ(std::get<std::uint64_t>(r1.message.fields[0].value), 100u);

    // Message 2: pmap bit for Val = 0 (absent, use default 42)
    // pmap: [tmpl-id=0, Val=0] -> 0x80 (just stop bit)
    auto msg2 = hex("80");

    std::cout << "Decoding message 2..." << std::endl;
    auto r2 = session.decode_exact(msg2.data(), msg2.size());
    CHECK(r2.status == DecodeStatus::Ok);
    CHECK(r2.message.fields[0].source == ValueSource::Default);
    CHECK_EQ(std::get<std::uint64_t>(r2.message.fields[0].value), 42u);

    TEST_PASS("default_operator");
}

// Test: copy operator - wire update then previous reuse
static void test_copy_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="CopyMsg">
    <uInt32 name="Val" id="1"><copy/></uInt32>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Message 1: Val present, value = 55
    // pmap: [tmpl-id=1, Val=1] -> 0xE0
    // tmpl-id=10 -> 0x8A
    // Val=55 -> 0xB7 (55 | 0x80)
    auto msg1 = hex("E0" "8A" "B7");

    DecoderSession session(ts);
    auto r1 = session.decode_exact(msg1.data(), msg1.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK_EQ(std::get<std::uint64_t>(r1.message.fields[0].value), 55u);

    // Message 2: Val absent (copy previous = 55)
    // pmap: [tmpl-id=0, Val=0] -> 0x80
    auto msg2 = hex("80");

    auto r2 = session.decode_exact(msg2.data(), msg2.size());
    CHECK(r2.status == DecodeStatus::Ok);
    CHECK(r2.message.fields[0].source == ValueSource::Copy);
    CHECK_EQ(std::get<std::uint64_t>(r2.message.fields[0].value), 55u);

    TEST_PASS("copy_operator");
}

// Test: increment operator
static void test_increment_operator() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="IncrMsg">
    <uInt32 name="Seq" id="1"><increment>1</increment></uInt32>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Message 1: Seq present, value = 10
    auto msg1 = hex("E0" "8A" "8A");
    DecoderSession session(ts);
    auto r1 = session.decode_exact(msg1.data(), msg1.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK_EQ(std::get<std::uint64_t>(r1.message.fields[0].value), 10u);

    // Message 2: Seq absent (increment previous 10 -> 11)
    auto msg2 = hex("80");
    auto r2 = session.decode_exact(msg2.data(), msg2.size());
    CHECK(r2.status == DecodeStatus::Ok);
    CHECK(r2.message.fields[0].source == ValueSource::Increment);
    CHECK_EQ(std::get<std::uint64_t>(r2.message.fields[0].value), 11u);

    // Message 3: Seq absent again (increment 11 -> 12)
    auto msg3 = hex("80");
    auto r3 = session.decode_exact(msg3.data(), msg3.size());
    CHECK(r3.status == DecodeStatus::Ok);
    CHECK_EQ(std::get<std::uint64_t>(r3.message.fields[0].value), 12u);

    TEST_PASS("increment_operator");
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

// Test: 9B1 integration - template id 10 with four mandatory uInt64 fields
// constant, default, copy and increment each set to UINT64_MAX.
// Message C0 8A decodes all four fields as exact UINT64_MAX with correct ValueSource.
static void test_uint64_max_integration() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="UInt64MaxMsg">
    <uInt64 name="ConstField" id="1"><constant>18446744073709551615</constant></uInt64>
    <uInt64 name="DefaultField" id="2"><default>18446744073709551615</default></uInt64>
    <uInt64 name="CopyField" id="3"><copy>18446744073709551615</copy></uInt64>
    <uInt64 name="IncrField" id="4"><increment>18446744073709551615</increment></uInt64>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Message: C0 8A
    // pmap C0 = 11000000: tmpl-id present, Default/Copy/Incr absent (pmap bit 1/2/3 = 0)
    // ConstField has no pmap bit (mandatory + constant)
    // Default/Copy/Incr have pmap bits: all absent => use initial values
    auto msg = hex("C0" "8A");

    DecoderSession session(ts);
    auto r = session.decode_exact(msg.data(), msg.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.template_id, 10u);
    CHECK_EQ(r.message.fields.size(), 4u);

    // ConstField: constant UINT64_MAX
    CHECK_EQ(r.message.fields[0].name, "ConstField");
    CHECK(r.message.fields[0].source == ValueSource::Constant);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].value), UINT64_MAX);

    // DefaultField: default initial UINT64_MAX
    CHECK_EQ(r.message.fields[1].name, "DefaultField");
    CHECK(r.message.fields[1].source == ValueSource::Default);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].value), UINT64_MAX);

    // CopyField: copy initial UINT64_MAX
    CHECK_EQ(r.message.fields[2].name, "CopyField");
    CHECK(r.message.fields[2].source == ValueSource::Copy);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[2].value), UINT64_MAX);

    // IncrField: increment initial UINT64_MAX
    CHECK_EQ(r.message.fields[3].name, "IncrField");
    CHECK(r.message.fields[3].source == ValueSource::Increment);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[3].value), UINT64_MAX);

    TEST_PASS("uint64_max_integration");
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

int main() {
    test_default_operator();
    test_copy_operator();
    test_increment_operator();
    test_constant_operator();
    test_uint64_max_integration();
    test_optional_decimal_non_null_no_field_bit();
    test_optional_decimal_null_no_mantissa();
    std::cout << "All decoder operator tests passed.\n";
    return 0;
}
