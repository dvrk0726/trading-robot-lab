# Roadmap

Дата обновления: 2026-07-15  
Статус: gated engineering roadmap  
Текущий gate: RT-4 Gate A1 post-merge state sync — Issue #44 / Draft PR #45

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

The connectivity blocker does not invalidate synthetic A1 acceptance. Production endian acceptance still requires live T0/T1 evidence, an official vector or written MOEX confirmation.

## Current gate — RT-4 A1 post-merge state sync

```text
Issue #44: open
Draft PR #45: open
Branch: docs/issue-44-rt4-a1-post-merge-state-sync
Base main SHA: ebfb3096b8a62704e5bf57a77d7971fd36acef2a
Current scope: exactly AI_CONTEXT.md, PROJECT_STATE.md and ROADMAP.md
Code, tests, CMake and workflow changes: prohibited
MiMo: prohibited
Merge: not authorized
A2: not started and not authorized
```

Required sequence:

```text
complete exact three-file state synchronization
-> docs-only CI
-> Architecture Review
-> separate Owner merge authorization
-> post-merge main CI verification
-> close Issue #44 completed
```

## RT-4 implementation gates

### Gate A1 — UDP framing — DONE

```text
4-byte external MsgSeqNum preamble
one current UDP datagram and exactly one FAST body
explicit LittleEndian or BigEndian; no default guess
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
invalid limits or invalid byte-order enum -> InvalidConfig
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
preamble AutoVerify
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

### Gate A2 — A/B sequencing and duplicate suppression — BLOCKED

```text
uint32 serial arithmetic with fail-closed ambiguity handling
A/B logical-feed arbitration
duplicate suppression
in-order emission
```

A2 requires a separate current-state review, bounded plan, new Issue/branch/PR and explicit Owner authorization.

### Gate A3 — bounded reordering — BLOCKED

```text
fixed pending storage
bounded message and byte capacity
out-of-order insertion
contiguous flush
```

### Gate A4 — gap deadline and fail-closed — BLOCKED

```text
fixed monotonic non-extendable deadline
gap confirmation
terminal FailedClosed state
explicit reset/restart only
```

### Gate A5 — Release benchmarks and Gate A review — BLOCKED

```text
Windows/Linux Release measurements
allocation evidence
latency distribution and variability
Owner acceptance before Gate B
```

### Gate B — replay and decoder integration — BLOCKED

```text
RT-2 .mxraw A/B replay merge
RT-3 exact-body integration
compare external preamble with decoded tag 34
one-time endian AutoVerify and per-feed lock
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

No gate begins automatically. Each implementation stage requires an existing Issue, feature branch and PR, tests, commit, push, CI, Architecture Review and Owner approval.

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