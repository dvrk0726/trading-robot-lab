# Project State

Дата обновления: 2026-07-11  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-2 DONE; RT-3 implementation PR #23 architecture review (CHANGES_REQUIRED)

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

## RT-2 — DONE

```text
Issue #18: DONE
Specification PR #19: merged
Implementation PR #20: merged
Merge commit: 060371112d921c1c1f4055cfbdb99049bdf8a2af
Current main control head: 5f1f9c1beaee080fe44eaccda7c7370d9324546d
Post-merge/current-main CI #74: passed
Owner Release build: passed
Owner CTest: 18/18
Owner strict synthetic inspect: valid, 4 segments, 10 records, 0 issues
Owner replay: valid, 4 segments, 10 records, 320 payload bytes, 0 issues
```

Delivered:

```text
C++20/CMake module: cpp/moex_raw/;
v1 binary segment contract;
CRC32C and SHA-256 provenance;
checked little-endian serialization;
RawSegmentWriter .partial -> finalized lifecycle;
deterministic record/byte rotation;
bounded streaming validation;
canonical stream-set grouping and ordering;
per-stream text/JSON reports;
deterministic MXREPLAY1 replay;
CLI synth/inspect/replay;
Windows/Linux Release tests: 18 each.
```

Explicit RT-2 non-goals remain:

```text
no sockets, multicast or real capture;
no FAST binary decode;
no A/B deduplication or recovery;
no book building;
no database/object-storage integration;
no FIX/TWIME or order sending;
no raw/private data in Git.
```

## RT-3 — implementation (CHANGES_REQUIRED)

```text
Issue #21: CHANGES_REQUIRED
Implementation branch: mimo/issue-21-rt3-fast-decoder
Implementation PR: #23
Specification PR: #22 (merged)
Task package: tasks/RT-3-specialized-fast-decoder-foundation/
RT-4: BLOCKED
```

Implemented decoder foundation:

```text
exactly one bounded FAST message payload;
immutable compiled template tree preserving operators and nesting;
normative FIX FAST 1.1 stop-bit primitives with overflow-before-shift;
nullable integer/string/byte-vector values with correct mappings;
exact decimal exponent/mantissa with null-exponent semantics;
template-ID state with reuse and rollback;
none/constant/default/copy/increment/delta/tail operator table;
canonical dictionaries and per-stream DecoderSession;
transactional state journal and rollback;
groups, sequences and explicit safety limits;
deterministic DecodedMessage tree and CLI reports;
span-based compatibility with RawPacketRecord.payload;
13 tests (6 RT-1 + 7 RT-3 decoder) on Windows/MSVC and Linux/GCC;
independent golden vectors and reference encoder;
Windows/Linux Release-active tests.
```

RT-3 does not include:

```text
SPECTRA UDP packet framing;
network capture;
MsgSeqNum/gap policy;
A/B merge or deduplication;
Snapshot/Incremental bootstrap or recovery;
normalized market events;
order-log/book semantics;
FIX/TWIME or order sending;
strategy/paper/production enablement.
```

Required specification files:

```text
tasks/RT-3-specialized-fast-decoder-foundation/00_OVERVIEW.md
tasks/RT-3-specialized-fast-decoder-foundation/01_REQUIREMENTS.md
tasks/RT-3-specialized-fast-decoder-foundation/02_TEST_PLAN.md
tasks/RT-3-specialized-fast-decoder-foundation/03_ACCEPTANCE.md
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
1. Owner and Architecture/Review inspect RT-3 specification PR #22.
2. Do not run MiMo implementation from the docs branch.
3. After explicit owner approval, merge PR #22 manually.
4. Confirm green post-merge CI on main.
5. Only then move Issue #21 to READY_FOR_MIMO.
6. Do not start RT-4 until RT-3 is DONE.
```