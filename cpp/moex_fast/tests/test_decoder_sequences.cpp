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

// Test: simple sequence with 2 entries
static void test_simple_sequence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="SeqMsg">
    <uInt32 name="Header" id="1"/>
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Action" id="279"/>
      <uInt32 name="ID" id="48"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Encode:
    // pmap: [tmpl-id=1] -> 0xC0
    // tmpl-id=10 -> 0x8A
    // Header=1 -> 0x81
    // Sequence length=2 -> 0x82
    // Entry pmap for each entry (2 optional fields in entry? No, both mandatory)
    // Actually, both fields in the sequence are mandatory (no presence="optional")
    // So no entry-level pmap needed... but the code always reads entry pmap.
    // Hmm, let me check: entry_has_pmap is always set to true in the compiler.
    // For mandatory fields with no operator, has_pmap_bit = false.
    // So entry_pmap_bits = 0, and we don't read a pmap.
    // Entry 0: Action=5 -> 0x85, ID=100 -> 0x64|0x80 = 0xE4
    // Entry 1: Action=3 -> 0x83, ID=200 -> 0x01|0x48 = 0x01, 0xC8... wait
    // 200 = 0xC8. 200 >> 7 = 1, 200 & 0x7F = 0x48. So: 0x01, 0xC8.
    // Wait: 0x01 << 7 | 0x48 = 128 + 72 = 200. But stop bit encoding:
    // First byte: 0x01 (no stop bit), second byte: 0x48 | 0x80 = 0xC8 (stop bit)
    // Hmm, 200 in stop-bit: 200 = 1*128 + 72. First byte = 1 (0x01), second = 72|0x80 = 0xC8
    auto data = hex("C0"  // pmap
                     "8A"  // tmpl-id=10
                     "81"  // Header=1
                     "82"  // seq length=2
                     "85" "E4"  // entry0: Action=5, ID=100
                     "83" "01C8");  // entry1: Action=3, ID=200

    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    if (r.status != DecodeStatus::Ok) {
        std::cerr << "Decode failed: " << decode_status_name(r.status) << "\n";
        for (const auto& issue : r.issues) {
            std::cerr << "  [" << issue.code << "] " << issue.message << "\n";
        }
    }
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 2u);
    CHECK_EQ(r.message.fields[0].name, "Header");
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].value), 1u);

    CHECK(r.message.fields[1].is_sequence);
    CHECK_EQ(r.message.fields[1].entries.size(), 2u);

    // Entry 0
    CHECK_EQ(r.message.fields[1].entries[0].size(), 2u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].entries[0][0].value), 5u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].entries[0][1].value), 100u);

    // Entry 1
    CHECK_EQ(r.message.fields[1].entries[1].size(), 2u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].entries[1][0].value), 3u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].entries[1][1].value), 200u);

    TEST_PASS("simple_sequence");
}

// Test: empty sequence
static void test_empty_sequence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="EmptySeq">
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // pmap: [tmpl-id=1] -> 0xC0, tmpl=10->0x8A, seq length=0->0x80
    auto data = hex("C0" "8A" "80");
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK(r.message.fields[0].is_sequence);
    CHECK(r.message.fields[0].entries.empty());

    TEST_PASS("empty_sequence");
}

// Test: mandatory empty sequence — length 0, zero entries, no entry pmap
static void test_mandatory_empty_sequence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="MandatoryEmptySeq">
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // pmap: [tmpl-id=1] -> 0xC0, tmpl=10->0x8A, seq length=0->0x80
    auto data = hex("C0" "8A" "80");
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_sequence);
    CHECK(!r.message.fields[0].sequence_is_null);
    CHECK(r.message.fields[0].entries.empty());

    TEST_PASS("mandatory_empty_sequence");
}

// Test: optional NULL sequence — nullable length NULL, no entry pmap
static void test_optional_null_sequence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="OptNullSeq">
    <sequence name="Entries" presence="optional">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify compiler: sequence has no pmap bit, length is nullable
    {
        const auto& seq = ts.find(10)->fields[0];
        CHECK(seq.is_sequence);
        CHECK(!seq.has_pmap_bit);
        CHECK(!seq.is_mandatory);
        CHECK(!seq.children[0].is_mandatory);  // length is nullable
    }

    // pmap: [tmpl-id=1] -> 0xC0, tmpl=10->0x8A
    // Optional sequence length: nullable uInt32 NULL = 0x80
    auto data = hex("C0" "8A" "80");
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_sequence);
    CHECK(r.message.fields[0].sequence_is_null);
    CHECK(!r.message.fields[0].is_present);
    CHECK(r.message.fields[0].entries.empty());

    TEST_PASS("optional_null_sequence");
}

// Test: optional empty sequence — nullable length 0, zero entries, no entry pmap
static void test_optional_empty_sequence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="OptEmptySeq">
    <sequence name="Entries" presence="optional">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // pmap: [tmpl-id=1] -> 0xC0, tmpl=10->0x8A
    // Optional sequence length: nullable uInt32 0 = 0x81
    auto data = hex("C0" "8A" "81");
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(r.message.fields.size(), 1u);
    CHECK(r.message.fields[0].is_sequence);
    CHECK(!r.message.fields[0].sequence_is_null);
    CHECK(r.message.fields[0].is_present);
    CHECK(r.message.fields[0].entries.empty());

    TEST_PASS("optional_empty_sequence");
}

// Test: entry without a presence map — mandatory fields with no operator
static void test_entry_without_presence_map() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="NoEntryPmap">
    <uInt32 name="Header" id="1"/>
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Action" id="279"/>
      <uInt32 name="ID" id="48"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify: entry fields have no pmap bits
    {
        const auto& seq = ts.find(10)->fields[1];
        CHECK(seq.is_sequence);
        CHECK(!seq.children[1].has_pmap_bit);  // Action: mandatory no-operator
        CHECK(!seq.children[2].has_pmap_bit);  // ID: mandatory no-operator
    }

    // Wire: pmap [tmpl-id=1] -> 0xC0, tmpl=10->0x8A, Header=1->0x81
    // Sequence length=1->0x81
    // Entry: no entry pmap, Action=5->0x85, ID=42->0xAA
    auto data = hex("C0" "8A" "81" "81" "85" "AA");
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK(r.message.fields[1].is_sequence);
    CHECK_EQ(r.message.fields[1].entries.size(), 1u);
    CHECK_EQ(r.message.fields[1].entries[0].size(), 2u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].entries[0][0].value), 5u);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[1].entries[0][1].value), 42u);

    TEST_PASS("entry_without_presence_map");
}

// Test: entry with optional constant — entry pmap exists for the optional constant
static void test_entry_with_optional_constant() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="EntryConstPmap">
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1"/>
      <string name="Const" id="2" presence="optional"><constant>ABC</constant></string>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify: Val has no pmap bit, Const (optional constant) has pmap bit
    {
        const auto& seq = ts.find(10)->fields[0];
        CHECK(seq.is_sequence);
        CHECK(!seq.children[1].has_pmap_bit);  // Val: mandatory no-operator
        CHECK(seq.children[2].has_pmap_bit);   // Const: optional constant
    }

    // Wire: pmap [tmpl-id=1] -> 0xC0, tmpl=10->0x8A
    // Sequence length=1->0x81
    // Entry pmap: [Const present=1] -> 0xE0 (bit7=stop, bit6=1 for Const)
    // Entry: Val=7->0x87
    auto data = hex("C0" "8A" "81" "E0" "87");
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    if (r.status != DecodeStatus::Ok) {
        std::cerr << "Decode failed: " << decode_status_name(r.status) << "\n";
        for (const auto& issue : r.issues) {
            std::cerr << "  [" << issue.code << "] " << issue.message << "\n";
        }
    }
    CHECK(r.status == DecodeStatus::Ok);
    CHECK(r.message.fields[0].is_sequence);
    CHECK_EQ(r.message.fields[0].entries.size(), 1u);
    CHECK_EQ(r.message.fields[0].entries[0].size(), 2u);
    // Val = 7
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].entries[0][0].value), 7u);
    // Const = "ABC" (from constant)
    CHECK(r.message.fields[0].entries[0][1].source == ValueSource::Constant);
    CHECK_EQ(std::get<std::string>(r.message.fields[0].entries[0][1].value), "ABC");

    TEST_PASS("entry_with_optional_constant");
}

// Test: sequence entry with optional none only has no entry pmap
static void test_optional_none_only_no_entry_pmap() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="OptNoneSeq">
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1" presence="optional"/>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify: optional none has no pmap bit
    {
        const auto& seq = ts.find(10)->fields[0];
        CHECK(seq.is_sequence);
        CHECK(!seq.children[1].has_pmap_bit);
        CHECK(seq.children[1].op.kind == OpKind::None);
    }

    // Decode 2 entries: one NULL, one non-NULL
    // pmap [tmpl-id=1] -> 0xC0, tmpl=10->0x8A, seq length=2->0x82
    // Entry 0: NO entry pmap, optional uInt32 NULL=0x80
    // Entry 1: NO entry pmap, optional uInt32 7 -> nullable raw 8=0x88
    auto data = hex("C0" "8A" "82"
                     "80"       // entry0: Val=NULL
                     "88");     // entry1: Val=7 (nullable raw 8)
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    if (r.status != DecodeStatus::Ok) {
        std::cerr << "Decode failed: " << decode_status_name(r.status) << "\n";
        for (const auto& issue : r.issues) {
            std::cerr << "  [" << issue.code << "] " << issue.message << "\n";
        }
    }
    CHECK(r.status == DecodeStatus::Ok);
    CHECK(r.message.fields[0].is_sequence);
    CHECK_EQ(r.message.fields[0].entries.size(), 2u);

    // Entry 0: NULL
    CHECK_EQ(r.message.fields[0].entries[0].size(), 1u);
    CHECK(r.message.fields[0].entries[0][0].is_null);

    // Entry 1: value 7
    CHECK_EQ(r.message.fields[0].entries[1].size(), 1u);
    CHECK(!r.message.fields[0].entries[1][0].is_null);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].entries[1][0].value), 7u);

    TEST_PASS("optional_none_only_no_entry_pmap");
}

// Test: sequence entry with optional none followed by optional constant has exactly one entry pmap bit
static void test_optional_none_then_constant_one_entry_bit() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="OptNoneConstSeq">
    <sequence name="Entries">
      <length name="NoEntries" id="268"/>
      <uInt32 name="Val" id="1" presence="optional"/>
      <string name="Const" id="2" presence="optional"><constant>ABC</constant></string>
    </sequence>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Verify: Val (optional none) has no pmap bit, Const (optional constant) has pmap bit
    {
        const auto& seq = ts.find(10)->fields[0];
        CHECK(seq.is_sequence);
        CHECK(!seq.children[1].has_pmap_bit);  // optional none: no pmap bit
        CHECK(seq.children[1].op.kind == OpKind::None);
        CHECK(seq.children[2].has_pmap_bit);   // optional constant: has pmap bit
        CHECK(seq.children[2].op.kind == OpKind::Constant);
    }

    // Decode 1 entry:
    // pmap [tmpl-id=1] -> 0xC0, tmpl=10->0x8A, seq length=1->0x81
    // Entry pmap: [Const present=1] -> 0xE0 (1 bit, stop=1, value=1)
    // Entry: optional uInt32 NULL=0x80, Const="ABC" (from constant)
    auto data = hex("C0" "8A" "81"
                     "E0"       // entry pmap: Const present=1
                     "80");     // Val=NULL (nullable wire, no pmap bit)
    DecoderSession session(ts);
    auto r = session.decode_exact(data.data(), data.size());
    if (r.status != DecodeStatus::Ok) {
        std::cerr << "Decode failed: " << decode_status_name(r.status) << "\n";
        for (const auto& issue : r.issues) {
            std::cerr << "  [" << issue.code << "] " << issue.message << "\n";
        }
    }
    CHECK(r.status == DecodeStatus::Ok);
    CHECK(r.message.fields[0].is_sequence);
    CHECK_EQ(r.message.fields[0].entries.size(), 1u);
    CHECK_EQ(r.message.fields[0].entries[0].size(), 2u);

    // Val = NULL (nullable wire, no pmap)
    CHECK(r.message.fields[0].entries[0][0].is_null);

    // Const = "ABC" (from constant, pmap bit=1)
    CHECK(r.message.fields[0].entries[0][1].source == ValueSource::Constant);
    CHECK_EQ(std::get<std::string>(r.message.fields[0].entries[0][1].value), "ABC");

    TEST_PASS("optional_none_then_constant_one_entry_bit");
}

int main() {
    test_simple_sequence();
    test_empty_sequence();
    test_mandatory_empty_sequence();
    test_optional_null_sequence();
    test_optional_empty_sequence();
    test_entry_without_presence_map();
    test_entry_with_optional_constant();
    test_optional_none_only_no_entry_pmap();
    test_optional_none_then_constant_one_entry_bit();
    std::cout << "All decoder sequence tests passed.\n";
    return 0;
}
