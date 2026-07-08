# System Architecture and User Interface Requirements

Дата создания: 2026-07-08
Статус: working requirements / developer handoff draft
Связано с: `decisions/ADR-0002-two-system-lab-runtime-architecture.md`

## Цель документа

Этот документ объясняет разработчику, какую систему мы строим, из каких частей она должна состоять и как она должна выглядеть для пользователя.

## Главный принцип

Мы строим не одну большую программу, а две отдельные системы с общими контрактами:

```text
Trading Lab      — исследование, тестирование, replay, отчеты, анализ.
Trading Runtime  — легкое исполнение утвержденных Strategy Packages.
Shared Contracts — общие форматы данных, сигналов, заявок, отчетов.
```

## Почему две системы

Trading Lab и Trading Runtime имеют разные требования.

### Trading Lab

```text
удобство важнее скорости
можно использовать тяжелую аналитику
нужны графики и отчеты
нужны эксперименты
нужен подбор параметров
нужны бэктесты и replay
```

### Trading Runtime

```text
скорость и надежность важнее удобства
никакой исследовательской логики в critical path
только утвержденные стратегии
обязательный risk engine
строгие логи
минимальный операторский UI
```

## High-level Architecture

```text
                ┌────────────────────────┐
                │      Trading Lab       │
                │ research/backtest/replay│
                └───────────┬────────────┘
                            │
                            │ Strategy Package
                            v
                ┌────────────────────────┐
                │    Shared Contracts    │
                │ schemas/test vectors   │
                └───────────┬────────────┘
                            │
                            v
                ┌────────────────────────┐
                │   Trading Runtime      │
                │ risk + order execution │
                └───────────┬────────────┘
                            │
                            v
                ┌────────────────────────┐
                │ Broker / Exchange API  │
                │ future stage only      │
                └────────────────────────┘
```

## 1. Trading Lab Requirements

Trading Lab is the main research and operator analysis application.

### Functional Requirements

Trading Lab must support:

```text
historical data loading
market data quality checks
synthetic index construction
lead-lag analysis
feature research
statistical validation
simple backtest
event-driven replay
commission/slippage sensitivity
parameter comparison
strategy evaluation reports
paper/live log import
real trading analysis
Strategy Package export
```

### Trading Lab must NOT

```text
send real orders
store real broker secrets in repository
connect directly to live broker for execution
allow bypassing risk engine
mark strategy as live-approved automatically
```

## 2. Trading Runtime Requirements

Trading Runtime is a separate lightweight execution application.

### Functional Requirements

Trading Runtime must support:

```text
load runtime config
load approved Strategy Packages
validate package hash and approval status
run signal parity test before enabling package
connect to market data source
produce FeatureSnapshot
run TradeAgent
receive OrderIntent
run RiskEngine
manage active orders
manage position
publish telemetry
write audit logs
trigger kill switch
```

### Trading Runtime must NOT

```text
run research notebooks
optimize parameters
draw reports in critical path
load unapproved strategies
allow strategy direct broker access
trade live by default
```

## 3. Shared Contracts Requirements

Shared contracts prevent logical drift between Trading Lab and Trading Runtime.

Required schemas:

```text
MarketEvent
FeatureSnapshot
StrategySignal
OrderIntent
RiskDecision
OrderState
TradeEvent
PositionSnapshot
StrategyPackage
BacktestReport
RuntimeLog
```

Each schema must be:

```text
versioned
documented
tested
backward-compatible when possible
```

## 4. Strategy Package Standard

Strategy Package is the only allowed way to move a strategy from Lab to Runtime.

Minimal package layout:

```text
strategy_package/
  manifest.yaml
  params.yaml
  risk_limits.yaml
  instruments.yaml
  trade_agent
  validation_report.json
  test_vectors/
    market_events.csv
    expected_features.csv
    expected_signals.csv
    expected_order_intents.csv
    expected_risk_decisions.csv
  approval.json
  package.hash
```

### Required manifest fields

```text
strategy_id
strategy_version
created_at
author
allowed_modes
required_market_data
required_features
min_runtime_version
validation_report_id
package_hash
```

### Required approval states

```text
research_approved
backtest_approved
replay_approved
paper_approved
live_approved
```

Default:

```text
live_approved = false
```

## 5. Risk Engine Requirements

Risk Engine must exist in Runtime from the beginning, even before real live trading.

Minimum controls:

```text
max_position
max_order_size
max_daily_loss
max_drawdown
max_orders_per_second
max_signal_age_ms
max_market_data_staleness_ms
max_order_age_ms
max_spread
max_volatility
session_end_protection
duplicate_order_protection
kill_switch
```

Risk Engine output:

```text
ALLOW
REDUCE
REJECT
CANCEL_ALL
STOP_STRATEGY
SAFE_MODE
```

Every OrderIntent must receive a RiskDecision.

## 6. Trading Lab UI

Trading Lab should have a full web interface.

### Main navigation

```text
Dashboard
Strategies
Data
Research Lab
Backtests
Replay
Reports
Real Trading Analysis
Strategy Packages
Risk Templates
Settings
```

### Dashboard

Should show:

```text
system status
current stage
live disabled/enabled state
active research items
last backtest/replay results
data quality warnings
risk warnings
open GitHub tasks/issues
```

### Strategies Page

For each strategy:

```text
strategy name
hypothesis
status
required data
latest report
current stage
allowed modes
related files
next action
```

Statuses:

```text
idea
research
strategy_spec
backtest
replay
paper
rejected
paused
```

### Data Page

Should show:

```text
loaded instruments
loaded date ranges
data source
timestamp precision
missing data
session calendar
contract expiration
commission model
quality score
```

### Research Lab Page

For RI / Synthetic Index Lead-Lag:

```text
synthetic index chart
RI vs synthetic chart
spread chart
lead-lag correlation by lag
rolling lead-lag heatmap
session segment breakdown
volatility regime breakdown
research notes
```

### Backtest Page

Should support:

```text
choose strategy
choose date range
choose parameter set
choose commission model
choose slippage model
choose risk template
run backtest
compare runs
open report
export result
```

Backtest result must show:

```text
PnL
equity curve
number of trades
average trade
median trade
profit factor
max drawdown
drawdown duration
PnL distribution
MAE/MFE
commission sensitivity
slippage sensitivity
regime breakdown
risk rejects
```

### Replay Page

Should support:

```text
select historical day
select event stream
set market data delay
set order delay
set cancel delay
set queue model
run replay
compare with simple backtest
```

### Real Trading Analysis Page

Should import Runtime logs and show:

```text
real/paper trades
expected vs actual fills
signal-to-order latency
order-to-ack latency
cancel latency
slippage
missed fills
risk rejects
strategy drift
PnL vs backtest expectation
```

### Strategy Packages Page

Should show:

```text
package id
strategy id
version
allowed modes
validation report
approval state
hash
signal parity status
export status
runtime compatibility
```

## 7. Trading Runtime UI

Trading Runtime should have minimal operator UI.

### Runtime Dashboard

Show only critical state:

```text
Runtime status: STOPPED / RUNNING / SAFE_MODE
Mode: REPLAY / PAPER / LIVE
Live trading: DISABLED / ENABLED
Market data: OK / STALE
Broker: DISCONNECTED / CONNECTED
Active strategies
Current positions
PnL today
Risk status
Last risk event
Last order event
Kill switch button
```

Runtime UI must not contain:

```text
research charts
parameter optimization
manual strategy editing
backtest exploration
complex analytics
```

## 8. MVP Scope

The first MVP should not include live trading.

### MVP 1: Trading Lab Research MVP

```text
historical data import
synthetic index prototype
RI/synthetic lead-lag analyzer
basic report generation
strategy idea/spec linkage
```

### MVP 2: Backtest MVP

```text
simple backtest engine
fees/slippage model
standard metrics report
risk engine in simulation mode
```

### MVP 3: Runtime Skeleton

```text
load Strategy Package
validate package
run on replay/paper stream
produce OrderIntent
run RiskEngine
write logs
no broker adapter
```

### MVP 4: Replay Bridge

```text
event-driven replay
latency simulation
cancel simulation
runtime log import back into Trading Lab
```

## 9. Suggested Repository Structure

```text
apps/
  lab/
    backend/
    frontend/
    research/
    reports/

  runtime/
    core/
    risk/
    order_manager/
    strategy_loader/
    telemetry/
    execution/

shared/
  contracts/
  schemas/
  strategy_sdk/
  test_vectors/

strategy_packages/
  examples/

docs/
decisions/
strategy_knowledge_base/
```

## 10. Non-negotiable Rules

```text
Trading Lab cannot send real orders.
Trading Runtime cannot run unapproved packages.
Strategy cannot call broker directly.
Every order must pass RiskEngine.
Live mode disabled by default.
Owner approval required for live.
Signal parity required before runtime launch.
All runtime actions must be logged.
```

## Immediate Developer Tasks

1. Create repository structure for `apps/lab`, `apps/runtime`, `shared/contracts`.
2. Define first contract schemas: `MarketEvent`, `FeatureSnapshot`, `OrderIntent`, `RiskDecision`.
3. Define `StrategyPackage` manifest format.
4. Create Trading Lab MVP screen list / wireframe draft.
5. Create Runtime skeleton without broker adapter.
6. Create first Python research issue for RI/synthetic lead-lag.

## Bottom Line

The system should be built as:

```text
separate programs
shared contracts
strict risk gate
strategy packages
reproducible research-to-runtime handoff
```

This architecture is more work at the beginning, but it is safer, cleaner and better suited for eventual low-latency trading.
