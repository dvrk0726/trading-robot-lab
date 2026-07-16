// Benchmark harness for RT-4 SPECTRA transport / Gate A
// Phase 3B1: zero-allocation evidence, correct timing
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
#include <new>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

// --- Allocation tracking state ---
// Single-threaded; no lock or atomic required.

static bool g_tracking_enabled = false;
static std::size_t g_allocation_count = 0;

// --- Portable aligned allocation helpers ---

#ifdef _WIN32
static void* platform_aligned_alloc(std::size_t alignment,
                                     std::size_t size) noexcept {
    return _aligned_malloc(size, alignment);
}
static void platform_aligned_free(void* ptr) noexcept {
    _aligned_free(ptr);
}
#else
static void* platform_aligned_alloc(std::size_t alignment,
                                     std::size_t size) noexcept {
    std::size_t rounded = (size + alignment - 1) & ~(alignment - 1);
    return std::aligned_alloc(alignment, rounded);
}
static void platform_aligned_free(void* ptr) noexcept {
    std::free(ptr);
}
#endif

// --- Global operator new/delete replacements ---
// Intercept all standard forms: scalar, array, nothrow, sized, aligned.

void* operator new(std::size_t size) {
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    if (g_tracking_enabled) ++g_allocation_count;
    return p;
}

void* operator new[](std::size_t size) {
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    if (g_tracking_enabled) ++g_allocation_count;
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    void* p = std::malloc(size);
    if (g_tracking_enabled && p) ++g_allocation_count;
    return p;
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    void* p = std::malloc(size);
    if (g_tracking_enabled && p) ++g_allocation_count;
    return p;
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    void* p = platform_aligned_alloc(
        static_cast<std::size_t>(alignment), size);
    if (!p) throw std::bad_alloc();
    if (g_tracking_enabled) ++g_allocation_count;
    return p;
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    void* p = platform_aligned_alloc(
        static_cast<std::size_t>(alignment), size);
    if (!p) throw std::bad_alloc();
    if (g_tracking_enabled) ++g_allocation_count;
    return p;
}

void* operator new(std::size_t size, std::align_val_t alignment,
                    const std::nothrow_t&) noexcept {
    void* p = platform_aligned_alloc(
        static_cast<std::size_t>(alignment), size);
    if (g_tracking_enabled && p) ++g_allocation_count;
    return p;
}

void* operator new[](std::size_t size, std::align_val_t alignment,
                      const std::nothrow_t&) noexcept {
    void* p = platform_aligned_alloc(
        static_cast<std::size_t>(alignment), size);
    if (g_tracking_enabled && p) ++g_allocation_count;
    return p;
}

void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    std::free(ptr);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

void operator delete(void* ptr, std::align_val_t) noexcept {
    platform_aligned_free(ptr);
}
void operator delete[](void* ptr, std::align_val_t) noexcept {
    platform_aligned_free(ptr);
}

void operator delete(void* ptr, std::size_t,
                      std::align_val_t) noexcept {
    platform_aligned_free(ptr);
}
void operator delete[](void* ptr, std::size_t,
                        std::align_val_t) noexcept {
    platform_aligned_free(ptr);
}

void operator delete(void* ptr, std::align_val_t,
                      const std::nothrow_t&) noexcept {
    platform_aligned_free(ptr);
}
void operator delete[](void* ptr, std::align_val_t,
                        const std::nothrow_t&) noexcept {
    platform_aligned_free(ptr);
}

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

// --- Allocation tracking helpers ---

void enable_tracking() noexcept {
    g_tracking_enabled = true;
    g_allocation_count = 0;
}

void disable_tracking() noexcept {
    g_tracking_enabled = false;
}

std::size_t current_alloc_count() noexcept {
    return g_allocation_count;
}

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
    std::size_t allocation_count{};
    bool functional_ok{true};
};

void compute_percentiles(ScenarioResult& r) {
    std::sort(g_samples.begin(), g_samples.end());
    r.p50_ns = g_samples[SAMPLE_COUNT / 2];
    r.p95_ns = g_samples[SAMPLE_COUNT * 95 / 100];
    r.p99_ns = g_samples[SAMPLE_COUNT - 1];
}

ScenarioResult finalize(const char* name, std::uint64_t ops,
                         std::int64_t measured_total_ns,
                         std::size_t alloc_count,
                         const HighWaterMarks& hwm) {
    ScenarioResult r{};
    r.name = name;
    r.operations = ops;
    r.samples = SAMPLE_COUNT;
    r.total_elapsed_ns = measured_total_ns;
    r.allocation_count = alloc_count;
    if (ops == 0 || measured_total_ns <= 0) {
        r.ns_per_op = 0;
        r.throughput_ops_per_sec = 0;
        r.functional_ok = false;
    } else {
        r.ns_per_op = static_cast<double>(measured_total_ns) /
                       static_cast<double>(ops);
        r.throughput_ops_per_sec =
            static_cast<double>(ops) * 1e9 /
            static_cast<double>(measured_total_ns);
    }
    compute_percentiles(r);
    r.max_pending_messages = hwm.max_pending_messages;
    r.max_pending_bytes = hwm.max_pending_bytes;
    r.checksum = g_sink_count ^ g_sink_bytes ^
                 static_cast<std::uint64_t>(g_result_xor);
    if (alloc_count != 0) r.functional_ok = false;
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
    bool functional_ok{true};
};

void init_context(BenchContext& ctx, std::uint32_t start_seq = 0) {
    ctx.storage.initialize(g_slots, g_arena,
                           {MAX_REORDER, ARENA_SIZE, MAX_MSG_BYTES});
    if (!ctx.storage.initialized()) {
        ctx.functional_ok = false;
        return;
    }
    SequencerConfig cfg{};
    cfg.logical_feed.value = FEED_ID;
    cfg.max_reorder_distance = MAX_REORDER;
    cfg.reorder_wait_ns = WAIT_NS;
    cfg.storage = {MAX_REORDER, ARENA_SIZE, MAX_MSG_BYTES};
    auto init_code = ctx.seq.initialize(LogicalFeedId{FEED_ID}, cfg, ctx.storage);
    if (init_code != SequencerCode::Initialized) {
        ctx.functional_ok = false;
        return;
    }
    auto start_code = ctx.seq.start(start_seq);
    if (start_code != SequencerCode::Started) {
        ctx.functional_ok = false;
        return;
    }
    if (ctx.seq.state() != SequencerState::Running) {
        ctx.functional_ok = false;
    }
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

    // Warm-up: expect Emitted for each
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto v = make_view(static_cast<std::uint32_t>(i), FeedSide::A,
                           g_body_a, ctx.event_time);
        auto r = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        if (r.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
    }

    // Measured: 1 op per iteration
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(
            static_cast<std::uint32_t>(WARMUP_COUNT + i), FeedSide::A,
            g_body_a, ctx.event_time);
        auto r = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        record_result(r);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 1;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ++ctx.ops;
        advance_time(ctx);
        if (r.code != SequencerCode::Emitted) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("ordered_first_copy_emission", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 2: expected A message followed by stale B duplicate
// =====================================================================
ScenarioResult scenario_expected_a_then_stale_b() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: A emits, B is duplicate
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto v = make_view(static_cast<std::uint32_t>(i), FeedSide::A,
                           g_body_a, ctx.event_time);
        auto ra = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        if (ra.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
        auto v2 = make_view(static_cast<std::uint32_t>(i), FeedSide::B,
                            g_body_a, ctx.event_time);
        auto rb = ctx.seq.on_message(v2, ctx.event_time, ctx.sink);
        if (rb.code != SequencerCode::DuplicateDropped) ctx.functional_ok = false;
        advance_time(ctx);
    }

    // Measured: 2 ops per iteration
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto seq = static_cast<std::uint32_t>(WARMUP_COUNT + i);
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(seq, FeedSide::A, g_body_a, ctx.event_time);
        auto ra = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        record_result(ra);
        advance_time(ctx);
        auto v2 = make_view(seq, FeedSide::B, g_body_a, ctx.event_time);
        auto rb = ctx.seq.on_message(v2, ctx.event_time, ctx.sink);
        record_result(rb);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 2;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 2;
        advance_time(ctx);
        if (ra.code != SequencerCode::Emitted) ctx.functional_ok = false;
        if (rb.code != SequencerCode::DuplicateDropped) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("expected_a_then_stale_b", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 3: alternating A/B winning side
// =====================================================================
ScenarioResult scenario_alternating_ab() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: expect Emitted for each
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto side = (i % 2 == 0) ? FeedSide::A : FeedSide::B;
        auto v = make_view(static_cast<std::uint32_t>(i), side,
                           g_body_a, ctx.event_time);
        auto r = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        if (r.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
    }

    // Measured: 1 op per iteration
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto side = (i % 2 == 0) ? FeedSide::A : FeedSide::B;
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(
            static_cast<std::uint32_t>(WARMUP_COUNT + i), side,
            g_body_a, ctx.event_time);
        auto r = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        record_result(r);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 1;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ++ctx.ops;
        advance_time(ctx);
        if (r.code != SequencerCode::Emitted) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("alternating_ab_winning_side", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 4: bounded reorder and contiguous flush at depth 1
// =====================================================================
ScenarioResult scenario_reorder_flush_depth_1() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer future (BufferedOutOfOrder), then send expected (Emitted)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 2);
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }

    // Measured: 2 ops per iteration
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 2);
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base = measured_base + static_cast<std::uint32_t>(i * 2);
        auto s = std::chrono::steady_clock::now();
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        record_result(rf);
        update_hwm(ctx.hwm, ctx.storage);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        record_result(re);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 2;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 2;
        advance_time(ctx);
        if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("reorder_flush_depth_1", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 5: bounded reorder and contiguous flush at depth 4
// =====================================================================
ScenarioResult scenario_reorder_flush_depth_4() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer 4 future (BufferedOutOfOrder), then send expected (Emitted)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 5);
        for (std::uint32_t j = 1; j <= 4; ++j) {
            auto vf = make_view(base + j, FeedSide::A, g_body_a,
                                ctx.event_time);
            auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
            if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
            advance_time(ctx);
        }
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }

    // Measured: 5 ops per iteration
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 5);
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base =
            measured_base + static_cast<std::uint32_t>(i * 5);
        auto s = std::chrono::steady_clock::now();
        for (std::uint32_t j = 1; j <= 4; ++j) {
            auto vf = make_view(base + j, FeedSide::A, g_body_a,
                                ctx.event_time);
            auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
            record_result(rf);
            update_hwm(ctx.hwm, ctx.storage);
            advance_time(ctx);
            if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        }
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        record_result(re);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 5;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 5;
        advance_time(ctx);
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("reorder_flush_depth_4", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 6: equal pending duplicate drop
// =====================================================================
ScenarioResult scenario_equal_pending_duplicate() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer future A (BufferedOutOfOrder), duplicate B (DuplicateDropped),
    //          send expected (Emitted). base = i * 2.
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 2);
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        advance_time(ctx);
        auto vd = make_view(base + 1, FeedSide::B, g_body_a, ctx.event_time);
        auto rd = ctx.seq.on_message(vd, ctx.event_time, ctx.sink);
        if (rd.code != SequencerCode::DuplicateDropped) ctx.functional_ok = false;
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }

    // Measured: 3 ops per iteration. base = i * 2.
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 2);
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base =
            measured_base + static_cast<std::uint32_t>(i * 2);
        auto s = std::chrono::steady_clock::now();
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        record_result(rf);
        update_hwm(ctx.hwm, ctx.storage);
        advance_time(ctx);
        auto vd = make_view(base + 1, FeedSide::B, g_body_a, ctx.event_time);
        auto rd = ctx.seq.on_message(vd, ctx.event_time, ctx.sink);
        record_result(rd);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        record_result(re);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 3;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 3;
        advance_time(ctx);
        if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        if (rd.code != SequencerCode::DuplicateDropped) ctx.functional_ok = false;
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("equal_pending_duplicate_drop", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 7: GapWait on_time before deadline
// =====================================================================
ScenarioResult scenario_gapwait_ontime() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);

    // Warm-up: buffer future (BufferedOutOfOrder), on_time (GapWaiting),
    //          send expected (Emitted)
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        std::uint32_t base = static_cast<std::uint32_t>(i * 2);
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        advance_time(ctx);
        auto rt = ctx.seq.on_time(ctx.event_time, ctx.sink);
        if (rt.code != SequencerCode::GapWaiting) ctx.functional_ok = false;
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        advance_time(ctx);
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }

    // Measured: 3 ops per iteration
    std::uint32_t measured_base = static_cast<std::uint32_t>(WARMUP_COUNT * 2);
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        std::uint32_t base = measured_base + static_cast<std::uint32_t>(i * 2);
        auto s = std::chrono::steady_clock::now();
        auto vf = make_view(base + 1, FeedSide::A, g_body_a, ctx.event_time);
        auto rf = ctx.seq.on_message(vf, ctx.event_time, ctx.sink);
        record_result(rf);
        update_hwm(ctx.hwm, ctx.storage);
        advance_time(ctx);
        auto rt = ctx.seq.on_time(ctx.event_time, ctx.sink);
        record_result(rt);
        advance_time(ctx);
        auto ve = make_view(base, FeedSide::A, g_body_a, ctx.event_time);
        auto re = ctx.seq.on_message(ve, ctx.event_time, ctx.sink);
        record_result(re);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 3;
        measured_total_ns += elapsed;
        update_hwm(ctx.hwm, ctx.storage);
        ctx.ops += 3;
        advance_time(ctx);
        if (rf.code != SequencerCode::BufferedOutOfOrder) ctx.functional_ok = false;
        if (rt.code != SequencerCode::GapWaiting) ctx.functional_ok = false;
        if (re.code != SequencerCode::Emitted) ctx.functional_ok = false;
        if (ctx.seq.state() != SequencerState::Running) ctx.functional_ok = false;
        if (ctx.storage.pending_count() != 0) ctx.functional_ok = false;
        if (ctx.storage.pending_bytes() != 0) ctx.functional_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("gapwait_ontime_before_deadline", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && ctx.functional_ok;
    return res;
}

// =====================================================================
// Scenario 8: terminal FailedClosed steady-state calls
// =====================================================================
ScenarioResult scenario_failedclosed_steady() {
    g_sink_count = 0; g_sink_bytes = 0; g_result_xor = 0;
    g_slots.fill({}); g_arena.fill(0);

    BenchContext ctx;
    init_context(ctx);
    bool trigger_ok = ctx.functional_ok;

    // Trigger FailedClosed (wrong logical feed)
    {
        auto bad = make_view(0, FeedSide::A, g_body_a, ctx.event_time);
        bad.feed.value = 999;
        auto r = ctx.seq.on_message(bad, ctx.event_time, ctx.sink);
        g_result_xor ^= static_cast<int>(r.code);
        if (r.code != SequencerCode::WrongLogicalFeed) trigger_ok = false;
        if (ctx.seq.state() != SequencerState::FailedClosed) trigger_ok = false;
        advance_time(ctx);
    }
    // Verify g_sink_count is zero after trigger
    if (g_sink_count != 0) trigger_ok = false;

    // Warm-up: every call must return FailedClosed
    for (std::size_t i = 0; i < WARMUP_COUNT; ++i) {
        auto v = make_view(static_cast<std::uint32_t>(1000 + i), FeedSide::A,
                           g_body_a, ctx.event_time);
        auto r = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        if (r.code != SequencerCode::FailedClosed) trigger_ok = false;
        advance_time(ctx);
    }
    // Verify g_sink_count is zero after warm-up
    if (g_sink_count != 0) trigger_ok = false;

    // Measured: 1 op per iteration, all FailedClosed, no sink emission
    std::int64_t measured_total_ns = 0;
    enable_tracking();
    for (std::size_t i = 0; i < SAMPLE_COUNT; ++i) {
        auto saved_sink = g_sink_count;
        auto s = std::chrono::steady_clock::now();
        auto v = make_view(
            static_cast<std::uint32_t>(2000 + i), FeedSide::A,
            g_body_a, ctx.event_time);
        auto r = ctx.seq.on_message(v, ctx.event_time, ctx.sink);
        record_result(r);
        auto e = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            e - s).count();
        g_samples[i] = elapsed / 1;
        measured_total_ns += elapsed;
        ++ctx.ops;
        advance_time(ctx);
        if (r.code != SequencerCode::FailedClosed) trigger_ok = false;
        if (g_sink_count != saved_sink) trigger_ok = false;
    }
    disable_tracking();

    auto alloc_count = current_alloc_count();
    auto res = finalize("failedclosed_steady_state", ctx.ops,
                    measured_total_ns, alloc_count, ctx.hwm);
    res.functional_ok = res.functional_ok && trigger_ok;
    return res;
}

// --- Output ---

void print_result(const ScenarioResult& r) {
    std::printf(
        "%s %llu %llu %lld %.2f %.2f %lld %lld %lld %llu %llu %llu"
        " %llu\n",
        r.name, static_cast<unsigned long long>(r.operations),
        static_cast<unsigned long long>(r.samples),
        static_cast<long long>(r.total_elapsed_ns), r.ns_per_op,
        r.throughput_ops_per_sec, static_cast<long long>(r.p50_ns),
        static_cast<long long>(r.p95_ns), static_cast<long long>(r.p99_ns),
        static_cast<unsigned long long>(r.max_pending_messages),
        static_cast<unsigned long long>(r.max_pending_bytes),
        static_cast<unsigned long long>(r.checksum),
        static_cast<unsigned long long>(r.allocation_count));
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
        if (r.allocation_count != 0) {
            std::fprintf(stderr, "FAIL: %s had %zu allocations (expected 0)\n",
                         label, r.allocation_count);
            exit_code = 1;
        }
        if (!r.functional_ok) {
            std::fprintf(stderr, "FAIL: %s functional invariant violation\n", label);
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
