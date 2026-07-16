# Roadmap

Дата обновления: 2026-07-16  
Статус: gated engineering roadmap  
Текущий gate: RT-4 Gate A Completion DONE

## Главный порядок

```text
repository workflow and protection
-> local MOEX FAST metadata inspection
-> raw segment contract and deterministic replay
-> specialized MOEX T0/T1 FAST decoding
-> CI routing optimization
-> QSH retirement
-> RT-4 SPECTRA framing, sequencing and recovery
-> realtime data quality and books
-> research/backtest/paper
-> VPTS/certification
-> Owner-approved production only later
```

Следующий этап не начинается до закрытия текущего gate.

## Завершено

### Workflow foundation — DONE

```text
branch-only implementation
Pull Request review
CI baseline
no MiMo merge or auto-merge
procedural main protection
scope-freeze protocol
```

### RT-1 — DONE

```text
local configuration.xml/templates.xml inspector
normalized metadata and provenance
Windows/Linux Release tests
```

### RT-2 — DONE

```text
.mxraw v1 raw segment contract
synthetic capture/inspect/replay
bounded validation
CRC32C and SHA-256 provenance
deterministic replay
Windows/Linux Release tests
```

### RT-3 — DONE

```text
Specialized MOEX SPECTRA T0/T1 decoder
Issue #21: closed completed
PR #23: merged
Final reviewed PR head: a1443d3f909151d327b83042e43c1cc4c04cc732
Merge commit: 377618c360c165d88dde4cfe0cee87f8747cba03
Pre-merge CI #156: success
Post-merge CI #157: success
Owner-local Windows Release acceptance: inventory 15/15, PASS
```

Accepted T0/T1 implementation hashes and profile are recorded in `PROJECT_STATE.md`.

Accepted operators: field without operator, constant.

Excluded and fail-closed: default, copy, increment, delta, tail, generic dictionaries, references, generic groups outside accepted T0/T1 inventory, byteVector, decimal component operators and historical-profile compatibility.

### CI-1 — DONE

```text
Required-check-preserving routing
Docs-only smoke PR #32
Main SHA: 0699a533a1ed44a9d47e05b049aca4061bebaac0
Post-merge CI #165: success
```

### QSH retirement — DONE

```text
Issue #33
PR #34 merged
Main merge SHA: 7c05cfb979cd0144be508e41a6f3a6229bfab1cb
Post-merge CI #175: success
```

QSH/QScalp/OrdLog product support, old QSH L3/L2 book, Trading Lab QSH integration, tombstone job, `run_qsh` and QSH routing are retired and absent. `*.qsh` remains only a raw-market-data safety ban.

### Performance-first documentation — DONE

```text
Issue #36
PR #37 merged
Main merge SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
CI-2 caching postponed until a measured bottleneck
RT-4 selected as the next gate
```

### RT-4 specification — DONE

```text
Issue #38: closed completed
PR #39: merged
Reviewed PR head: afd128a49584fce1131323ac7b19e5b5d7b1997a
Main merge SHA: 136293ede211619b7d9198d85ed3afb0f2577514
Post-merge main CI #189: success
```

Authoritative documents:

```text
docs/rt4_spectra_framing_sequencing_recovery_spec.md
docs/rt4_moex_fast_source_update_2026-07-15.md
```

### RT-4 post-merge state sync — DONE

```text
Issue #40: closed completed
PR #41: merged
Reviewed PR head: 6789fb3621d70465114a32d2b146562e7f6809e8
Main merge SHA: acb74763e7dd395f210ac738c425c7d544a6cb51
Post-merge main CI #194: success
```

### RT-4 Gate A1 UDP framing — DONE

```text
Issue #42: closed completed
PR #43: merged
Reviewed PR head: fc8c42bcd34ed65851267e9fefbc379d7206d2ca
Main merge SHA: ebfb3096b8a62704e5bf57a77d7971fd36acef2a
Pre-merge CI #199: success
Post-merge main CI #200: success
MOEX FAST inventory: 16 = RT-1 6 + RT-3 9 + RT-4 A1 1
```
### RT-4 A2/A3 implementation-stage amendment — DONE

```text
Issue #46: closed completed
PR #47: merged
Reviewed PR head: 7f42d2d14e4f54ad184e64688870030ce11b5f92
Main merge SHA: eb1e851bc685b8abefa61c4dbb0c5fc4de8f46a9
Pre-merge CI #209: success
Post-merge main CI #210: success
```

```text
A2: stateless uint32 serial-relation classification only
A3: fixed preallocated MessageStorage plus complete mutable A/B sequencer
A4: fixed deadline and terminal fail-closed behavior
```

### RT-4 Gate A Completion — DONE

```text
Issue #51: open only until merge PR #53 and post-merge CI verification
PR #52: merged
Final reviewed PR head: f88cb16b26d08b23718214f02d2df5f383c8b52e
Gate A merge commit: 155a8d12f62b461e8a7f5daf1b0d20a654a70f69
Pre-merge CI #237, run 29530159266, success, 6 jobs
Post-merge CI #238 on main: success
Final Architecture Review: PASS
MOEX FAST inventory: 18
Gate A internal tests: 98
Release benchmark scenarios: 8
allocation_count: 0 in every measured scenario
```

## MOEX connectivity checkpoint

```text
FAST access for the registered external static IPv4: confirmed by MOEX support
Official Windows VPN parameters: PPTP; data encryption optional
Current home external IPv4: matches registered address
Windows VPN: error 807
TCP 1723 to supplied endpoint: unreachable from registered connection
Windows Firewall default outbound policy: Allow
Endpoint-specific enabled outbound block: not found
Third-party antivirus/network filter: not found
MOEX support follow-up: pending
```

Private connection addresses, credentials, VPN profiles and real raw/decoded market-data packets are prohibited in Git.

The connectivity blocker does not invalidate synthetic A1 acceptance. Preamble byte order is resolved by written MOEX support confirmation (little-endian, 2026-07-16).

## RT-4 implementation gates

### Gate A1 — UDP framing — DONE

```text
4-byte external MsgSeqNum preamble
one current UDP datagram and exactly one FAST body
fixed little-endian decoding; no runtime endian selector
host-endian-independent uint32 conversion
borrowed FAST-body span beginning at byte 4
bounded deterministic validation
stable FrameCode and deterministic empty output on failure
no payload copy or heap allocation in production framing code
one synthetic Windows/Linux Release CTest
MOEX FAST test inventory 16 total
required-check job names unchanged
```

Deterministic error precedence:

```text
invalid limits                          -> InvalidConfig
payload size 0..3                       -> DatagramTooShort
payload size 4                          -> EmptyFastBody
payload size > max_datagram_bytes       -> DatagramTooLarge
payload size 5..max                     -> Ok
```

Excluded from A1:

```text
A/B sequencing and duplicate suppression
serial arithmetic
bounded reordering
gap deadline and FailedClosed state
sockets and multicast
.mxraw integration
RT-3 decode integration
SequenceReset
Snapshot recovery
Release benchmark claims
```

Accepted implementation files:

```text
cpp/moex_fast/include/moex_fast/spectra_udp_framing.hpp
cpp/moex_fast/src/spectra_udp_framing.cpp
cpp/moex_fast/tests/test_spectra_udp_framing.cpp
cpp/moex_fast/CMakeLists.txt
.github/workflows/ci.yml
```

### Gate A2 — deterministic uint32 serial arithmetic — DONE

Verified checkpoint:

```text
Issue #48: closed completed
PR #49: merged
Reviewed PR head: 8ed659ffffbf42fd0671935d53182622289b4ec6
Main merge SHA: d026a13245ea4f92ea1fe46edf049df205f981ea
Pre-merge CI #216: success
Post-merge main CI #217: success
MOEX FAST inventory: 17 total
```

Accepted API:

```text
header-only spectra_sequence_arithmetic.hpp
SequenceRelation:
  Expected
  FutureWithinWindow
  FutureBeyondWindow
  Ambiguous
  Stale
  InvalidConfig
SequenceClassification:
  relation
  uint32 delta
classify_sequence_relation(observed, next_expected, max_reorder_messages)
constexpr and noexcept
```

Deterministic classification:

```text
max == 0 or max >= 0x80000000 -> InvalidConfig, delta 0
delta == 0                     -> Expected
1 <= delta <= max              -> FutureWithinWindow
max < delta < 0x80000000       -> FutureBeyondWindow
delta == 0x80000000            -> Ambiguous
delta > 0x80000000             -> Stale
```

Accepted implementation files:

```text
cpp/moex_fast/include/moex_fast/spectra_sequence_arithmetic.hpp
cpp/moex_fast/tests/test_spectra_sequence_arithmetic.cpp
cpp/moex_fast/CMakeLists.txt
.github/workflows/ci.yml
```

Accepted test contract:

```text
one Release-active test_spectra_sequence_arithmetic
invalid zero, half-range and UINT32_MAX configuration
exact expected
future at one and exact configured maximum
future beyond maximum
exact half-range ambiguity
stale relation
natural modulo-2^32 wrap
constexpr, noexcept and trivially-copyable checks
MOEX FAST inventory: 17 total
required-check job names unchanged
```

A2 explicitly excludes:

```text
DualFeedSequencer
mutable logical-feed state
A/B arbitration
MessageStorage or pending slots
payload or metadata access
in-order emission
stale duplicate action
future buffering
pending duplicate byte comparison
contiguous flush
gap deadline
FailedClosed transition
SequenceReset
.mxraw or RT-3 integration
benchmarks
tools/ci_route.py changes
temporary FutureUnsupported-style API or result
```

### Gate A Completion — DONE

Formerly tracked as A1, A2, A3, A4 and A5. These remain as historical labels for internal implementation phases only. A1 and A2 are verified historical checkpoints. A3/A4/A5 implementation and acceptance-evidence phases are complete.

```text
Issue #51: open only until merge PR #53 and post-merge CI verification
PR #52: merged
Final reviewed PR head: f88cb16b26d08b23718214f02d2df5f383c8b52e
Gate A merge commit: 155a8d12f62b461e8a7f5daf1b0d20a654a70f69
Pre-merge CI #237, run 29530159266, success, 6 jobs
Post-merge CI #238 on main: success
Final Architecture Review: PASS
MOEX FAST inventory: 18 = RT-1 6 + RT-3 9 + RT-4 A1 1 + RT-4 A2 1 + RT-4 Gate A 1
Gate A internal tests: 98
Release benchmark scenarios: 8
allocation_count: 0 in every measured scenario
```

Verified Gate A implementation evidence:

```text
fixed little-endian UDP preamble framing
written MOEX support confirmation: value 1 is 01 00 00 00, same rule for T0/T1/production
A2 modulo-2^32 sequence classifier
fixed caller-owned MessageStorage
complete A/B DualFeedSequencer
bounded reordering
fixed non-extendable gap deadline
deterministic fail-closed behavior
98 internal Gate A test cases
eight Release benchmark scenarios
allocation_count equals zero in every measured scenario
benchmark executed successfully in both Windows and Linux FAST CI jobs
```

PR #43 originally introduced an explicit endian selector, but this production contract was superseded in PR #52 after written MOEX support confirmation; current contract is fixed little-endian.

### Gate B — replay and decoder integration — BLOCKED

```text
RT-2 .mxraw A/B replay merge
RT-3 exact-body integration
compare fixed little-endian external preamble with decoded tag 34
stream initialization and SequenceReset policy
```

### Gate C — Snapshot recovery — BLOCKED

```text
queue Incremental while recovering
apply complete Snapshot cycle and LastFragment rules
replay contiguous queued Incremental messages
fail closed on ambiguity, overflow or unresolved second gap
```

### Gate D — Release acceptance — BLOCKED

```text
Windows/MSVC and Linux/GCC end-to-end Release evidence
latency distribution, throughput, allocations and memory behaviour
real T0/T1 packet, official vector or written MOEX confirmation
explicit Owner acceptance
```

## Current verified boundary

```text
Gate A: DONE
Gate B: BLOCKED — not started, not authorized, requiring a separate Owner decision
Gate C: BLOCKED
Gate D: BLOCKED
RT-5 / RT-6 / CI-2: not authorized
```

Gate B is not active and not started. A separate Owner decision is required before Gate B can begin.

CI-2 caching is POSTPONED, not started and not authorized. Reconsider only when measured CI duration or cost materially slows development.

## Later stages

```text
RT-5 realtime data quality and normalized events
RT-6 ORDERS-LOG L3/L2 and storage
RT-7 T0 pilot and measured capacity
RT-8 research/backtest/paper on certified data
RT-9 FIX/TWIME test and VPTS readiness
RT-10 production certification and explicit Owner gate
```

RT-5 is prohibited until RT-4 is accepted, merged by the Owner and verified by post-merge CI.

Future normalized events and the new L3/L2 book are designed from official MOEX SPECTRA data; no automatic reuse of the old QSH code. Names and scope of later stages remain provisional until the preceding gate supplies evidence.
