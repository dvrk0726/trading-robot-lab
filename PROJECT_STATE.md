# Project State

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-2 specification review — Issue #18 / PR #19

## Архитектура

```text
Trading Lab      — research, data quality, replay, backtest, reports and UI.
Trading Runtime  — future execution of approved Strategy Packages.
Shared Contracts — normalized events, signals, OrderIntent, RiskDecision and reports.
```

Неизменяемые решения:

```text
Trading Lab cannot send real orders.
Strategy cannot call execution directly.
Every OrderIntent passes RiskEngine.
Live is disabled by default.
Production order entry requires VPTS/certification and Owner approval.
C++ is used for low-level data/realtime/runtime work.
Python is used for research, reports and UI.
Raw/private market data and credentials do not enter Git.
```

## Completed foundation

```text
private repository and security rules;
Trading Lab / Trading Runtime / Shared Contracts skeleton;
shared schemas and test vectors;
Strategy Package standard and no-trade example;
local Trading Lab dashboard/demo foundation;
C++ QSH/OrdLog ingest and historical book reconstruction;
M10X milestone and 20/20 regression tests;
MOEX official-source research and realtime architecture;
ADR-0001 through ADR-0004;
MiMo/GitHub workflow gate.
```

## Historical QSH status

```text
M10X: complete
Regression tests: 20/20
Control commit: 54cd53df4b92473e49dd5dff96b2024590b82e42
Remaining crossed snapshots: 907
strategy_ready for affected data: false
```

Confirmed flags:

```text
0x94  = Add + Buy + Quote
0x414 = Add + Buy + TxEnd
```

No QSH semantic weakening is authorized.

## MOEX realtime status

Studied and documented:

```text
FAST 1.29.x and FAST_9.0;
spectra-1.29 / spectra-1.30 compatibility;
T0 configuration structure;
FIX SPECTRA and Drop Copy requirements;
VPTS/certification requirements;
Snapshot + Incremental architecture;
QuickFAST diagnostic-only decision;
specialized C++ SPECTRA decoder direction.
```

Private access and connection parameters remain outside Git.

## Workflow gate — complete

```text
Issue #1: DONE
PR #15: merged (82077f6e54e439f27027301ac02813c018d380fc)
Main protection: Option B active
```

MiMo rules:

```text
one active implementation task;
READY_FOR_MIMO Issue required;
clean main preflight;
new mimo/issue-* branch;
full tests and hygiene;
separate Pull Request;
no MiMo merge or auto-merge;
stop at READY_FOR_REVIEW.
```

## RT-1 — DONE

```text
Issue #14: DONE
PR #16: merged
Merge commit: ab74f560c1bcf9d09ae7bdfb8552c745928fd022
Post-merge main CI #32: passed
Owner local Release build: passed
Owner CTest: 6/6
Owner strict real-file integration: valid, zero issues
```

Delivered:

```text
offline C++20/CMake FAST configuration/templates inspector;
official-style MOEX configuration hierarchy support;
FAST field/sequence/<length> metadata;
spectra-1.29 and spectra-1.30 profile compatibility;
deterministic text/JSON reports;
Windows/Linux Release-active tests.
```

## RT-2 — specification review

```text
Issue #18: [MIMO][C++] RT-2 raw segment format and synthetic capture/replay
Status: DRAFT
Specification branch: docs/issue-18-rt2-raw-capture-replay-spec
Specification PR: #19
Implementation branch/PR: none
```

Task package in PR #19:

```text
tasks/RT-2-raw-capture-replay-contract/00_OVERVIEW.md
tasks/RT-2-raw-capture-replay-contract/01_REQUIREMENTS.md
tasks/RT-2-raw-capture-replay-contract/02_TEST_PLAN.md
tasks/RT-2-raw-capture-replay-contract/03_ACCEPTANCE.md
```

Planned implementation scope after specification approval:

```text
C++20/CMake module, preferably cpp/moex_raw/;
versioned immutable .mxraw segments;
manual fixed-width little-endian encoding;
logical source identity, explicit timestamp domains and local capture_index;
CRC32C record/footer checks and SHA-256 provenance;
.partial -> finalized lifecycle;
deterministic record/byte rotation;
bounded streaming reader/validator;
deterministic synthetic replay digest;
CLI and Release-active Windows/Linux tests.
```

Explicit RT-2 non-goals:

```text
no sockets or multicast;
no real packet capture;
no FAST binary decode;
no A/B deduplication or recovery;
no book building;
no database/object-storage integration;
no FIX/TWIME or order sending;
no pcap/raw/private data in Git.
```

## Future runtime requirement preservation

Issue #17 stores future FIX 4.4 session, order-control and Drop Copy requirements. It is not active implementation work.

## Task lifecycle

```text
DRAFT
-> READY_FOR_MIMO
-> IN_PROGRESS
-> READY_FOR_REVIEW
-> CHANGES_REQUIRED -> READY_FOR_REVIEW
-> OWNER_REVIEW
-> OWNER_APPROVED
-> DONE
```

## Immediate next actions

```text
1. Architecture/Owner review PR #19.
2. Merge PR #19 only after the specification is accepted.
3. Update Issue #18 to READY_FOR_MIMO.
4. Run the universal MiMo command once.
5. MiMo creates a separate implementation branch and PR.
6. Do not start RT-3 or any network/FIX work during RT-2.
```