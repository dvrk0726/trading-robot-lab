# IDEA-20260708-003: Regime-Aware Market Making with Price Function Filter

Дата создания: 2026-07-08
Статус: raw / needs_clarification / needs_modern_data_validation
Источник: `NOTE-20260708-003-market-maker-algorithms-parts-5-8.md`

## Краткое описание

Идея: market making должен включаться не всегда, а только в подходящем рыночном режиме.

Перед запуском inventory-aware quoting нужно определить:

```text
market making regime
momentum regime
cycle regime
no-trade / toxic regime
```

Если рынок находится в сильном импульсе, one-sided order flow или news/volatility spike, market making должен быть выключен или заменен другим режимом.

## Рыночная гипотеза

Market making работает лучше, когда:

```text
order flow balanced
price moves inside range
spread compensates execution risk
volatility not abnormal
no strong directional impulse
```

Market making опасен, когда:

```text
strong momentum
one-sided aggressive flow
news gap
fast regime shift
spread collapses faster than fills/cancels
cancel latency is large
```

## Почему это важно

Без regime filter маркетмейкер получает adverse selection:

```text
market goes up -> ask quotes filled -> short inventory against rising price
market goes down -> bid quotes filled -> long inventory against falling price
```

Поэтому перед котированием нужен отдельный слой:

```text
Regime Classifier / Price Function Engine
```

## Price function

Price function должна оценивать:

```text
expected mean price over forecast horizon
slope
curvature
noise bands
mean reversion activity
trend/cycle amplitude
trend/cycle period
```

В market making режиме нужна почти линейная/монотонная функция, которая показывает среднее значение цены внутри шума.

В momentum/cycle режиме нужна функция, которая быстрее следует за кривизной и помогает определить направление/разворот.

## Candidate features

```text
short-term price slope
price curvature
realized volatility
range score
trend score
cycle score
spread width
spread stability
trade intensity
order flow imbalance
book imbalance
volume imbalance persistence
return autocorrelation
mean reversion score
news/session/clearing flags
```

## Candidate regime labels

```text
MARKET_MAKING_OK
MARKET_MAKING_SPREAD_TOO_LOW
MOMENTUM_RISK_UP
MOMENTUM_RISK_DOWN
CYCLE_MODE
HIGH_VOL_NO_TRADE
LOW_LIQUIDITY_NO_TRADE
CLEARING_NEAR_NO_TRADE
UNKNOWN_NO_TRADE
```

## Use inside strategy

Architecture:

```text
Market Data
  -> Feature Engine
  -> Price Function Engine
  -> Regime Classifier
  -> Inventory / Imbalance Policy Engine
  -> Risk Engine
  -> Execution / Backtest / Replay
```

Decision rule example:

```text
if regime != MARKET_MAKING_OK:
    cancel market making quotes
    do not open new MM inventory
    only reduce existing inventory if risk requires
else:
    allow inventory-aware market making policy
```

## Required tests

### Test 1: Regime labeling vs PnL

Проверить:

```text
PnL of market making by regime bucket
adverse selection by regime bucket
fill quality by regime bucket
```

### Test 2: Spread threshold

Проверить:

```text
PnL after fees by spread bucket
```

В старом исследовании полезным оказался фильтр Make Strategy only when S > 4, но это нужно заново проверять на современных данных.

### Test 3: Price function stability

Проверить разные price function approaches:

```text
spline least squares
signal filters
Kalman filter
simple rolling linear fit
robust regression
Bayesian/SDE later
```

### Test 4: Live/backtest mismatch

Проверить чувствительность к:

```text
cancel latency
packetized data arrival
stale signal duration
queue position
partial fills
```

## Risks

### 1. Regime classifier overfitting

Красивый classifier может просто подгонять историю.

### 2. Late detection

Если режим определяется с задержкой, market maker уже успеет получить плохой inventory.

### 3. Too many no-trade periods

Слишком строгий фильтр может почти полностью выключить стратегию.

### 4. Price function instability

Kalman-like systems могут быть нестабильными при неожиданных шумах и плохой настройке ковариаций.

### 5. Spread filter may not generalize

Старый фильтр `S > 4` мог быть artifact конкретного Si sample.

## Why it could work

- Убирает market making из токсичных моментов.
- Снижает adverse selection.
- Позволяет использовать разные стратегии для разных режимов.
- Хорошо ложится на архитектуру Feature Engine -> Strategy Engine -> Risk Engine.

## Why it could fail

- Режимы трудно классифицировать в real time.
- Реальный рынок меняется быстрее фильтра.
- Увеличивается сложность системы.
- Ошибочная классификация режима может быть хуже, чем отсутствие фильтра.

## Current decision

```text
Research only.
Do not implement live.
Use as filter concept for future strategy/backtest specification.
```

## Related files

```text
strategy_knowledge_base/research_notes/NOTE-20260708-003-market-maker-algorithms-parts-5-8.md
strategy_knowledge_base/ideas/IDEA-20260708-002-inventory-imbalance-market-making.md
```
