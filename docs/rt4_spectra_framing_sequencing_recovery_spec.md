# RT-4 SPECTRA framing, sequencing and recovery specification

**Date:** 2026-07-15  
**Issue:** #38  
**Status:** `OWNER_APPROVED_ARCHITECTURE — DOCUMENTATION_PR`  
**Implementation:** not started and not authorized

## 1. Purpose

RT-4 defines the deterministic C++20 transport-control layer between MOEX SPECTRA packet input and the existing RT-3 FAST decoder.

```text
UDP datagram
→ MOEX 4-byte preamble framing
→ A/B logical-feed arbitration
→ duplicate suppression
→ bounded reordering
→ gap detection
→ fail-closed recovery transition
→ later RT-3 integration and Snapshot recovery
```

This document is architecture and implementation specification only. It does not authorize code, MiMo, merge, RT-5, RT-6 or CI-2.

## 2. Authoritative sources

Source priority:

1. Current official MOEX SPECTRA FAST specification and current official test XML.
2. FIX FAST 1.1 only for base FAST semantics that MOEX does not fully define.
3. Third-party implementations only as cross-check.

Current normative document:

```text
spectra_fastgate_en.pdf
version 1.30.2
2026-04-10
```

Confirmed MOEX-specific facts:

- every UDP FAST message has a 4-byte external preamble before the FAST body;
- the preamble contains `MsgSeqNum(34)`;
- the same tag 34 is present inside the FAST message;
- the current MOEX system sends at most one FAST-coded message per UDP datagram;
- A and B are equal copies of one logical ordered feed;
- duplicate sequence numbers are ignored;
- bounded waiting for UDP reordering precedes confirmed gap recovery;
- after a confirmed gap, maintained market state is not trustworthy;
- TCP Historical Replay uses a different 4-byte prefix containing message length.

## 3. RT-4 gates

### Gate A — framing, A/B sequencing and gaps

- A1: UDP framing primitive.
- A2: A/B arbitration and duplicate suppression.
- A3: bounded reordering and gap detection.
- A4: explicit monotonic timer and fail-closed state.
- A5: Release benchmarks, allocation evidence and architecture review.

### Gate B — replay and RT-3 integration

- integrate RT-2 `.mxraw` A/B replay;
- feed exactly one bounded FAST body into RT-3;
- verify external preamble against decoded tag 34;
- implement one-time byte-order `AutoVerify`;
- add stream initialization and `SequenceReset` policy.

### Gate C — Snapshot recovery

- queue realtime Incremental messages while recovery is active;
- apply complete Snapshot cycle according to MOEX `LastFragment` semantics;
- replay contiguous queued Incremental messages;
- remain fail-closed on ambiguity, overflow or a second unresolved gap.

### Gate D — release acceptance

- end-to-end Windows/MSVC and Linux/GCC Release tests;
- latency distribution, throughput, allocation and memory evidence;
- real T0/T1 or official-vector validation;
- explicit Owner acceptance.

Each gate stops for architecture review. No later gate starts automatically.

## 4. Gate A boundaries

Included:

- framing of one current UDP datagram;
- explicit preamble byte order;
- A/B logical-feed sequencing;
- duplicate suppression;
- bounded out-of-order buffering;
- deterministic gap confirmation;
- terminal fail-closed transition;
- synthetic tests and Release benchmarks.

Excluded:

- sockets and multicast subscription;
- raw packet capture;
- TCP Historical Replay;
- `.mxraw` integration;
- RT-3 decode integration;
- automatic byte-order verification;
- FIX `SequenceReset` semantics;
- Snapshot recovery;
- normalized events and order books.

## 5. Threading model

Gate A is single-writer.

One logical feed owns exactly one mutable sequencer. A and B receivers or replay sources must serialize their messages before entering the sequencer.

```text
physical A ┐
           ├→ serialized input → one logical-feed sequencer
physical B ┘
```

The sequencer is not internally thread-safe. The hot path contains no locks.

## 6. A1 — UDP framing

### 6.1 Current packet contract

```text
bytes 0..3   external MsgSeqNum preamble
bytes 4..end exactly one complete current FAST body
```

The UDP datagram boundary is the message boundary. Gate A must not scan for additional internal messages.

### 6.2 Preamble byte order

MOEX 1.30.2 does not explicitly state the byte order of the external preamble. The XML does not define it because the preamble is outside the FAST body.

Gate A therefore requires explicit configuration:

```cpp
enum class PreambleByteOrder : std::uint8_t {
    LittleEndian,
    BigEndian
};
```

Rules:

- there is no default;
- Gate A does not guess;
- Gate B may compare both interpretations with decoded tag 34;
- ambiguous or neither-match verification fails closed;
- after verification, one order is locked per logical feed and the hot path performs one direct conversion.

### 6.3 Types and interface

```cpp
namespace moex::spectra {

enum class FeedSide : std::uint8_t {
    A,
    B
};

struct LogicalFeedId {
    std::uint32_t value{};
};

struct DatagramView {
    LogicalFeedId feed;
    FeedSide side;
    std::uint64_t capture_index;
    std::uint64_t capture_monotonic_ns;
    std::span<const std::uint8_t> payload;
};

struct FramedMessageView {
    LogicalFeedId feed;
    FeedSide side;
    std::uint32_t msg_seq_num;
    std::uint64_t capture_index;
    std::uint64_t capture_monotonic_ns;
    std::span<const std::uint8_t> fast_body;
};

struct FramingLimits {
    std::size_t max_datagram_bytes;
};

enum class FrameCode : std::uint8_t {
    Ok,
    InvalidConfig,
    DatagramTooShort,
    DatagramTooLarge,
    EmptyFastBody
};

struct FrameResult {
    FrameCode code;
};

FrameResult frame_udp_message(
    const DatagramView& input,
    PreambleByteOrder byte_order,
    FramingLimits limits,
    FramedMessageView& output
) noexcept;

} // namespace moex::spectra
```

### 6.4 Framing rules

- Interpret the preamble as unsigned `std::uint32_t`.
- Do not use an unaligned pointer cast.
- Do not depend on host endian.
- Use explicit byte shifts or an equivalent verified primitive.
- Reject payload shorter than five bytes.
- Reject payload above the configured bound.
- `fast_body` is a borrowed span beginning at byte four.
- No payload copy or heap allocation.
- On failure, output has a deterministic empty state.
- No FAST decode and no boundary guessing.

## 7. A2 — A/B sequencing and deduplication

### 7.1 Logical-feed invariant

A and B are physical copies of one logical sequence. Each logical feed has:

```text
one next_expected sequence
one bounded pending window
one mutable state machine
```

A is not primary and B is not backup. Whichever valid copy arrives first may advance the logical feed.

### 7.2 Explicit initialization

```cpp
void start(std::uint32_t next_expected_seq) noexcept;
```

Gate A does not infer the initial sequence and does not process `SequenceReset`. Gate B owns stream-specific initialization and reset semantics.

### 7.3 Message rules

```text
seq < next_expected
    drop as stale duplicate or late packet
    do not change ordered state

seq == next_expected
    emit synchronously
    increment next_expected
    flush all newly contiguous pending messages

seq > next_expected
    copy once into preallocated pending storage
    enter or remain in GapWait
    do not emit later messages
```

If a future sequence is already pending:

- same length and identical bytes: drop duplicate;
- different length or bytes: `DuplicatePayloadMismatch`, then fail closed.

A late duplicate after prior emission is dropped by sequence number. Gate A does not retain every emitted payload solely to compare a later A/B copy.

## 8. A3/A4 — bounded reordering and gap confirmation

### 8.1 State machine

```text
Stopped
  ↓ start()
Running
  ↓ future sequence observed
GapWait
  ├─ missing sequence arrives before deadline and limits → Running
  └─ deadline or capacity condition → FailedClosed
```

`FailedClosed` is terminal until the higher recovery layer explicitly resets and starts the sequencer.

### 8.2 Configuration

```cpp
struct SequencerConfig {
    std::uint32_t max_reorder_messages;
    std::size_t max_reorder_bytes;
    std::uint64_t reorder_wait_ns;
};
```

All values are explicit. Gate A defines no guessed production defaults.

### 8.3 Gap confirmation

A gap is confirmed when any of these conditions occurs:

- `reorder_wait_ns` expires;
- sequence distance exceeds `max_reorder_messages`;
- pending-message capacity is exhausted;
- pending-byte capacity would be exceeded;
- monotonic time regresses;
- payload mismatch or an internal invariant failure occurs.

After confirmation:

- no subsequent message is emitted;
- the missing sequence is never silently skipped;
- input may be counted for diagnostics but cannot advance state;
- a deterministic recovery-required result is returned.

### 8.4 Time model

The caller supplies monotonic time explicitly:

```cpp
template<class OrderedSink>
SequencerResult on_time(
    std::uint64_t monotonic_ns,
    OrderedSink& sink
) noexcept;
```

Gate A does not call a wall-clock API. Supplied time for one sequencer must be non-decreasing. This keeps live and replay behaviour deterministic.

## 9. Sequencer interface

```cpp
enum class SequencerState : std::uint8_t {
    Stopped,
    Running,
    GapWait,
    FailedClosed
};

enum class SequencerCode : std::uint16_t {
    Emitted,
    DuplicateDropped,
    BufferedOutOfOrder,
    GapWaiting,

    NotStarted,
    InvalidConfig,
    DuplicatePayloadMismatch,
    ReorderDistanceExceeded,
    PendingMessageCapacityExceeded,
    PendingByteCapacityExceeded,
    ClockRegression,
    GapConfirmed,
    FailedClosed,
    InternalInvariantViolation
};

struct SequencerResult {
    SequencerCode code;
    std::uint32_t observed_seq;
    std::uint32_t expected_seq;
};

class DualFeedSequencer {
public:
    DualFeedSequencer(
        SequencerConfig config,
        MessageStorage& storage
    ) noexcept;

    void start(std::uint32_t next_expected_seq) noexcept;
    void reset() noexcept;

    SequencerState state() const noexcept;
    std::uint32_t next_expected_seq() const noexcept;

    template<class OrderedSink>
    SequencerResult on_message(
        const FramedMessageView& message,
        std::uint64_t monotonic_ns,
        OrderedSink& sink
    ) noexcept;

    template<class OrderedSink>
    SequencerResult on_time(
        std::uint64_t monotonic_ns,
        OrderedSink& sink
    ) noexcept;
};
```

The sink is templated or statically bound. Per-message `std::function`, virtual dispatch, exceptions and locks are not required.

## 10. Ownership and lifetime

### In-order path

- datagram payload is borrowed;
- framed FAST body is borrowed;
- sink callback is synchronous;
- no copy occurs;
- sink must not retain the span after callback return.

### Out-of-order path

- payload is copied exactly once into preallocated storage;
- the sequencer owns the slot;
- emitted slot view is valid only during the callback;
- slot becomes reusable immediately after callback completion.

### Storage

```text
fixed slot array
fixed byte arena or slab
bounded metadata
no capacity growth
```

Memory is bounded by `max_reorder_messages` and `max_reorder_bytes`.

## 11. Allocation and hot-path policy

After initialization, these paths must allocate zero heap objects:

- framing;
- in-order emission;
- duplicate drop;
- out-of-order insertion within configured capacity;
- contiguous pending flush;
- gap waiting and confirmation;
- failed-closed handling.

Prohibited in the hot path without measured justification:

- growing `std::vector`;
- `std::map` or `std::unordered_map` insertion;
- per-message `std::string`;
- `std::function`;
- exceptions;
- formatted logging;
- speculative generic abstractions.

## 12. Deterministic diagnostics

Errors use stable enum codes and fixed numeric context:

```cpp
struct GateAIssue {
    SequencerCode code;
    LogicalFeedId feed;
    FeedSide side;
    std::uint32_t observed_seq;
    std::uint32_t expected_seq;
    std::uint64_t capture_index;
    std::uint64_t capture_monotonic_ns;
};
```

Human-readable formatting is offline and outside the hot path.

The same input and configuration must produce the same ordered output, drops, buffers, gap transition, issue codes and final state.

## 13. Synthetic test plan

### Framing

- payload sizes zero through four;
- minimum valid size five;
- exact configured maximum and one byte above;
- `01 00 00 00`: little `1`, big `16777216`;
- `00 00 00 01`: little `16777216`, big `1`;
- `01 02 03 04`: little `0x04030201`, big `0x01020304`;
- `FF FF FF FF`: `UINT32_MAX`;
- body starts exactly at offset four;
- source bytes are not modified or copied.

### A/B sequencing

- `1A`;
- `1A,1B` and `1B,1A`;
- alternating winning side;
- `1A,2A,1B,2B`;
- duplicate future message while pending;
- same pending sequence with different bytes;
- stale duplicate after emission;
- explicit start at a value other than one.

### Reordering and gaps

- `1,3,2`;
- `1,4,3,2`;
- gap filled before deadline;
- defined boundary-time case;
- timeout before missing message;
- distance, count and byte limits;
- monotonic time regression;
- no emission after `FailedClosed`;
- explicit reset and deterministic restart;
- repeated run produces byte-identical event log.

### Platforms

- Windows/MSVC Release with warnings as errors;
- Linux/GCC Release with warnings as errors;
- exact CTest inventory check;
- tests remain active under `NDEBUG`;
- no real raw market-data capture committed.

## 14. Release benchmark plan

Workloads:

- framing at representative payload sizes up to near MTU;
- one-side ordered stream;
- 100% A/B duplicates;
- alternating winning side;
- reorder depths 1, 4, 16 and near configured limit;
- burst reorder followed by contiguous flush;
- confirmed gap and failed-closed input.

Metrics:

- p50, p90, p99 and p99.9 latency;
- maximum observed latency;
- messages and payload bytes per second;
- allocations and allocated bytes per message;
- post-initialization allocation count;
- fixed-storage high-water mark;
- peak process memory;
- execution-time variability.

Windows and Linux results are recorded separately. The first measurement establishes a baseline. No invented nanosecond threshold or flaky latency CI gate is accepted.

## 15. Small implementation stages

After separate implementation authorization:

1. A1 framing types, parser and endian vectors.
2. A2 in-order sequencing and duplicate suppression.
3. A3 fixed pending storage and bounded reordering.
4. A4 explicit timer, gap confirmation and fail-closed state.
5. A5 Release benchmarks, allocation evidence and Gate A review.

Each stage is one small logical task, one commit, push, CI and review. MiMo stops after each stage. Gate B cannot begin before Gate A Owner acceptance.

## 16. Gate A acceptance review

Before Gate B:

1. Re-read current MOEX specification and official T0/T1 files.
2. Verify framing and A/B semantics against official evidence.
3. Review complete diff, tests, exact inventory and CI.
4. Prove that no message after a confirmed gap can be emitted.
5. Prove bounded memory and zero post-initialization Gate A allocations.
6. Review Release benchmark distributions and variability.
7. Reject unmeasured abstraction or diagnostic overhead.
8. Obtain explicit Owner approval.

## 17. Open evidence

These items do not block specification approval but remain unresolved for production acceptance:

- written MOEX confirmation, official vector or live packet for external preamble endian;
- production reorder timeout and capacity values;
- stream-specific initial sequence and `SequenceReset` handling;
- `.mxraw` A/B replay merge;
- Snapshot + buffered Incremental recovery;
- end-to-end allocation behaviour with RT-3.

## 18. Authorization boundary

Owner approval of this architecture and documentation PR does not authorize:

```text
C++ implementation
MiMo launch
Issue #38 implementation work
merge or auto-merge
force push or branch deletion
RT-5 / RT-6 / CI-2
production or live trading
```
