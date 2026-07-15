# RT-4 Gate A Completion contract resolution

**Date:** 2026-07-15  
**Issue:** #51  
**PR:** #52  
**Status:** `SETUP_REVIEW_RESOLUTION — IMPLEMENTATION_BLOCKED`

This document resolves non-obvious implementation ambiguities found while reviewing the unified Gate A setup. It supplements `rt4_gate_a_completion_amendment.md` and is folded into the main RT-4 specification before final merge.

## 1. Storage geometry

Gate A uses caller-owned fixed memory:

```text
fixed slot metadata array
fixed byte arena
configured max_message_bytes
one dedicated contiguous payload slice per slot
```

Required validation:

```text
slot_capacity >= max_reorder_messages
max_message_bytes > 0
checked(slot_capacity * max_message_bytes) <= payload_arena.size()
0 < max_reorder_bytes <= checked(slot_capacity * max_message_bytes)
```

A variable-size free-list arena is prohibited. Dedicated slices avoid fragmentation, compaction and a second payload copy.

Lookup is O(1):

```text
slot_index = msg_seq_num % slot_capacity
```

The bounded forward window guarantees that two valid pending sequences cannot occupy the same index when `slot_capacity >= max_reorder_messages`. An occupied-index mismatch is `InternalInvariantViolation`.

`max_reorder_bytes` counts actual pending payload lengths. All checks complete before a slot is marked occupied. Failure never publishes a partial message.

Every accepted body, including an in-order body, must fit `max_message_bytes`; otherwise the result is `PendingByteCapacityExceeded`. This prevents configuration-dependent acceptance differences between in-order and reordered paths.

## 2. Time semantics

The two monotonic values have different meanings:

```text
FramedMessageView.capture_monotonic_ns
  immutable capture metadata retained for diagnostics and pending emission

event_monotonic_ns passed to on_message/on_time
  authoritative sequencer clock sampled at serialized Gate A entry
  or supplied by deterministic replay
```

They are not required to be equal. Independent A/B capture timestamps therefore cannot accidentally define sequencer event order.

## 3. API corrections

The final Gate A interface uses:

```cpp
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
```

Corrections to the older interface:

```text
reset() returns SequencerCode instead of void
on_time() in Running returns NoAction
on_time() in GapWait before deadline returns GapWaiting
```

Lifecycle rules:

```text
initialize: valid only from Uninitialized
invalid initialize: remain Uninitialized
start: valid only from Stopped
reset: invalid from Uninitialized
successful reset: clear pending, deadline and clock state; preserve feed/config/storage; enter Stopped
```

## 4. SequencerResult semantics

The existing three-field result remains compact and deterministic:

```text
on_message.observed_seq = input message sequence
on_message.expected_seq = next_expected at event entry
on_time.observed_seq     = next_expected at event entry
on_time.expected_seq     = next_expected at event entry
```

The post-event value is available from `next_expected_seq()`.

## 5. Event precedence

For Running and GapWait, processing order is fixed:

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

```text
ClockRegression wins over GapConfirmed
GapConfirmed wins over message contents at or after deadline
DeadlineOverflow is checked before copying the first future message
message-capacity failure wins over byte-capacity failure when both apply
```

Only successful A1 framing output is valid input. Invalid side, empty body or other A1-contract violation maps to `InternalInvariantViolation`.

## 6. Emission and failure guarantees

```text
sink callback is synchronous and statically verified noexcept
in-order payload remains borrowed
out-of-order payload is copied once into its dedicated slice
pending callback span is valid only during the callback
slot is released immediately after callback return
```

A terminal result enters `FailedClosed`. Pending bytes may remain for diagnostics, but no later call may emit them. Reset clears the pending state.

A late duplicate after prior emission is dropped by sequence number without retaining or comparing the previously emitted payload.

## 7. Required additional tests

```text
invalid storage multiplication and geometry
modulo slot lookup across uint32 wrap
occupied-index invariant failure
no partial slot publication after failed insertion
capture time different from sequencer event time
NoAction and GapWaiting timer results
reset transition matrix
result-field entry-state semantics
clock-regression precedence over deadline
body too large on both expected and future paths
```

Exact production filenames and CTest targets are frozen immediately before separate Owner implementation authorization in the existing Issue, branch and Draft PR.

No code, tests, CMake, workflow, MiMo or merge is authorized by this resolution.
