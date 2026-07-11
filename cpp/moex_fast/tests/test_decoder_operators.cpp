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

int main() {
    test_default_operator();
    test_copy_operator();
    test_increment_operator();
    test_constant_operator();
    std::cout << "All decoder operator tests passed.\n";
    return 0;
}
