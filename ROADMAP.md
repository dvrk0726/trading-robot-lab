# ROADMAP

Дата последнего обновления: 2026-07-10  
Статус: active roadmap

## Главная цель

Построить две отдельные системы с общими контрактами:

```text
Trading Lab      — research, replay, backtests, reports and analysis.
Trading Runtime  — lightweight execution of approved Strategy Packages.
Shared Contracts — normalized data, signals, orders, risk decisions and reports.
```

## Неизменяемые архитектурные правила

```text
Trading Lab cannot send real orders.
Strategy cannot call exchange/broker execution directly.
Every OrderIntent passes RiskEngine.
Runtime executes only approved Strategy Packages.
Live is disabled by default and owner-gated.
Production order entry is blocked by VPTS/certification gate.
C++ is the low-latency/data/runtime language.
Python is the research, analysis and UI language.
```

Основные ADR:

```text
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Current Priority

```text
MOEX Realtime RT-1
```

Цель ближайшего этапа:

```text
local configuration.xml/templates.xml inspector;
normalized C++ FAST contracts;
template/channel/hash validation;
no network;
no order sending;
no production QuickFAST hot path.
```

Подробная realtime-архитектура:

```text
docs/moex/MOEX_REALTIME_ARCHITECTURE.md
```

## Completed Foundation

### Phase 0 — Knowledge Base and Architecture

Статус: done / maintained

```text
AI_CONTEXT.md
PROJECT_STATE.md
SECURITY.md
ROADMAP.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
strategy_knowledge_base/
```

### Phase 1 — Repository Skeleton and Shared Contracts

Статус: mostly done

```text
apps/lab/
apps/runtime/
shared/schemas/
shared/test_vectors/
strategy_packages/examples/
Strategy Package standard
basic risk/config models
Trading Lab demo
```

### Phase 2 — Historical QSH / OrdLog Engineering Layer

Статус: completed at current engineering level

Результат:

```text
C++20 qsh_ingest;
QSH/OrdLog parser;
transaction grouping;
L3 reconstruction;
L3 -> L2 export;
Data Quality report;
strategy_ready gating;
NonSystem/crossed/Quote semantics investigation;
M10X tests 20/20.
```

Контрольный commit:

```text
54cd53df4b92473e49dd5dff96b2024590b82e42
```

Исторический QSH остаётся поддерживаемой research/replay веткой, но не является текущим главным направлением.

## MOEX Realtime Roadmap

### RT-0 — Official Sources and Architecture Alignment

Статус: mostly done

```text
FAST 1.29.1 read;
FAST_9.0 templates aligned;
T0 configuration parsed;
FIX SPECTRA notes;
VPTS gate recorded;
realtime architecture recorded;
MOEX test-access questionnaire submitted.
```

### RT-1 — Local FAST Config/Template Inspector

Статус: next

Deliverables:

```text
C++ inspector CLI;
configuration.xml parser;
templates.xml parser;
ORDERS-LOG channel inventory;
template IDs and field contracts;
SHA-256/version report;
normalized contracts skeleton;
unit tests;
MiMo implementation report.
```

Non-goals:

```text
no UDP;
no TCP Recovery connection;
no real credentials;
no FIX/TWIME;
no orders.
```

### RT-2 — Raw Capture Format and Offline Replay

Статус: queued

```text
immutable raw segment format;
packet timestamps;
checksums;
rotation;
synthetic capture generator;
offline deterministic replay;
replay hash verification.
```

### RT-3 — Specialized SPECTRA FAST Decoder

Статус: queued

```text
wire primitives;
presence maps;
stop-bit integers;
nullable values;
decimal/string support;
generated direct decoders from templates.xml;
first templates: 29 and 30;
differential tests against reference decoder.
```

QuickFAST may be used only as reference/diagnostic tooling, not as production hot-path dependency.

### RT-4 — Sequencing, A/B, Snapshot and Recovery

Статус: queued

```text
A/B deduplication;
MsgSeqNum/RptSeq control;
gap detection;
Snapshot bootstrap;
fragment handling;
TCP Historical Replay worker;
recovery limits and backoff;
Data Quality session gate.
```

### RT-5 — T0 Connectivity

Статус: waits for MOEX response

```text
network access validation;
FUT-INFO;
ORDERS-LOG Incremental A/B;
Snapshot A/B;
Historical Replay;
real throughput measurements;
no trading orders.
```

### RT-6 — L3/L2 and Storage

Статус: future

```text
FAST normalized events;
L3/L2 book builder;
raw NVMe storage;
Parquet archive;
ClickHouse analytics;
PostgreSQL control metadata;
DuckDB local research.
```

### RT-7 — Production Market Data Pilot

Статус: future / agreement-gated

```text
production FAST / Full_orders_log access;
one-month capture pilot;
capacity sizing;
loss/recovery report;
data-retention estimate;
production hardware decision.
```

### RT-8 — Research and Paper Trading

Статус: future

```text
current-data feature research;
lead-lag validation;
market-making research;
deterministic replay;
paper orders;
real-time RiskEngine;
no real order sending.
```

### RT-9 — Test FIX/TWIME and VPTS Readiness

Статус: future / owner-gated

```text
session engines;
OrderManager;
Cancel/Replace;
Drop Copy;
Cancel On Disconnect;
full test trading day;
risk and audit logs;
VPTS test plan.
```

### RT-10 — Production Trading Gate

Статус: blocked

Required before implementation/activation:

```text
validated strategy;
backtest and replay;
paper trading;
risk review;
security review;
kill switch;
VPTS/certification;
production agreements;
owner approval.
```

## Research Roadmap

После появления качественных текущих данных:

```text
R-1 normalized market-data research contracts;
R-2 RI / Synthetic Index Lead-Lag analysis;
R-3 reproducible backtest and standardized metrics;
R-4 event-driven fill/queue/latency replay;
R-5 regime-aware inventory market-making research.
```

## Agent Workflow

```text
Architecture/Review Agent:
source analysis -> architecture -> task spec -> review -> acceptance.

MiMo Code:
local implementation -> build -> tests -> commit -> push -> report.

Owner:
final scope, cost, access, paper/live and production decisions.
```

Правила записи в GitHub:

```text
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

## Explicit Non-Goals Now

```text
no live trading;
no production order sending;
no real credentials in Git;
no direct port of old robot;
no profitability claims from 2021 QSH;
no raw market archives in Git;
no premature microservices/Kafka/Kubernetes;
no production hardware purchase before measurements.
```

## Project Principle

```text
Correctness first.
Raw reproducibility first.
Data Quality first.
Measure before optimization.
Research before execution.
RiskEngine is mandatory.
Live remains owner-gated.
```
