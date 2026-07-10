# MOEX Realtime Architecture

Date: 2026-07-09
Status: proposed

## Goal

```text
FAST -> raw capture -> decode -> sequence/recovery -> ORDERS-LOG L3/L2 -> storage -> research
```

Future trading stays separate:

```text
Strategy Package -> RiskEngine -> OrderManager -> FIX/TWIME
```

## Main rule

The capture path must not depend on a database, dashboard, Python process or strategy. If downstream systems fail, raw capture continues or the session is explicitly invalidated. Silent loss is forbidden.

## Languages

```text
C++20/23:
UDP multicast, FAST decode, A/B deduplication, sequencing, recovery, raw recorder, L3/L2, replay, future Runtime.

Python:
research, statistics, backtests, quality reports, dashboards and offline conversion.

SQL:
ClickHouse analytics, PostgreSQL metadata, DuckDB local Parquet research.
```

Do not add another language without measured need.

## Performance approach

Real ORDERS-LOG load is unknown until T0 measurements. Design for:

```text
>= 2x measured peak capacity
short bursts >= 5x average
bounded queues only
batch writes only
zero unexplained loss
deterministic replay
```

Initial benchmark targets are provisional:

```text
1,000,000 decoded entries/sec
250 MB/sec raw write
5x realtime replay
1 GbE for test; 10 GbE-ready design later
```

## Process

Start with one C++ collector, not microservices:

```text
Feed A/B receivers
Raw writer
FAST decoder
Sequencer/deduplicator
Instrument/session state
ORDERS-LOG book builder
Batch normalized writer
Metrics/health
TCP Recovery worker
```

Use preallocated bounded queues, batch transfer, minimal hot-path allocations and asynchronous bounded logs.

## Storage

### Raw source of truth

```text
local high-endurance NVMe
-> immutable rotated binary segments or pcapng
-> checksums
-> compressed S3-compatible/MinIO archive later
```

Raw data is never edited. Decoder fixes replay the original capture.

### Normalized archive

Use compressed Parquet, partitioned by market/date/feed/hour. Do not partition files by instrument.

### Hot analytics

Use ClickHouse MergeTree with batch inserts. ClickHouse is derived storage, not the only copy.

### Control metadata

Use PostgreSQL for capture sessions, collector nodes, template/config hashes, experiments, approvals, runs and incidents. Do not put every market event in PostgreSQL.

### Local research

Use DuckDB/Python directly over Parquet.

Kafka/Redpanda and Redis are not required for the first collector.

## Data quality

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

## Platform

```text
Windows: development and early T0 tests
Linux: preferred production collector/runtime
```

Do not buy production hardware before real T0 measurements.

## Focused roadmap

```text
RT-0 Source/architecture alignment                 mostly done
RT-1 Local config/templates inspector              next
RT-2 Raw segment format + synthetic capture/replay
RT-3 Template-driven FAST decoder
RT-4 A/B sequencing, snapshot and recovery
RT-5 T0 FAST connectivity                          waits for MOEX
RT-6 L3/L2 + Parquet/ClickHouse/PostgreSQL
RT-7 Production Full_orders_log one-month pilot
RT-8 Paper trading on realtime data
RT-9 Test FIX/TWIME + VPTS readiness
RT-10 Production trading certification/owner gate
```

## Next step

```text
RT-1: local configuration.xml/templates.xml inspector and normalized C++ contracts.
No network connection and no order sending in this step.
```
