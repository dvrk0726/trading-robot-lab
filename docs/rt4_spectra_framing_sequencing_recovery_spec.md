# RT-4 SPECTRA framing, sequencing and recovery specification

**Date:** 2026-07-16
**Issues:** #38, #51
**PR:** #52
**Status:** `GATE_A_IMPLEMENTATION_COMPLETE — ARCHITECTURE_REVIEW_PENDING`

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

Gate A implementation is complete in Draft PR #52. Final Architecture Review is pending. Ready-for-review is not authorized. Merge is not authorized. Gate B, Gate C and Gate D remain blocked.

This document is the sole authoritative Gate A contract.

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

If MOEX later allows several FAST messages in one UDP datagram, that is a source-contract change. Gate A must fail closed through downstream exact-body validation until a separately reviewed framing revision exists.

## 3. RT-4 gates

### Gate A Completion — framing, sequencing, recovery and acceptance

Formerly tracked as A1, A2, A3, A4 and A5. These remain as historical labels for internal implementation phases only. The single current gate is:

```text
RT-4 Gate A Completion
```

Subsumed historical phases:

- A1: UDP framing primitive — done.
- A2: deterministic `std::uint32_t` serial-number classification primitive — done.
- A3/A4/A5: implementation and acceptance-evidence phases are complete in Draft PR #52; final Architecture Review is pending; Ready is not authorized; merge is not authorized.

Gate A is a single coherent transport-control subsystem:

```text
fixed preallocated MessageStorage
bounded message and byte capacity
one mutable sequencer per LogicalFeedId
explicit initialize, start and reset
A/B first-valid-copy arbitration
synchronous in-order emission
stale duplicate suppression
future-message insertion
same pending sequence payload comparison
contiguous pending flush
fixed non-extendable gap deadline
monotonic-time validation
exact now >= deadline semantics
all deterministic fail-closed transitions
Release benchmarks and allocation evidence
final Gate A architecture acceptance
```

A valid future packet is never silently discarded or emitted out of order. Mutable sequencing begins only with bounded storage available.

### Gate B — replay and RT-3 integration

- integrate RT-2 `.mxraw` A/B replay;
- feed exactly one bounded FAST body into RT-3;
- consume the fixed little-endian external sequence value from A1;
- decode RT-3 tag 34;
- compare numeric values for every integrated message;
- mismatch fails closed;
- add stream initialization and `SequenceReset` policy.

### Gate C — Snapshot recovery

- queue realtime Incremental messages while recovery is active;
- apply a complete Snapshot cycle according to MOEX `LastFragment` semantics;
- replay contiguous queued Incremental messages;
- remain fail-closed on ambiguity, overflow or a second unresolved gap.

### Gate D — Release acceptance

- end-to-end Windows/MSVC and Linux/GCC Release tests;
- latency distribution, throughput, allocation and memory evidence;
- real T0/T1 or official-vector validation;
- explicit Owner acceptance.

Each gate stops for architecture review. No later gate starts automatically.

## 4. Gate A boundaries

Included:

- framing of one current UDP datagram;
- fixed little-endian preamble decoding;
- A/B logical-feed sequencing;
- duplicate suppression;
- bounded out-of-order buffering;
- deterministic sequence arithmetic;
- deterministic gap confirmation;
- terminal fail-closed transition;
- synthetic tests and Release benchmarks.

Excluded:

- sockets and multicast subscription;
- raw packet capture;
- TCP Historical Replay;
- `.mxraw` integration;
- RT-3 decode integration;
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

The UDP datagram boundary is the current message boundary. Gate A does not scan for additional internal messages.

TCP Historical Replay uses a different 4-byte prefix containing message length, not the UDP sequence preamble. The two formats must not be conflated.

### 6.2 Preamble byte order

Written MOEX support confirmation (paraphrased, 2026-07-16):

- the UDP FAST preamble is four bytes and uses little-endian byte order;
- MsgSeqNum value 1 is encoded as `01 00 00 00`;
- the same little-endian rule applies to SPECTRA T0, T1 and production feeds;
- the numeric preamble value is guaranteed to equal decoded FAST MsgSeqNum tag 34;
- this concerns the UDP multicast preamble, not the TCP Historical Replay length prefix.

Gate A supports only fixed little-endian decoding. There is no runtime byte-order selector, alternative production byte-order path or automatic endian discovery.

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
    LogicalFeedId feed{};
    FeedSide side{FeedSide::A};
    std::uint32_t msg_seq_num{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
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
    FramingLimits limits,
    FramedMessageView& output
) noexcept;

} // namespace moex::spectra
```

### 6.4 Framing rules

- Fixed little-endian decoding of bytes 0..3 using explicit shifts.
- No unaligned pointer cast.
- No dependence on host endian.
- `max_datagram_bytes` must be at least five.
- Reject payload shorter than four bytes.
- Reject payload of exactly four bytes (empty FAST body).
- Reject payload above the configured bound.
- `fast_body` is a borrowed span beginning at byte four.
- No payload copy or heap allocation.
- On failure, output has a deterministic empty state.
- No FAST decode and no boundary guessing.

### 6.5 Little-endian decoding vectors

| Raw bytes | Decoded value |
|---|---|
| `01 00 00 00` | 1 |
| `00 00 00 01` | 0x01000000 |
| `01 02 03 04` | 0x04030201 |
| `FF FF FF FF` | UINT32_MAX |

## 7. A3 — A/B sequencing and deduplication

This section defines the stateful A3 sequencer contract. It is not part of the A2 serial-arithmetic implementation.

### 7.1 Logical-feed invariant

A and B are physical copies of one logical sequence. Each logical feed has:

```text
one configured LogicalFeedId
one next_expected sequence
one bounded pending window
one mutable state machine
```

A is not primary and B is not backup. Whichever valid copy arrives first may advance the logical feed.

A message whose `LogicalFeedId` differs from the sequencer configuration is rejected fail-closed as `WrongLogicalFeed`.

### 7.2 Explicit initialization

```cpp
SequencerCode initialize(
    LogicalFeedId feed,
    SequencerConfig config,
    MessageStorage& storage
) noexcept;

SequencerCode start(std::uint32_t initial_expected_seq) noexcept;
SequencerCode reset() noexcept;
```

Lifecycle rules:

- `initialize` is valid only from `Uninitialized`;
- invalid `initialize` remains `Uninitialized`;
- `start` is valid only from `Stopped` after successful initialization;
- `reset` from `Uninitialized` returns `NotInitialized`;
- successful `reset` clears pending state, deadline and clock state, preserves feed/config/storage, and enters `Stopped`;
- recovery requires explicit `reset` followed by `start`;
- Gate A does not infer the initial sequence and does not process `SequenceReset`;
- Gate B owns stream-specific initialization and reset semantics.

## 8. A2 — Sequence-number arithmetic

A2 implements only this stateless classification primitive. It does not expose or instantiate `DualFeedSequencer`, `MessageStorage`, an ordered sink or any mutable logical-feed state.

The implementation interface must express these stable outcomes without temporary staging codes:

```text
Expected
FutureWithinWindow
FutureBeyondWindow
Ambiguous
Stale
InvalidConfig
```

Plain signed or unsigned `<` comparison is prohibited.

Gate A uses deterministic 32-bit serial-number arithmetic:

```cpp
const std::uint32_t delta = observed - next_expected;
```

Interpretation:

```text
delta == 0
    observed is exactly next_expected

1 <= delta <= max_reorder_messages
    observed is a permitted future sequence

max_reorder_messages < delta < 0x80000000
    future distance exceeds the configured bound; fail closed in the later stateful sequencer

delta == 0x80000000
    half-range relation is ambiguous; fail closed in the later stateful sequencer

0x80000000 < delta <= 0xFFFFFFFF
    observed is stale or behind next_expected; duplicate/late drop in the later stateful sequencer
```

Configuration invariant:

```text
0 < max_reorder_messages < 0x80000000
```

`next_expected` increments modulo `2^32`. This safely classifies a natural wrap only when the forward distance is inside the configured bounded window. Explicit MOEX reset semantics remain Gate B scope.

The A2 primitive classifies only. A3 owns every state transition and action associated with the classification, including emission, drop, buffering and fail-closed handling.

## 9. A3 message-processing rules

```text
delta in stale range
    drop as stale duplicate or late packet
    do not change ordered state

delta == 0
    emit synchronously
    increment next_expected modulo 2^32
    flush all newly contiguous pending messages

delta in permitted future range
    copy once into preallocated pending storage
    enter or remain in GapWait
    do not emit later messages
```

If a future sequence is already pending:

- same length and identical FAST-body bytes: drop duplicate;
- different length or bytes: `DuplicatePayloadMismatch`, then fail closed.

A late duplicate after prior emission is dropped by sequence number. Gate A does not retain every emitted payload solely to compare a later A/B copy.

## 10. A3/A4 — bounded reordering and gap confirmation

### 10.1 State machine

```text
Uninitialized
  ↓ initialize()
Stopped
  ↓ start()
Running
  ↓ permitted future sequence observed
GapWait
  ├─ pending becomes empty after missing messages arrive → Running
  └─ deadline, capacity or invariant condition → FailedClosed
```

`FailedClosed` is terminal until explicit `reset()` and `start()` by the higher recovery layer.

### 10.2 Configuration

```cpp
struct MessageStorageConfig {
    std::uint32_t max_reorder_messages{};
    std::size_t max_reorder_bytes{};
    std::size_t max_message_bytes{};
};

struct SequencerConfig {
    LogicalFeedId logical_feed{};
    std::uint32_t max_reorder_distance{};
    std::uint64_t reorder_wait_ns{};
    MessageStorageConfig storage{};
};
```

Validation:

- `logical_feed` must match the `feed` argument to `initialize`;
- `max_reorder_distance` is nonzero and below `0x80000000`;
- `reorder_wait_ns` is nonzero;
- `storage.max_reorder_messages` is nonzero and below `0x80000000`;
- `storage.max_message_bytes` is nonzero;
- `storage.max_reorder_bytes` is nonzero;
- supplied storage must be initialized and empty;
- `slot_capacity >= max_reorder_distance`;
- storage actual limits must cover declared limits;
- invalid configuration cannot enter `Running`.

Gate A defines no guessed production defaults.

### 10.3 Fixed gap episode and deadline

When the first permitted future message arrives while `Running`:

```text
gap_start_ns = supplied monotonic time
gap_deadline_ns = checked(gap_start_ns + reorder_wait_ns)
state = GapWait
```

Rules:

- addition overflow is an initialization/runtime invariant failure and fails closed;
- later future messages and duplicates do not extend or restart the deadline;
- resolving one missing number does not restart the deadline while pending messages still expose another hole;
- the gap episode ends only when pending storage becomes empty after contiguous emission;
- a later independent gap episode receives a new deadline.

This prevents an endless wait caused by continuing out-of-order traffic.

### 10.4 Exact deadline semantics

For both `on_message(now)` and `on_time(now)`:

1. reject monotonic-time regression;
2. when in `GapWait`, check the deadline before processing the event;
3. `now >= gap_deadline_ns` confirms the gap immediately;
4. therefore a missing message stamped exactly at the deadline is too late and is not emitted.

This boundary rule is deterministic for live and replay execution.

### 10.5 Other gap/failure conditions

The sequencer fails closed when any condition occurs:

- future sequence distance exceeds `max_reorder_messages`;
- pending-message capacity is exhausted;
- pending-byte capacity would be exceeded;
- duplicate payload mismatch;
- wrong logical feed;
- ambiguous half-range sequence relation;
- monotonic time regression;
- internal storage or state invariant failure.

After fail-closed transition:

- a terminal result enters `FailedClosed`;
- pending storage may remain intact for diagnostics;
- no later call may emit or advance it;
- `reset` clears pending storage before restart;
- no subsequent message is emitted;
- the missing sequence is never silently skipped;
- input may be counted for diagnostics but cannot advance state;
- a deterministic recovery-required result is returned.

## 11. Sequencer interface

The following is the complete Gate A public interface, synchronized with the current implementation.

### 11.1 Storage geometry

```cpp
struct StorageGeometryLimits {
    std::uint32_t max_reorder_messages{};
    std::size_t max_reorder_bytes{};
    std::size_t max_message_bytes{};
};

enum class GeometryCode : std::uint8_t {
    Ok,
    ZeroMaxReorderMessages,
    MaxReorderMessagesTooLarge,
    ZeroMaxMessageBytes,
    SlotCapacityTooSmall,
    CapacityOverflow,
    ArenaTooSmall,
    ZeroMaxReorderBytes,
    MaxReorderBytesExceedsCapacity
};

[[nodiscard]] constexpr GeometryCode validate_storage_geometry(
    std::size_t slot_capacity,
    std::span<const std::uint8_t> payload_arena,
    StorageGeometryLimits limits
) noexcept;
```

Validation checks, in order:

- `max_reorder_messages` nonzero and below `0x80000000`;
- `max_message_bytes` nonzero;
- `slot_capacity >= max_reorder_messages`;
- checked multiplication `slot_capacity * max_message_bytes` does not overflow;
- `payload_arena.size() >= slot_capacity * max_message_bytes`;
- `max_reorder_bytes` nonzero;
- `max_reorder_bytes <= slot_capacity * max_message_bytes`.

### 11.2 MessageStorage

```cpp
enum class StorageInitCode : std::uint8_t {
    Ok = 0,
    InvalidGeometry = 1,
    AlreadyInitialized = 2
};

struct StorageInitResult {
    StorageInitCode code;
    GeometryCode geometry_code;
};

enum class InsertResult : std::uint8_t {
    Ok,
    NotInitialized,
    EmptyBody,
    BodyTooLarge,
    DuplicateEqual,
    DuplicateMismatch,
    PendingMessageCapacityExceeded,
    PendingByteCapacityExceeded,
    InternalInvariantViolation
};

struct SlotMetadata {
    std::uint32_t sequence{};
    FeedSide side{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
    std::size_t payload_offset{};
    std::size_t payload_length{};
    bool occupied{};
};

struct StoredMessageView {
    bool found{};
    std::uint32_t sequence{};
    FeedSide side{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
    std::span<const std::uint8_t> body{};
};

class MessageStorage {
public:
    MessageStorage() = default;
    MessageStorage(const MessageStorage&) = delete;
    MessageStorage& operator=(const MessageStorage&) = delete;
    MessageStorage(MessageStorage&&) noexcept = delete;
    MessageStorage& operator=(MessageStorage&&) noexcept = delete;
    ~MessageStorage() = default;

    [[nodiscard]] static constexpr GeometryCode validate_geometry(
        std::size_t slot_capacity,
        std::span<const std::uint8_t> payload_arena,
        StorageGeometryLimits limits
    ) noexcept;

    [[nodiscard]] StorageInitResult initialize(
        std::span<SlotMetadata> slots,
        std::span<std::uint8_t> arena,
        MessageStorageConfig config
    ) noexcept;

    [[nodiscard]] InsertResult insert(
        std::uint32_t msg_seq_num,
        FeedSide side,
        std::uint64_t capture_index,
        std::uint64_t capture_monotonic_ns,
        std::span<const std::uint8_t> body
    ) noexcept;

    [[nodiscard]] const SlotMetadata& view(std::uint32_t msg_seq_num) const noexcept;
    [[nodiscard]] StoredMessageView view_message(std::uint32_t msg_seq_num) const noexcept;
    [[nodiscard]] bool release(std::uint32_t msg_seq_num) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool is_occupied(std::uint32_t msg_seq_num) const noexcept;
    [[nodiscard]] std::size_t pending_count() const noexcept;
    [[nodiscard]] std::size_t pending_bytes() const noexcept;
    [[nodiscard]] std::size_t slot_capacity() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] std::uint32_t max_reorder_messages() const noexcept;
    [[nodiscard]] std::size_t max_reorder_bytes() const noexcept;
    [[nodiscard]] std::size_t max_message_bytes() const noexcept;

    [[nodiscard]] static constexpr bool can_add_pending_bytes(
        std::size_t current, std::size_t addition, std::size_t limit
    ) noexcept;
};
```

Storage geometry:

- caller-owned fixed slot span and byte arena;
- one dedicated contiguous payload slice per slot at `slot_index * max_message_bytes`;
- geometry validation with checked multiplication;
- exact-sequence O(1) modulo lookup: `slot_index = msg_seq_num % slot_capacity`;
- the bounded forward window guarantees two valid pending sequences cannot occupy the same index when `slot_capacity >= max_reorder_messages`;
- an occupied-index mismatch is `InternalInvariantViolation`;
- `max_reorder_bytes` counts actual pending payload lengths;
- all checks complete before a slot is marked occupied;
- failure never publishes a partial message;
- `MessageStorage` is non-copyable and non-movable.

Behavior:

- `initialize` lifecycle:
  - first call with invalid geometry: returns `InvalidGeometry` with exact `GeometryCode`; object remains fully uninitialized; internal spans are empty; limits, capacity and counters are zero; caller-provided slots and arena are unchanged; valid retry is permitted;
  - first call with valid geometry: returns `Ok`/`Ok`; object becomes initialized;
  - any call after first success: returns `AlreadyInitialized`/`Ok` regardless of new arguments; `AlreadyInitialized` is checked before geometry validation; old spans, limits, capacity, counters, metadata, pending payload and payload pointers are preserved; new buffers are not changed;
  - `reset` clears pending state but preserves initialized state and initial buffer binding; `initialize` after `reset` returns `AlreadyInitialized`;
  - caller-owned initial buffers must live for the entire lifetime of `MessageStorage` use;
- `insert`: checks body size, modulo lookup, duplicate comparison (byte-by-byte), capacity limits, copies payload into dedicated slice;
- `view(seq)`: returns exact-sequence `SlotMetadata` reference, or deterministic empty/default metadata when uninitialized, absent or colliding; does not return an arena body span;
- `view_message(seq)`: returns `StoredMessageView` with the borrowed read-only payload span only for an exact pending sequence; absent or colliding `view_message` returns a deterministic empty view;
- `release`: clears slot metadata, preserves payload offset for reuse, decrements pending counters;
- `reset`: clears all slot occupancy, preserves initialized state and payload offsets; pending counters return to zero.

Normative per-message size rule:

- every accepted FAST body, including an Expected in-order body, must fit the sequencer's declared `storage.max_message_bytes`;
- an oversized Expected or future body enters `FailedClosed` with `PendingByteCapacityExceeded`;
- the sink is not invoked and `next_expected` is not advanced for an oversized Expected body.

### 11.3 Sequencer state machine

```cpp
enum class SequencerState : std::uint8_t {
    Uninitialized,
    Stopped,
    Running,
    GapWait,
    FailedClosed
};

enum class SequencerCode : std::uint16_t {
    NoAction,
    Initialized,
    Started,
    Reset,
    Emitted,
    DuplicateDropped,
    BufferedOutOfOrder,
    GapWaiting,

    NotInitialized,
    InvalidTransition,
    InvalidConfig,
    WrongLogicalFeed,
    DuplicatePayloadMismatch,
    ReorderDistanceExceeded,
    AmbiguousSequenceRelation,
    PendingMessageCapacityExceeded,
    PendingByteCapacityExceeded,
    ClockRegression,
    DeadlineOverflow,
    GapConfirmed,
    FailedClosed,
    InternalInvariantViolation
};

struct SequencerResult {
    SequencerCode code{SequencerCode::NoAction};
    std::uint32_t observed_seq{};
    std::uint32_t expected_seq{};
};
```

`SequencerResult` entry-state semantics:

- `on_message.observed_seq` = input message sequence;
- `on_message.expected_seq` = `next_expected` at event entry;
- `on_time.observed_seq` = `next_expected` at event entry;
- `on_time.expected_seq` = `next_expected` at event entry;
- the post-event value is available from `next_expected_seq()`.

### 11.4 OrderedMessageMetadata

```cpp
struct OrderedMessageMetadata {
    std::uint32_t msg_seq_num{};
    FeedSide side{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
};
```

### 11.5 DualFeedSequencer

```cpp
class DualFeedSequencer {
public:
    DualFeedSequencer() = default;
    DualFeedSequencer(const DualFeedSequencer&) = delete;
    DualFeedSequencer& operator=(const DualFeedSequencer&) = delete;
    DualFeedSequencer(DualFeedSequencer&&) noexcept = delete;
    DualFeedSequencer& operator=(DualFeedSequencer&&) noexcept = delete;
    ~DualFeedSequencer() = default;

    [[nodiscard]] SequencerCode initialize(
        LogicalFeedId feed,
        SequencerConfig config,
        MessageStorage& storage
    ) noexcept;

    [[nodiscard]] SequencerCode start(std::uint32_t initial_expected_seq) noexcept;
    [[nodiscard]] SequencerCode reset() noexcept;

    [[nodiscard]] SequencerState state() const noexcept;
    [[nodiscard]] std::uint32_t next_expected_seq() const noexcept;

    template<class Sink>
    [[nodiscard]] SequencerResult on_message(
        const FramedMessageView& message,
        std::uint64_t event_monotonic_ns,
        Sink& sink
    ) noexcept;

    template<class Sink>
    [[nodiscard]] SequencerResult on_time(
        std::uint64_t event_monotonic_ns,
        Sink& sink
    ) noexcept;
};
```

`DualFeedSequencer` is non-copyable and non-movable.

### 11.6 Sink contract

The sink is templated or statically bound, synchronous and statically verified `noexcept`. It must accept `(const OrderedMessageMetadata&, std::span<const std::uint8_t>)` during the callback and must not retain the span. Per-message `std::function`, virtual dispatch, exceptions and locks are not required.

### 11.7 Time semantics

Two monotonic values have different meanings:

- `FramedMessageView.capture_monotonic_ns`: immutable capture metadata retained for diagnostics and pending emission;
- `event_monotonic_ns` passed to `on_message` / `on_time`: authoritative sequencer clock sampled at serialized Gate A entry or supplied by deterministic replay.

They are not required to be equal. Independent A/B capture timestamps therefore cannot accidentally define sequencer event order.

### 11.8 Event precedence

For `Running` and `GapWait`, processing order is fixed:

```text
1. lifecycle/state validity
2. event clock regression
3. active deadline
4. message feed and structural preconditions
5. A2 sequence classification
6. relation-specific fatal result
7. pending duplicate comparison
8. pending message capacity
9. per-message and total byte capacity
10. copy or emit and atomic state commit
```

Consequences:

- `ClockRegression` wins over `GapConfirmed`;
- `GapConfirmed` wins over message contents at or after deadline;
- `DeadlineOverflow` is checked before copying the first future message;
- message-capacity failure wins over byte-capacity failure when both apply;
- only successful A1 framing output is valid input.

### 11.9 on_time result semantics

Normative `on_time` behavior:

- `on_time` in `Running` returns `NoAction`;
- `on_time` in `GapWait` before the active deadline returns `GapWaiting`;
- `on_time` in `GapWait` at or after the deadline returns `GapConfirmed` and enters `FailedClosed`;
- `ClockRegression` is checked first and therefore wins over an expired deadline.

## 12. Ownership and lifetime

### In-order path

- datagram payload is borrowed;
- framed FAST body is borrowed;
- sink callback is synchronous;
- no copy occurs;
- sink must not retain the span after callback return.

### Out-of-order path

- FAST-body payload is copied exactly once into preallocated storage;
- the sequencer owns the slot;
- emitted slot view is valid only during the callback;
- slot becomes reusable immediately after callback completion.

Metadata retained per pending message includes sequence, side, capture index, monotonic timestamp, payload offset and payload length.

### Storage

```text
fixed slot array
fixed byte arena or slab
bounded deterministic lookup metadata
no capacity growth
```

Memory is bounded by `max_reorder_messages` and `max_reorder_bytes`.

## 13. Allocation and hot-path policy

After initialization, these paths must allocate zero heap objects:

- framing;
- in-order emission;
- duplicate drop;
- out-of-order insertion within configured capacity;
- duplicate byte comparison;
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

## 14. Deterministic diagnostics

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

The same input and configuration must produce the same ordered output, drops, buffers, deadline, gap transition, issue codes and final state.

## 15. Accepted test evidence

### CTest inventory

Total moex_fast CTest inventory: 18.

- RT-1 inspector: 6 tests.
- RT-3 decoder: 9 tests.
- RT-4 A1 framing: 1 test (`test_spectra_udp_framing`).
- RT-4 A2 sequence arithmetic: 1 test (`test_spectra_sequence_arithmetic`).
- RT-4 Gate A: 1 test (`test_spectra_gate_a`).

### Gate A test binary

One Gate A CTest binary with 98 internal test cases, covering:

- storage geometry validation and checked multiplication;
- caller-owned fixed slot span and byte arena;
- one dedicated contiguous payload slice per slot;
- exact-sequence O(1) modulo lookup including uint32 wrap;
- occupied-index invariant failure;
- insert, view, release, reset, view_message;
- `MessageStorage` non-copyable and non-movable;
- `StoredMessageView` borrowed pointer and metadata;
- no partial slot publication after failed insertion;
- `StorageInitCode`, `StorageInitResult` and `initialize` lifecycle;
- observable exact geometry failure with precise `GeometryCode`;
- valid retry after invalid geometry;
- second valid `initialize` preserves complete state;
- `AlreadyInitialized` precedence over invalid geometry;
- `reset` preserves one-shot storage binding;
- `SequencerConfig` field validation;
- `SequencerState` lifecycle: `Uninitialized`, `Stopped`, `Running`, `GapWait`, `FailedClosed`;
- complete `SequencerCode` including `NoAction`, `GapWaiting`, `ClockRegression`, `DeadlineOverflow`;
- `SequencerResult` entry-state semantics;
- `DualFeedSequencer` exact signatures: `initialize`, `start`, `reset`, `on_message`, `on_time`;
- `reset` returns `SequencerCode`;
- A/B first-valid-copy arbitration and alternating winning side;
- stale duplicate after emission;
- wrong logical feed;
- future duplicate with equal bytes and different bytes;
- `1,3,2` and deeper contiguous flush vectors;
- natural uint32 wrap;
- message-count and byte-capacity limits;
- message-capacity failure precedence over byte-capacity failure;
- fixed deadline that later traffic cannot extend;
- arrival exactly at deadline;
- partial resolution does not extend deadline;
- clock regression and deadline overflow;
- `ClockRegression` precedence over `GapConfirmed`;
- no emission after `FailedClosed`;
- explicit reset and deterministic restart;
- deterministic replay comparison including per-event result, state and `next_expected`;
- synchronous statically verified noexcept sink;
- capture_monotonic_ns versus authoritative event_monotonic_ns;
- declared limits enforced with larger storage;
- `on_time` in `Running` returns `NoAction`;
- `on_time` in `GapWait` before deadline returns `GapWaiting`;
- reset transition matrix;
- result-field entry-state semantics;
- body too large on both expected and future paths;
- deadline overflow leaves arena unchanged.

### Platforms

- Windows/MSVC Release with warnings as errors.
- Linux/GCC Release with warnings as errors.
- exact CTest inventory verified in both CI jobs.
- tests remain active under `NDEBUG`.
- no real raw market-data capture committed.

## 16. Accepted benchmark evidence

One Release benchmark executable (`bench_spectra_gate_a`), not a CTest. Runs in both the Windows/MSVC and Linux/GCC CI jobs.

Eight deterministic scenarios:

1. `ordered_first_copy_emission` — one-side ordered stream.
2. `expected_a_then_stale_b` — expected A message followed by stale B duplicate.
3. `alternating_ab_winning_side` — alternating A/B first-copy wins.
4. `reorder_flush_depth_1` — bounded reorder depth 1 and contiguous flush.
5. `reorder_flush_depth_4` — bounded reorder depth 4 and contiguous flush.
6. `equal_pending_duplicate_drop` — equal pending duplicate drop.
7. `gapwait_ontime_before_deadline` — GapWait on_time before deadline.
8. `failedclosed_steady_state` — terminal FailedClosed steady-state calls.

Per scenario:

- 21 samples;
- per-operation p50, p95 and p99 latency;
- throughput (ops/sec);
- pending-message and pending-byte high-water marks;
- deterministic checksum;
- executable-local allocation interception via global `operator new` replacement;
- `allocation_count == 0` for all eight measured scenarios;
- functional correctness invariant check per scenario.

This is Gate A evidence. It does not constitute end-to-end RT-3 production performance acceptance. No latency pass/fail threshold or flaky latency CI gate is accepted.

## 17. Implementation history

Historical internal phases (now consolidated into Gate A Completion):

1. A1 framing types, parser and little-endian decoding — done.
2. A2 deterministic serial-number classification primitive — done.
3. A3/A4/A5: bounded `MessageStorage`, complete A/B sequencer, gap recovery, Release benchmarks and allocation evidence — done in Draft PR #52.

The work is one Issue (#51), one branch, one Draft PR (#52) and one final merge. Gate B cannot begin before Gate A Owner acceptance.

## 18. Gate A acceptance review

Before Gate B:

1. Re-read current MOEX specification and official T0/T1 files.
2. Verify framing, serial arithmetic and A/B semantics against official evidence.
3. Review complete diff, tests, exact inventory and CI.
4. Prove that later packets cannot extend an active gap deadline.
5. Prove that no message after a confirmed gap can be emitted.
6. Prove bounded memory and zero post-initialization Gate A allocations.
7. Review Release benchmark distributions and variability.
8. Reject unmeasured abstraction or diagnostic overhead.
9. Obtain explicit Owner approval.

## 19. Open evidence

These items do not block specification approval but remain unresolved for production acceptance:

- production reorder timeout and capacity values;
- stream-specific initial sequence and `SequenceReset` handling;
- `.mxraw` A/B replay merge;
- Snapshot + buffered Incremental recovery;
- end-to-end allocation behaviour with RT-3;
- real production packet-path validation where still required.

## 20. Authorization boundary

This documentation change does not authorize:

```text
Ready-for-review
merge or auto-merge
force push or branch deletion
Gate B
Gate C
Gate D
RT-5 / RT-6 / CI-2
production or live trading
```
