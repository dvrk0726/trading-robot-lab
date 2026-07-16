# Project State

Дата обновления: 2026-07-16  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-4 Gate A Completion IMPLEMENTED_IN_DRAFT_PR; Architecture Review PENDING

## Архитектурные границы

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry требует VPTS/certification и решения Owner.
Secrets, private connection data and raw market data не хранятся в Git.
```

## Performance-first development policy

```text
C++20 owns every latency-critical realtime/hot-path component:
  MOEX packet/framing, sequencing and recovery, FAST decoding,
  normalized realtime market-data events, future L3/L2 books,
  realtime state, RiskEngine, OrderManager and future runtime
  execution components.
Python is not used in the hot path; it is used for research,
  analysis, reports, UI, orchestration and offline tooling.
Correctness, deterministic behavior and fail-closed validation
  are mandatory and cannot be sacrificed for speed.
Performance claims require measured Release benchmarks.
Avoid speculative abstractions, compatibility layers and
  unnecessary allocations in the critical path.
```

## Завершённые этапы

### RT-1 — DONE

```text
Issue #14
Implementation PR #16
Merge commit: ab74f560c1bcf9d09ae7bdfb8552c745928fd022
Offline MOEX configuration/templates inspector
Windows/Linux Release tests
```

### RT-2 — DONE

```text
Issue #18
Specification PR #19
Implementation PR #20
Merge commit: 060371112d921c1c1f4055cfbdb99049bdf8a2af
.mxraw v1 raw segments and deterministic replay
Owner local Release acceptance passed
```

RT-2 does not decode FAST and does not infer exchange sequence from `capture_index`.

### RT-3 — DONE

```text
Issue #21: closed completed
Implementation PR #23: merged
Final reviewed PR head: a1443d3f909151d327b83042e43c1cc4c04cc732
Merge commit on main: 377618c360c165d88dde4cfe0cee87f8747cba03
Pre-merge CI #156: success
Post-merge main CI #157: success
Owner-local Windows Release acceptance: inventory 15/15, PASS
```

Accepted implementation: specialized MOEX SPECTRA T0/T1 decoder, not a general-purpose FAST 1.1 engine.

Historical accepted RT-3 compile/test profiles:

```text
T0 SHA-256:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Current official endpoint contents are tracked separately by RT-4.

Required RT-3 scope:

```text
one bounded FAST message body
template ID and previous-template-ID reuse
presence maps
ordinary/nullable integer primitives
ASCII and Unicode strings
exact decimal
field without operator
constant
sequences and single length instruction
limits, reset, deterministic errors and transactional rollback
T0/T1 official XML compilation
Windows/Linux Release tests
```

Excluded and fail-closed:

```text
default/copy/increment/delta/tail
generic field dictionaries/scopes/keys
user-defined dictionaries
typeRef/templateRef/groupRef
reference resolution/cycles
generic groups outside accepted inventory
byteVector
decimal component operators
historical profile compatibility
```

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
Issue #36: closed completed
PR #37: merged
Main merge SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
CI-2 caching: POSTPONED
RT-4 selected as next gate
```

### RT-4 specification — DONE

```text
Issue #38: closed completed
PR #39: merged
Final reviewed PR head: afd128a49584fce1131323ac7b19e5b5d7b1997a
Main merge SHA: 136293ede211619b7d9198d85ed3afb0f2577514
Post-merge main CI #189: success
```

Authoritative files:

```text
docs/rt4_spectra_framing_sequencing_recovery_spec.md
docs/rt4_moex_fast_source_update_2026-07-15.md
```

Approved architecture structure:

```text
Gate A: UDP framing, A/B sequencing, bounded reordering,
        gap detection, explicit monotonic timeout and fail-closed
Gate B: RT-2 .mxraw + RT-3 integration, tag-34 verification,
        one-time preamble endian AutoVerify
Gate C: Snapshot + buffered Incremental recovery
Gate D: Windows/Linux Release performance and production evidence
```

Current official-source evidence:

```text
MOEX SPECTRA FAST document: v1.30.2, 2026-04-10
T0 configuration SHA-256:
AE80702BC3E179CAF5DA025E94FDAC6AC7A6A4FF1353E7FB5D0396DE987C4118
Current T0 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Current T1 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
External UDP preamble byte order: little-endian (written MOEX support confirmation, 2026-07-16)
```

The 2026-07-11 RT-3 source audit remains historical evidence. The 2026-07-15 endpoint update is recorded separately by RT-4.

Gate A uses fixed little-endian decoding with no runtime byte-order selector. Gate B consumes the fixed little-endian external value from A1 and compares numerically against decoded tag 34.

### RT-4 post-merge state sync — DONE

```text
Issue #40: closed completed
PR #41: merged
Final reviewed PR head: 6789fb3621d70465114a32d2b146562e7f6809e8
Main merge SHA: acb74763e7dd395f210ac738c425c7d544a6cb51
Post-merge main CI #194: success
```

### RT-4 Gate A1 UDP framing — DONE

```text
Issue #42: closed completed
PR #43: merged
Final reviewed PR head: fc8c42bcd34ed65851267e9fefbc379d7206d2ca
Main merge SHA: ebfb3096b8a62704e5bf57a77d7971fd36acef2a
Pre-merge CI #199: success
Post-merge main CI #200: success
MOEX FAST inventory: 16 = RT-1 6 + RT-3 9 + RT-4 A1 1
```

Accepted A1 contract:

```text
one current UDP datagram
4-byte external MsgSeqNum preamble
explicit LittleEndian or BigEndian; no default
exactly one borrowed FAST body beginning at byte 4
bounded validation and stable FrameCode result
no payload copy and no heap allocation in production framing code
one Release-active framing CTest
required-check job names unchanged
```

Deterministic framing error precedence:

```text
invalid limits or invalid byte-order enum -> InvalidConfig
payload size 0..3                       -> DatagramTooShort
payload size 4                          -> EmptyFastBody
payload size > max_datagram_bytes       -> DatagramTooLarge
payload size 5..max                     -> Ok
```

A1 excludes A/B sequencing, serial arithmetic, duplicate suppression, reorder storage, gap deadlines, sockets, `.mxraw`, RT-3 integration, AutoVerify, SequenceReset, Snapshot recovery and benchmarks.

### RT-4 A2/A3 implementation-stage amendment — DONE

```text
Issue #46: closed completed
PR #47: merged
Final reviewed PR head: 7f42d2d14e4f54ad184e64688870030ce11b5f92
Main merge SHA: eb1e851bc685b8abefa61c4dbb0c5fc4de8f46a9
Pre-merge CI #209: success
Post-merge main CI #210: success
```

Accepted stage boundary:

```text
A2: stateless uint32 serial-relation classification only
A3: fixed preallocated MessageStorage plus complete mutable A/B sequencer
A4: fixed deadline and terminal fail-closed behavior
```

### RT-4 Gate A2 deterministic uint32 serial arithmetic — DONE

Verified checkpoint:

```text
Issue #48: closed completed
PR #49: merged
Final reviewed PR head: 8ed659ffffbf42fd0671935d53182622289b4ec6
Main merge SHA: d026a13245ea4f92ea1fe46edf049df205f981ea
Pre-merge CI #216: success
Post-merge main CI #217: success
MOEX FAST inventory: 18 = RT-1 6 + RT-3 9 + RT-4 A1 1 + RT-4 A2 1 + RT-4 Gate A 1
```

Accepted public API:

```cpp
enum class SequenceRelation : std::uint8_t {
    Expected,
    FutureWithinWindow,
    FutureBeyondWindow,
    Ambiguous,
    Stale,
    InvalidConfig
};

struct SequenceClassification {
    SequenceRelation relation{SequenceRelation::InvalidConfig};
    std::uint32_t delta{};
};

[[nodiscard]] constexpr SequenceClassification classify_sequence_relation(
    std::uint32_t observed,
    std::uint32_t next_expected,
    std::uint32_t max_reorder_messages
) noexcept;
```

Accepted implementation files:

```text
cpp/moex_fast/include/moex_fast/spectra_sequence_arithmetic.hpp
cpp/moex_fast/tests/test_spectra_sequence_arithmetic.cpp
cpp/moex_fast/CMakeLists.txt
.github/workflows/ci.yml
```

The primitive is header-only. No production `.cpp` is permitted. It performs one unsigned modulo subtraction and deterministic classification only. Invalid configuration returns `InvalidConfig` with `delta = 0`.

Test contract:

```text
one Release-active test_spectra_sequence_arithmetic
invalid max: 0, 0x80000000, UINT32_MAX
Expected, bounded future, beyond-window, ambiguous and stale vectors
natural modulo-2^32 wrap vectors
constexpr, noexcept and trivially-copyable checks
MOEX FAST inventory: 18
required-check job names unchanged
```

Explicitly excluded from A2:

```text
DualFeedSequencer
mutable logical-feed state
LogicalFeedId, FeedSide, payload or metadata access
A/B arbitration
MessageStorage or pending slots
emission, drop actions or sink
future buffering or contiguous flush
gap deadline, clocks or FailedClosed
SequenceReset
.mxraw or RT-3 integration
benchmarks
tools/ci_route.py changes
temporary FutureUnsupported-style API or result
```

### RT-4 Gate A Completion — IMPLEMENTED_IN_DRAFT_PR

```text
Issue #51: open
PR #52: open, Draft, not merged
Branch: mimo/issue-51-rt4-gate-a-completion
Technical implementation head: 40fb4de9d8355bb4b019d29a0479178f2128955f
Current main: c35f37f07cfbb4a5f7ff44fb69d3782d02dc3917
Latest verified technical CI: #231, run ID 29499974934, success
MOEX FAST inventory: 18 = RT-1 6 + RT-3 9 + RT-4 A1 1 + RT-4 A2 1 + RT-4 Gate A 1
```

Accepted Gate A implementation evidence in Draft PR #52:

```text
fixed little-endian UDP preamble framing
written MOEX support confirmation: value 1 is 01 00 00 00, same rule for T0/T1/production, numeric preamble equals decoded tag 34
A2 modulo-2^32 sequence classifier
fixed caller-owned MessageStorage
complete A/B DualFeedSequencer
bounded reordering
fixed non-extendable gap deadline
deterministic fail-closed behavior
93 internal Gate A test cases
eight Release benchmark scenarios
allocation_count equals zero in every measured scenario
benchmark executed successfully in both Windows and Linux FAST CI jobs
```

Status: IMPLEMENTED_IN_DRAFT_PR, FINAL_ARCHITECTURE_REVIEW_PENDING, READY_NOT_AUTHORIZED, MERGE_NOT_AUTHORIZED.

Former internal phases A1, A2, A3, A4 and A5 are consolidated into Gate A Completion. A1 and A2 remain as historical verified checkpoints. The A1 production byte-order contract was later amended in PR #52 based on written MOEX support confirmation.

Current verified boundary:

```text
RT-4 Gate A1: DONE
RT-4 Gate A2: DONE
RT-4 Gate A Completion: IMPLEMENTED_IN_DRAFT_PR — Architecture Review pending
Gate B: BLOCKED
Gate C: BLOCKED
Gate D: BLOCKED
RT-5 / RT-6 / CI-2: not authorized
```

Next transition: final Architecture Review of complete PR #52 -> separate Owner authorization to mark Ready -> separate Owner authorization to merge -> post-merge CI verification -> only then a separate Gate B decision.

## MOEX access and connectivity state

```text
MOEX support confirmed FAST access activation for the registered external static IPv4.
Official Windows VPN instruction: PPTP; data encryption optional.
Current home external IPv4 matches the registered address.
Windows VPN: remote-access error 807.
TCP 1723 to supplied VPN endpoint: unreachable from registered home connection.
Windows Firewall default outbound policy: Allow.
Explicit enabled outbound block rule targeting endpoint: not found.
Registered antivirus: Windows Defender only.
Third-party antivirus/network filter: not found.
MOEX support follow-up: pending.
```

The VPN endpoint, external/private IP addresses, credentials, VPN profiles and raw/decoded market-data packets are not stored.

This connectivity state does not authorize production acceptance. Preamble byte order is resolved independently by written MOEX support confirmation (little-endian).

CI-2 caching is POSTPONED, not started and not authorized.

Future normalized events and the new L3/L2 book are designed from official MOEX SPECTRA data; no automatic reuse of the old QSH code.
