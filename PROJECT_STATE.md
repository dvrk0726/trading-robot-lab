# PROJECT_STATE

Дата последнего обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: active / MOEX realtime foundation

## Назначение

Этот файл фиксирует текущее проверенное состояние проекта: что уже работает, какие решения приняты, что ожидается извне и какой следующий шаг.

Любой агент читает его после `AI_CONTEXT.md`.

## Целевая система

```text
Trading Lab      — research, replay, backtests, reports and visualization.
Trading Runtime  — execution of approved Strategy Packages.
Shared Contracts — normalized market events, OrderIntent, RiskDecision and reports.
```

Основной путь:

```text
market data -> Data Quality -> deterministic replay -> research -> backtest -> paper -> certification/owner gate -> live later
```

## Принятые решения

```text
Trading Lab cannot send real orders.
Strategy cannot call execution directly.
Every OrderIntent passes RiskEngine.
Runtime only runs approved Strategy Packages.
Live is disabled by default.
Production order entry requires VPTS/certification and owner approval.
C++ is used for low-latency data/replay/runtime core.
Python is used for research, statistics, reports and UI.
```

ADR:

```text
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Что уже работает

### Repository and workflow

```text
private GitHub repository;
AI_CONTEXT / PROJECT_STATE / ROADMAP / SECURITY;
AI-agent workflow and task template;
Strategy Knowledge Base;
Shared schemas and test vectors;
Trading Lab/Runtime skeleton;
GitHub connector access;
MiMo local implementation workflow.
```

### Historical QSH / OrdLog engine

Текущий инженерный уровень завершён:

```text
C++20 qsh_ingest;
QSH header/stream parsing;
OrdLog decoding;
transaction grouping;
L3 book reconstruction;
L3 -> L2 export;
Data Quality reports;
strategy_ready gating;
NonSystem semantics analysis;
persistent crossed-state analysis;
Quote flag semantics analysis;
20/20 tests at M10X.
```

Контрольный commit:

```text
54cd53df4b92473e49dd5dff96b2024590b82e42
```

Подтверждённые QSH flags:

```text
0x94  = Add + Buy + Quote
0x414 = Add + Buy + TxEnd
```

Остаются 907 crossed snapshots, поэтому соответствующие данные имеют `strategy_ready=false`.

Исторический QSH 2021 используется как engineering sample для parser/replay/book mechanics. Он не является доказательством современной прибыльности.

## MOEX official-source work

Изучены и зафиксированы:

```text
SPECTRA test access;
FAST test directory;
FIX test documentation;
production FAST direction;
Full_orders_log direction;
VPTS certification procedure;
FAST specification 1.29.1;
FAST_9.0 templates.xml;
T0 configuration.xml;
FIX SPECTRA manual.
```

Ключевые документы:

```text
MOEX_SPECTRA_FIX_FAST_REALTIME_DATA_AND_TEST_ACCESS_PLAN.md
docs/moex/MOEX_SOURCE_INDEX.md
docs/moex/fast_spectra_notes.md
docs/moex/fast_spectra_t0_templates_notes.md
docs/moex/fix_spectra_notes.md
docs/moex/moex_source_version_check.md
docs/moex/MOEX_REALTIME_ARCHITECTURE.md
```

### FAST template alignment

```text
FAST current documentation: 1.29.1 dated 2025-11-19.
FAST_9.0 templates match the uploaded T0 template set for the checked file.
```

Relevant template IDs:

```text
29 OrdersLogMessage
30 BookMessage
31 DefaultIncrementalRefreshMessage
32 DefaultSnapshotMessage
40 SecurityDefinition
45 SecurityGroupStatus
46 TradingSessionStatus
```

### T0 ORDERS-LOG path

Для T0 подтверждены необходимые логические компоненты:

```text
FUT-INFO;
ORDERS-LOG Incremental A/B;
ORDERS-LOG Snapshot A/B;
TCP Historical Replay;
template-driven decoding;
Snapshot + buffered Incremental bootstrap.
```

## Decision on FAST decoding

QuickFAST is not the planned production hot-path decoder.

Target:

```text
specialized C++ SPECTRA FAST decoder;
small wire-codec primitives;
generated direct decoders from templates.xml;
no XML interpretation in hot path;
no universal FIX message tree;
minimal allocations;
template hash validation;
differential testing against reference tools.
```

QuickFAST, fast_sensor and reference codecs may be used only as diagnostic/correctness references.

## Realtime architecture

```text
FAST -> raw capture -> decode -> sequence/recovery -> normalized events -> L3/L2 -> storage -> research
```

Future trading path remains separate:

```text
Strategy Package -> RiskEngine -> OrderManager -> FIX/TWIME
```

Storage direction:

```text
Raw: immutable binary segments or pcapng on NVMe.
Archive: compressed Parquet.
Analytics: ClickHouse.
Control metadata: PostgreSQL.
Local research: DuckDB + Python.
Future object storage: MinIO/S3-compatible.
```

Database failure must not block raw capture.

## External status

MOEX test-access questionnaire has been submitted.

Requested broadly for future work:

```text
T0/T+1/T+2 where available;
FAST;
FIX;
TWIME;
SIMBA;
Plaza II;
Spectra terminal;
Drop Copy information.
```

Waiting for MOEX response with approved services, addresses, ports, identifiers and network requirements.

Credentials and private connection parameters must not be committed or pasted into public project documents.

## Current priority

```text
RT-1 — Local FAST configuration/templates inspector
```

Expected result:

```text
C++ CLI inspector;
parse configuration.xml;
parse templates.xml;
list ORDERS-LOG channels;
validate template IDs and field order/types;
calculate/report template/config hashes;
create normalized FAST contract skeleton;
unit tests;
MiMo report and commit SHA.
```

Explicit RT-1 non-goals:

```text
no network connection;
no UDP multicast;
no TCP Recovery connection;
no FIX/TWIME;
no real credentials;
no order sending;
no full FAST decoder yet.
```

## Agent responsibilities

```text
ChatGPT / Architecture-Review Agent:
official sources, architecture, task specification, diff/test review and acceptance.

MiMo Code / Implementation Agent:
local coding, build, tests, commit, push and implementation report.

Owner:
final scope, access, costs, hardware, paper/live and production gates.
```

MiMo must not silently change architecture or expand task scope.

## GitHub write workflow

Rules are stored in:

```text
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

Important:

```text
large code/docs are written locally by MiMo and pushed with Git;
connector writes stay small and focused;
large tasks are split into Issue + task-spec files;
PROJECT_STATE is current state, not an unlimited history log;
raw data, binaries and secrets never enter normal Git.
```

## Engineering improvement log

Future rewrite lessons are recorded in:

```text
docs/engineering/FUTURE_REWRITE_NOTES.md
```

Only evidence-backed architecture/performance/reliability observations belong there.

## Immediate next actions

```text
1. Wait for MOEX response without blocking local work.
2. Prepare an approved RT-1 task specification for MiMo.
3. MiMo implements RT-1 locally and runs tests.
4. Architecture agent reviews commit/diff/test evidence.
5. Only after acceptance start RT-2 raw capture/replay format.
```

## Explicit non-goals now

```text
no live trading;
no production order sending;
no broker/MOEX secrets in Git;
no production hardware purchase before measurements;
no direct reuse of old HFT robot;
no QuickFAST dependency in production hot path;
no Kafka/Kubernetes/microservice expansion without evidence;
no claims of profitability from historical sample data.
```

## Project principle

```text
Correctness first.
Raw reproducibility first.
Data Quality first.
Measure before optimization.
Research before execution.
RiskEngine is mandatory.
Live remains owner-gated.
```
