# ROADMAP

Дата создания: 2026-07-08  
Дата последнего обновления: 2026-07-08  
Статус: active roadmap

## Главная цель

Построить не одного монолитного торгового робота, а две отдельные системы с общими контрактами:

```text
Trading Lab      — исследование, тестирование, replay, отчеты, анализ.
Trading Runtime  — легкое исполнение утвержденных Strategy Packages.
Shared Contracts — единые форматы данных, сигналов, заявок, risk decisions и отчетов.
```

## Current Strategic Decisions

Приняты архитектурные решения:

```text
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
```

Суть:

```text
Trading Lab cannot send real orders.
Trading Runtime cannot run unapproved strategy packages.
Every OrderIntent must pass RiskEngine.
Live mode is disabled by default and requires owner approval.
C++20 is the primary language for QSH/OrdLog ingest and future low-level replay/runtime core.
Python remains the research, dashboard, analysis and reporting layer.
```

## Current Priority — M9 C++ QSH / OrdLog Data Layer

Текущий главный фокус:

```text
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
```

Цель текущего этапа:

```text
Raw QSH / OrdLog / Quotes / Deals / AuxInfo
  -> C++20 parser / normalizer
  -> L3 order book reconstruction
  -> L2 snapshots / event stream
  -> Data Quality report
  -> Python Trading Lab visualization and research
```

Ключевое правило:

```text
2021 OrdLog = engineering sample, not current trading evidence.
```

Исторический `OrdLog.qsh` используется для:

```text
parser development
L3 order book reconstruction
replay mechanics
queue/fill model prototype
Data Quality checks
historical order book visualization
```

Он не используется как доказательство текущей прибыльности стратегии.

## Phase 0 — Knowledge Base and Architecture Foundation

Статус: done / maintained

### Done

```text
AI_CONTEXT.md
PROJECT_STATE.md
SECURITY.md
docs/01_hybrid_architecture.md
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
.github/ISSUE_TEMPLATE/ai_agent_task.md
strategy_knowledge_base/
```

## Phase 1 — Shared Contracts and Repository Skeleton

Статус: mostly done

### Done / already started

```text
apps/lab/
apps/runtime/
shared/schemas/
shared/test_vectors/
strategy_packages/examples/
Strategy Package standard
dummy no-trade package
basic shared JSON schemas
basic validation examples
Trading Lab demo dashboard
Trading Lab demo charts
```

### Remaining cleanup

```text
1. Keep schemas aligned with QSH / microstructure events.
2. Add normalized market data contracts for OrdLog, L3 events and L2 snapshots.
3. Keep PROJECT_STATE.md updated after each major MiMo task.
```

## Phase 2 — Historical Market Data Layer MVP

Статус: current / in progress through M9

Цель: создать первый настоящий слой данных Trading Lab для микроструктурного анализа.

### Tasks

1. C++ QSH ingest skeleton:

```text
cpp/qsh_ingest/
CMake build
qsh-ingest CLI
inspect / quality / convert / l3-to-l2 commands
```

2. QSH stream support:

```text
OrdLog.qsh first priority
Quotes.qsh
Deals.qsh
AuxInfo.qsh
```

3. OrdLog reconstruction:

```text
TxEnd transaction grouping
NewSession reset
Add / Fill / Cancel / Remove classification
L3 order book reconstruction
L3 -> L2 snapshot export
```

4. Data Quality:

```text
records_total
records_valid
records_rejected
first_timestamp
last_timestamp
stream_type
instrument
new_session_count
tx_count
add_count
fill_count
cancel_count
remove_count
unknown_side_count
non_system_count
book_reconstruction_errors
warnings
```

5. Trading Lab integration:

```text
show imported files
show Data Quality status
show stream type
show warnings/errors
show historical order book availability
prepare order book replay UI
```

### Done Criteria

```text
C++ qsh-ingest builds.
QSH header and stream type are detected.
OrdLog records can be scanned.
Data Quality report is generated.
Limited L3 -> L2 reconstruction works safely.
Python Trading Lab can display generated quality metadata.
No raw real QSH is committed.
No broker/live/order sending exists.
```

## Phase 3 — Trading Lab Research MVP

Статус: upcoming after M9 data layer

Цель: первая рабочая лаборатория для исследования RI / Synthetic Index Lead-Lag and market microstructure signals.

### Tasks

1. Load normalized data:

```text
L2 snapshots
trades
spread
mid price
depth
book imbalance
```

2. Feature MVP:

```text
synthetic_index
spread = RI - synthetic_index
returns
rolling volatility
session time bucket
bid/ask imbalance
trades per second
cancel/add ratio
```

3. Lead-lag analyzer:

```text
cross-correlation by lag
rolling lead-lag
leader/lagger classification
stability by session segment
```

4. Research report:

```text
charts
summary metrics
data quality notes
hypothesis verdict: continue / reject / needs more data
```

### Done Criteria

```text
The system can answer: does RI lead synthetic or synthetic lead RI on a sample dataset?
The answer is marked as historical/sample-only unless current data is used.
No trading logic is active.
No broker connection exists.
```

## Phase 4 — Backtest and Metrics MVP

Статус: upcoming

Цель: добавить простой backtest and standardized reports.

### Tasks

1. Define formal STRAT file:

```text
strategy_knowledge_base/strategies/STRAT-20260708-001-ri-synthetic-index-lead-lag.md
```

2. Implement simple backtest execution:

```text
signal generation
entry/exit assumptions
fees
basic slippage
position tracking
PnL
```

3. Implement metrics:

```text
number of trades
win rate
average trade
median trade
profit factor
max drawdown
drawdown duration
PnL distribution
MAE/MFE
commission sensitivity
slippage sensitivity
```

4. Produce backtest report.

### Done Criteria

```text
Backtest report is reproducible.
Parameters and data range are recorded.
Risk Engine runs in simulation mode.
```

## Phase 5 — Event-Driven Replay Bridge

Статус: later

Цель: проверить реалистичность исполнения.

### Tasks

```text
event stream playback
historical order book replay
order lifecycle simulation
partial fills
cancel/replace simulation
latency model
stale signal detection
queue approximation
commission/slippage model
runtime logs imported back into Lab
```

### Done Criteria

```text
Replay result can be compared against simple backtest.
Differences are visible and explainable.
Historical order book state is visible in Trading Lab.
```

## Phase 6 — Trading Runtime Skeleton

Статус: later / after data layer and replay foundation

Цель: создать отдельную легкую программу Runtime, пока без брокера.

### Tasks

```text
runtime config loader
Strategy Package loader
Strategy Package validator
TradeAgent interface
RiskEngine minimal rules
OrderManager stub
Telemetry/logging
Replay/paper event input
```

### Done Criteria

```text
Runtime loads approved package.
Runtime receives MarketEvent stream.
Runtime produces OrderIntent.
Runtime applies RiskDecision.
Runtime writes audit logs.
Runtime cannot send real orders.
```

## Phase 7 — Paper Trading

Статус: later / owner-gated

Цель: подключить live market data but no real orders.

### Tasks

```text
live market data input
paper orders
real-time risk engine
paper fills approximation
runtime health checks
Trading Lab paper analysis
```

### Done Criteria

```text
Paper trading can run without real broker order sending.
Runtime logs can be analyzed in Trading Lab.
```

## Phase 8 — Live Gate Preparation

Статус: future / blocked by owner decision

Live is not a current task.

Before any live work:

```text
strategy must pass research/backtest/replay/paper
risk review required
security review required
owner approval required
broker adapter design required
kill switch tested
small size only
manual monitoring plan required
```

## Explicit Non-Goals Now

```text
no live trading
no broker connection
no real API keys
no colocation setup
no real order sending
no direct port of old robot into production
no profitability claims from old historical data
```

## Project Principle

```text
Research first.
Data Quality first.
Runtime only executes approved packages.
Risk Engine is mandatory.
Live is a future owner-gated stage.
```
