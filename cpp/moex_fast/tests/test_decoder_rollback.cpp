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

// Test: failed decode does not commit session state (transactional rollback)
static void test_rollback_session_state() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="F1" id="1"/>
    <uInt32 name="F2" id="2"/>
  </template>
</templates>)";

    auto ts = compile(xml);
    DecoderSession session(ts);

    // Message 1: establish template 10, F1=55, F2=77
    auto msg1 = hex("C0" "8A" "B7" "CD");
    auto r1 = session.decode_exact(msg1.data(), msg1.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK_EQ(r1.message.template_id, 10u);

    // Snapshot state
    auto fp1 = session.fingerprint();

    // Message 2: reuse template 10, F1=30 consumed, then truncated (no F2)
    auto msg2 = hex("80" "9E");
    auto r2 = session.decode_one(msg2.data(), msg2.size());
    CHECK(r2.status != DecodeStatus::Ok);

    // Verify state unchanged after failed decode
    auto fp2 = session.fingerprint();
    CHECK_EQ(fp1.has_template_id, fp2.has_template_id);
    CHECK_EQ(fp1.template_id, fp2.template_id);
    CHECK_EQ(fp1.dict_entry_count, fp2.dict_entry_count);
    CHECK_EQ(fp1.dict_hash, fp2.dict_hash);

    // Message 3: reuse template 10 successfully, F1=40, F2=50
    auto msg3 = hex("80" "A8" "B2");
    auto r3 = session.decode_exact(msg3.data(), msg3.size());
    CHECK(r3.status == DecodeStatus::Ok);
    CHECK_EQ(r3.message.template_id, 10u);
    CHECK_EQ(std::get<std::uint64_t>(r3.message.fields[0].value), 40u);
    CHECK_EQ(std::get<std::uint64_t>(r3.message.fields[1].value), 50u);

    TEST_PASS("rollback_session_state");
}

// Test: failed decode preserves template-id state
static void test_rollback_template_id() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
  <template id="20" name="Msg20">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile(xml);
    DecoderSession session(ts);

    // Message 1: template 10, Val=1
    auto msg1 = hex("C0" "8A" "81");
    auto r1 = session.decode_exact(msg1.data(), msg1.size());
    CHECK(r1.status == DecodeStatus::Ok);
    CHECK_EQ(r1.message.template_id, 10u);

    // Message 2: switch to template 20, but truncated
    // pmap: [tmpl-id=1, ...] -> 0xC0, tmpl=20->0x94
    auto msg2 = hex("C0" "94");  // missing Val
    auto r2 = session.decode_one(msg2.data(), msg2.size());
    CHECK(r2.status != DecodeStatus::Ok);

    // Message 3: reuse template-id (should still be 10, not 20)
    // pmap: [tmpl-id=0] -> 0x80, Val=2 -> 0x82
    auto msg3 = hex("80" "82");
    auto r3 = session.decode_exact(msg3.data(), msg3.size());
    CHECK(r3.status == DecodeStatus::Ok);
    CHECK_EQ(r3.message.template_id, 10u);  // not 20!
    CHECK_EQ(std::get<std::uint64_t>(r3.message.fields[0].value), 2u);

    TEST_PASS("rollback_template_id");
}

// Test: fingerprint is deterministic
static void test_fingerprint_deterministic() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg10">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    DecoderSession s1(ts);
    DecoderSession s2(ts);

    // Both sessions start with same state
    CHECK_EQ(s1.fingerprint().dict_hash, s2.fingerprint().dict_hash);

    // Decode same message in both (template present)
    auto msg1 = hex("C0" "8A" "81");
    s1.decode_exact(msg1.data(), msg1.size());
    s2.decode_exact(msg1.data(), msg1.size());

    CHECK_EQ(s1.fingerprint().dict_hash, s2.fingerprint().dict_hash);

    // Reuse previous template ID in both
    auto msg2 = hex("80" "82");
    s1.decode_exact(msg2.data(), msg2.size());
    s2.decode_exact(msg2.data(), msg2.size());

    CHECK_EQ(s1.fingerprint().dict_hash, s2.fingerprint().dict_hash);

    TEST_PASS("fingerprint_deterministic");
}

int main() {
    test_rollback_session_state();
    test_rollback_template_id();
    test_fingerprint_deterministic();
    std::cout << "All decoder rollback tests passed.\n";
    return 0;
}
