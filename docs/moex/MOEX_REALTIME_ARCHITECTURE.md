# MOEX Realtime Architecture

Date: 2026-07-13  
Status: current architecture direction; execution is gated by `ROADMAP.md`

## Goal

```text
raw capture
-> bounded FAST message body
-> template-driven MOEX T0/T1 decode
-> framing/sequencing/recovery
-> normalized events and ORDERS-LOG L3/L2
-> storage
-> research/backtest/paper
```

Future trading remains separate:

```text
Strategy Package -> RiskEngine -> OrderManager -> FIX/TWIME
```

This document describes architecture direction. Current task order and gate status are authoritative only in `ROADMAP.md` and `PROJECT_STATE.md`.

## Source hierarchy for SPECTRA FAST

```text
1. Official MOEX SPECTRA FAST documentation and templates:
   https://ftp.moex.com/pub/FAST/Spectra/test/
2. FIX FAST 1.1 only for base wire semantics MOEX does not restate.
3. QuickFAST, OpenFAST and third-party articles only as cross-checks.
```

Current accepted decoder targets:

```text
T0 templatesT0/templates.xml
SHA-256 DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 templatesT1/templates.xml
SHA-256 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

T0 corresponds to the production trading-system version; T1 is the next-release test system. `FAST_9.0/templates.xml` is byte-identical to accepted T0 and is not a third profile. Historical `FAST_8.6` and `backup/` are not current compatibility targets.

## Main safety rule

The capture path must not depend on a database, dashboard, Python process or strategy. If downstream systems fail, raw capture continues or the session is explicitly invalidated. Silent loss is forbidden.

Raw data is immutable. Decoder fixes replay the original capture.

## Languages

```text
C++20/23:
raw capture, FAST decode, framing, A/B sequencing, recovery,
replay, books and future Trading Runtime.

Python:
research, statistics, backtests, quality reports, dashboards and offline conversion.

SQL/storage:
ClickHouse analytics, PostgreSQL metadata, DuckDB/Parquet local research.
```

Do not add another language without measured need.

## Current RT-3 boundary

RT-3 accepts exactly one bounded FAST message body. It does not parse the MOEX 4-byte preamble, UDP datagram framing, A/B source state or recovery.

Supported profile is limited to constructs present in accepted T0/T1:

```text
field without operator
constant
template ID and previous-template-ID reuse
presence maps
ordinary and nullable integer primitives
ASCII and Unicode strings
exact decimal
mandatory and optional sequences
single sequence length instruction
limits, reset and transactional rollback
```

The following are not current capabilities and must fail closed:

```text
default/copy/increment/delta/tail field operators
generic field dictionaries and scopes
user-defined dictionaries
typeRef/templateRef/groupRef
reference resolution and cycles
generic groups outside accepted T0/T1
decimal component operators
historical profile compatibility
```

Previous-template-ID reuse is not the XML `<copy>` operator.

## Performance approach

Real ORDERS-LOG load is unknown until measured T0 access. Design targets remain provisional:

```text
>= 2x measured peak capacity
short bursts >= 5x average
bounded queues only
batch writes only
zero unexplained loss
deterministic replay
```

Do not buy production hardware before real T0 measurements.

## Initial process topology

Start with one C++ collector/process boundary, not microservices:

```text
Feed A/B receivers
Raw writer
FAST decoder
Sequencer/deduplicator
Instrument/session state
ORDERS-LOG book builder
Batch normalized writer
Metrics/health
Recovery worker
```

These components are introduced only by their roadmap gates. RT-3 implements only the offline decoder boundary.

Use preallocated bounded queues, batch transfer, minimal hot-path allocations and bounded asynchronous logging after measurements justify them.

## Storage

### Raw source of truth

```text
local high-endurance NVMe
-> immutable rotated binary segments or approved packet capture
-> checksums
-> compressed object archive later
```

### Normalized archive

Use compressed Parquet partitioned by market/date/feed/hour. Do not partition files by instrument.

### Hot analytics

Use ClickHouse MergeTree with batch inserts. ClickHouse is derived storage, not the only copy.

### Control metadata

Use PostgreSQL for capture sessions, collector nodes, template/config hashes, experiments, approvals, runs and incidents. Do not put every market event in PostgreSQL.

### Local research

Use DuckDB/Python directly over Parquet.

Kafka/Redpanda and Redis are not required for the first collector.

## Data quality gate

A session is not research-ready unless:

```text
template/config hashes recorded
gaps recovered or marked unresolved
A/B dedup deterministic
snapshot/bootstrap completed
Empty Book handled
FAST non-system MDFlags 0x4 excluded from visible book
unknown values counted and preserved
queue overflows = 0
raw checksums valid
replay reproduces identical hashes
```

These are later gates and are not RT-3 acceptance criteria.

## Platform

```text
Windows: development and early T0 tests
Linux: preferred production collector/runtime
```

## Gated roadmap

```text
RT-0 source/architecture alignment                  DONE at current level
RT-1 local config/templates inspector               DONE
RT-2 raw segment format and deterministic replay    DONE
RT-3 specialized MOEX T0/T1 FAST decoder            CURRENT — CHANGES_REQUIRED
RT-4 preamble/framing, A/B sequencing and recovery  BLOCKED
RT-5 realtime data quality and normalized events
RT-6 ORDERS-LOG L3/L2 and storage
RT-7 T0 pilot and measured capacity
RT-8 research/backtest/paper on certified data
RT-9 FIX/TWIME test and VPTS readiness
RT-10 production certification and owner gate
```

Later stage names remain provisional until evidence from the preceding gate is reviewed.

## Current next step

```text
Correct existing RT-3 PR #23 to the merged T0/T1 specification.
Do not start RT-4, network connectivity or order sending.
```
