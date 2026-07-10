# ROADMAP

Дата последнего обновления: 2026-07-10  
Статус: active roadmap

## Главная цель

```text
Trading Lab      — research, replay, backtests, reports and analysis.
Trading Runtime  — execution of approved Strategy Packages.
Shared Contracts — normalized data, signals, orders, risk decisions and reports.
```

## Неизменяемые правила

```text
Trading Lab cannot send real orders.
Strategy cannot call execution directly.
Every OrderIntent passes RiskEngine.
Runtime executes only approved Strategy Packages.
Live is disabled by default and owner-gated.
Production order entry requires VPTS/certification.
C++ is the low-latency/data/runtime language.
Python is the research, analysis and UI language.
Raw data and secrets are not stored in normal Git.
```

Основные ADR:

```text
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Current Priority — RT-1

```text
RT-1 — Local FAST configuration/templates inspector
```

Текущий статус:

```text
task specification ready;
GitHub Issue #14 open;
implementation not started;
MiMo Code is the implementation agent;
Architecture/Review Agent accepts or rejects the result.
```

Task package:

```text
tasks/RT-1-fast-config-template-inspector/00_OVERVIEW.md
tasks/RT-1-fast-config-template-inspector/01_REQUIREMENTS.md
tasks/RT-1-fast-config-template-inspector/02_TEST_PLAN.md
tasks/RT-1-fast-config-template-inspector/03_ACCEPTANCE.md
```

Issue:

```text
#14 [MIMO][C++] RT-1 FAST configuration/templates inspector
```

RT-1 scope:

```text
local C++ inspector;
configuration.xml parsing;
templates.xml parsing;
ORDERS-LOG/FUT-INFO inventory;
template IDs, field order and type validation;
SHA-256 provenance;
deterministic JSON report;
normalized FAST metadata contracts;
unit tests and MiMo report.
```

RT-1 non-goals:

```text
no network;
no UDP/TCP connection;
no FAST binary decoder yet;
no QuickFAST production dependency;
no FIX/TWIME;
no order sending;
no credentials;
no official XML committed without approval.
```

## Completed Foundation

### Architecture and workflow

Статус: done / maintained

```text
AI_CONTEXT.md
PROJECT_STATE.md
SECURITY.md
ROADMAP.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
docs/engineering/FUTURE_REWRITE_NOTES.md
strategy_knowledge_base/
```

### Repository skeleton

Статус: mostly done

```text
apps/lab/
apps/runtime/
shared/schemas/
shared/test_vectors/
strategy_packages/examples/
Trading Lab demo
basic risk/config models
```

### Historical QSH / OrdLog engineering layer

Статус: completed at current engineering level

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

Historical QSH remains a research/replay branch, not evidence of current profitability.

## MOEX Realtime Roadmap

### RT-0 — Official sources and architecture

Статус: mostly done

```text
FAST 1.29.1 studied;
FAST_9.0 templates aligned;
T0 configuration parsed;
FIX SPECTRA notes recorded;
VPTS gate recorded;
realtime architecture recorded;
MOEX test-access questionnaire submitted.
```

### RT-1 — Local FAST config/template inspector

Статус: specification ready / Issue #14 open

Gate:

```text
MiMo implementation;
build and tests;
review of diff and evidence;
owner acceptance.
```

### RT-2 — Raw capture format and offline replay

Статус: blocked until RT-1 acceptance

```text
immutable raw segments;
packet timestamps;
checksums and rotation;
synthetic capture generator;
deterministic offline replay;
replay hash verification.
```

### RT-3 — Specialized SPECTRA FAST decoder

Статус: queued

```text
wire primitives;
presence maps;
stop-bit and nullable values;
decimal/string support;
generated direct decoders from templates.xml;
first templates: 29 OrdersLogMessage and 30 BookMessage;
differential tests against reference tools.
```

QuickFAST may be used only as a diagnostic/correctness reference.

### RT-4 — Sequencing, A/B, Snapshot and Recovery

Статус: queued

```text
A/B deduplication;
MsgSeqNum/RptSeq control;
gap detection;
Snapshot bootstrap;
fragment handling;
TCP Historical Replay;
recovery limits/backoff;
Data Quality session gate.
```

### RT-5 — T0 connectivity

Статус: waits for MOEX response

```text
network validation;
FUT-INFO;
ORDERS-LOG Incremental A/B;
Snapshot A/B;
Historical Replay;
real throughput measurements;
no trading orders.
```

### RT-6 — L3/L2 and storage

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

### RT-7 — Production market-data pilot

Статус: agreement-gated

```text
production FAST / Full_orders_log;
one-month capture pilot;
capacity sizing;
loss/recovery report;
retention estimate;
hardware decision after measurements.
```

### RT-8 — Research and paper trading

Статус: future

```text
current-data research;
lead-lag validation;
market-making research;
deterministic replay;
paper orders;
real-time RiskEngine;
no real order sending.
```

### RT-9 — Test FIX/TWIME and VPTS readiness

Статус: future / owner-gated

```text
session engines;
OrderManager;
Cancel/Replace;
Drop Copy;
Cancel On Disconnect;
full test trading day;
risk/audit logs;
VPTS test plan.
```

### RT-10 — Production trading gate

Статус: blocked

Required:

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

After current high-quality data exists:

```text
R-1 normalized research contracts;
R-2 RI / Synthetic Index Lead-Lag;
R-3 reproducible backtest and metrics;
R-4 fill/queue/latency replay;
R-5 regime-aware inventory market making.
```

## Agent Workflow

```text
Architecture/Review Agent:
sources -> architecture -> task spec -> review -> acceptance.

MiMo Code:
local implementation -> build -> tests -> commit -> push -> report.

Owner:
final scope, access, costs, hardware, paper/live and production gates.
```

GitHub write rules:

```text
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

## Explicit Non-Goals Now

```text
no live trading;
no production order sending;
no secrets in Git;
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
