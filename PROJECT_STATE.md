# Project State

Дата обновления: 2026-07-11  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-2 Round 9 corrections complete — Issue #18 / PR #20 READY_FOR_REVIEW

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

## RT-2 — Round 9 corrections complete

```text
Issue #18: READY_FOR_REVIEW
Implementation PR: #20
Branch: mimo/issue-18-rt2-raw-capture-replay
Implementation commit: `95a6626`
Implementation CI #66 (run 29142699176): ALL GREEN (7/7 jobs)
```

Delivered:

```text
C++20/CMake module: cpp/moex_raw/
v1 binary segment contract (preamble, header, records, footer)
CRC32C (Castagnoli) and pure C++ SHA-256 implementations
Little-endian serialization primitives with checked arithmetic
UTF-8 string validation with 128-byte limit
RawSegmentWriter with .partial -> finalized lifecycle
Writer metadata validation before file creation
write_length_string rejects oversized strings (no silent truncation)
Hard 64 GiB segment cap regardless of rotation policy
Incremental content SHA-256 (no fread on wb-stream)
Deterministic rotation by record count and byte limits
First-record byte-limit validation
Bounded streaming reader/validator (no whole-file loading)
Canonical filename parsing and filename/content identity
Stream-set validation: numeric sorting, duplicate/missing detection,
  full metadata/hash equality, monotonic timestamp across boundaries
Directory grouping by full (session_id, source_id, channel_id) key
Per-stream independent summaries in text and JSON reports
Expanded report schema with format_version, source metadata, provenance hashes
Ambiguity detection for multiple matching streams (strict: matches.size() != 1)
Deterministic replay callback with MXREPLAY1 canonical digest
Single streaming SHA-256 context in replay_stream()
replay_from_stream_set() for explicit session selection
Status classification: unsupported, partial, truncated, corrupt, I/O error
CLI: moex-raw synth, moex-raw inspect, moex-raw replay
Strict CLI numeric/hex validation (reject negative/signed/whitespace)
Release-active CHECK macros (active under NDEBUG)
Independent golden byte-vector test
End-to-end content SHA-256 verification test
Independent MXREPLAY1 golden digest test
CI jobs for Windows/MSVC and Linux/GCC (18 tests each)
```

Test results:

```text
RT-2:         18/18 tests passed (Windows + Linux Release)
RT-1:          6/6  tests passed (no regression)
QSH/M10X:     20/20 tests passed (no regression)
Python:         3/3  passed
Hygiene:        PASS (276 files checked)
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
1. Owner reviews RT-2 Round 9 corrections in PR #20.
2. If accepted, merge PR #20.
3. Move Issue #18 to DONE.
4. Do not start RT-3 until RT-2 is DONE.
```