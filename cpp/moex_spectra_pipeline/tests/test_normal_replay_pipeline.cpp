#include "moex_spectra_pipeline/normal_replay_pipeline.hpp"
#include "moex_fast/fast_compiler.hpp"
#include "test_check.hpp"

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using namespace moex_spectra_pipeline;
using namespace moex_fast;
using namespace moex::spectra;
using namespace moex_raw;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void encode_u32_le(std::vector<std::uint8_t>& out, std::uint32_t val) {
    out.push_back(static_cast<std::uint8_t>(val & 0xFF));
    out.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((val >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((val >> 24) & 0xFF));
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

static void sha256_fill(const char* data, std::size_t len, std::uint8_t out[32]) {
    std::memset(out, 0, 32);
    for (std::size_t i = 0; i < len && i < 32; ++i) {
        out[i] = static_cast<std::uint8_t>(data[i]);
    }
}

static std::vector<std::uint8_t> make_fast_msg(std::uint32_t seq) {
    std::vector<std::uint8_t> body;
    body.push_back(0xC0);
    body.push_back(0x81);
    encode_stopbit_u32(body, seq);
    return body;
}

static std::vector<std::uint8_t> make_datagram(const std::vector<std::uint8_t>& fast_body,
                                                std::uint32_t seq) {
    std::vector<std::uint8_t> dgram;
    encode_u32_le(dgram, seq);
    dgram.insert(dgram.end(), fast_body.begin(), fast_body.end());
    return dgram;
}

static std::vector<std::uint8_t> make_datagram(std::uint32_t seq) {
    return make_datagram(make_fast_msg(seq), seq);
}

static StreamSetInfo make_stream_set(const std::vector<std::string>& paths) {
    StreamSetInfo ss;
    if (paths.empty()) return ss;
    ParsedFilename pf;
    parse_canonical_filename(fs::path(paths[0]).filename().string(), pf);
    std::memcpy(ss.session_id, pf.session_id, 16);
    ss.source_id = pf.source_id;
    ss.channel_id = pf.channel_id;

    std::vector<std::pair<std::string, std::uint64_t>> indexed;
    for (const auto& p : paths) {
        ParsedFilename pf2;
        parse_canonical_filename(fs::path(p).filename().string(), pf2);
        indexed.emplace_back(p, pf2.segment_index);
    }
    std::sort(indexed.begin(), indexed.end(),
              [](auto& a, auto& b) { return a.second < b.second; });
    for (auto& [path, idx] : indexed) {
        ss.segment_paths.push_back(path);
        ss.segment_indexes.push_back(idx);
    }
    return ss;
}

static const char* kBasicTemplateXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="TestMsg">
    <string name="MsgType" id="35"><constant>D</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";

struct MetaPair {
    RawSegmentMetadata a;
    RawSegmentMetadata b;
};

static MetaPair make_meta_pair(const CompiledTemplateSet& compiled) {
    MetaPair mp;
    mp.a = {};
    for (int i = 0; i < 16; ++i) mp.a.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
    mp.a.session.feed_group = "ORDERS-LOG";
    mp.a.session.endpoint_role = "Incremental-A";
    mp.a.session.source_label = "test-a";
    mp.a.source.clock_domain = ClockDomain::Synthetic;
    mp.a.source.transport = Transport::Udp;
    mp.a.source.source_side = SourceSide::A;
    mp.a.source.source_id = 1;
    mp.a.source.channel_id = 1;
    sha256_fill("c", 1, mp.a.source.configuration_sha256);
    sha256_fill("f-a", 3, mp.a.source.endpoint_fingerprint_sha256);
    mp.a.created_utc_ns = 1700000000000000000ULL;

    mp.b = mp.a;
    mp.b.session.endpoint_role = "Incremental-B";
    mp.b.session.source_label = "test-b";
    mp.b.source.source_side = SourceSide::B;
    mp.b.source.source_id = 2;
    mp.b.source.channel_id = 2;
    sha256_fill("f-b", 3, mp.b.source.endpoint_fingerprint_sha256);

    const std::string& hex = compiled.templates_sha256();
    auto hexbyte = [](char hi, char lo) -> std::uint8_t {
        auto nib = [](char c) -> std::uint8_t {
            if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
            return static_cast<std::uint8_t>(c - 'A' + 10);
        };
        return static_cast<std::uint8_t>((nib(hi) << 4) | nib(lo));
    };
    for (std::size_t i = 0; i < 32; ++i) {
        std::uint8_t b = hexbyte(hex[i * 2], hex[i * 2 + 1]);
        mp.a.source.templates_sha256[i] = b;
        mp.b.source.templates_sha256[i] = b;
    }
    return mp;
}

static NormalPipelineConfig make_default_config() {
    NormalPipelineConfig cfg{};
    cfg.framing.max_datagram_bytes = 65536;
    cfg.sequencer.logical_feed = {1};
    cfg.sequencer.max_reorder_distance = 64;
    cfg.sequencer.reorder_wait_ns = 1000000000ULL;
    cfg.sequencer.storage.max_reorder_messages = 64;
    cfg.sequencer.storage.max_reorder_bytes = 64 * 1024;
    cfg.sequencer.storage.max_message_bytes = 4096;
    cfg.decode_limits = DecodeLimits{};
    cfg.initial_expected_seq = 1;
    return cfg;
}

struct CallbackCollector {
    std::vector<OrderedDecodedMessage> messages;
    std::vector<FeedSide> sides;
    std::vector<std::uint64_t> capture_indices;
    std::vector<std::uint64_t> capture_monotonic_nss;
    static void collect(void* ctx, OrderedDecodedMessage&& msg) {
        auto* c = static_cast<CallbackCollector*>(ctx);
        c->sides.push_back(msg.transport.side);
        c->capture_indices.push_back(msg.transport.capture_index);
        c->capture_monotonic_nss.push_back(msg.transport.capture_monotonic_ns);
        c->messages.push_back(std::move(msg));
    }
};

static std::vector<std::string> write_segment(
    const RawSegmentMetadata& meta,
    const std::string& dir,
    std::uint64_t capture_index,
    std::uint64_t capture_monotonic_ns,
    const std::vector<std::uint8_t>& payload
) {
    RawSegmentWriter w(meta, dir, {});
    CHECK(w.open().empty());
    RawPacketRecord rec{};
    rec.capture_index = capture_index;
    rec.capture_monotonic_ns = capture_monotonic_ns;
    rec.payload = payload;
    CHECK(w.append(rec).empty());
    CHECK(w.finalize().empty());
    return w.finalized_paths();
}

static std::vector<std::string> write_segment_multi(
    const RawSegmentMetadata& meta,
    const std::string& dir,
    const std::vector<std::pair<std::uint64_t, std::vector<std::uint8_t>>>& records,
    std::uint64_t start_index = 0,
    std::uint64_t base_ts = 1000
) {
    RawSegmentWriter w(meta, dir, {});
    CHECK(w.open().empty());
    for (std::size_t i = 0; i < records.size(); ++i) {
        RawPacketRecord rec{};
        rec.capture_index = start_index + static_cast<std::uint64_t>(i);
        rec.capture_monotonic_ns = base_ts + static_cast<std::uint64_t>(i);
        rec.payload = records[i].second;
        CHECK(w.append(rec).empty());
    }
    CHECK(w.finalize().empty());
    return w.finalized_paths();
}

// ===========================================================================
// Tests — each test scopes the pipeline so it's destroyed before fs::remove_all
// ===========================================================================

static void test_same_packet_ab_decoded_once() {
    auto dir = fs::temp_directory_path() / "nrp_test_same_ab";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.input_packets == 2u);
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages.size() == 1u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
        CHECK(collector.messages[0].msg_type == "D");
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("same_packet_ab_decoded_once");
}

static void test_a_before_b() {
    auto dir = fs::temp_directory_path() / "nrp_test_a_before_b";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 50, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 100, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.input_packets == 2u);
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("a_before_b");
}

static void test_b_before_a() {
    auto dir = fs::temp_directory_path() / "nrp_test_b_before_a";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 50, make_datagram(1));
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.input_packets == 2u);
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("b_before_a");
}

static void test_out_of_order_flush() {
    auto dir = fs::temp_directory_path() / "nrp_test_ooo";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment_multi(mp.a, (dir / "a").string(),
        {{0, make_datagram(2)}, {0, make_datagram(1)}}, 0, 100);
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, make_datagram(2)}, {0, make_datagram(1)}}, 0, 200);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.emitted_messages == 2u);
        CHECK(collector.messages.size() == 2u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
        CHECK(collector.messages[1].msg_seq_num == 2u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("out_of_order_flush");
}

static void test_multi_buffered_flush() {
    auto dir = fs::temp_directory_path() / "nrp_test_multi_flush";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment_multi(mp.a, (dir / "a").string(),
        {{0, make_datagram(1)}, {0, make_datagram(3)},
         {0, make_datagram(4)}, {0, make_datagram(5)},
         {0, make_datagram(2)}}, 0, 100);
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, make_datagram(1)}, {0, make_datagram(2)},
         {0, make_datagram(3)}, {0, make_datagram(4)},
         {0, make_datagram(5)}}, 0, 500);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.emitted_messages == 5u);
        CHECK(collector.messages.size() == 5u);
        for (std::uint32_t i = 0; i < 5; ++i) {
            CHECK(collector.messages[i].msg_seq_num == i + 1);
        }
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("multi_buffered_flush");
}

static void test_one_side_ends_early() {
    auto dir = fs::temp_directory_path() / "nrp_test_early_end";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    // B has seq 1,2,3 (earlier ts), A has only seq 5 (latest ts)
    // B emits 1,2,3. A seq 5 arrives (expected=4, future -> buffered). GapWait
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, make_datagram(1)}, {0, make_datagram(2)}, {0, make_datagram(3)}}, 0, 100);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 5000, make_datagram(5));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
    }
    CHECK(rr.code == NormalPipelineCode::ReplayEndedWithPendingGap);
    CHECK(rr.replay_code == moex_raw::AbReplayCode::End);
    fs::remove_all(dir);
    TEST_PASS("one_side_ends_early");
}

static void test_duplicate_equal_dropped() {
    auto dir = fs::temp_directory_path() / "nrp_test_dup_eq";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.input_packets == 2u);
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages.size() == 1u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("duplicate_equal_dropped");
}

static void test_duplicate_payload_mismatch_fails() {
    auto dir = fs::temp_directory_path() / "nrp_test_dup_mismatch";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);

    auto dgram3 = make_datagram(3);
    auto dgram2_orig = make_datagram(2);
    auto dgram2_mod_body = make_fast_msg(2);
    dgram2_mod_body.push_back(0xFF);
    auto dgram2_mod = make_datagram(dgram2_mod_body, 2);

    auto paths_a = write_segment_multi(mp.a, (dir / "a").string(),
        {{0, dgram3}, {0, dgram2_orig}, {0, dgram2_mod}}, 0, 100);
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, dgram3}, {0, dgram2_orig}, {0, dgram2_mod}}, 0, 200);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.sequencer_code == SequencerCode::DuplicatePayloadMismatch);
        CHECK(collector.messages.empty());
    }
    CHECK(rr.code == NormalPipelineCode::SequencerFailed);
    fs::remove_all(dir);
    TEST_PASS("duplicate_payload_mismatch_fails");
}

static void test_datagram_too_short_0_3_bytes() {
    auto dir = fs::temp_directory_path() / "nrp_test_short";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    std::vector<std::uint8_t> short_payload = {0x01, 0x02, 0x03};
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, short_payload);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.frame_code == FrameCode::DatagramTooShort);
    }
    CHECK(rr.code == NormalPipelineCode::FramingFailed);
    fs::remove_all(dir);
    TEST_PASS("datagram_too_short_0_3_bytes");
}

static void test_datagram_exactly_4_bytes() {
    auto dir = fs::temp_directory_path() / "nrp_test_4bytes";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    std::vector<std::uint8_t> empty_dgram = {0x01, 0x00, 0x00, 0x00};
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, empty_dgram);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.frame_code == FrameCode::EmptyFastBody);
    }
    CHECK(rr.code == NormalPipelineCode::FramingFailed);
    fs::remove_all(dir);
    TEST_PASS("datagram_exactly_4_bytes");
}

static void test_datagram_over_limit() {
    auto dir = fs::temp_directory_path() / "nrp_test_over_limit";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    NormalPipelineConfig cfg = make_default_config();
    cfg.framing.max_datagram_bytes = 10;
    std::vector<std::uint8_t> big_dgram(20, 0x42);
    encode_u32_le(big_dgram, 1);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, big_dgram);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.frame_code == FrameCode::DatagramTooLarge);
    }
    CHECK(rr.code == NormalPipelineCode::FramingFailed);
    fs::remove_all(dir);
    TEST_PASS("datagram_over_limit");
}

static void test_malformed_fast() {
    auto dir = fs::temp_directory_path() / "nrp_test_malformed";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    // 4-byte LE header (seq=1) + malformed FAST body
    std::vector<std::uint8_t> bad_dgram;
    encode_u32_le(bad_dgram, 1);
    bad_dgram.push_back(0xFF);
    bad_dgram.push_back(0xFF);
    bad_dgram.push_back(0xFF);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, bad_dgram);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.decode_code == OrderedDecodeCode::DecodeFailed);
    }
    CHECK(rr.code == NormalPipelineCode::DecodeFailed);
    fs::remove_all(dir);
    TEST_PASS("malformed_fast");
}

static void test_trailing_bytes() {
    auto dir = fs::temp_directory_path() / "nrp_test_trailing";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto fast = make_fast_msg(1);
    fast.push_back(0xFF);
    auto dgram = make_datagram(fast, 1);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, dgram);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.decode_code == OrderedDecodeCode::DecodeFailed);
    }
    CHECK(rr.code == NormalPipelineCode::DecodeFailed);
    fs::remove_all(dir);
    TEST_PASS("trailing_bytes");
}

static void test_external_tag34_mismatch() {
    auto dir = fs::temp_directory_path() / "nrp_test_tag34_mismatch";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    // Header seq=1 (expected), FAST body tag34=42 (mismatch)
    auto fast = make_fast_msg(42);
    auto dgram = make_datagram(fast, 1);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, dgram);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.decode_code == OrderedDecodeCode::ExternalInternalSequenceMismatch);
    }
    CHECK(rr.code == NormalPipelineCode::DecodeFailed);
    fs::remove_all(dir);
    TEST_PASS("external_tag34_mismatch");
}

static void test_template_id_reuse_after_ordering() {
    auto dir = fs::temp_directory_path() / "nrp_test_tmpl_reuse";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    std::vector<std::uint8_t> fast2;
    fast2.push_back(0x80);
    encode_stopbit_u32(fast2, 2);
    auto dgram2 = make_datagram(fast2, 2);
    auto dgram1 = make_datagram(1);
    auto paths_a = write_segment_multi(mp.a, (dir / "a").string(),
        {{0, dgram2}, {0, dgram1}}, 0, 100);
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, dgram2}, {0, dgram1}}, 0, 200);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.emitted_messages == 2u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
        CHECK(collector.messages[1].msg_seq_num == 2u);
        CHECK(collector.messages[1].msg_type == "D");
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("template_id_reuse_after_ordering");
}

static void test_sequence_uint32_max_then_zero() {
    auto dir = fs::temp_directory_path() / "nrp_test_wraparound";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    NormalPipelineConfig cfg = make_default_config();
    cfg.initial_expected_seq = UINT32_MAX;
    auto dgram_max = make_datagram(UINT32_MAX);
    auto dgram_zero = make_datagram(0);
    auto paths_a = write_segment_multi(mp.a, (dir / "a").string(),
        {{0, dgram_max}, {0, dgram_zero}}, 0, 100);
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, dgram_max}, {0, dgram_zero}}, 0, 200);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.emitted_messages == 2u);
        CHECK(collector.messages[0].msg_seq_num == UINT32_MAX);
        CHECK(collector.messages[1].msg_seq_num == 0u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("sequence_uint32_max_then_zero");
}

static void test_clean_replay_end() {
    auto dir = fs::temp_directory_path() / "nrp_test_clean_end";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.replay_code == moex_raw::AbReplayCode::End);
        CHECK(rr.input_packets == 2u);
        CHECK(rr.emitted_messages == 1u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("clean_replay_end");
}

static void test_replay_end_with_pending_gap() {
    auto dir = fs::temp_directory_path() / "nrp_test_pending_gap";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(2));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(2));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.replay_code == moex_raw::AbReplayCode::End);
        CHECK(collector.messages.empty());
    }
    CHECK(rr.code == NormalPipelineCode::ReplayEndedWithPendingGap);
    fs::remove_all(dir);
    TEST_PASS("replay_end_with_pending_gap");
}

static void test_tag35_sequence_reset_unsupported() {
    auto dir = fs::temp_directory_path() / "nrp_test_seq_reset";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="1" name="SeqReset">
    <string name="MsgType" id="35"><constant>4</constant></string>
    <uInt32 name="MsgSeqNum" id="34"/>
  </template>
</templates>)";
    auto compiled = compile_templates_from_string(xml).compiled;
    auto mp = make_meta_pair(compiled);
    std::vector<std::uint8_t> fast;
    fast.push_back(0xC0);
    fast.push_back(0x81);
    encode_stopbit_u32(fast, 1);
    auto dgram = make_datagram(fast, 1);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, dgram);
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, dgram);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.decode_code == OrderedDecodeCode::SequenceResetUnsupported);
        CHECK(collector.messages.empty());
    }
    CHECK(rr.code == NormalPipelineCode::DecodeFailed);
    fs::remove_all(dir);
    TEST_PASS("tag35_sequence_reset_unsupported");
}

static bool g_callback_threw = false;
static void throwing_callback(void*, OrderedDecodedMessage&&) {
    g_callback_threw = true;
    throw std::runtime_error("test throw");
}

static void test_callback_throw_internal_invariant() {
    auto dir = fs::temp_directory_path() / "nrp_test_cb_throw";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    g_callback_threw = false;
    NormalPipelineRunResult rr, rr2;
    {
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, throwing_callback, nullptr);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(g_callback_threw);
        // Stable Failed
        rr2 = pipeline.run_to_end();
    }
    CHECK(rr.code == NormalPipelineCode::InternalInvariantViolation);
    CHECK(rr2.code == NormalPipelineCode::InternalInvariantViolation);
    fs::remove_all(dir);
    TEST_PASS("callback_throw_internal_invariant");
}

class PhaseAwareIoErrorFileSystem : public IFileSystem {
    DefaultFileSystem real_;
    bool armed_ = false;
public:
    void arm() { armed_ = true; }
    bool exists(const std::string& path) override {
        if (armed_) return false;
        return real_.exists(path);
    }
    bool rename(const std::string& a, const std::string& b) override {
        return real_.rename(a, b);
    }
    bool remove(const std::string& path) override {
        return real_.remove(path);
    }
    std::uint64_t file_size(const std::string& path, bool& ok) override {
        if (armed_) { ok = false; return 0; }
        return real_.file_size(path, ok);
    }
    std::unique_ptr<IFileHandle> open_read(const std::string& path) override {
        if (armed_) return nullptr;
        return real_.open_read(path);
    }
    std::unique_ptr<IFileHandle> open_write(const std::string& path) override {
        return real_.open_write(path);
    }
};

class PhaseAwareStreamChangedFileSystem : public IFileSystem {
    DefaultFileSystem real_;
    std::unordered_map<std::string, std::uint64_t> original_sizes_;
    bool armed_ = false;
public:
    void arm() { armed_ = true; }
    bool exists(const std::string& path) override { return real_.exists(path); }
    bool rename(const std::string& a, const std::string& b) override { return real_.rename(a, b); }
    bool remove(const std::string& path) override { return real_.remove(path); }
    std::uint64_t file_size(const std::string& path, bool& ok) override {
        auto sz = real_.file_size(path, ok);
        if (!ok) return sz;
        if (!armed_) {
            original_sizes_[path] = sz;
            return sz;
        }
        auto it = original_sizes_.find(path);
        if (it != original_sizes_.end()) {
            return it->second + 1;
        }
        return sz;
    }
    std::unique_ptr<IFileHandle> open_read(const std::string& path) override {
        return real_.open_read(path);
    }
    std::unique_ptr<IFileHandle> open_write(const std::string& path) override {
        return real_.open_write(path);
    }
};

namespace moex_spectra_pipeline {
struct NormalReplayPipelineTestAccess {
    static NormalPipelineCode classify_replay_code(moex_raw::AbReplayCode code) {
        return NormalReplayPipeline::classify_replay_code(code);
    }
    static NormalPipelineCode classify_decode_result(OrderedDecodeCode code, bool has_message) {
        return NormalReplayPipeline::classify_decode_result(code, has_message);
    }
};
}  // namespace moex_spectra_pipeline

static void test_deterministic_io_error_runtime() {
    auto dir = fs::temp_directory_path() / "nrp_test_det_io_err";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);

    // Two segments for side A with sequential capture indices
    auto mp_a0 = mp.a; mp_a0.segment_index = 0;
    auto paths_a0 = write_segment(mp_a0, (dir / "a").string(), 0, 100, make_datagram(1));
    auto mp_a1 = mp.a; mp_a1.segment_index = 1; mp_a1.start_capture_index = 1;
    auto paths_a1 = write_segment(mp_a1, (dir / "a").string(), 1, 200, make_datagram(2));

    std::vector<std::string> all_a;
    all_a.insert(all_a.end(), paths_a0.begin(), paths_a0.end());
    all_a.insert(all_a.end(), paths_a1.begin(), paths_a1.end());

    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 150, make_datagram(1));

    PhaseAwareIoErrorFileSystem fs_a, fs_b;
    NormalPipelineRunResult rr1, rr2;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(all_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector,
                                      &fs_a, &fs_b);
        CHECK(ir.code == NormalPipelineCode::Ok);

        fs_a.arm();
        fs_b.arm();

        rr1 = pipeline.run_to_end();
        CHECK(rr1.code == NormalPipelineCode::ReplayFailed);
        CHECK(rr1.replay_code == moex_raw::AbReplayCode::IoError);

        rr2 = pipeline.run_to_end();
        CHECK(rr2.code == NormalPipelineCode::ReplayFailed);
        CHECK(rr2.replay_code == moex_raw::AbReplayCode::IoError);
        CHECK(rr2.input_packets == rr1.input_packets);
        CHECK(rr2.emitted_messages == rr1.emitted_messages);
    }
    fs::remove_all(dir);
    TEST_PASS("deterministic_io_error_runtime");
}

static void test_deterministic_stream_changed_runtime() {
    auto dir = fs::temp_directory_path() / "nrp_test_det_sc";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);

    // Two segments for side A with sequential capture indices
    auto mp_a0 = mp.a; mp_a0.segment_index = 0;
    auto paths_a0 = write_segment(mp_a0, (dir / "a").string(), 0, 100, make_datagram(1));
    auto mp_a1 = mp.a; mp_a1.segment_index = 1; mp_a1.start_capture_index = 1;
    auto paths_a1 = write_segment(mp_a1, (dir / "a").string(), 1, 200, make_datagram(2));

    std::vector<std::string> all_a;
    all_a.insert(all_a.end(), paths_a0.begin(), paths_a0.end());
    all_a.insert(all_a.end(), paths_a1.begin(), paths_a1.end());

    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 150, make_datagram(1));

    PhaseAwareStreamChangedFileSystem fs_a, fs_b;
    NormalPipelineRunResult rr1, rr2;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(all_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector,
                                      &fs_a, &fs_b);
        CHECK(ir.code == NormalPipelineCode::Ok);

        fs_a.arm();
        fs_b.arm();

        rr1 = pipeline.run_to_end();
        CHECK(rr1.code == NormalPipelineCode::ReplayFailed);
        CHECK(rr1.replay_code == moex_raw::AbReplayCode::StreamChanged);

        rr2 = pipeline.run_to_end();
        CHECK(rr2.code == NormalPipelineCode::ReplayFailed);
        CHECK(rr2.replay_code == moex_raw::AbReplayCode::StreamChanged);
        CHECK(rr2.input_packets == rr1.input_packets);
        CHECK(rr2.emitted_messages == rr1.emitted_messages);
    }
    fs::remove_all(dir);
    TEST_PASS("deterministic_stream_changed_runtime");
}

static void test_invalid_config_retry() {
    auto dir = fs::temp_directory_path() / "nrp_test_retry";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineConfig bad_cfg = make_default_config();
    bad_cfg.framing.max_datagram_bytes = 0;
    NormalPipelineConfig good_cfg = make_default_config();
    NormalPipelineInitResult ir1, ir2;
    NormalPipelineState state1, state2;
    {
        CallbackCollector collector;
        NormalReplayPipeline pipeline;
        ir1 = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                  compiled, bad_cfg, CallbackCollector::collect, &collector);
        state1 = pipeline.state();
        ir2 = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                  compiled, good_cfg, CallbackCollector::collect, &collector);
        state2 = pipeline.state();
    }
    CHECK(ir1.code == NormalPipelineCode::InvalidConfig);
    CHECK(state1 == NormalPipelineState::Uninitialized);
    CHECK(ir2.code == NormalPipelineCode::Ok);
    CHECK(state2 == NormalPipelineState::Ready);
    fs::remove_all(dir);
    TEST_PASS("invalid_config_retry");
}

static void test_already_initialized() {
    auto dir = fs::temp_directory_path() / "nrp_test_already_init";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineInitResult ir2;
    NormalPipelineState state;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        ir2 = pipeline.initialize({}, {}, compiled, cfg, CallbackCollector::collect, &collector);
        state = pipeline.state();
    }
    CHECK(ir2.code == NormalPipelineCode::AlreadyInitialized);
    CHECK(state == NormalPipelineState::Ready);
    fs::remove_all(dir);
    TEST_PASS("already_initialized");
}

static void test_already_initialized_after_end() {
    auto dir = fs::temp_directory_path() / "nrp_test_already_end";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineInitResult ir2;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        auto rr = pipeline.run_to_end();
        CHECK(rr.code == NormalPipelineCode::End);
        ir2 = pipeline.initialize({}, {}, compiled, cfg, CallbackCollector::collect, &collector);
    }
    CHECK(ir2.code == NormalPipelineCode::AlreadyInitialized);
    fs::remove_all(dir);
    TEST_PASS("already_initialized_after_end");
}

static void test_already_initialized_after_failed() {
    auto dir = fs::temp_directory_path() / "nrp_test_already_failed";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineInitResult ir2;
    NormalPipelineState state;
    {
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, throwing_callback, nullptr);
        CHECK(ir.code == NormalPipelineCode::Ok);
        auto rr = pipeline.run_to_end();
        CHECK(rr.code == NormalPipelineCode::InternalInvariantViolation);
        state = pipeline.state();
        ir2 = pipeline.initialize({}, {}, compiled, cfg, CallbackCollector::collect, nullptr);
    }
    CHECK(state == NormalPipelineState::Failed);
    CHECK(ir2.code == NormalPipelineCode::AlreadyInitialized);
    fs::remove_all(dir);
    TEST_PASS("already_initialized_after_failed");
}

static void test_stable_end() {
    auto dir = fs::temp_directory_path() / "nrp_test_stable_end";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr1, rr2;
    std::size_t msg_count = 0;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr1 = pipeline.run_to_end();
        CHECK(rr1.emitted_messages == 1u);
        msg_count = collector.messages.size();
        rr2 = pipeline.run_to_end();
        CHECK(rr2.emitted_messages == rr1.emitted_messages);
        CHECK(collector.messages.size() == msg_count);
    }
    CHECK(rr1.code == NormalPipelineCode::End);
    CHECK(rr2.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("stable_end");
}

static void test_stable_failed() {
    auto dir = fs::temp_directory_path() / "nrp_test_stable_failed";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr1, rr2;
    {
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, throwing_callback, nullptr);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr1 = pipeline.run_to_end();
        g_callback_threw = false;
        rr2 = pipeline.run_to_end();
        CHECK(!g_callback_threw);
        CHECK(rr2.input_packets == rr1.input_packets);
        CHECK(rr2.emitted_messages == rr1.emitted_messages);
    }
    CHECK(rr1.code == NormalPipelineCode::InternalInvariantViolation);
    CHECK(rr2.code == NormalPipelineCode::InternalInvariantViolation);
    fs::remove_all(dir);
    TEST_PASS("stable_failed");
}

static void test_not_initialized() {
    NormalReplayPipeline pipeline;
    auto rr = pipeline.run_to_end();
    CHECK(rr.code == NormalPipelineCode::NotInitialized);
    CHECK(rr.input_packets == 0u);
    CHECK(rr.emitted_messages == 0u);
    TEST_PASS("not_initialized");
}

static void test_null_callback_rejected() {
    NormalReplayPipeline pipeline;
    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto ir = pipeline.initialize({}, {}, compiled, make_default_config(), nullptr);
    CHECK(ir.code == NormalPipelineCode::InvalidConfig);
    CHECK(pipeline.state() == NormalPipelineState::Uninitialized);
    TEST_PASS("null_callback_rejected");
}

static void test_move_construction() {
    auto dir = fs::temp_directory_path() / "nrp_test_move_ctor";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        CHECK(pipeline.state() == NormalPipelineState::Ready);

        NormalReplayPipeline moved(std::move(pipeline));
        CHECK(moved.state() == NormalPipelineState::Ready);
        rr = moved.run_to_end();
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("move_construction");
}

static void test_move_assignment() {
    auto dir = fs::temp_directory_path() / "nrp_test_move_assign";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);

        NormalReplayPipeline other;
        other = std::move(pipeline);
        CHECK(other.state() == NormalPipelineState::Ready);
        rr = other.run_to_end();
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("move_assignment");
}

static void test_callback_transport_metadata() {
    auto dir = fs::temp_directory_path() / "nrp_test_transport";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.input_packets == 2u);
        CHECK(rr.emitted_messages == 1u);
        CHECK(collector.messages.size() == 1u);
        CHECK(collector.sides[0] == FeedSide::A);
        CHECK(collector.capture_indices[0] == 0u);
        CHECK(collector.capture_monotonic_nss[0] == 100u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    fs::remove_all(dir);
    TEST_PASS("callback_transport_metadata");
}

static void test_invalid_config_zero_max_message_bytes() {
    auto dir = fs::temp_directory_path() / "nrp_test_zero_msg_bytes";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineConfig cfg = make_default_config();
    cfg.sequencer.storage.max_message_bytes = 0;
    NormalPipelineInitResult ir;
    {
        CallbackCollector collector;
        NormalReplayPipeline pipeline;
        ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                 compiled, cfg, CallbackCollector::collect, &collector);
    }
    CHECK(ir.code == NormalPipelineCode::InvalidConfig);
    CHECK(ir.sequencer_code == SequencerCode::InvalidConfig);
    fs::remove_all(dir);
    TEST_PASS("invalid_config_zero_max_message_bytes");
}

static void test_early_end_one_side_success() {
    auto dir = fs::temp_directory_path() / "nrp_test_early_end_ok";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    // A: seq 1 only (early ts), B: seq 2 and 3 (later ts)
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 50, make_datagram(1));
    auto paths_b = write_segment_multi(mp.b, (dir / "b").string(),
        {{0, make_datagram(2)}, {0, make_datagram(3)}}, 0, 200);

    NormalPipelineRunResult rr;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        rr = pipeline.run_to_end();
        CHECK(rr.emitted_messages == 3u);
        CHECK(collector.messages.size() == 3u);
        CHECK(collector.messages[0].msg_seq_num == 1u);
        CHECK(collector.messages[1].msg_seq_num == 2u);
        CHECK(collector.messages[2].msg_seq_num == 3u);
    }
    CHECK(rr.code == NormalPipelineCode::End);
    CHECK(rr.replay_code == moex_raw::AbReplayCode::End);
    fs::remove_all(dir);
    TEST_PASS("early_end_one_side_success");
}

static void test_sticky_already_initialized_null_callback() {
    auto dir = fs::temp_directory_path() / "nrp_test_sticky_null";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    // Ready state + null callback → AlreadyInitialized
    NormalPipelineInitResult ir_ready;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        ir_ready = pipeline.initialize({}, {}, compiled, cfg, nullptr);
    }
    CHECK(ir_ready.code == NormalPipelineCode::AlreadyInitialized);

    // End state + null callback → AlreadyInitialized
    NormalPipelineInitResult ir_end;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        auto rr = pipeline.run_to_end();
        CHECK(rr.code == NormalPipelineCode::End);
        ir_end = pipeline.initialize({}, {}, compiled, cfg, nullptr);
    }
    CHECK(ir_end.code == NormalPipelineCode::AlreadyInitialized);

    // Failed state + null callback → AlreadyInitialized
    NormalPipelineInitResult ir_failed;
    {
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, throwing_callback, nullptr);
        CHECK(ir.code == NormalPipelineCode::Ok);
        auto rr = pipeline.run_to_end();
        CHECK(rr.code == NormalPipelineCode::InternalInvariantViolation);
        ir_failed = pipeline.initialize({}, {}, compiled, cfg, nullptr);
    }
    CHECK(ir_failed.code == NormalPipelineCode::AlreadyInitialized);
    fs::remove_all(dir);
    TEST_PASS("sticky_already_initialized_null_callback");
}

static void test_sticky_already_initialized_invalid_args() {
    auto dir = fs::temp_directory_path() / "nrp_test_sticky_inv";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    NormalPipelineConfig bad_cfg = make_default_config();
    bad_cfg.framing.max_datagram_bytes = 0;

    // Ready state + invalid config → AlreadyInitialized
    NormalPipelineInitResult ir_ready;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        ir_ready = pipeline.initialize({}, {}, compiled, bad_cfg, CallbackCollector::collect, nullptr);
    }
    CHECK(ir_ready.code == NormalPipelineCode::AlreadyInitialized);

    // End state + invalid config → AlreadyInitialized
    NormalPipelineInitResult ir_end;
    {
        CallbackCollector collector;
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, CallbackCollector::collect, &collector);
        CHECK(ir.code == NormalPipelineCode::Ok);
        auto rr = pipeline.run_to_end();
        CHECK(rr.code == NormalPipelineCode::End);
        ir_end = pipeline.initialize({}, {}, compiled, bad_cfg, CallbackCollector::collect, nullptr);
    }
    CHECK(ir_end.code == NormalPipelineCode::AlreadyInitialized);

    // Failed state + invalid config → AlreadyInitialized
    NormalPipelineInitResult ir_failed;
    {
        NormalPipelineConfig cfg = make_default_config();
        NormalReplayPipeline pipeline;
        auto ir = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                      compiled, cfg, throwing_callback, nullptr);
        CHECK(ir.code == NormalPipelineCode::Ok);
        auto rr = pipeline.run_to_end();
        CHECK(rr.code == NormalPipelineCode::InternalInvariantViolation);
        ir_failed = pipeline.initialize({}, {}, compiled, bad_cfg, CallbackCollector::collect, nullptr);
    }
    CHECK(ir_failed.code == NormalPipelineCode::AlreadyInitialized);
    fs::remove_all(dir);
    TEST_PASS("sticky_already_initialized_invalid_args");
}

static void test_overflow_initialization_retry() {
    auto dir = fs::temp_directory_path() / "nrp_test_overflow";
    fs::remove_all(dir);
    fs::create_directories(dir / "a");
    fs::create_directories(dir / "b");

    auto compiled = compile_templates_from_string(kBasicTemplateXml).compiled;
    auto mp = make_meta_pair(compiled);
    auto paths_a = write_segment(mp.a, (dir / "a").string(), 0, 100, make_datagram(1));
    auto paths_b = write_segment(mp.b, (dir / "b").string(), 0, 200, make_datagram(1));

    // Arithmetic overflow guard: slot_count=2, max_message_bytes=SIZE_MAX
    // → 2 > SIZE_MAX / SIZE_MAX → 2 > 1 → true (hits guard before allocation)
    NormalPipelineConfig overflow_cfg = make_default_config();
    overflow_cfg.sequencer.storage.max_reorder_messages = 2;
    overflow_cfg.sequencer.storage.max_message_bytes = (std::numeric_limits<std::size_t>::max)();

    NormalPipelineConfig good_cfg = make_default_config();

    NormalPipelineInitResult ir1, ir2;
    NormalPipelineState state1;
    {
        CallbackCollector collector;
        NormalReplayPipeline pipeline;
        ir1 = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                  compiled, overflow_cfg, CallbackCollector::collect, &collector);
        state1 = pipeline.state();
        CHECK(ir1.code == NormalPipelineCode::InternalInvariantViolation);
        CHECK(state1 == NormalPipelineState::Uninitialized);

        // Retry with good config
        ir2 = pipeline.initialize(make_stream_set(paths_a), make_stream_set(paths_b),
                                  compiled, good_cfg, CallbackCollector::collect, &collector);
        CHECK(ir2.code == NormalPipelineCode::Ok);
        CHECK(pipeline.state() == NormalPipelineState::Ready);
    }
    fs::remove_all(dir);
    TEST_PASS("overflow_initialization_retry");
}

static void test_replay_code_classification() {
    using namespace moex_raw;
    using TA = NormalReplayPipelineTestAccess;

    // Runtime replay codes → ReplayFailed
    CHECK(TA::classify_replay_code(AbReplayCode::ValidationFailed) == NormalPipelineCode::ReplayFailed);
    CHECK(TA::classify_replay_code(AbReplayCode::IoError) == NormalPipelineCode::ReplayFailed);
    CHECK(TA::classify_replay_code(AbReplayCode::StreamChanged) == NormalPipelineCode::ReplayFailed);
    CHECK(TA::classify_replay_code(AbReplayCode::ClockRegression) == NormalPipelineCode::ReplayFailed);
    CHECK(TA::classify_replay_code(AbReplayCode::InternalInvariantViolation) == NormalPipelineCode::ReplayFailed);

    // End is handled separately (not classified here)
    // Ok / NotInitialized / AlreadyInitialized are not runtime replay errors
    CHECK(TA::classify_replay_code(AbReplayCode::Ok) == NormalPipelineCode::InternalInvariantViolation);
    CHECK(TA::classify_replay_code(AbReplayCode::NotInitialized) == NormalPipelineCode::InternalInvariantViolation);
    CHECK(TA::classify_replay_code(AbReplayCode::AlreadyInitialized) == NormalPipelineCode::InternalInvariantViolation);

    TEST_PASS("replay_code_classification");
}

static void test_ok_without_message_classification() {
    using TA = NormalReplayPipelineTestAccess;

    // Ok + message → Ok
    CHECK(TA::classify_decode_result(OrderedDecodeCode::Ok, true) == NormalPipelineCode::Ok);

    // Ok without message → InternalInvariantViolation
    CHECK(TA::classify_decode_result(OrderedDecodeCode::Ok, false) == NormalPipelineCode::InternalInvariantViolation);

    // Non-Ok codes → DecodeFailed (table-driven)
    const OrderedDecodeCode non_ok_codes[] = {
        OrderedDecodeCode::NotInitialized,
        OrderedDecodeCode::AlreadyInitialized,
        OrderedDecodeCode::InvalidConfig,
        OrderedDecodeCode::TemplateHashMismatch,
        OrderedDecodeCode::DecodeFailed,
        OrderedDecodeCode::MissingTag34,
        OrderedDecodeCode::DuplicateTag34,
        OrderedDecodeCode::InvalidTag34,
        OrderedDecodeCode::Tag34OutOfRange,
        OrderedDecodeCode::MissingTag35,
        OrderedDecodeCode::DuplicateTag35,
        OrderedDecodeCode::InvalidTag35,
        OrderedDecodeCode::SequenceResetUnsupported,
        OrderedDecodeCode::ExternalInternalSequenceMismatch,
        OrderedDecodeCode::InternalInvariantViolation,
    };
    for (auto code : non_ok_codes) {
        CHECK(TA::classify_decode_result(code, false) == NormalPipelineCode::DecodeFailed);
        CHECK(TA::classify_decode_result(code, true) == NormalPipelineCode::DecodeFailed);
    }

    TEST_PASS("ok_without_message_classification");
}

// ===========================================================================
// main
// ===========================================================================

int main() {
    test_not_initialized();
    test_null_callback_rejected();
    test_same_packet_ab_decoded_once();
    test_a_before_b();
    test_b_before_a();
    test_out_of_order_flush();
    test_multi_buffered_flush();
    test_one_side_ends_early();
    test_duplicate_equal_dropped();
    test_duplicate_payload_mismatch_fails();
    test_datagram_too_short_0_3_bytes();
    test_datagram_exactly_4_bytes();
    test_datagram_over_limit();
    test_malformed_fast();
    test_trailing_bytes();
    test_external_tag34_mismatch();
    test_template_id_reuse_after_ordering();
    test_sequence_uint32_max_then_zero();
    test_clean_replay_end();
    test_replay_end_with_pending_gap();
    test_tag35_sequence_reset_unsupported();
    test_callback_throw_internal_invariant();
    test_deterministic_io_error_runtime();
    test_deterministic_stream_changed_runtime();
    test_invalid_config_retry();
    test_already_initialized();
    test_already_initialized_after_end();
    test_already_initialized_after_failed();
    test_stable_end();
    test_stable_failed();
    test_move_construction();
    test_move_assignment();
    test_callback_transport_metadata();
    test_invalid_config_zero_max_message_bytes();
    test_early_end_one_side_success();
    test_sticky_already_initialized_null_callback();
    test_sticky_already_initialized_invalid_args();
    test_overflow_initialization_retry();
    test_replay_code_classification();
    test_ok_without_message_classification();

    std::cout << "ALL TESTS PASSED (40 tests)\n";
    return 0;
}
