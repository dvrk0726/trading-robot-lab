#include "moex_fast/fast_compiler.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "test_check.hpp"
#include <iostream>
#include <vector>

using namespace moex_fast;

// Helper: build a minimal template XML and compile it
static CompiledTemplateSet compile_minimal(const char* xml) {
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    return result.compiled;
}

// Helper: hex string to bytes
static std::vector<std::uint8_t> hex(const char* h) {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; h[i] && h[i + 1]; i += 2) {
        auto hexval = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        bytes.push_back(static_cast<std::uint8_t>(hexval(h[i]) * 16 + hexval(h[i + 1])));
    }
    return bytes;
}

// Test: decode a single message with constants and mandatory integers
static void test_single_message_constants() {
    // Template: constant string + mandatory uInt32
    // Template ID 100, pmap bit for template-id = 1
    // Wire: pmap(1 bit for tmpl-id), tmpl-id=100, constant="HI", SeqNum=42
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="100" name="SimpleMsg">
    <string name="Const" id="1"><constant>HI</constant></string>
    <uInt32 name="SeqNum" id="34"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // Encode manually:
    // pmap: bit0=1 (template-id present) -> single byte with stop bit
    // pmap byte: 0xC0 (bit6=1 for tmpl-id, rest zero, stop bit set)
    // Wait: pmap is read as MSB-first within each byte, with stop bit in bit 7.
    // For 1 pmap bit (template-id): data bit is bit 6 of first byte.
    // Set bit 6 = 1: byte = 0x40 | 0x80 = 0xC0
    // template-id = 100 -> stop-bit: 0x64 | 0x80 = 0xE4
    // Const "HI": stop-bit terminated ASCII: 0x48 0x49 0x00
    // SeqNum = 42 -> stop-bit: 0x2A | 0x80 = 0xAA
    // Constant "HI" consumes NO wire bytes. Data = pmap + tmpl-id + SeqNum only.
    auto data = hex("C0" "E4" "AA");

    DecoderSession session(ts);
    auto result = session.decode_exact(data.data(), data.size());

    CHECK(result.status == DecodeStatus::Ok);
    CHECK_EQ(result.message.template_id, 100u);
    CHECK_EQ(result.message.template_name, "SimpleMsg");
    CHECK_EQ(result.message.fields.size(), 2u);
    CHECK_EQ(result.message.fields[0].name, "Const");
    CHECK(result.message.fields[0].source == ValueSource::Constant);
    CHECK(std::holds_alternative<std::string>(result.message.fields[0].value));
    CHECK_EQ(std::get<std::string>(result.message.fields[0].value), "HI");

    CHECK_EQ(result.message.fields[1].name, "SeqNum");
    CHECK(result.message.fields[1].source == ValueSource::Wire);
    CHECK(std::holds_alternative<std::uint64_t>(result.message.fields[1].value));
    CHECK_EQ(std::get<std::uint64_t>(result.message.fields[1].value), 42u);

    TEST_PASS("single_message_constants");
}

// Test: template-ID reuse (second message omits template-id)
static void test_template_id_reuse() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // Message 1: template-id present (10), Val=100
    // pmap: bit0=1 -> 0xC0, tmpl-id=10 -> 0x8A, Val=100 -> 0x64|0x80 = 0xE4
    auto msg1 = hex("C0" "8A" "E4");

    // Message 2: template-id absent (reuse 10), Val=200
    // pmap: bit0=0 -> 0x80 (stop bit, no data bits set)
    // Val=200 -> 0x01 0x48... let me compute: 200 = 0xC8
    // 200 >> 7 = 1, 200 & 0x7F = 0x48. So 0x01 0xC8
    auto msg2 = hex("80" "01C8");

    DecoderSession session(ts);
    auto r1 = session.decode_one(msg1.data(), msg1.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK_EQ(r1.message.template_id, 10u);

    auto r2 = session.decode_one(msg2.data(), msg2.size());
    CHECK(r2.status == DecodeStatus::Ok);
    CHECK_EQ(r2.message.template_id, 10u);  // reused
    CHECK_EQ(std::get<std::uint64_t>(r2.message.fields[0].value), 200u);

    TEST_PASS("template_id_reuse");
}

// Test: first message without template-id fails
static void test_first_message_no_template_id() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: bit0=0 (no template-id), stop bit set
    auto data = hex("80");
    DecoderSession session(ts);
    auto result = session.decode_one(data.data(), data.size());
    CHECK(result.status == DecodeStatus::MissingPreviousTemplate);
    TEST_PASS("first_message_no_template_id");
}

// Test: unknown template ID
static void test_unknown_template_id() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: bit0=1 -> 0xC0, tmpl-id=99 -> 0x63|0x80 = 0xE3... wait 99 = 0x63, |0x80 = 0xE3
    auto data = hex("C0" "E3");
    DecoderSession session(ts);
    auto result = session.decode_one(data.data(), data.size());
    CHECK(result.status == DecodeStatus::UnknownTemplate);
    TEST_PASS("unknown_template_id");
}

// Test: decode_exact with trailing bytes
static void test_trailing_bytes() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // Valid message + trailing byte
    // pmap: 0xC0, tmpl-id=10 -> 0x8A, Val=1 -> 0x81, trailing=0xFF
    auto data = hex("C0" "8A" "81" "FF");
    DecoderSession session(ts);
    auto result = session.decode_exact(data.data(), data.size());
    CHECK(result.status == DecodeStatus::TrailingBytes);
    TEST_PASS("trailing_bytes");
}

// Test: decode_one with prefix returns bytes_consumed
static void test_bytes_consumed() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // Two messages concatenated. First: pmap=0xC0, tmpl=0x8A, val=0x81
    // Second: pmap=0xC0, tmpl=0x8A, val=0x82
    auto data = hex("C0" "8A" "81" "C0" "8A" "82");

    DecoderSession session(ts);
    auto r1 = session.decode_one(data.data(), data.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK_EQ(r1.bytes_consumed, 3u);

    auto r2 = session.decode_one(data.data() + r1.bytes_consumed,
                                  data.size() - r1.bytes_consumed);
    CHECK(r2.status == DecodeStatus::Ok);
    CHECK_EQ(r2.bytes_consumed, 3u);

    TEST_PASS("bytes_consumed");
}

// Test: reset clears state
static void test_reset() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);
    DecoderSession session(ts);

    // Decode a message to set template-id state
    auto data = hex("C0" "8A" "81");
    auto r = session.decode_one(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);

    // Reset
    session.reset();

    // Now first message without template-id should fail
    auto data2 = hex("80");
    auto r2 = session.decode_one(data2.data(), data2.size());
    CHECK(r2.status == DecodeStatus::MissingPreviousTemplate);

    TEST_PASS("reset");
}

// Test: optional null field
static void test_optional_null() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: bit0=1 (tmpl-id), bit1=1 (Val present but null)
    // Byte: data bits are bit6=tmpl-id, bit5=Val
    // Set bit6=1, bit5=0 (absent): 0x40 | 0x80 = 0xC0
    // Hmm, bit5=0 means absent. Let me use: bit6=1, bit5=1 (present but null)
    // Actually, for optional fields: pmap bit = 1 means "present on wire", 0 means "use previous/initial"
    // For none operator with optional: pmap=1 means read from wire, pmap=0 means null
    // So to get null: set pmap bit to 0
    // pmap bits: [tmpl-id=1, Val=0] -> byte: bit6=1, bit5=0 -> 0x40 | 0x80 = 0xC0
    auto data = hex("C0" "8A");  // tmpl-id=10, Val absent (null)

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_null);
    CHECK(!r.message.fields[0].is_present);

    TEST_PASS("optional_null");
}

int main() {
    test_single_message_constants();
    test_template_id_reuse();
    test_first_message_no_template_id();
    test_unknown_template_id();
    test_trailing_bytes();
    test_bytes_consumed();
    test_reset();
    test_optional_null();
    std::cout << "All decoder session tests passed.\n";
    return 0;
}
