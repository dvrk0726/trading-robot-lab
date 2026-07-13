#include "moex_fast/fast_compiler.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "test_check.hpp"
#include <iostream>
#include <vector>
#include <functional>

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
    // template-id = 100 -> stop-bit: 0x64 | 0x80 = 0xE4
    // Const "HI": consumes NO wire bytes.
    // SeqNum = 42 -> stop-bit: 0x2A | 0x80 = 0xAA
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
    auto msg1 = hex("C0" "8A" "E4");

    // Message 2: template-id absent (reuse 10), Val=200
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

    // pmap: bit0=1 -> 0xC0, tmpl-id=99 -> 0x63|0x80 = 0xE3
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

    // Two messages concatenated.
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

// Test: optional null field via nullable wire NULL (no pmap field bit)
static void test_optional_null() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // Optional field without operator: no pmap field bit, nullable wire.
    // pmap: bit0=1 (tmpl-id only) -> 0xC0
    // tmpl-id=10 -> 0x8A
    // Val nullable uInt32 NULL -> 0x80
    auto data = hex("C0" "8A" "80");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_null);
    CHECK(!r.message.fields[0].is_present);
    CHECK(std::holds_alternative<std::monostate>(r.message.fields[0].value));

    TEST_PASS("optional_null");
}

// Test: optional non-null field via nullable wire (no pmap field bit)
static void test_optional_non_null() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: tmpl-id present -> 0xC0
    // tmpl-id=10 -> 0x8A
    // Val nullable uInt32 value 5: raw 6 -> 0x86
    auto data = hex("C0" "8A" "86");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(!r.message.fields[0].is_null);
    CHECK(r.message.fields[0].is_present);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].value), 5u);

    TEST_PASS("optional_non_null");
}

// Test: optional ASCII NULL via nullable wire (no pmap field bit)
static void test_optional_ascii_null() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="S" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: tmpl-id present -> 0xC0
    // tmpl-id=1 -> 0x81
    // Optional ASCII NULL: [80]
    auto data = hex("C0" "81" "80");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_null);
    CHECK(!r.message.fields[0].is_present);
    CHECK(std::holds_alternative<std::monostate>(r.message.fields[0].value));

    TEST_PASS("optional_ascii_null");
}

// Test: optional ASCII empty via nullable wire (no pmap field bit)
static void test_optional_ascii_empty() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="S" id="1" presence="optional"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: tmpl-id present -> 0xC0
    // tmpl-id=1 -> 0x81
    // Optional ASCII empty: [00 80]
    auto data = hex("C0" "81" "0080");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(!r.message.fields[0].is_null);
    CHECK(r.message.fields[0].is_present);
    CHECK_EQ(std::get<std::string>(r.message.fields[0].value), "");

    TEST_PASS("optional_ascii_empty");
}

// Test: optional Unicode NULL via nullable wire (no pmap field bit)
static void test_optional_unicode_null() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="S" id="1" presence="optional" charset="unicode"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: tmpl-id present -> 0xC0
    // tmpl-id=1 -> 0x81
    // Optional Unicode NULL: length nullable uInt32 NULL = [80]
    auto data = hex("C0" "81" "80");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_null);
    CHECK(!r.message.fields[0].is_present);
    CHECK(std::holds_alternative<std::monostate>(r.message.fields[0].value));

    TEST_PASS("optional_unicode_null");
}

// Test: optional Unicode empty via nullable wire (no pmap field bit)
static void test_optional_unicode_empty() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="Msg">
    <string name="S" id="1" presence="optional" charset="unicode"/>
  </template>
</templates>)";

    auto ts = compile_minimal(xml);

    // pmap: tmpl-id present -> 0xC0
    // tmpl-id=1 -> 0x81
    // Optional Unicode empty: length nullable raw 1 (0 empty + 1 offset) -> [81]
    auto data = hex("C0" "81" "81");

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(!r.message.fields[0].is_null);
    CHECK(r.message.fields[0].is_present);
    CHECK_EQ(std::get<std::string>(r.message.fields[0].value), "");

    TEST_PASS("optional_unicode_empty");
}

// --- New tests for round 9A ---

// Test: templates() returns const reference to session-owned handle
static void test_session_templates_accessor() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg"><uInt32 name="Val"/></template>
</templates>)";

    auto ts = compile_minimal(xml);
    DecoderSession session(ts);

    const auto& handle = session.templates();
    CHECK(handle.valid());
    CHECK_EQ(handle.size(), 1u);
    CHECK_EQ(handle.find(10)->name, "Msg");

    // Type check: must be const reference
    static_assert(std::is_same_v<
        decltype(session.templates()),
        const CompiledTemplateSet&>,
        "templates() must return const CompiledTemplateSet&");

    TEST_PASS("session_templates_accessor");
}

// Test 5: DecoderSession lifetime — session from local CompileResult works after result destroyed
static void test_session_lifetime() {
    // Create session via helper/lambda that destroys local compile result
    auto make_session = []() -> DecoderSession {
        const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";
        auto result = compile_templates_from_string(xml);
        CHECK(result.ok);
        // Construct session from local result's handle
        DecoderSession s(result.compiled);
        // result goes out of scope here — session must still work
        return s;
    };

    DecoderSession session = make_session();

    // Session must still be functional after local CompileResult destroyed
    auto data = hex("C0" "8A" "81");
    auto r = session.decode_one(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.template_id, 10u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].value), 1u);

    // Templates accessor must still return valid handle
    CHECK(session.templates().valid());
    CHECK_EQ(session.templates().size(), 1u);

    TEST_PASS("session_lifetime");
}

// Test 6: invalid-handle session returns InternalError, zero consumption, unchanged fingerprint
static void test_invalid_handle_session() {
    // Default-constructed (invalid) handle
    CompiledTemplateSet invalid;
    CHECK(!invalid.valid());

    DecoderSession session(invalid);

    // Fingerprint before decode
    auto fp_before = session.fingerprint();
    CHECK(!fp_before.has_template_id);
    CHECK_EQ(fp_before.template_id, 0u);

    // decode_one must return InternalError
    auto data = hex("C0" "8A" "81");
    auto r1 = session.decode_one(data.data(), data.size());
    CHECK(r1.status == DecodeStatus::InternalError);
    CHECK_EQ(r1.bytes_consumed, 0u);
    CHECK(!r1.issues.empty());
    CHECK_EQ(r1.issues[0].code, "invalid_compiled_template_set");
    CHECK_EQ(r1.issues[0].offset, 0u);

    // decode_exact must also return InternalError
    auto r2 = session.decode_exact(data.data(), data.size());
    CHECK(r2.status == DecodeStatus::InternalError);
    CHECK_EQ(r2.bytes_consumed, 0u);
    CHECK(!r2.issues.empty());
    CHECK_EQ(r2.issues[0].code, "invalid_compiled_template_set");
    CHECK_EQ(r2.issues[0].offset, 0u);

    // Fingerprint must be unchanged (session state unmodified)
    auto fp_after = session.fingerprint();
    CHECK(!fp_after.has_template_id);
    CHECK_EQ(fp_after.template_id, 0u);

    TEST_PASS("invalid_handle_session");
}

// Test: session from invalid handle; decode_exact variant
static void test_invalid_handle_session_exact() {
    CompiledTemplateSet invalid;
    DecoderSession session(invalid);

    auto data = hex("C0" "8A" "81");
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::InternalError);
    CHECK_EQ(r.bytes_consumed, 0u);
    CHECK_EQ(r.issues[0].code, "invalid_compiled_template_set");
    CHECK_EQ(r.issues[0].offset, 0u);

    TEST_PASS("invalid_handle_session_exact");
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
    test_optional_non_null();
    test_optional_ascii_null();
    test_optional_ascii_empty();
    test_optional_unicode_null();
    test_optional_unicode_empty();
    test_session_templates_accessor();
    test_session_lifetime();
    test_invalid_handle_session();
    test_invalid_handle_session_exact();
    std::cout << "All decoder session tests passed.\n";
    return 0;
}
