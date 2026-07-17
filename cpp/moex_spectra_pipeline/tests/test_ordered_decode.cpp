#include "moex_spectra_pipeline/ordered_decode.hpp"
#include "moex_fast/fast_compiler.hpp"
#include "test_check.hpp"

#include <cstring>
#include <iostream>
#include <vector>

using namespace moex_spectra_pipeline;
using namespace moex_fast;
using namespace moex::spectra;
using namespace moex_raw;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static CompiledTemplateSet compile_minimal(const char* xml) {
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    return result.compiled;
}

// Encode a uInt32 with stop-bit encoding
static void encode_stopbit_u32(std::vector<std::uint8_t>& out, std::uint32_t val) {
    if (val < 128) {
        out.push_back(static_cast<std::uint8_t>(val | 0x80));
    } else if (val < 16384) {
        out.push_back(static_cast<std::uint8_t>((val >> 7) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val & 0x7F) | 0x80));
    } else if (val < 2097152) {
        out.push_back(static_cast<std::uint8_t>((val >> 14) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val >> 7) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val & 0x7F) | 0x80));
    } else if (val < 268435456u) {
        out.push_back(static_cast<std::uint8_t>((val >> 21) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val >> 14) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val >> 7) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val & 0x7F) | 0x80));
    } else {
        out.push_back(static_cast<std::uint8_t>((val >> 28) & 0x0F));
        out.push_back(static_cast<std::uint8_t>((val >> 21) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val >> 14) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val >> 7) & 0x7F));
        out.push_back(static_cast<std::uint8_t>((val & 0x7F) | 0x80));
    }
}

// Template: id=1, name="TestMsg"
//   <string name="MsgType" id="35"><constant>D</constant></string>
//   <uInt32 name="MsgSeqNum" id="34"/>
static const char* kBasicTemplateXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

// Build a FAST message for kBasicTemplateXml with tag34 = seq_num
static std::vector<std::uint8_t> make_msg(std::uint32_t seq_num) {
    std::vector<std::uint8_t> msg;
    msg.push_back(0xC0); // pmap: stop + template-id present
    msg.push_back(0x81); // template-id = 1
    encode_stopbit_u32(msg, seq_num);
    return msg;
}

// Make two metadata sides from a compiled template set's SHA-256 hex
static void make_metadata_pair(
    const CompiledTemplateSet& compiled,
    RawSegmentMetadata& out_a,
    RawSegmentMetadata& out_b
) {
    const std::string& hex_str = compiled.templates_sha256();
    CHECK(hex_str.size() == 64);

    out_a = {};
    out_a.source.source_side = SourceSide::A;
    out_b = {};
    out_b.source.source_side = SourceSide::B;

    auto hexbyte = [](char hi, char lo) -> std::uint8_t {
        auto nib = [](char c) -> std::uint8_t {
            if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
            return static_cast<std::uint8_t>(c - 'A' + 10);
        };
        return static_cast<std::uint8_t>((nib(hi) << 4) | nib(lo));
    };

    for (std::size_t i = 0; i < 32; ++i) {
        std::uint8_t b = hexbyte(hex_str[i * 2], hex_str[i * 2 + 1]);
        out_a.source.templates_sha256[i] = b;
        out_b.source.templates_sha256[i] = b;
    }
}

static OrderedMessageMetadata make_transport(std::uint32_t seq) {
    OrderedMessageMetadata t{};
    t.msg_seq_num = seq;
    t.side = FeedSide::A;
    t.capture_index = 1;
    t.capture_monotonic_ns = 1000;
    return t;
}

// ===========================================================================
// Tests
// ===========================================================================

static void test_valid_decode() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::Ok);
    CHECK(session.state() == OrderedDecodeState::Ready);

    auto body = make_msg(42);
    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);
    CHECK(dr.decoded_message.has_value());
    CHECK(dr.decoded_message->msg_seq_num == 42u);
    CHECK(dr.decoded_message->msg_type == "D");
    CHECK(dr.decoded_message->transport.msg_seq_num == 42u);

    TEST_PASS("valid_decode");
}

static void test_tag34_zero() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(0);
    auto dr = session.decode_ordered(make_transport(0), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);
    CHECK(dr.decoded_message.has_value());
    CHECK(dr.decoded_message->msg_seq_num == 0u);

    TEST_PASS("tag34_zero");
}

static void test_tag34_uint32_max() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(UINT32_MAX);
    auto dr = session.decode_ordered(make_transport(UINT32_MAX), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);
    CHECK(dr.decoded_message.has_value());
    CHECK(dr.decoded_message->msg_seq_num == UINT32_MAX);

    TEST_PASS("tag34_uint32_max");
}

static void test_external_internal_match() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(100);
    auto dr = session.decode_ordered(make_transport(100), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);

    TEST_PASS("external_internal_match");
}

static void test_external_internal_mismatch() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(100);
    auto dr = session.decode_ordered(make_transport(99), body);
    CHECK(dr.code == OrderedDecodeCode::ExternalInternalSequenceMismatch);
    CHECK(!dr.decoded_message.has_value());
    CHECK(session.state() == OrderedDecodeState::Failed);

    TEST_PASS("external_internal_mismatch");
}

static void test_missing_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="Other" id="99"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    encode_stopbit_u32(body, 42);
    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::MissingTag34);
    CHECK(!dr.decoded_message.has_value());

    TEST_PASS("missing_tag34");
}

static void test_duplicate_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="Seq1" id="34"/>
    <uInt32 name="Seq2" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 42);
    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::DuplicateTag34);

    TEST_PASS("duplicate_tag34");
}

static void test_null_tag34() {
    // Optional uInt32 with no operator uses nullable wire encoding (no pmap bit)
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="MsgSeqNum" id="34" presence="optional"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    // PMAP: 1 bit (template-id only). Optional field uses nullable wire, no pmap bit.
    // Wire: template-id=1, then nullable uInt32 NULL (0x80)
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: stop + tmpl present
    body.push_back(0x81); // template-id = 1
    body.push_back(0x80); // nullable uInt32 NULL

    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::MissingTag34);

    TEST_PASS("null_tag34");
}

static void test_wrong_type_tag34() {
    // Tag34 is int64 (signed) - variant is int64_t, not uint64_t
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <int64 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    // Encode: pmap=0xC0, template-id=1, int64 value 42
    // int64 encoding: same stop-bit encoding but for signed value 42
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap
    body.push_back(0x81); // template-id = 1
    body.push_back(0xAA); // int64 value 42 with stop bit

    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::InvalidTag34);

    TEST_PASS("wrong_type_tag34");
}

static void test_constant_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="MsgSeqNum" id="34"><constant>7</constant></uInt32>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);

    auto dr = session.decode_ordered(make_transport(7), body);
    CHECK(dr.code == OrderedDecodeCode::InvalidTag34);

    TEST_PASS("constant_tag34");
}

static void test_overflow_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt64 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    // Encode UINT32_MAX + 1 = 4294967296 as uInt64 stop-bit
    // 0x100000000: chunks LSB-first: 0,0,0,0,16
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap
    body.push_back(0x81); // template-id = 1
    body.push_back(0x10); // continuation
    body.push_back(0x00); // continuation
    body.push_back(0x00); // continuation
    body.push_back(0x00); // continuation
    body.push_back(0x80); // stop

    auto dr = session.decode_ordered(make_transport(0), body);
    CHECK(dr.code == OrderedDecodeCode::Tag34OutOfRange);

    TEST_PASS("overflow_tag34");
}

static void test_missing_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="Other" id="99"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 7);
    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::MissingTag35);

    TEST_PASS("missing_tag35");
}

static void test_duplicate_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="Type1" id="35"><constant>A</constant></string>
    <string name="Type2" id="35"><constant>B</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    encode_stopbit_u32(body, 42);
    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::DuplicateTag35);

    TEST_PASS("duplicate_tag35");
}

static void test_null_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35" presence="optional"/>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    // Optional string absent: pmap bit for tag35 not set, nullable wire NULL
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: tmpl present, tag35 absent
    body.push_back(0x81); // template-id = 1
    body.push_back(0x80); // nullable string NULL
    body.push_back(0xAA); // tag34 = 42

    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::MissingTag35);

    TEST_PASS("null_tag35");
}

static void test_wrong_type_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <uInt32 name="MsgType" id="35"/>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    body.push_back(0xC4); // tag35 = 68 ('D') with stop bit
    body.push_back(0xAA); // tag34 = 42

    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::InvalidTag35);

    TEST_PASS("wrong_type_tag35");
}

static void test_empty_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"/>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    // Wire string "": length=0 with stop bit = 0x80
    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    body.push_back(0x80); // string length 0 (empty)
    body.push_back(0xAA); // tag34 = 42

    auto dr = session.decode_ordered(make_transport(42), body);
    CHECK(dr.code == OrderedDecodeCode::InvalidTag35);

    TEST_PASS("empty_tag35");
}

static void test_constant_tag35() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(7);
    auto dr = session.decode_ordered(make_transport(7), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);
    CHECK(dr.decoded_message.has_value());
    CHECK(dr.decoded_message->msg_type == "D");

    TEST_PASS("constant_tag35");
}

static void test_sequence_reset_rejected() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    encode_stopbit_u32(body, 10);
    auto dr = session.decode_ordered(make_transport(10), body);
    CHECK(dr.code == OrderedDecodeCode::SequenceResetUnsupported);
    CHECK(!dr.decoded_message.has_value());
    CHECK(session.state() == OrderedDecodeState::Failed);

    TEST_PASS("sequence_reset_rejected");
}

static void test_malformed_fast() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body = {0xFF, 0xFF, 0xFF};
    auto dr = session.decode_ordered(make_transport(1), body);
    CHECK(dr.code == OrderedDecodeCode::DecodeFailed);
    CHECK(dr.decode_status != DecodeStatus::Ok);
    CHECK(!dr.decoded_message.has_value());
    CHECK(session.state() == OrderedDecodeState::Failed);

    TEST_PASS("malformed_fast");
}

static void test_trailing_bytes() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(1);
    body.push_back(0xFF);
    auto dr = session.decode_ordered(make_transport(1), body);
    CHECK(dr.code == OrderedDecodeCode::DecodeFailed);
    CHECK(dr.decode_status == DecodeStatus::TrailingBytes);

    TEST_PASS("trailing_bytes");
}

static void test_unknown_template() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: tmpl present
    body.push_back(0xE3); // template-id = 99 (0x63 | 0x80)
    body.push_back(0xAA);

    auto dr = session.decode_ordered(make_transport(1), body);
    CHECK(dr.code == OrderedDecodeCode::DecodeFailed);
    CHECK(dr.decode_status == DecodeStatus::UnknownTemplate);

    TEST_PASS("unknown_template");
}

static void test_decode_limits() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    DecodeLimits limits;
    limits.max_message_bytes = 512;
    limits.max_presence_map_bytes = 32;
    limits.max_sequence_entries = 1000;
    limits.max_total_nodes = 10000;
    limits.max_string_bytes = 512;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb, limits);
    CHECK(ir.code == OrderedDecodeCode::Ok);

    auto body = make_msg(1);
    auto dr = session.decode_ordered(make_transport(1), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);

    TEST_PASS("decode_limits");
}

static void test_template_id_reuse() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body1 = make_msg(1);
    auto dr1 = session.decode_ordered(make_transport(1), body1);
    CHECK(dr1.code == OrderedDecodeCode::Ok);

    auto body2 = make_msg(2);
    auto dr2 = session.decode_ordered(make_transport(2), body2);
    CHECK(dr2.code == OrderedDecodeCode::Ok);
    CHECK(dr2.decoded_message->msg_seq_num == 2u);

    TEST_PASS("template_id_reuse");
}

static void test_valid_template_hash() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::Ok);

    TEST_PASS("valid_template_hash");
}

static void test_template_hash_mismatch() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    mb.source.templates_sha256[0] ^= 0xFF;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::TemplateHashMismatch);
    CHECK(session.state() == OrderedDecodeState::Uninitialized);

    TEST_PASS("template_hash_mismatch");
}

static void test_invalid_template_handle() {
    CompiledTemplateSet empty;

    RawSegmentMetadata ma, mb;
    ma.source.source_side = SourceSide::A;
    mb.source.source_side = SourceSide::B;

    OrderedDecodeSession session;
    auto ir = session.initialize(empty, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);
    CHECK(session.state() == OrderedDecodeState::Uninitialized);

    TEST_PASS("invalid_template_handle");
}

static void test_empty_template_set() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
</templates>)";

    auto result = compile_templates_from_string(xml);

    RawSegmentMetadata ma, mb;
    ma.source.source_side = SourceSide::A;
    mb.source.source_side = SourceSide::B;

    OrderedDecodeSession session;
    auto ir = session.initialize(result.compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);

    TEST_PASS("empty_template_set");
}

static void test_metadata_a_b() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::Ok);

    TEST_PASS("metadata_a_b");
}

static void test_metadata_b_a() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);
    std::swap(ma.source.source_side, mb.source.source_side);

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, mb, ma);
    CHECK(ir.code == OrderedDecodeCode::Ok);

    TEST_PASS("metadata_b_a");
}

static void test_metadata_a_a() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);
    mb.source.source_side = SourceSide::A;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);

    TEST_PASS("metadata_a_a");
}

static void test_metadata_b_b() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);
    ma.source.source_side = SourceSide::B;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);

    TEST_PASS("metadata_b_b");
}

static void test_metadata_none() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);
    ma.source.source_side = SourceSide::None;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);

    TEST_PASS("metadata_none");
}

static void test_failed_init_retry() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;

    mb.source.templates_sha256[0] ^= 0xFF;
    auto ir1 = session.initialize(compiled, ma, mb);
    CHECK(ir1.code == OrderedDecodeCode::TemplateHashMismatch);
    CHECK(session.state() == OrderedDecodeState::Uninitialized);

    mb.source.templates_sha256[0] ^= 0xFF;
    auto ir2 = session.initialize(compiled, ma, mb);
    CHECK(ir2.code == OrderedDecodeCode::Ok);
    CHECK(session.state() == OrderedDecodeState::Ready);

    TEST_PASS("failed_init_retry");
}

static void test_stable_failed() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(100);
    auto dr1 = session.decode_ordered(make_transport(99), body);
    CHECK(dr1.code == OrderedDecodeCode::ExternalInternalSequenceMismatch);
    CHECK(session.state() == OrderedDecodeState::Failed);

    auto dr2 = session.decode_ordered(make_transport(100), make_msg(100));
    CHECK(dr2.code == OrderedDecodeCode::ExternalInternalSequenceMismatch);
    CHECK(dr2.decode_issues.size() == 0u);
    CHECK(!dr2.decoded_message.has_value());

    TEST_PASS("stable_failed");
}

static void test_already_initialized_after_ready() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);
    CHECK(session.state() == OrderedDecodeState::Ready);

    auto ir2 = session.initialize(compiled, ma, mb);
    CHECK(ir2.code == OrderedDecodeCode::AlreadyInitialized);
    CHECK(session.state() == OrderedDecodeState::Ready);

    TEST_PASS("already_initialized_after_ready");
}

static void test_already_initialized_after_failed() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    auto body = make_msg(100);
    (void)session.decode_ordered(make_transport(99), body);
    CHECK(session.state() == OrderedDecodeState::Failed);

    auto ir2 = session.initialize(compiled, ma, mb);
    CHECK(ir2.code == OrderedDecodeCode::AlreadyInitialized);
    CHECK(session.state() == OrderedDecodeState::Failed);

    TEST_PASS("already_initialized_after_failed");
}

static void test_decode_limits_zero() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    DecodeLimits limits;
    limits.max_message_bytes = 0;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb, limits);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);

    TEST_PASS("decode_limits_zero");
}

static void test_decode_limits_exceeds() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    DecodeLimits limits;
    limits.max_message_bytes = 2 * 1024 * 1024;

    OrderedDecodeSession session;
    auto ir = session.initialize(compiled, ma, mb, limits);
    CHECK(ir.code == OrderedDecodeCode::InvalidConfig);

    TEST_PASS("decode_limits_exceeds");
}

static void test_move_construction() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);
    CHECK(session.state() == OrderedDecodeState::Ready);

    OrderedDecodeSession moved(std::move(session));
    CHECK(moved.state() == OrderedDecodeState::Ready);

    auto body = make_msg(1);
    auto dr = moved.decode_ordered(make_transport(1), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);

    TEST_PASS("move_construction");
}

static void test_move_assignment() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    OrderedDecodeSession other;
    other = std::move(session);
    CHECK(other.state() == OrderedDecodeState::Ready);

    auto body = make_msg(5);
    auto dr = other.decode_ordered(make_transport(5), body);
    CHECK(dr.code == OrderedDecodeCode::Ok);
    CHECK(dr.decoded_message->msg_seq_num == 5u);

    TEST_PASS("move_assignment");
}

static void test_failed_then_decode() {
    auto compiled = compile_minimal(kBasicTemplateXml);
    RawSegmentMetadata ma, mb;
    make_metadata_pair(compiled, ma, mb);

    OrderedDecodeSession session;
    (void)session.initialize(compiled, ma, mb);

    std::vector<std::uint8_t> bad = {0xFF, 0xFF};
    auto dr1 = session.decode_ordered(make_transport(1), bad);
    CHECK(dr1.code == OrderedDecodeCode::DecodeFailed);
    CHECK(session.state() == OrderedDecodeState::Failed);

    auto body = make_msg(1);
    auto dr2 = session.decode_ordered(make_transport(1), body);
    CHECK(dr2.code == OrderedDecodeCode::DecodeFailed);
    CHECK(dr2.decode_issues.size() == 0u);
    CHECK(!dr2.decoded_message.has_value());

    TEST_PASS("failed_then_decode");
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    test_valid_decode();
    test_tag34_zero();
    test_tag34_uint32_max();
    test_external_internal_match();
    test_external_internal_mismatch();
    test_missing_tag34();
    test_duplicate_tag34();
    test_null_tag34();
    test_wrong_type_tag34();
    test_constant_tag34();
    test_overflow_tag34();
    test_missing_tag35();
    test_duplicate_tag35();
    test_null_tag35();
    test_wrong_type_tag35();
    test_empty_tag35();
    test_constant_tag35();
    test_sequence_reset_rejected();
    test_malformed_fast();
    test_trailing_bytes();
    test_unknown_template();
    test_decode_limits();
    test_template_id_reuse();
    test_valid_template_hash();
    test_template_hash_mismatch();
    test_invalid_template_handle();
    test_empty_template_set();
    test_metadata_a_b();
    test_metadata_b_a();
    test_metadata_a_a();
    test_metadata_b_b();
    test_metadata_none();
    test_failed_init_retry();
    test_stable_failed();
    test_already_initialized_after_ready();
    test_already_initialized_after_failed();
    test_decode_limits_zero();
    test_decode_limits_exceeds();
    test_move_construction();
    test_move_assignment();
    test_failed_then_decode();

    std::cout << "ALL TESTS PASSED (41 tests)\n";
    return 0;
}
