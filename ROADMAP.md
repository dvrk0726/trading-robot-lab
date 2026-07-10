# Roadmap

Дата обновления: 2026-07-10  
Статус: gated engineering roadmap  
Текущий gate: RT-2 Round 3 corrections — Issue #18 / PR #20

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
Issue #18: READY_FOR_REVIEW (Round 3 corrections complete)
Implementation PR: #20
Branch: mimo/issue-18-rt2-raw-capture-replay
Implementation commit: `8e9a61ef26d99a2b47b2d05fa354952797e46ec2`
CI #51 (run 29111638220): ALL GREEN (7/7 jobs)
```

Delivered:

```text
C++20/CMake module: cpp/moex_raw/
versioned immutable .mxraw binary segments;
fixed-width little-endian manual serialization;
logical source identity and explicit timestamp domains;
local capture_index distinct from exchange sequence;
CRC32C record/footer validation and SHA-256 provenance;
incremental content SHA-256 (no fread on wb-stream);
.partial -> finalized lifecycle;
deterministic record/byte rotation with first-record validation;
bounded streaming reader/validator (no whole-file loading);
canonical filename parsing and filename/content identity;
full (session_id, source_id, channel_id) stream key;
numeric sorting, duplicate/missing detection;
full metadata/hash equality validation;
monotonic timestamp enforcement across segment boundaries;
per-stream independent summaries in reports;
expanded report schema with format_version, source metadata, provenance hashes;
strict ambiguity detection (matches.size() != 1 always fails);
deterministic replay callback with MXREPLAY1 canonical digest;
single streaming SHA-256 context in replay_stream();
status classification: unsupported, partial, truncated, corrupt, I/O error;
CLI with strict numeric/hex validation (reject negative/signed/whitespace);
Release-active CHECK macros (active under NDEBUG);
golden byte-vector and end-to-end SHA-256 tests;
independent MXREPLAY1 golden digest test;
Windows/Linux CI jobs (16 tests each);
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
1. Owner reviews RT-2 Round 3 corrections in PR #20.
2. If accepted, merge PR #20.
3. Move Issue #18 to DONE.
4. Do not start RT-3 until RT-2 is DONE.
```