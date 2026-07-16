// Benchmark harness for RT-4 SPECTRA transport / Gate A
// Phase 3A: latency distribution, throughput, high-water marks
//
// Portable: Windows/MSVC, Linux/GCC. Release execution.
// No heap allocation inside measured loops.
// No files, network, external captures, or random_device.

#include "moex_fast/spectra_gate_a.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {

using namespace moex::spectra;

// --- Configuration ---

constexpr std::uint32_t FEED_ID = 42;
constexpr std::size_t MAX_REORDER = 8;
constexpr std::size_t MAX_MSG_BYTES = 64;
constexpr std::uint64_t WAIT_NS = 10000;
constexpr std::size_t ARENA_SIZE = MAX_REORDER * MAX_MSG_BYTES;
constexpr std::size_t SAMPLE_COUNT = 21;   // odd, >= 21
constexpr std::size_t WARMUP_COUNT = 100;

// --- Global fixed storage (no heap in measured loops) ---

alignas(64) std::array<SlotMetadata, MAX_REORDER> g_slots;
alignas(64) std::array<std::uint8_t, ARENA_SIZE> g_arena;
alignas(64) std::array<std::uint8_t, MAX_MSG_BYTES> g_body_a;
alignas(64) std::array<std::uint8_t, MAX_MSG_BYTES> g_body_b;
std::array<std::int64_t, SAMPLE_COUNT> g_samples;

// --- Accumulators used in output prevent dead-code elimination ---

std::uint64_t g_sink_count;
std::uint64_t g_sink_bytes;
int g_result_xor;

// --- Noexcept sink, does not retain body spans ---

struct BenchSink {
    void operator()(const OrderedMessageMetadata&,
                    std::span<const std::uint8_t> body) noexcept {
        ++g_sink_count;
        g_sink_bytes += body.size();
        if (!body.empty())
            g_result_xor ^= body[0];
    }
};

// --- High-water marks ---

struct HighWaterMarks {
    std::size_t max_pending_messages{};
    std::size_t max_pending_bytes{};
};

void update_hwm(HighWaterMarks& hwm, const MessageStorage& s) noexcept {
    if (s.pending_count() > hwm.max_pending_messages)
        hwm.max_pending_messages = s.pending_count();
    if (s.pending_bytes() > hwm.max_pending_bytes)
        hwm.max_pending_bytes = s.pending_bytes();
}

// --- FramedMessageView helper ---

FramedMessageView make_view(std::uint32_t seq, FeedSide side,
                             std::span<const std::uint8_t> body,
                             std::uint64_t event_time) noexcept {
    FramedMessageView v{};
    v.feed.value = FEED_ID;
    v.side = side;
    v.msg_seq_num = seq;
    v.capture_index = seq;
    v.capture_monotonic_ns = event_time;
    v.fast_body = body;
    return v;
}

// --- Scenario result ---

struct ScenarioResult {
    const char* name{};
    std::uint64_t operations{};
    std::uint64_t samples{};
    std::int64_t total_elapsed_ns{};
    double ns_per_op{};
    double throughput_ops_per_sec{};
    std::int64_t p50_ns{};
    std::int64_t p95_ns{};
    std::int64_t p99_ns{};
    std::size_t max_pending_messages{};
    std::size_t max_pending_bytes{};
    std::uint64_t checksum{};
};

void compute_percentiles(ScenarioResult& r) {
    std::sort(g_samples.begin(), g_samples.end());
    r.p50_ns = g_samples[SAMPLE_COUNT / 2];
    r.p95_ns = g_samples[SAMPLE_COUNT * 95 / 100];
    r.p99_ns = g_samples[SAMPLE_COUNT - 1];
}

ScenarioResult finalize(const char* name, std::uint64_t ops,
                         std::int64_t total_ns, const HighWaterMarks& hwm) {
    ScenarioResult r{};
    r.name = name;
    r.operations = ops;
    r.samples = SAMPLE_COUNT;
    r.total_elapsed_ns = total_ns;
    r.ns_per_op = static_cast<double>(total_ns) / static_cast<double>(ops);
    r.throughput_ops_per_sec =
        static_cast<double>(ops) * 1e9 / static_cast<double>(total_ns);
    compute_percentiles(r);
    r.max_pending_messages = hwm.max_pending_messages;
    r.max_pending_bytes = hwm.max_pending_bytes;
    r.checksum = g_sink_count ^ g_sink_bytes ^
                 static_cast<std::uint64_t>(g_result_xor);
    return r;
}

// --- Sequencer setup helper ---

struct BenchContext {
    MessageStorage storage;
    DualFeedSequencer seq;
    BenchSink sink;
    HighWaterMarks hwm;
    std::uint64_t ops{};
    std::uint64_t event_time{100};
};

void init_context(BenchContext& ctx, std::uint32_t start_seq = 0) {
    ctx.storage.initialize(g_slots, g_arena,
                           {MAX_REORDER, ARENA_SIZE, MAX_MSG_BYTES});
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = MAX_REORDER;
    cfg.reorder_wait_ns = WAIT_NS;
    cfg.storage = {MAX_REORDER, ARENA_SIZE, MAX_MSG_BYTES};
    (void)ctx.seq.initialize(LogicalFeedId{FEED_ID}, cfg, ctx.storage);
    (void)ctx.seq.start(start_seq);
}

inline void record_result(SequencerResult r) noexcept {
    g_result_xor ^= static_cast<int>(r.code);
}

inline void advance_time(BenchContext& ctx) noexcept {
    ctx.event_time += 100;
}

// =====================================================================
// Scenario 1: ordered first-copy emission
// =====================================================================
ScenarioResult scenario_ordered_emission() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto v = make_view(static_cast<std::uint32_t>(i), FeedSide::A,
                           g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(
            static_cast<std::uint32_t>(WARMUP_COUNT + i), FeedSide::A,
            g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(v, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ++ctx.ops;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("ordered_first_copy_emission", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 2: expected A message followed by stale B duplicate
// =====================================================================
ScenarioResult scenario_expected_a_then_stale_b() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto v = make_view(static_cast<std::uint32_t>(i), FeedSide::A,
                           g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        advance_time(ctx);
        auto v2 = make_view(static_cast<std::uint32_t>(i), FeedSide::B,
                            g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(v2, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto seq = static_cast<std::uint32_t>(WARMUP_COUNT + i);
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(seq, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(v, ctx.event_time, ctx.sink));
        advance_time(ctx);
        auto v2 = make_view(seq, FeedSide::B, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(v2, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 2;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("expected_a_then_stale_b", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 3: alternating A/B winning side
// =====================================================================
ScenarioResult scenario_alternating_ab() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto side = (i % 2 == 0) ? FeedSide::A : FeedSide::B;
        auto v = make_view(static_cast<std::uint32_t>(i), side,
                           g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto side = (i % 2 == 0) ? FeedSide::A : FeedSide::B;
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(
            static_cast<std::uint32_t>(WARMUP_COUNT + i), side,
            g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(v, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ++ctx.ops;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("alternating_ab_winning_side", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 4: bounded reorder and contiguous flush at depth 1
// =====================================================================
ScenarioResult scenario_reorder_flush_depth_1() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer future, then send expected (flushes both)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 2);
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 2);
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base = measured_base + static_cast<std::uint32_t>(i * 2);
        auto s = std::chrono::steady_clock::now();
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(vf, ctx.event_time, ctx.sink));
        update_hwm(ctx.hwm, ctx.storage);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(ve, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 2;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("reorder_flush_depth_1", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 5: bounded reorder and contiguous flush at depth 4
// =====================================================================
ScenarioResult scenario_reorder_flush_depth_4() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer 4 future, then send expected (flushes all 5)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 5);
        for (std::uint32_t j = 1; j <= 4; ++j) {
            auto vf = make_view(base + j, FeedSide::A, g_body_a,
                                ctx.event_time);
            (void)ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
            advance_time(ctx);
        }
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 5);
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base =
            measured_base + static_cast<std::uint32_t>(i * 5);
        auto s = std::chrono::steady_clock::now();
        for (std::uint32_t j = 1; j <= 4; ++j) {
            auto vf = make_view(base + j, FeedSide::A, g_body_a,
                                ctx.event_time);
            record_result(
                ctx.seq.on_message(vf, ctx.event_time, ctx.sink));
            update_hwm(ctx.hwm, ctx.storage);
            advance_time(ctx);
        }
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(ve, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 5;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("reorder_flush_depth_4", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 6: equal pending duplicate drop
// =====================================================================
ScenarioResult scenario_equal_pending_duplicate() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer future A, duplicate B (equal), send expected (flushes)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 3);
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        advance_time(ctx);
        auto vd = make_view(base + 1, FeedSide::B, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(vd, ctx.event_time, ctx.sink);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 3);
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base =
            measured_base + static_cast<std::uint32_t>(i * 3);
        auto s = std::chrono::steady_clock::now();
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(vf, ctx.event_time, ctx.sink));
        update_hwm(ctx.hwm, ctx.storage);
        advance_time(ctx);
        auto vd = make_view(base + 1, FeedSide::B, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(vd, ctx.event_time, ctx.sink));
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(ve, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 3;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("equal_pending_duplicate_drop", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 7: GapWait on_time before deadline
// =====================================================================
ScenarioResult scenario_gapwait_ontime() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer future, on_time before deadline, send expected (flushes)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 2);
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        advance_time(ctx);
        (void)ctx.seq.on_time(ctx.event_time, ctx.sink);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 2);
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base = measured_base + static_cast<std::uint32_t>(i * 2);
        auto s = std::chrono::steady_clock::now();
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(vf, ctx.event_time, ctx.sink));
        update_hwm(ctx.hwm, ctx.storage);
        advance_time(ctx);
        record_result(ctx.seq.on_time(ctx.event_time, ctx.sink));
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(ve, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 3;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("gapwait_ontime_before_deadline", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// =====================================================================
// Scenario 8: terminal FailedClosed steady-state calls
// =====================================================================
ScenarioResult scenario_failedclosed_steady() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Trigger FailedClosed (wrong logical feed)
    {
        auto bad = make_view(0, FeedSide::A, g_body_a, ctx.event_time);
        bad.feed.value = 999;
        auto r = ctx.seq.on_message(bad, ctx.event_time, ctx.sink);
        g_result_xor ^= static_cast<int>(r.code);
        advance_time(ctx);
    }

    // Warm-up
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto v = make_view(static_cast<std::uint32_t>(1000 + i), FeedSide::A,
                           g_body_a, ctx.event_time);
        (void)ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        advance_time(ctx);
    }

    // Measured
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(
            static_cast<std::uint32_t>(2000 + i), FeedSide::A,
            g_body_a, ctx.event_time);
        record_result(ctx.seq.on_message(v, ctx.event_time, ctx.sink));
        auto e = std::chrono::steady_clock::now();
        g_samples[i] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        ++ctx.ops;
        advance_time(ctx);
    }
    auto t1 = std::chrono::steady_clock::now();

    return finalize("failedclosed_steady_state", ctx.ops,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        t1 - t0)
                        .count(),
                    ctx.hwm);
}

// --- Output ---

void print_result(const ScenarioResult& r) {
    std::printf(
        "%s %llu %llu %lld %.2f %.2f %lld %lld %lld %llu %llu %llu\n",
        r.name, static_cast<unsigned long long>(r.operations),
        static_cast<unsigned long long>(r.samples),
        static_cast<long long>(r.total_elapsed_ns), r.ns_per_op,
        r.throughput_ops_per_sec, static_cast<long long>(r.p50_ns),
        static_cast<long long>(r.p95_ns), static_cast<long long>(r.p99_ns),
        static_cast<unsigned long long>(r.max_pending_messages),
        static_cast<unsigned long long>(r.max_pending_bytes),
        static_cast<unsigned long long>(r.checksum));
}

} // namespace

int main() {
    // Deterministic body fill
    for (std::size_t i = 0; i < MAX_MSG_BYTES; ++i) {
        g_body_a[i] = static_cast<std::uint8_t>(0xAA ^ (i & 0xFF));
        g_body_b[i] = static_cast<std::uint8_t>(0xBB ^ (i & 0xFF));
    }

    int exit_code = 0;
    std::uint64_t total_ops = 0;

    auto run_scenario = [&](ScenarioResult (*fn)(), const char* label) {
        auto r = fn();
        print_result(r);
        total_ops += r.operations;
        if (r.operations == 0) {
            std::fprintf(stderr, "FAIL: %s produced zero operations\n", label);
            exit_code = 1;
        }
    };

    run_scenario(scenario_ordered_emission, "ordered_first_copy_emission");
    run_scenario(scenario_expected_a_then_stale_b, "expected_a_then_stale_b");
    run_scenario(scenario_alternating_ab, "alternating_ab_winning_side");
    run_scenario(scenario_reorder_flush_depth_1, "reorder_flush_depth_1");
    run_scenario(scenario_reorder_flush_depth_4, "reorder_flush_depth_4");
    run_scenario(scenario_equal_pending_duplicate, "equal_pending_duplicate_drop");
    run_scenario(scenario_gapwait_ontime, "gapwait_ontime_before_deadline");
    run_scenario(scenario_failedclosed_steady, "failedclosed_steady_state");

    if (total_ops == 0) {
        std::fprintf(stderr, "FAIL: zero total operations across all scenarios\n");
        exit_code = 1;
    }

    return exit_code;
}
