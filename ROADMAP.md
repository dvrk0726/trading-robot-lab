# Roadmap

Дата обновления: 2026-07-15
Статус: gated engineering roadmap
Текущий gate: RT-4 research/specification — Issue #38, Draft PR #39

## Главный порядок

```text
repository workflow and protection
-> local MOEX FAST metadata inspection
-> raw segment contract and deterministic replay
-> specialized MOEX T0/T1 FAST decoding
-> CI routing optimization (CI-1 DONE)
-> QSH retirement (DONE)
-> RT-4 SPECTRA framing, sequencing and recovery
-> realtime data quality and books
-> research/backtest/paper
-> VPTS/certification
-> owner-approved production only later
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
Issue #21: closed, completed
PR #23: merged
Final reviewed PR head: a1443d3f909151d327b83042e43c1cc4c04cc732
Merge commit on main: 377618c360c165d88dde4cfe0cee87f8747cba03
Pre-merge CI #156: success, all 7 jobs passed
Post-merge main CI #157: success, all 7 jobs passed
Owner-local Windows Release acceptance: inventory 15/15, PASS
```

Accepted T0/T1 implementation SHA-256 and profile: see `PROJECT_STATE.md`.

Accepted operators: field without operator, constant.

Excluded and fail-closed: default, copy, increment, delta, tail, generic dictionaries, references, generic groups outside T0/T1, byteVector, decimal component operators and historical-profile compatibility.

### CI-1 — DONE

```text
Required-check-preserving routing
Docs-only smoke PR #32
Main SHA: 0699a533a1ed44a9d47e05b049aca4061bebaac0
Post-merge CI #165: success
```

### QSH retirement — DONE

```text
Issue: #33
PR: #34, merged
Main merge SHA: 7c05cfb979cd0144be508e41a6f3a6229bfab1cb
Post-merge CI: #175 / run 29361711016 — success, exactly 6 jobs

The QSH/QScalp/OrdLog product support, old QSH L3/L2 book,
Trading Lab QSH integration, tombstone job, run_qsh and QSH routing
are retired and absent.

*.qsh remains only a raw-market-data safety ban.
```

### Performance-first documentation — DONE

```text
Issue #36
PR #37, merged
Main merge SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
CI-2 caching postponed until a measured bottleneck
RT-4 research/specification selected as the next gate
```

## Current sequence

```text
Issue #38 / Draft PR #39: RT-4 architecture and source evidence
-> docs-only CI
-> Architecture Review
-> separate Owner merge authorization
-> post-merge main CI verification
-> separate Owner authorization for Gate A implementation
```

CI-2 caching is POSTPONED, not started and not authorized. Reconsider only when measured CI duration or cost materially slows development.

## RT-4 — specification current; implementation not authorized

```text
Issue: #38 open
Draft PR: #39 open
Branch: docs/issue-38-rt4-spec
Base main SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
Scope: documentation only
MiMo: not authorized
Implementation: not started and not authorized
Merge: not authorized
```

### Gate A — framing, sequencing and gaps

```text
4-byte UDP preamble and one current FAST body
explicit LittleEndian or BigEndian; no default guess
A/B logical-feed arbitration and duplicate suppression
bounded reordering and explicit monotonic timeout
gap confirmation and terminal fail-closed state
fixed bounded storage and zero post-init allocations
synthetic Windows/Linux Release tests
Release benchmark and allocation evidence
```

### Gate B — replay and decoder integration

```text
RT-2 .mxraw A/B replay merge
RT-3 exact-body integration
compare external preamble with decoded tag 34
one-time endian AutoVerify and per-feed lock
stream initialization and SequenceReset policy
```

### Gate C — Snapshot recovery

```text
queue Incremental while recovering
apply complete Snapshot cycle and LastFragment rules
replay contiguous queued Incremental messages
fail closed on ambiguity, overflow or unresolved second gap
```

### Gate D — Release acceptance

```text
Windows/MSVC and Linux/GCC end-to-end Release evidence
latency distribution, throughput, allocations and memory behaviour
real T0/T1 packet, official vector or written MOEX confirmation
explicit Owner acceptance
```

No gate begins automatically. Each requires implementation, tests, commit, push, CI, Architecture Review and separate Owner approval.

Current official-source evidence is recorded in:

```text
docs/rt4_moex_fast_source_update_2026-07-15.md
```

The external preamble byte order remains unresolved in official text. The current design supports both explicit byte orders and defers one-time tag-34 verification to Gate B.

## Later stages

```text
RT-5 realtime data quality and normalized events
RT-6 ORDERS-LOG L3/L2 and storage
RT-7 T0 pilot and measured capacity
RT-8 research/backtest/paper on certified data
RT-9 FIX/TWIME test and VPTS readiness
RT-10 production certification and explicit owner gate
```

RT-5 is prohibited until RT-4 is accepted, merged by the Owner and verified by post-merge CI.

Future normalized events and new L3/L2 book are designed from official MOEX SPECTRA data; no automatic reuse of the old QSH code. Names and scope of later stages remain provisional until the preceding gate supplies evidence.
