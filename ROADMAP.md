# Roadmap

Дата обновления: 2026-07-10  
Статус: gated engineering roadmap  
Текущий gate: RT-2 specification review — Issue #18 / PR #19

## Главный порядок

```text
repository workflow and protection
-> local FAST metadata inspection
-> raw segment contract and deterministic replay
-> specialized FAST decoding
-> feed sequencing/recovery
-> realtime data quality and books
-> research/backtest/paper
-> VPTS/certification
-> owner-approved production only later
```

Нельзя перескакивать gate из-за готовности отдельного компонента.

## Completed foundation

### Architecture and repository

```text
Trading Lab / Trading Runtime / Shared Contracts architecture;
ADR-0001 through ADR-0004;
security baseline;
shared schemas/test vectors;
Strategy Package standard;
MiMo/GitHub branch + PR workflow;
Option B procedural main protection.
```

### Historical C++ data contour

```text
QSH/OrdLog ingest;
L3/order-book reconstruction and data-quality diagnostics;
M10X complete;
20/20 regression tests;
control commit 54cd53df4b92473e49dd5dff96b2024590b82e42.
```

Remaining 907 crossed snapshots remain gated with `strategy_ready=false`.

### MOEX realtime research

```text
FAST 1.29.x and FAST_9.0 studied;
T0 configuration structure studied;
FIX SPECTRA / Drop Copy and VPTS requirements preserved;
MOEX realtime architecture documented;
QuickFAST rejected as production hot-path foundation;
specialized C++ decoder direction accepted.
```

## WF-1 — DONE

```text
Issue #1: DONE
PR #15: merged (82077f6e54e439f27027301ac02813c018d380fc)
```

Delivered workflow:

```text
READY_FOR_MIMO task selection;
one task at a time;
implementation only in dedicated branch;
Pull Request and CI required;
no MiMo merge or auto-merge;
Python/C++/hygiene checks;
QSH/M10X 20-test gate;
secret/raw-data/large-file hygiene;
Owner Review Package;
canonical statuses and handoff evidence.
```

## RT-1 — DONE

```text
Issue #14: DONE
PR #16: merged
Merge commit: ab74f560c1bcf9d09ae7bdfb8552c745928fd022
Post-merge main CI #32: passed
Owner Release build and CTest: passed
Owner strict official-file integration: valid, zero issues
```

Delivered:

```text
offline C++20/CMake inspector;
configuration.xml and templates.xml parsing;
MOEX feed-group/endpoint metadata;
FAST field, sequence and <length> metadata;
spectra-1.29 / spectra-1.30 detection;
strict compatibility checks;
deterministic text and JSON reports;
Windows/Linux Release-active tests.
```

RT-1 deliberately excluded network, binary FAST decoding, recovery, books and order entry.

## RT-2 — Raw segment format + synthetic capture/replay

Current status:

```text
Issue #18: DRAFT
Specification branch: docs/issue-18-rt2-raw-capture-replay-spec
Specification PR: #19
Implementation: not started
```

Specification package:

```text
tasks/RT-2-raw-capture-replay-contract/00_OVERVIEW.md
tasks/RT-2-raw-capture-replay-contract/01_REQUIREMENTS.md
tasks/RT-2-raw-capture-replay-contract/02_TEST_PLAN.md
tasks/RT-2-raw-capture-replay-contract/03_ACCEPTANCE.md
```

Planned scope after PR #19 is accepted and merged:

```text
versioned immutable .mxraw binary segments;
fixed-width little-endian manual serialization;
logical source identity and explicit timestamp domains;
local capture_index distinct from exchange sequence;
CRC32C record/footer validation and SHA-256 provenance;
.partial -> finalized lifecycle;
deterministic record/byte rotation;
bounded streaming reader/validator;
deterministic synthetic replay callback and digest;
CLI plus Windows/Linux Release-active tests.
```

RT-2 non-goals:

```text
no sockets, multicast or real capture;
no FAST decode;
no exchange sequence extraction;
no A/B deduplication;
no Snapshot/Incremental recovery;
no books;
no pcap/pcapng dependency;
no database, compression or object-storage integration;
no FIX/TWIME or order sending.
```

RT-2 gate:

```text
review PR #19
-> owner approves specification
-> merge PR #19
-> move Issue #18 to READY_FOR_MIMO
-> MiMo implements in a new branch/PR
```

## RT-3 — Specialized C++ FAST decoder foundation

Blocked by accepted RT-2.

Planned scope:

```text
FAST primitives/operators;
template-driven state;
mandatory-field checks;
sequence/reset handling;
differential tests against reference tools;
no full order-entry stack.
```

## RT-4 — SPECTRA feed processors and recovery

Blocked by decoder correctness.

```text
FUT-INFO;
ORDERS-LOG Snapshot;
ORDERS-LOG Incremental;
Snapshot + buffered Incremental bootstrap;
A/B sequencing/deduplication;
gap/recovery state;
normalized market events.
```

## RT-5 — Realtime Data Quality and book state

Blocked by RT-4.

```text
sequence freshness;
book invariants;
crossed/locked diagnostics;
recovery status;
strategy_ready gating;
replay/live parity evidence.
```

No strategy-ready claim without measured evidence.

## Later data and research stages

After trustworthy realtime data:

```text
normalized Parquet/ClickHouse/PostgreSQL contour;
research hypotheses;
deterministic backtests;
fees/slippage/latency sensitivity;
out-of-sample checks;
paper trading;
operational and risk evidence.
```

Historical results do not authorize live.

## Certification and production gate

Before any production order entry:

```text
VPTS/certification satisfied;
approved access and network architecture;
RiskEngine and kill switch reviewed;
monitoring/audit/recovery complete;
Owner explicitly approves stage, cost and access;
production remains disabled by default.
```

Issue #17 preserves future SPECTRA FIX 4.4 session, order-control and Drop Copy requirements. It does not authorize implementation now.

## Immediate sequence

```text
1. Complete architecture review of RT-2 specification PR #19.
2. Owner reviews and approves or requests exact specification changes.
3. Merge PR #19 only after acceptance.
4. Move Issue #18 from DRAFT to READY_FOR_MIMO.
5. Run the universal MiMo command once.
6. MiMo implements RT-2 in a separate mimo/issue-18-* branch and PR.
7. Do not start RT-3, network capture or FIX work during RT-2.
```