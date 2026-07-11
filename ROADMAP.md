# Roadmap

Дата обновления: 2026-07-11  
Статус: gated engineering roadmap  
Текущий gate: RT-3 specification PR #22 — owner review

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

## RT-2 — DONE

```text
Issue #18: DONE
Specification PR #19: merged
Implementation PR #20: merged
Merge commit: 060371112d921c1c1f4055cfbdb99049bdf8a2af
Current main control head after no-op cleanup: 5f1f9c1beaee080fe44eaccda7c7370d9324546d
Post-merge/current-main CI #74: passed
Owner local Release build: passed
Owner CTest: 18/18
Owner strict synthetic inspect: 4 segments, 10 records, 0 issues
Owner replay: 4 segments, 10 records, 320 payload bytes, 0 issues
```

Delivered:

```text
C++20/CMake module: cpp/moex_raw/;
versioned immutable .mxraw binary segments;
fixed-width little-endian manual serialization;
logical source identity and explicit timestamp domains;
local capture_index distinct from exchange sequence;
CRC32C record/footer validation and SHA-256 provenance;
incremental content SHA-256;
.partial -> finalized lifecycle;
deterministic record/byte rotation;
bounded streaming reader/validator;
canonical filename parsing and filename/content identity;
full (session_id, source_id, channel_id) stream key;
numeric sorting, duplicate/missing detection;
metadata/hash equality and monotonic validation;
per-stream deterministic reports;
deterministic replay callback with MXREPLAY1 digest;
strict status classification and CLI validation;
Windows/Linux Release-active tests (18 each).
```

RT-2 non-goals remain:

```text
no sockets, multicast or real capture;
no FAST decode;
no exchange sequence extraction;
no A/B deduplication;
no Snapshot/Incremental recovery;
no books;
no database, pcap, FIX/TWIME or order sending.
```

## RT-3 — Specialized C++ FAST decoder foundation

Current status:

```text
Issue #21: DRAFT — OWNER_SPEC_REVIEW
Architecture branch: docs/issue-21-rt3-fast-decoder-spec
Specification PR: #22
Specification head before evidence update: 8dd3cfb7e53c85e9bcfbdcdafc2a735dd4dd0708
Specification CI #75 (run 29148020256): green 7/7
Task package: tasks/RT-3-specialized-fast-decoder-foundation/
MiMo implementation: NOT AUTHORIZED
RT-4: BLOCKED
```

Specification scope:

```text
exactly one bounded FAST message payload;
immutable decoder-specific compiled template tree;
stop-bit integers, nullable values and presence maps;
ASCII, Unicode, byte vector and exact decimal primitives;
template-ID session state;
none/constant/default/copy/increment/delta/tail operators;
explicit canonical dictionary scopes/keys;
groups, sequences and bounded recursion;
transactional dictionary/template-ID commit and rollback;
explicit reset API;
deterministic typed message tree and text/JSON reports;
RT-2 RawPacketRecord.payload span integration test;
independent golden vectors and test-only reference encoder;
Windows/Linux Release tests.
```

RT-3 boundaries:

```text
no SPECTRA UDP packet header or datagram framing;
no multicast/network capture;
no MsgSeqNum gap policy;
no A/B sequencing/deduplication;
no Snapshot/Incremental recovery;
no normalized market events or book building;
no FIX/TWIME or order sending;
no strategy, paper or production enablement.
```

RT-3 gate:

```text
owner reviews specification PR #22
-> owner explicitly approves merge
-> merge specification PR manually
-> post-merge main CI green
-> Issue #21 moves to READY_FOR_MIMO
-> MiMo implements in a separate mimo/issue-21-* branch and PR
-> architecture review and owner local acceptance
-> owner-approved merge only
```

## RT-4 — SPECTRA feed processors and recovery

Blocked by accepted RT-3 decoder correctness.

```text
SPECTRA packet/message framing;
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
1. Owner reviews RT-3 specification PR #22.
2. Do not run MiMo implementation from the docs branch.
3. If accepted, merge PR #22 manually.
4. Confirm green post-merge CI on main.
5. Only then move Issue #21 to READY_FOR_MIMO.
6. Do not start RT-4 before RT-3 is DONE.
```