# Trading Robot Vision and Research Plan

Дата создания: 2026-07-08
Статус: strategic project vision / working plan

## Краткий вывод

Проект не должен развиваться как попытка быстро запустить старого HFT-робота.

Правильная цель:

```text
создать модульную торговую лабораторию для фьючерсов MOEX,
которая умеет исследовать, тестировать, симулировать и только потом осторожно запускать стратегии.
```

Наша целевая система:

```text
Research Platform
+ Strategy Knowledge Base
+ Data Pipeline
+ Backtest Engine
+ Event-driven Replay
+ Risk Engine
+ Paper Trading
+ Future Low-latency Execution Core
```

## Что мы узнали из обработанных материалов

### 1. Старый robot_uralpro ценен как источник идеи, но не как готовый робот

Исторически робот дал сильный результат на ЛЧИ 2010, но сам автор прямо указывает, что в прежнем виде алгоритм уже не работает и требует адаптации к современным биржевым параметрам.

Из старого робота берем:

```text
RI / synthetic index idea
structure of HFT robot
market data / execution separation
order management thinking
historical implementation lessons
```

Не берем напрямую:

```text
old live code
old connectors as production solution
old assumptions about microstructure
old execution logic without modern replay
```

### 2. Главная первая стратегия-кандидат: RI / synthetic index lead-lag stat arb

Старая идея:

```text
RI futures follows synthetic index
```

Современное уточнение:

```text
надо измерять, кто кого реально опережает: RI futures или synthetic index.
```

В материалах отмечается, что на современном рынке фьючерс часто двигается раньше синтетического индекса из-за большей активности, волатильности и меньшего порога входа.

Наша формулировка:

```text
Do not assume lead-lag direction.
Measure it from data.
```

### 3. Вторая стратегия-кандидат: regime-aware inventory market making

Из серии материалов по маркетмейкеру и статьи Avellaneda-Stoikov:

```text
market making cannot be just bid=best_bid and ask=best_ask.
```

Нужны:

```text
inventory risk control
adverse selection filter
depth imbalance feature
spread regime filter
volatility regime filter
policy map / lookup table
realistic fill model
cancel latency model
```

### 4. Бэктест не равен реальности

Исторические тесты на Si показывают:

```text
simple backtest can look good;
real trading is worse because of packetized data, stale signals, cancel latency, queue/fill effects, fees and volatility.
```

Поэтому обязательный pipeline:

```text
simple backtest -> event-driven backtest -> replay with latency -> paper -> owner gate
```

### 5. Статистика — отдельный фундамент проекта

Из книги Булашева берем project rule:

```text
No strategy is accepted without statistical validation.
```

Нужны:

```text
sample size
expected trade return
confidence interval
PnL distribution
skew/kurtosis
MAE/MFE
drawdown
loss streaks
probability of ruin
VaR / Expected Shortfall
out-of-sample
parameter stability
```

## Наше целевое видение

### Версия 1: Research Lab

Сначала строим не торгового робота, а исследовательскую систему:

```text
historical data loader
synthetic index builder
feature engine
lead-lag analyzer
spread/residual analyzer
market-making feature analyzer
backtest report generator
risk metrics module
```

Цель: быстро и безопасно проверять гипотезы.

### Версия 2: Event-driven Simulator / Replay

После базового research:

```text
market events
order book events
order placement
cancel/replace
partial fills
queue approximation
latency model
commission model
slippage model
```

Цель: понять, выживает ли стратегия в реалистичном исполнении.

### Версия 3: Paper Trading

Только после replay:

```text
live market data
paper orders
real-time risk engine
no real order sending
compare paper result with simulated expectations
```

### Версия 4: Low-latency Core

Только если strategy edge survives research/replay/paper:

```text
C++ core
FAST market data adapter
TWIME/FIX order adapter
strict risk gate
kill switch
owner-controlled live mode
```

## Целевая архитектура

```text
Market Data Sources
  -> Data Normalizer
  -> Historical Storage
  -> Feature Engine
      -> synthetic index
      -> lead-lag features
      -> spread/residual
      -> depth imbalance
      -> volatility/regime
      -> fill probability features
  -> Strategy Engine
      -> RI synthetic lead-lag strategy
      -> inventory market making strategy
      -> future strategy plugins
  -> Risk Engine
      -> max position
      -> max daily loss
      -> max drawdown
      -> stale data check
      -> volatility kill switch
      -> order rate limit
      -> duplicate order protection
  -> Execution Layer
      -> backtest execution
      -> replay execution
      -> paper execution
      -> future live execution
  -> Reporting
      -> backtest report
      -> MAE/MFE
      -> PnL distribution
      -> regime breakdown
      -> strategy evaluation
```

## Основные модули

### Data module

Должен уметь:

```text
load historical prices
load tick/order book data if available
align timestamps
handle missing data
store data outside Git if large
produce clean event stream
```

### Feature Engine

Первый набор features:

```text
synthetic_index
RI_minus_synthetic_spread
lead_lag_cross_correlation
depth_imbalance
spread_state
realized_volatility
trade_intensity
session_time_bucket
regime_label
```

### Strategy Engine

Стратегии не должны отправлять заявки напрямую.

Они должны выдавать:

```text
OrderIntent
```

Пример:

```text
signal_id
strategy_id
instrument
side
quantity
order_type
price_policy
reason
risk_tags
confidence
expiry_time
```

### Risk Engine

Risk engine — обязательный gate.

Он может:

```text
allow
reduce
reject
cancel_all
switch_to_safe_mode
```

### Execution Engine

Исполнение делится на режимы:

```text
backtest
replay
paper
live
```

Live disabled by default.

## Приоритетные стратегии

### Strategy A: RI / Synthetic Index Lead-Lag

Цель:

```text
проверить, существует ли стабильная lead-lag связь между RI futures и synthetic index.
```

Первые вопросы:

```text
Who leads?
At what lag?
Is lag stable by session segment?
Does residual/spread mean-revert?
Does signal survive fees/slippage?
Does signal survive slower calculation interval?
```

Почему это первая стратегия:

```text
она напрямую связана со старым robot_uralpro;
она может быть исследована без live execution;
она требует статистики, но не обязательно сразу полного HFT;
она хорошо подходит для Python research.
```

### Strategy B: Regime-aware Inventory Market Making

Цель:

```text
создать research prototype для market making с inventory control, imbalance filter and volatility regime filter.
```

Первые вопросы:

```text
Does book imbalance predict next move or fill quality?
Does spread bucket produce net edge after fees?
When does market making fail?
Can volatility filter reduce losses?
How much cancel latency destroys the edge?
```

Почему это вторая стратегия:

```text
она требует более сложных данных;
нужен order book / event replay;
слишком опасно начинать с нее как live;
но она важна как развитие HFT direction.
```

## Что делаем сначала

### Phase 1: Documentation and formal specs

Создать:

```text
strategy_knowledge_base/ideas/IDEA-...-ri-synthetic-index-lead-lag.md
strategy_knowledge_base/strategies/STRAT-...-ri-synthetic-index-lead-lag.md
docs/data_requirements_moex_futures.md
docs/backtest_report_standard.md
docs/strategy_statistical_validation_checklist.md
```

### Phase 2: Python research prototype

Создать GitHub Issue:

```text
[PYTHON] Build RI synthetic index lead-lag research prototype
```

Минимальный результат:

```text
load sample data
build synthetic index
align RI and synthetic series
calculate cross-correlation by lag
show leader/lagger stability
produce markdown report
```

### Phase 3: Backtest metrics module

Создать:

```text
mean/median trade PnL
std/skew/kurtosis
profit factor
max drawdown
drawdown duration
loss streaks
MAE/MFE
VaR / Expected Shortfall
commission/slippage sensitivity
```

### Phase 4: Event-driven replay skeleton

После появления гипотезы с edge:

```text
event stream
orders
fills
cancel/replace
latency
fees
position
PnL
```

## Чего пока не делаем

```text
не подключаем брокера
не пишем live execution
не используем реальные ключи
не обещаем прибыль
не переносим старый робот как есть
не начинаем с колокации
не оптимизируем микросекунды до подтверждения edge
```

## Критерии перехода между этапами

### Idea -> Strategy Spec

Нужно:

```text
clear hypothesis
required data
entry/exit/risk defined
validation plan
```

### Strategy Spec -> Backtest

Нужно:

```text
data available
formulas fixed
assumptions documented
fees specified
```

### Backtest -> Replay

Нужно:

```text
positive expectancy after fees
sufficient number of trades
acceptable drawdown
stable parameters
reasonable out-of-sample
```

### Replay -> Paper

Нужно:

```text
latency sensitivity acceptable
fill model acceptable
stale order risk controlled
risk engine tested
kill switch tested
```

### Paper -> Live Gate

Нужно:

```text
owner decision
security review
risk review
broker/exchange integration review
small size only
manual monitoring plan
```

## Наше рабочее направление сейчас

Immediate next step:

```text
formalize RI / synthetic index lead-lag idea
```

Then:

```text
write Python Agent task for lead-lag prototype
```

Only after that:

```text
market-making prototype and policy-table research
```

## Bottom line

Проект должен прийти к системе, где новый ИИ-разработчик может открыть GitHub и увидеть:

```text
what strategies are being researched
what data is needed
what tests must pass
what risk gates exist
what is allowed and forbidden
what the next task is
```

Главное видение:

```text
Build a disciplined trading research platform first.
A combat robot is a later product of validated research, not the starting point.
```
