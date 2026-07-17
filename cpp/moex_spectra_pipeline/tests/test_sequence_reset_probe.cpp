#include "moex_spectra_pipeline/sequence_reset_probe.hpp"
#include "moex_spectra_pipeline/ordered_decode.hpp"
#include "moex_fast/fast_compiler.hpp"
#include "test_check.hpp"

#include <cstring>
#include <iostream>
#include <vector>

using namespace moex_spectra_pipeline;
using namespace moex_fast;
using namespace moex::spectra;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static CompiledTemplateSet compile_minimal(const char* xml) {
    auto result = compile_templates_from_string(xml);
    CHECK(result.ok);
    return result.compiled;
}

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

static OrderedMessageMetadata make_transport(std::uint32_t seq) {
    OrderedMessageMetadata t{};
    t.msg_seq_num = seq;
    t.side = FeedSide::A;
    t.capture_index = 1;
    t.capture_monotonic_ns = 1000;
    return t;
}

// Template with tag35="D" constant, tag34 uInt32, tag36 uInt32
// SequenceReset template (id=7): MsgType="4" constant, MsgSeqNum tag34, NewSeqNo tag36
static const char* kFullTemplateXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

// Template without template 7
static const char* kNoTemplate7Xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

// Build a SequenceReset message with explicit template ID 7
static std::vector<std::uint8_t> make_sequence_reset_explicit(
    std::uint32_t seq_num, std::uint32_t new_seq_no
) {
    std::vector<std::uint8_t> msg;
    msg.push_back(0xC0); // pmap: stop + template-id present
    msg.push_back(0x87); // template-id = 7
    encode_stopbit_u32(msg, seq_num);   // tag 34
    encode_stopbit_u32(msg, new_seq_no); // tag 36
    return msg;
}

// Build a message with no explicit template ID (reuse previous)
static std::vector<std::uint8_t> make_sequence_reset_implicit(
    std::uint32_t seq_num, std::uint32_t new_seq_no
) {
    std::vector<std::uint8_t> msg;
    msg.push_back(0x80); // pmap: stop only, no template-id
    encode_stopbit_u32(msg, seq_num);
    encode_stopbit_u32(msg, new_seq_no);
    return msg;
}

// Build a normal message (template ID 1, not 7)
static std::vector<std::uint8_t> make_normal_msg(std::uint32_t seq_num) {
    std::vector<std::uint8_t> msg;
    msg.push_back(0xC0); // pmap: stop + template-id present
    msg.push_back(0x81); // template-id = 1
    encode_stopbit_u32(msg, seq_num);
    return msg;
}

// Build a normal message with no explicit template ID
static std::vector<std::uint8_t> make_normal_msg_implicit(std::uint32_t seq_num) {
    std::vector<std::uint8_t> msg;
    msg.push_back(0x80); // pmap: stop only
    encode_stopbit_u32(msg, seq_num);
    return msg;
}

// ===========================================================================
// Tests
// ===========================================================================

static void test_not_initialized() {
    SequenceResetProbe probe;
    CHECK(probe.state() == SequenceResetProbeState::Uninitialized);

    auto r = probe.probe(make_transport(1), make_normal_msg(1));
    CHECK(r.code == SequenceResetProbeCode::NotInitialized);
    CHECK(!r.reset_message.has_value());

    TEST_PASS("not_initialized");
}

static void test_invalid_compiled_handle_and_retry() {
    CompiledTemplateSet empty;
    SequenceResetProbe probe;
    auto ir = probe.initialize(empty);
    CHECK(ir.code == SequenceResetProbeCode::InvalidConfig);
    CHECK(probe.state() == SequenceResetProbeState::Uninitialized);

    // Retry with valid
    auto compiled = compile_minimal(kFullTemplateXml);
    auto ir2 = probe.initialize(compiled);
    CHECK(ir2.code == SequenceResetProbeCode::NormalMessage);
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("invalid_compiled_handle_and_retry");
}

static void test_missing_template_7() {
    auto compiled = compile_minimal(kNoTemplate7Xml);
    SequenceResetProbe probe;
    auto ir = probe.initialize(compiled);
    CHECK(ir.code == SequenceResetProbeCode::InvalidConfig);
    CHECK(probe.state() == SequenceResetProbeState::Uninitialized);

    TEST_PASS("missing_template_7");
}

static void test_invalid_limits_zero() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    DecodeLimits limits;
    limits.max_message_bytes = 0;
    auto ir = probe.initialize(compiled, limits);
    CHECK(ir.code == SequenceResetProbeCode::InvalidConfig);

    TEST_PASS("invalid_limits_zero");
}

static void test_invalid_limits_exceeds() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    DecodeLimits limits;
    limits.max_presence_map_bytes = 128;
    auto ir = probe.initialize(compiled, limits);
    CHECK(ir.code == SequenceResetProbeCode::InvalidConfig);

    TEST_PASS("invalid_limits_exceeds");
}

static void test_already_initialized_ready() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    auto ir = probe.initialize(compiled);
    CHECK(ir.code == SequenceResetProbeCode::NormalMessage);
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    auto ir2 = probe.initialize(compiled);
    CHECK(ir2.code == SequenceResetProbeCode::AlreadyInitialized);
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("already_initialized_ready");
}

static void test_already_initialized_failed() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Force Failed via malformed pmap
    std::vector<std::uint8_t> bad = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    auto ir2 = probe.initialize(compiled);
    CHECK(ir2.code == SequenceResetProbeCode::AlreadyInitialized);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("already_initialized_failed");
}

static void test_normal_message_explicit_non7() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_normal_msg(42);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::NormalMessage);
    CHECK(!r.reset_message.has_value());
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("normal_message_explicit_non7");
}

static void test_normal_message_implicit_allow_false() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_normal_msg_implicit(42);
    SequenceResetProbeOptions opts;
    opts.allow_implicit_template_id = false;
    auto r = probe.probe(make_transport(42), body, opts);
    CHECK(r.code == SequenceResetProbeCode::NormalMessage);
    CHECK(!r.reset_message.has_value());
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("normal_message_implicit_allow_false");
}

static void test_correct_explicit_sequence_reset() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r.reset_message.has_value());
    CHECK(r.reset_message->msg_seq_num == 42u);
    CHECK(r.reset_message->new_seq_no == 100u);
    CHECK(r.reset_message->transport.msg_seq_num == 42u);
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("correct_explicit_sequence_reset");
}

static void test_second_implicit_after_first_explicit() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // First: explicit template 7
    auto body1 = make_sequence_reset_explicit(42, 100);
    auto r1 = probe.probe(make_transport(42), body1);
    CHECK(r1.code == SequenceResetProbeCode::SequenceReset);

    // Second: implicit (no template ID), allow=true
    auto body2 = make_sequence_reset_implicit(43, 101);
    SequenceResetProbeOptions opts;
    opts.allow_implicit_template_id = true;
    auto r2 = probe.probe(make_transport(43), body2, opts);
    CHECK(r2.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r2.reset_message.has_value());
    CHECK(r2.reset_message->msg_seq_num == 43u);
    CHECK(r2.reset_message->new_seq_no == 101u);

    TEST_PASS("second_implicit_after_first_explicit");
}

static void test_implicit_id_without_previous_7() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // First: normal message (template 1)
    auto body1 = make_normal_msg(1);
    auto r1 = probe.probe(make_transport(1), body1);
    CHECK(r1.code == SequenceResetProbeCode::NormalMessage);

    // Second: implicit with allow=true, but previous was 1, not 7
    auto body2 = make_sequence_reset_implicit(2, 100);
    SequenceResetProbeOptions opts;
    opts.allow_implicit_template_id = true;
    auto r2 = probe.probe(make_transport(2), body2, opts);
    CHECK(r2.code == SequenceResetProbeCode::MissingPreviousResetTemplate);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("implicit_id_without_previous_7");
}

static void test_malformed_pmap() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Valid pmap (0xC0: stop+tmpl present) but template-id has 6 continuation bytes without stop
    // -> exceeds 5-byte max for u32 stop-bit encoding -> NonCanonicalEncoding
    std::vector<std::uint8_t> bad = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("malformed_pmap");
}

static void test_unterminated_pmap() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // No stop bit
    std::vector<std::uint8_t> bad = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("unterminated_pmap");
}

static void test_pmap_limit() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    DecodeLimits limits;
    limits.max_presence_map_bytes = 2;
    limits.max_message_bytes = 1024 * 1024;
    limits.max_sequence_entries = 100000;
    limits.max_total_nodes = 1000000;
    limits.max_string_bytes = 1024 * 1024;
    (void)probe.initialize(compiled, limits);

    // 3 non-stop bytes without stop -> exceeds limit of 2
    std::vector<std::uint8_t> bad = {0x00, 0x00, 0x00};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::LimitExceeded);

    TEST_PASS("pmap_limit");
}

static void test_non_canonical_pmap() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Multi-byte pmap: first byte 0x00 (no stop), second byte 0x80 (stop, data bits all zero)
    // This is non-canonical per FAST 1.1 R7
    std::vector<std::uint8_t> bad = {0x00, 0x80};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::NonCanonicalEncoding);

    TEST_PASS("non_canonical_pmap");
}

static void test_truncated_template_id() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // pmap with template-id present, five continuation bytes without stop bit
    std::vector<std::uint8_t> bad = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::InvalidEncoding);

    TEST_PASS("truncated_template_id");
}

static void test_non_canonical_template_id() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // pmap with template-id present, leading zero byte: 0x00, 0x87
    std::vector<std::uint8_t> bad = {0xC0, 0x00, 0x87};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::NonCanonicalEncoding);

    TEST_PASS("non_canonical_template_id");
}

static void test_overflow_template_id() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // 5-byte stop-bit encoding of raw 2^32 (0x100000000) -> overflow
    std::vector<std::uint8_t> bad = {0xC0, 0x10, 0x00, 0x00, 0x00, 0x80};
    auto r = probe.probe(make_transport(1), bad);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::IntegerOverflow);

    TEST_PASS("overflow_template_id");
}

static void test_unknown_explicit_template_normal() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Template ID 99 (correctly encoded, not 7)
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: tmpl present
    body.push_back(0xE3); // template-id = 99 (0x63 | 0x80)
    encode_stopbit_u32(body, 42);

    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::NormalMessage);
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("unknown_explicit_template_normal");
}

static void test_malformed_fast_candidate() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Valid header for template 7, but body is garbage -> decode fails
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap
    body.push_back(0x87); // template-id = 7
    body.push_back(0xFF); // garbage
    body.push_back(0xFF);
    body.push_back(0xFF);

    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::DecodeFailed);
    CHECK(r.decode_status != DecodeStatus::Ok);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("malformed_fast_candidate");
}

static void test_trailing_bytes() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, 100);
    body.push_back(0xFF);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::DecodeFailed);
    CHECK(r.decode_status == DecodeStatus::TrailingBytes);

    TEST_PASS("trailing_bytes");
}

static void test_decoded_template_invariant() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Valid template 7 message
    auto body = make_sequence_reset_explicit(42, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r.reset_message.has_value());

    TEST_PASS("decoded_template_invariant");
}

static void test_missing_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::MissingTag34);

    TEST_PASS("missing_tag34");
}

static void test_duplicate_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="Seq1" id="34"/>
    <uInt32 name="Seq2" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::DuplicateTag34);

    TEST_PASS("duplicate_tag34");
}

static void test_null_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34" presence="optional"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    body.push_back(0x80); // nullable uInt32 NULL
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag34);

    TEST_PASS("null_tag34");
}

static void test_wrong_type_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <int64 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    body.push_back(0xAA); // int64 42
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag34);

    TEST_PASS("wrong_type_tag34");
}

static void test_constant_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"><constant>7</constant></uInt32>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(7), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag34);

    TEST_PASS("constant_tag34");
}

static void test_overflow_tag34() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt64 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // UINT32_MAX + 1
    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    body.push_back(0x10);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x80);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(0), body);
    CHECK(r.code == SequenceResetProbeCode::Tag34OutOfRange);

    TEST_PASS("overflow_tag34");
}

static void test_missing_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::MissingTag35);

    TEST_PASS("missing_tag35");
}

static void test_duplicate_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="Type1" id="35"><constant>A</constant></string>
    <string name="Type2" id="35"><constant>B</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::DuplicateTag35);

    TEST_PASS("duplicate_tag35");
}

static void test_null_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35" presence="optional"/>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // tmpl present, tag35 absent
    body.push_back(0x87); // template-id = 7
    body.push_back(0x80); // nullable string NULL
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag35);

    TEST_PASS("null_tag35");
}

static void test_wrong_type_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <uInt32 name="MsgType" id="35"/>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    body.push_back(0xC4); // uInt32 68
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag35);

    TEST_PASS("wrong_type_tag35");
}

static void test_empty_tag35() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"/>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    body.push_back(0x80); // empty string
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag35);

    TEST_PASS("empty_tag35");
}

static void test_constant_tag35_4() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);

    TEST_PASS("constant_tag35_4");
}

static void test_valid_tag35_not_4() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"/>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    body.push_back(0xC4); // string "D"
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::NotSequenceReset);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("valid_tag35_not_4");
}

static void test_missing_tag36() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::MissingTag36);

    TEST_PASS("missing_tag36");
}

static void test_duplicate_tag36() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeq1" id="36"/>
    <uInt32 name="NewSeq2" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    encode_stopbit_u32(body, 100);
    encode_stopbit_u32(body, 100);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::DuplicateTag36);

    TEST_PASS("duplicate_tag36");
}

static void test_null_tag36() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36" presence="optional"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    body.push_back(0x80); // nullable uInt32 NULL
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag36);

    TEST_PASS("null_tag36");
}

static void test_wrong_type_tag36() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <int64 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    body.push_back(0xAA); // int64 100
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag36);

    TEST_PASS("wrong_type_tag36");
}

static void test_constant_tag36() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt32 name="NewSeqNo" id="36"><constant>100</constant></uInt32>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::InvalidTag36);

    TEST_PASS("constant_tag36");
}

static void test_overflow_tag36() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="7" name="SequenceReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
    <uInt64 name="NewSeqNo" id="36"/>
  </template>
</templates>)";

    auto compiled = compile_minimal(xml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // UINT32_MAX + 1
    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x87);
    encode_stopbit_u32(body, 42);
    body.push_back(0x10);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x80);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::Tag36OutOfRange);

    TEST_PASS("overflow_tag36");
}

static void test_external_internal_mismatch() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, 100);
    auto r = probe.probe(make_transport(99), body);
    CHECK(r.code == SequenceResetProbeCode::ExternalInternalSequenceMismatch);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("external_internal_mismatch");
}

static void test_tag34_value_zero() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(0, 100);
    auto r = probe.probe(make_transport(0), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r.reset_message->msg_seq_num == 0u);

    TEST_PASS("tag34_value_zero");
}

static void test_tag34_value_uint32_max() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(UINT32_MAX, 100);
    auto r = probe.probe(make_transport(UINT32_MAX), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r.reset_message->msg_seq_num == UINT32_MAX);

    TEST_PASS("tag34_value_uint32_max");
}

static void test_tag36_value_zero() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, 0);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r.reset_message->new_seq_no == 0u);

    TEST_PASS("tag36_value_zero");
}

static void test_tag36_value_uint32_max() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, UINT32_MAX);
    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);
    CHECK(r.reset_message->new_seq_no == UINT32_MAX);

    TEST_PASS("tag36_value_uint32_max");
}

static void test_stable_failed() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // First: force Failed via malformed pmap
    std::vector<std::uint8_t> bad = {0x00, 0x80}; // non-canonical pmap
    auto r1 = probe.probe(make_transport(1), bad);
    CHECK(r1.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    // Second: returns terminal code, empty issues, no reset_message
    auto body = make_sequence_reset_explicit(42, 100);
    auto r2 = probe.probe(make_transport(42), body);
    CHECK(r2.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r2.decode_issues.size() == 0u);
    CHECK(!r2.reset_message.has_value());

    TEST_PASS("stable_failed");
}

static void test_move_construction() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    SequenceResetProbe moved(std::move(probe));
    CHECK(moved.state() == SequenceResetProbeState::Ready);

    auto body = make_sequence_reset_explicit(42, 100);
    auto r = moved.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);

    TEST_PASS("move_construction");
}

static void test_move_assignment() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    SequenceResetProbe other;
    other = std::move(probe);
    CHECK(other.state() == SequenceResetProbeState::Ready);

    auto body = make_sequence_reset_explicit(42, 100);
    auto r = other.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);

    TEST_PASS("move_assignment");
}

static void test_fast_body_unchanged_normal() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_normal_msg(42);
    const auto* orig_ptr = body.data();
    const auto orig_size = body.size();
    std::vector<std::uint8_t> body_copy(body);

    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::NormalMessage);

    // Verify pointer and content unchanged
    CHECK(body.data() == orig_ptr);
    CHECK(body.size() == orig_size);
    CHECK(body == body_copy);

    TEST_PASS("fast_body_unchanged_normal");
}

static void test_fast_body_unchanged_reset() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    auto body = make_sequence_reset_explicit(42, 100);
    const auto* orig_ptr = body.data();
    const auto orig_size = body.size();
    std::vector<std::uint8_t> body_copy(body);

    auto r = probe.probe(make_transport(42), body);
    CHECK(r.code == SequenceResetProbeCode::SequenceReset);

    CHECK(body.data() == orig_ptr);
    CHECK(body.size() == orig_size);
    CHECK(body == body_copy);

    TEST_PASS("fast_body_unchanged_reset");
}

static void test_valid_canonical_template_0x10000000() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Template ID 0x10000000 (canonical 5-byte stop-bit encoding)
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: stop + template-id present
    body.push_back(0x01); // (0x10000000 >> 28) & 0x0F
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x80); // stop bit

    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::NormalMessage);
    CHECK(!r.reset_message.has_value());
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("valid_canonical_template_0x10000000");
}

static void test_valid_canonical_template_uint32_max() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Template ID UINT32_MAX (canonical 5-byte stop-bit encoding)
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: stop + template-id present
    body.push_back(0x0F);
    body.push_back(0x7F);
    body.push_back(0x7F);
    body.push_back(0x7F);
    body.push_back(0xFF); // stop bit

    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::NormalMessage);
    CHECK(!r.reset_message.has_value());
    CHECK(probe.state() == SequenceResetProbeState::Ready);

    TEST_PASS("valid_canonical_template_uint32_max");
}

static void test_raw_2pow32_overflow() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Raw value 2^32 (0x100000000) -> IntegerOverflow
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: stop + template-id present
    body.push_back(0x10);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x80); // stop bit

    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::IntegerOverflow);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("raw_2pow32_overflow");
}

static void test_exactly_five_bytes_no_stop() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // Exactly 5 continuation bytes without stop bit -> InvalidEncoding
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: stop + template-id present
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);
    body.push_back(0x00);

    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::InvalidEncoding);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("exactly_five_bytes_no_stop");
}

static void test_early_truncation_need_more_data() {
    auto compiled = compile_minimal(kFullTemplateXml);
    SequenceResetProbe probe;
    (void)probe.initialize(compiled);

    // 2 continuation bytes, buffer ends before stop bit -> NeedMoreData
    std::vector<std::uint8_t> body;
    body.push_back(0xC0); // pmap: stop + template-id present
    body.push_back(0x00);
    body.push_back(0x00);

    auto r = probe.probe(make_transport(1), body);
    CHECK(r.code == SequenceResetProbeCode::HeaderDecodeFailed);
    CHECK(r.decode_status == DecodeStatus::NeedMoreData);
    CHECK(probe.state() == SequenceResetProbeState::Failed);

    TEST_PASS("early_truncation_need_more_data");
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    test_not_initialized();
    test_invalid_compiled_handle_and_retry();
    test_missing_template_7();
    test_invalid_limits_zero();
    test_invalid_limits_exceeds();
    test_already_initialized_ready();
    test_already_initialized_failed();
    test_normal_message_explicit_non7();
    test_normal_message_implicit_allow_false();
    test_correct_explicit_sequence_reset();
    test_second_implicit_after_first_explicit();
    test_implicit_id_without_previous_7();
    test_malformed_pmap();
    test_unterminated_pmap();
    test_pmap_limit();
    test_non_canonical_pmap();
    test_truncated_template_id();
    test_non_canonical_template_id();
    test_overflow_template_id();
    test_unknown_explicit_template_normal();
    test_malformed_fast_candidate();
    test_trailing_bytes();
    test_decoded_template_invariant();
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
    test_constant_tag35_4();
    test_valid_tag35_not_4();
    test_missing_tag36();
    test_duplicate_tag36();
    test_null_tag36();
    test_wrong_type_tag36();
    test_constant_tag36();
    test_overflow_tag36();
    test_external_internal_mismatch();
    test_tag34_value_zero();
    test_tag34_value_uint32_max();
    test_tag36_value_zero();
    test_tag36_value_uint32_max();
    test_stable_failed();
    test_move_construction();
    test_move_assignment();
    test_fast_body_unchanged_normal();
    test_fast_body_unchanged_reset();
    test_valid_canonical_template_0x10000000();
    test_valid_canonical_template_uint32_max();
    test_raw_2pow32_overflow();
    test_exactly_five_bytes_no_stop();
    test_early_truncation_need_more_data();

    std::cout << "ALL TESTS PASSED (57 tests)\n";
    return 0;
}
