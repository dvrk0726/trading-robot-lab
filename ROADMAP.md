# Roadmap

Дата обновления: 2026-07-14
Статус: gated engineering roadmap
Текущий gate: RT-1 DONE; RT-2 DONE; RT-3 DONE; CI-1 DONE; QSH retirement DONE

## Главный порядок

```text
repository workflow and protection
-> local MOEX FAST metadata inspection
-> raw segment contract and deterministic replay
-> specialized MOEX T0/T1 FAST decoding
-> CI routing optimization (CI-1 DONE)
-> QSH retirement (DONE)
-> SPECTRA framing, sequencing and recovery
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

Accepted T0/T1 SHA-256 and profile: see PROJECT_STATE.md.

Accepted operators: field without operator, constant.

Excluded and fail-closed: default, copy, increment, delta, tail, generic dictionaries, references, generic groups outside T0/T1, byteVector, decimal component operators, historical-profile compatibility.

### CI-1 — DONE

```text
Required-check-preserving routing
Docs-only smoke PR #32
Main SHA: 0699a533a1ed44a9d47e05b049aca4061bebaac0
Post-merge CI #165: success
```

### QSH retirement — DONE

```text
Issue: #33 - QSH retirement record
PR: #34, merged
Main merge SHA: 7c05cfb979cd0144be508e41a6f3a6229bfab1cb
Post-merge CI: #175 / run 29361711016 — success, exactly 6 jobs

Historical implementation evidence:
  Stage 2A head: 0a39e7cd5ace38adce28d32f6eb1a325a9e1d1c2
  Stage 2A CI: #172 / run 29359345488 — success, exactly 6 jobs

The QSH/QScalp/OrdLog product support, old QSH L3/L2 book,
Trading Lab QSH integration, tombstone job, run_qsh and QSH routing
are retired and absent. They are not part of the future architecture.

The active Protect main ruleset (ID 18924726) requires exactly
six checks:
  - Repository hygiene
  - Python tests and contracts
  - C++ MOEX FAST Windows/MSVC (RT-1: 6, RT-3: 9)
  - C++ MOEX FAST Linux/GCC (RT-1: 6, RT-3: 9)
  - C++ MOEX RAW Windows/MSVC (18 tests)
  - C++ MOEX RAW Linux/GCC (18 tests)

*.qsh remains mentioned only as a raw-market-data safety ban.
```

## Current sequence

```text
RT-4 research/specification (next separate gate)
-> separately specified and explicitly Owner-authorized RT-4 implementation
```

CI-2 caching is POSTPONED, not started and not authorized. Reconsider
only when measured CI duration or cost materially slows development.

## RT-4 — not started, not authorized

Requires a separate specification and explicit Owner authorization. Not automatically authorized by prior completion. RT-4 remains not started and not authorized.

```text
MOEX 4-byte preamble and message boundary
SPECTRA packet/framing contract
A/B sequencing and deduplication
gap detection and recovery
Snapshot + buffered Incremental bootstrap
```

## Later stages

```text
RT-5 realtime data quality and normalized events
RT-6 ORDERS-LOG L3/L2 and storage
RT-7 T0 pilot and measured capacity
RT-8 research/backtest/paper on certified data
RT-9 FIX/TWIME test and VPTS readiness
RT-10 production certification and explicit owner gate
```

Future normalized events and new L3/L2 book (RT-5/RT-6) are designed
from official MOEX SPECTRA data; no automatic reuse of the old QSH code.
Names and scope of later stages remain provisional until the preceding gate supplies evidence.
