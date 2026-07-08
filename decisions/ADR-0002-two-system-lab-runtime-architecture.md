# ADR-0002: Two-System Architecture — Trading Lab and Trading Runtime

Дата: 2026-07-08
Статус: accepted

## Контекст

После анализа материалов по `robot_uralpro`, Avellaneda-Stoikov, market-making алгоритмам, практическим тестам SIU5 и статистике для трейдеров стало понятно, что проект нельзя строить как одну универсальную программу, где одновременно живут:

```text
исследования
бэктесты
графики
подбор параметров
replay
анализ реальной торговли
боевое исполнение заявок
```

Для торговых систем, особенно с HFT/low-latency направлением, смешивание исследовательского кода и боевого исполнения опасно.

Основные риски одной большой программы:

```text
исследовательский код может случайно попасть в live path
тяжелые зависимости увеличивают задержки и сложность
сложнее гарантировать безопасность live режима
сложнее понять, какой код реально участвует в торговле
сложнее тестировать совпадение backtest/replay/live логики
выше риск случайной отправки реальных заявок
```

При этом полное разделение без общих стандартов тоже опасно:

```text
в лаборатории стратегия работает по одной логике,
а в live runtime переписана чуть иначе,
и результат в реальности не совпадает с тестами.
```

## Решение

Принимается архитектура из двух отдельных программ и одного общего слоя контрактов.

```text
1. Trading Lab
2. Trading Runtime
3. Shared Contracts / Strategy Package Standard
```

## 1. Trading Lab

Trading Lab — отдельная исследовательская и аналитическая система.

Назначение:

```text
загрузка исторических данных
проверка качества данных
построение synthetic index
lead-lag analysis
feature research
statistical validation
backtest
replay
paper result analysis
analysis of real trading logs
parameter search
reports
visualization
strategy package preparation
```

Trading Lab может быть тяжелой системой:

```text
Python
pandas / numpy
plotting
web UI
database
notebooks or research scripts
report generation
```

Trading Lab не должна иметь возможность отправлять реальные заявки.

Разрешенные режимы Trading Lab:

```text
research
backtest
replay
paper analysis
real trading analysis
```

Запрещено для Trading Lab:

```text
direct broker order sending
live execution
storage of real secrets in repository
manual bypass of risk engine
```

## 2. Trading Runtime

Trading Runtime — отдельная легкая программа для исполнения утвержденных стратегий.

Назначение:

```text
load approved strategy packages
receive market data
calculate only required runtime features
run trade_agent logic
produce OrderIntent
run RiskEngine
manage orders
manage positions
send orders only through ExecutionGateway
log all events
publish telemetry
trigger kill switch when needed
```

Trading Runtime не должна содержать:

```text
notebooks
heavy research code
parameter optimization
graph generation
large exploratory analytics
unapproved strategies
experimental code in critical path
```

Trading Runtime обязана содержать:

```text
RiskEngine
PositionManager
OrderManager
MarketDataHealthCheck
StrategyPackageValidator
KillSwitch
AuditLog
Telemetry
safe startup checks
```

Live mode остается disabled by default and owner-gated.

## 3. Shared Contracts

Чтобы две системы не расходились логически, вводится общий слой контрактов.

Общие структуры:

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

Shared Contracts должны быть версионированы.

Любое изменение схемы должно фиксироваться в docs / ADR and must not silently break Trading Runtime.

## Strategy Package

Trading Lab готовит `Strategy Package`.

Trading Runtime торгует только теми стратегиями, которые представлены в виде утвержденного пакета.

Минимальная структура:

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
    expected_signals.csv
    expected_order_intents.csv
  approval.json
  package.hash
```

### manifest.yaml

Должен содержать:

```text
strategy_id
strategy_version
created_at
author/agent
allowed_modes
required_market_data
required_features
min_runtime_version
validation_report_id
package_hash
```

### params.yaml

Содержит параметры стратегии.

### risk_limits.yaml

Содержит лимиты, специфичные для стратегии:

```text
max_position
max_order_size
max_daily_loss
max_drawdown
max_signal_age_ms
max_orders_per_second
max_spread
max_volatility
allowed_session_segments
```

### approval.json

Фиксирует, кто и когда разрешил пакет к режимам:

```text
research
replay
paper
live
```

Live approval requires explicit owner decision.

## Test Vectors / Signal Parity

Перед запуском Strategy Package в Runtime должен выполняться signal parity check.

Идея:

```text
на одном и том же потоке MarketEvent Trading Lab и Trading Runtime должны выдать одинаковые StrategySignal / OrderIntent.
```

Если проверка не проходит, Strategy Package не может быть запущен.

Минимальные test vectors:

```text
market_events.csv
expected_features.csv
expected_signals.csv
expected_order_intents.csv
expected_risk_decisions.csv
```

## Data Flow

```text
Trading Lab
  -> research/backtest/replay
  -> validation report
  -> Strategy Package
  -> manual approval
  -> Trading Runtime
  -> paper/live logs
  -> Trading Lab imports logs
  -> real trading analysis
```

## Repository Layout

На текущем этапе системы могут жить в одном private repository, но как отдельные приложения:

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
strategy_knowledge_base/
decisions/
```

Отдельный репозиторий для Runtime можно рассмотреть позже, когда runtime core станет зрелым.

## Technology Direction

### Trading Lab

Предпочтительно:

```text
Python
FastAPI or similar backend
web UI
PostgreSQL/ClickHouse/Parquet depending on data scale
pandas/numpy for research
```

### Trading Runtime

На раннем этапе:

```text
paper/replay runtime skeleton can be Python/C# if speed is not critical
```

На зрелом этапе:

```text
C++ core preferred for low-latency execution
```

Важно: преждевременный перенос в C++ запрещен до подтверждения strategy edge.

## Runtime Execution Flow

```text
MarketDataAdapter
  -> FeatureCalculator
  -> TradeAgent
  -> OrderIntent
  -> RiskEngine
  -> OrderManager
  -> ExecutionGateway
  -> AuditLog / Telemetry
```

TradeAgent не имеет прямого доступа к broker/exchange API.

## Safety Rules

```text
Trading Lab cannot send real orders.
Trading Runtime cannot run unapproved strategy packages.
Runtime live mode disabled by default.
Every OrderIntent must pass RiskEngine.
Every live-capable package must pass signal parity checks.
Every runtime action must be logged.
Owner approval required for any live mode.
```

## Consequences

### Positive

```text
clear separation between research and execution
lower risk of accidental live trading
smaller and faster runtime path
better reproducibility
cleaner developer responsibilities
future low-latency core remains possible
```

### Negative / Cost

```text
more engineering work
need to maintain shared contracts
need package validation and parity tests
need import/export between Lab and Runtime
```

## Final Decision

Adopt two-system architecture:

```text
Trading Lab — heavy research, testing, replay, reporting, analysis.
Trading Runtime — lightweight execution of approved Strategy Packages.
Shared Contracts — common schema and validation layer preventing logic drift.
```

This is the target architecture for further development.
