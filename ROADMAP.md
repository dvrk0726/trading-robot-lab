# ROADMAP

Дата создания: 2026-07-08
Статус: active roadmap

## Главная цель

Построить не одного монолитного торгового робота, а две отдельные системы с общими контрактами:

```text
Trading Lab      — исследование, тестирование, replay, отчеты, анализ.
Trading Runtime  — легкое исполнение утвержденных Strategy Packages.
Shared Contracts — единые форматы данных, сигналов, заявок, risk decisions и отчетов.
```

## Current Strategic Decision

Принято архитектурное решение:

```text
decisions/ADR-0002-two-system-lab-runtime-architecture.md
```

Суть:

```text
Trading Lab cannot send real orders.
Trading Runtime cannot run unapproved strategy packages.
Every OrderIntent must pass RiskEngine.
Live mode is disabled by default and requires owner approval.
```

## Phase 0 — Knowledge Base and Architecture Foundation

Статус: mostly done

### Done

```text
AI_CONTEXT.md
PROJECT_STATE.md
SECURITY.md
docs/01_hybrid_architecture.md
decisions/ADR-0001-hybrid-python-cpp-architecture.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
.github/ISSUE_TEMPLATE/ai_agent_task.md
strategy_knowledge_base/
```

### Added after research materials

```text
strategy_knowledge_base/research_notes/NOTE-20260708-002-market-maker-algorithms-parts-1-4.md
strategy_knowledge_base/research_notes/NOTE-20260708-003-market-maker-algorithms-parts-5-8.md
strategy_knowledge_base/research_notes/NOTE-20260708-004-bulashev-statistics-for-traders.md
strategy_knowledge_base/research_notes/NOTE-20260708-005-avellaneda-stoikov-limit-order-book.md
strategy_knowledge_base/research_notes/NOTE-20260708-006-r0man-market-maker-test-siu5.md
strategy_knowledge_base/research_notes/NOTE-20260708-007-robot-uralpro-hft-context.md
strategy_knowledge_base/ideas/IDEA-20260708-002-inventory-imbalance-market-making.md
strategy_knowledge_base/ideas/IDEA-20260708-003-regime-aware-market-making-price-function.md
strategy_knowledge_base/ideas/IDEA-20260708-004-ri-synthetic-index-lead-lag.md
strategy_knowledge_base/evaluations/EVAL-20260708-001-siu5-market-maker-r0man.md
docs/trading_robot_vision_and_research_plan.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
docs/system_architecture_and_user_interface_requirements.md
```

## Phase 1 — Shared Contracts and Repository Skeleton

Статус: next

Цель: создать минимальную структуру проекта и общие схемы, чтобы Lab и Runtime не расходились.

### Tasks

1. Create repository folders:

```text
apps/lab/
apps/runtime/
shared/contracts/
shared/schemas/
shared/strategy_sdk/
shared/test_vectors/
strategy_packages/examples/
```

2. Define first schemas:

```text
MarketEvent
FeatureSnapshot
StrategySignal
OrderIntent
RiskDecision
PositionSnapshot
TradeEvent
RuntimeLog
```

3. Define Strategy Package standard:

```text
manifest.yaml
params.yaml
risk_limits.yaml
instruments.yaml
validation_report.json
approval.json
package.hash
```

4. Create example dummy strategy package:

```text
strategy_packages/examples/dummy_no_trade_v001/
```

5. Create signal parity test concept:

```text
test_vectors/market_events.csv
test_vectors/expected_signals.csv
test_vectors/expected_order_intents.csv
```

### Done Criteria

```text
Lab and Runtime have common contracts.
Runtime can load a dummy Strategy Package.
Runtime can reject package if approval/hash/schema is invalid.
No broker adapter exists yet.
```

## Phase 2 — Trading Lab Research MVP

Статус: upcoming

Цель: первая рабочая лаборатория для исследования RI / Synthetic Index Lead-Lag.

### Tasks

1. Data import MVP:

```text
load RI futures data
load synthetic index components or prepared synthetic index sample
validate timestamps
handle missing data
```

2. Feature MVP:

```text
synthetic_index
spread = RI - synthetic_index
returns
rolling volatility
session time bucket
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
No trading logic is active.
No broker connection exists.
```

## Phase 3 — Backtest and Metrics MVP

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

## Phase 4 — Trading Runtime Skeleton

Статус: upcoming

Цель: создать отдельную легкую программу Runtime, пока без брокера.

### Tasks

1. Runtime config loader.
2. Strategy Package loader.
3. Strategy Package validator.
4. Simple TradeAgent interface.
5. RiskEngine minimal rules.
6. OrderManager stub.
7. Telemetry/logging.
8. Replay/paper event input.

### Done Criteria

```text
Runtime loads approved package.
Runtime receives MarketEvent stream.
Runtime produces OrderIntent.
Runtime applies RiskDecision.
Runtime writes audit logs.
Runtime cannot send real orders.
```

## Phase 5 — Event-Driven Replay Bridge

Статус: later

Цель: проверить реалистичность исполнения.

### Tasks

```text
event stream playback
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
```

## Phase 6 — Paper Trading

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

## Phase 7 — Live Gate Preparation

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

## Current Priority

Immediate next tasks:

```text
1. Create shared contracts and repository skeleton.
2. Formalize STRAT for RI / Synthetic Index Lead-Lag.
3. Create GitHub Issue for Python Research Agent: lead-lag prototype.
4. Create GitHub Issue for Architecture/Runtime Agent: shared contracts and Strategy Package standard.
```

## Explicit Non-Goals Now

```text
no live trading
no broker connection
no real API keys
no colocation setup
no ultra-low-latency optimization
no direct port of old robot into production
```

## Project Principle

```text
Research first.
Runtime only executes approved packages.
Risk Engine is mandatory.
Live is a future owner-gated stage.
```
