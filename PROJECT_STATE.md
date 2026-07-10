# PROJECT_STATE

Дата последнего обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: active / RT-1 specification ready

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

## Неизменяемые решения

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
GitHub write-limit and handoff rules;
Future Rewrite Notes;
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

Подтверждённые historical QSH flags:

```text
0x94  = Add + Buy + Quote
0x414 = Add + Buy + TxEnd
```

Остаются 907 crossed snapshots; соответствующие данные имеют `strategy_ready=false`.

Historical QSH 2021 remains an engineering sample for parser/replay/book mechanics and is not evidence of current profitability.

## MOEX official-source status

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

### FAST alignment

```text
Current FAST documentation: 1.29.1 dated 2025-11-19.
FAST_9.0 templates match the checked uploaded T0 template file.
Known template SHA-256:
dbd50f1e0becc2b2ebd9dac8e4c6609ba1538566811b610cde9b6dd3e7f66a8e
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

T0 logical path:

```text
FUT-INFO;
ORDERS-LOG Incremental A/B;
ORDERS-LOG Snapshot A/B;
TCP Historical Replay;
template-driven decoding;
Snapshot + buffered Incremental bootstrap.
```

## FAST decoder decision

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

QuickFAST, fast_sensor and reference codecs may be used only for diagnostic/correctness comparison.

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

Credentials and private connection parameters must not be committed or pasted into project documents.

## Current task — RT-1

```text
RT-1 — Local FAST configuration/templates inspector
```

Status:

```text
specification package created;
GitHub Issue #14 created and open;
implementation not started;
no MiMo commit yet;
no code review yet.
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

Expected result:

```text
C++ CLI inspector;
parse configuration.xml;
parse templates.xml;
list ORDERS-LOG/FUT-INFO channels;
validate template IDs and ordered field metadata;
calculate/report file hashes;
create normalized FAST metadata contracts;
produce deterministic JSON;
unit and regression tests;
MiMo report and commit SHA.
```

Explicit RT-1 non-goals:

```text
no network connection;
no UDP multicast;
no TCP Recovery connection;
no FAST binary decoder yet;
no FIX/TWIME;
no real credentials;
no order sending;
no official XML committed without approval;
no QuickFAST production dependency.
```

Required MiMo report:

```text
agent_workspaces/mimo/reports/2026-07-10-rt1-fast-config-template-inspector.md
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

Rules:

```text
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

Important:

```text
large code/docs are written locally by MiMo and pushed with Git;
connector writes remain small and focused;
large tasks are split into Issue + task-spec files;
PROJECT_STATE stores current state, not unlimited history;
raw data, binaries and secrets never enter normal Git.
```

## Engineering improvement log

```text
docs/engineering/FUTURE_REWRITE_NOTES.md
```

Only evidence-backed architecture, correctness, performance, reliability and operability findings belong there.

## Immediate next actions

```text
1. Owner gives MiMo the instruction to start Issue #14.
2. MiMo implements RT-1 locally within the specification.
3. MiMo runs build, new tests and existing QSH regressions.
4. MiMo commits, pushes and publishes the required report.
5. Architecture/Review Agent checks diff, tests and scope.
6. Owner accepts RT-1 or requests corrections.
7. RT-2 remains blocked until RT-1 acceptance.
8. MOEX response is processed separately when received.
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
