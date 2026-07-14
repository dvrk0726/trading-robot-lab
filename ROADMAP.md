# Roadmap

Дата обновления: 2026-07-14
Статус: gated engineering roadmap
Текущий gate: RT-1 DONE; RT-2 DONE; RT-3 DONE; CI-1 DONE; QSH retirement Stage 1 in progress

## Главный порядок

```text
repository workflow and protection
-> local MOEX FAST metadata inspection
-> raw segment contract and deterministic replay
-> specialized MOEX T0/T1 FAST decoding
-> CI routing optimization (CI-1 DONE)
-> QSH retirement (Stage 1 in progress)
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

### QSH retirement — in progress

```text
Issue: #33
Draft PR: #34, branch mimo/issue-33-qsh-retirement-stage1
Stage 1 in progress. Not merged. Not complete.

The QSH/QScalp/OrdLog implementation, old QSH L3/L2 book,
Trading Lab QSH integration and archive QSH documents are being
retired. They are not part of the future architecture.

The exact check name C++ QSH M10X regression (20 tests) remains
only as a temporary tombstone because the main ruleset still
requires seven checks. Stage 2 may remove the tombstone only after
the Owner updates the main ruleset from seven to six.

.qsh bans remain only as raw market-data safety barriers.
```

## Current sequence

```text
finish QSH retirement (Stage 1)
-> synchronize, accept, merge and post-merge verify
-> CI-2 caching
-> separately specified and authorized RT-4
```

## RT-4 — not started, not authorized

Requires a separate specification and explicit Owner authorization. Not automatically authorized by prior completion.

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

Future normalized events and order book (RT-5/RT-6) must be designed
from official MOEX SPECTRA data; no automatic reuse of the old QSH book.
Names and scope of later stages remain provisional until the preceding gate supplies evidence.
