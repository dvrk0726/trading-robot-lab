# RT-4 Gate A Completion amendment

**Date:** 2026-07-15  
**Issue:** #51  
**Status:** `OWNER_AUTHORIZED_SETUP_ONLY — IMPLEMENTATION_BLOCKED`

## 1. Purpose

This amendment consolidates the former RT-4 implementation stages A3, A4 and A5 into one review and merge gate:

```text
RT-4 Gate A Completion
```

The final Gate A behavior defined by `docs/rt4_spectra_framing_sequencing_recovery_spec.md` is unchanged. Only the implementation packaging and review workflow change.

This amendment governs Gate A implementation when the older specification or roadmap describes A3, A4 and A5 as separate merge gates.

## 2. Verified prerequisite

```text
base main SHA: c35f37f07cfbb4a5f7ff44fb69d3782d02dc3917
RT-4 Gate A1 UDP framing: DONE
RT-4 Gate A2 serial arithmetic: DONE
MOEX FAST CTest inventory: 17
```

A1 supplies `FramedMessageView`. A2 supplies deterministic modulo-2^32 sequence classification. Gate A Completion builds the bounded mutable sequencer on those accepted primitives.

## 3. Unified production scope

Gate A Completion includes one coherent transport-control subsystem:

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

## 4. Stable state machine

```text
Uninitialized
  -> initialize() -> Stopped
Stopped
  -> start() -> Running
Running
  -> permitted future message -> GapWait
GapWait
  -> pending becomes empty -> Running
Running or GapWait
  -> fatal condition -> FailedClosed
FailedClosed
  -> reset() -> Stopped
```

`FailedClosed` is terminal until explicit reset and restart. No input after the transition may advance ordered state or emit a message.

## 5. Message processing

For each accepted `FramedMessageView`, the sequencer uses the existing A2 classifier.

```text
Expected:
  emit synchronously
  increment next_expected modulo 2^32
  flush newly contiguous pending messages

Stale:
  drop as duplicate or late message
  do not change ordered state

FutureWithinWindow:
  copy FAST body exactly once into preallocated storage
  retain required metadata
  enter or remain in GapWait
  do not emit later messages

FutureBeyondWindow or Ambiguous:
  transition to FailedClosed
```

If the same future sequence is already pending:

```text
same length and identical FAST-body bytes -> duplicate drop
different length or bytes                 -> DuplicatePayloadMismatch and FailedClosed
```

A and B are equal physical copies. Neither side is permanently primary.

## 6. Deadline contract

The first permitted future message in `Running` creates one gap episode:

```text
gap_start_ns = supplied monotonic time
gap_deadline_ns = checked(gap_start_ns + reorder_wait_ns)
state = GapWait
```

Later future packets, duplicates and partial hole resolution do not extend or restart that deadline. The episode ends only when pending storage becomes empty after contiguous emission.

For message and timer input:

```text
reject monotonic-time regression
check active deadline before processing the event
now >= gap_deadline_ns confirms the gap
an expected message stamped exactly at the deadline is too late
```

## 7. Fatal conditions

Gate A Completion handles all final Gate A terminal conditions in the same implementation:

```text
WrongLogicalFeed
DuplicatePayloadMismatch
ReorderDistanceExceeded
AmbiguousSequenceRelation
PendingMessageCapacityExceeded
PendingByteCapacityExceeded
ClockRegression
DeadlineOverflow
GapConfirmed
InternalInvariantViolation
```

Every terminal condition produces a stable result code and enters `FailedClosed`.

## 8. Ownership and hot-path policy

```text
single writer
no internal locks
borrowed payload on the in-order path
one copy into preallocated storage on the out-of-order path
synchronous noexcept sink callback
no retained callback span
zero heap allocation after successful initialization
```

Growing containers, associative-container insertion, `std::function`, exceptions, formatted logging and per-message strings are prohibited on the hot path without measured evidence and separate review.

## 9. Internal implementation phases

The work remains one Issue, one branch, one Draft PR and one final merge. Internal commits and MiMo runs are review checkpoints, not separate project gates.

### Phase 1 — bounded storage

```text
MessageStorage representation
fixed slots and fixed byte arena or slab
bounded deterministic lookup metadata
insert, compare, view, release and reset behavior
direct storage tests
```

No partial `DualFeedSequencer` is introduced in this phase.

### Phase 2 — complete sequencer

```text
full public state and result enums
initialize, start and reset
on_message and on_time
A/B arbitration
emission, duplicate drop and future buffering
contiguous flush
deadline and all fail-closed behavior
complete synthetic state-machine tests
```

The sequencer is introduced only when every valid sequence relation has final behavior.

### Phase 3 — acceptance evidence

```text
Release benchmarks on Windows and Linux
allocation count and allocated-byte evidence
latency distribution and variability
throughput and storage high-water mark
final documentation synchronization
Architecture Review and Owner acceptance
```

No invented nanosecond threshold or flaky latency CI gate is allowed.

## 10. Test families

The final Gate A PR must cover at least:

```text
A/B first-copy order and alternating winning side
stale duplicate after emission
wrong logical feed
future duplicate with equal bytes
future duplicate with different bytes
1,3,2 and deeper contiguous flush vectors
natural uint32 wrap
message-count and byte-capacity limits
fixed deadline that later traffic cannot extend
arrival exactly at the deadline
clock regression and deadline overflow
no emission after FailedClosed
explicit reset and deterministic restart
repeatable byte-identical event log
zero post-initialization allocations
Windows/MSVC and Linux/GCC Release execution
```

Exact CTest target names and final inventory are frozen before implementation authorization and updated once in CMake and CI inside this PR.

## 11. Excluded scope

```text
socket or multicast receiver
raw capture
TCP Historical Replay
RT-2 .mxraw integration
RT-3 decode integration
external-preamble AutoVerify
stream-specific initial sequence policy
FIX SequenceReset policy
Snapshot recovery
normalized events
L3/L2 order book
RT-5, RT-6 or CI-2
```

These remain in later Gate B, Gate C or roadmap stages.

## 12. Workflow and documentation policy

```text
one Issue: #51
one branch: mimo/issue-51-rt4-gate-a-completion
one Draft PR
multiple small reviewed commits
one final Owner-authorized merge
```

The existing RT-4 specification, `AI_CONTEXT.md`, `PROJECT_STATE.md` and `ROADMAP.md` are updated once before final merge. There is no separate setup PR and no post-merge state-sync PR.

Post-merge CI evidence is recorded in the Issue/PR timeline. Stable state documents do not require another merge solely to record the new main SHA or CI run number.

## 13. Authorization boundary

This setup amendment does not authorize implementation, tests, CMake, workflow, benchmarks, MiMo, merge, auto-merge, force-push, branch deletion, Gate B, Gate C, RT-5 or RT-6.

Implementation starts only after setup diff review, docs-only CI success and separate explicit Owner authorization.
