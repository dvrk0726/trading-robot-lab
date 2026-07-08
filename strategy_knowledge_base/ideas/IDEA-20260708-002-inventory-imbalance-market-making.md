# IDEA-20260708-002: Inventory-Aware Market Making with Depth Imbalance Filter

Дата создания: 2026-07-08
Статус: raw / needs_modern_data_validation
Источник: `NOTE-20260708-002-market-maker-algorithms-parts-1-4.md`

## Краткое описание

Идея: построить market making стратегию, которая не просто выставляет лимитные ордера на best bid / best ask, а управляет котировками в зависимости от:

```text
inventory
best-level depth imbalance
spread state
time to session end / clearing
commission
expected fill probability
```

Стратегия должна избегать двух главных рисков:

```text
adverse selection risk
inventory risk
```

## Рыночная гипотеза

На коротких горизонтах дисбаланс объемов в стакане может нести информацию о вероятном ближайшем движении цены.

Если учитывать этот сигнал вместе с текущей позицией, маркетмейкер может:

- не выставляться против токсичного потока;
- сдвигать котировки при накоплении позиции;
- сокращать позицию при опасном сочетании inventory + imbalance;
- иногда улучшать цену на 1 tick при широком spread;
- использовать market orders только как защитный или momentum-механизм.

## Базовая формула imbalance

```text
F = log(Q_best_bid) - log(Q_best_ask)
```

Интерпретация:

```text
F > 0 -> давление покупателей
F < 0 -> давление продавцов
```

## State variables

Минимальные состояния:

```text
t               current time / time to liquidation
q or y          inventory / open position
F               depth imbalance
S               spread state
mid             mid price
bid/ask         best bid / best ask
volatility      short-term volatility
commission      fee model
```

## Candidate actions

```text
HOLD
QUOTE_BOTH_SIDES
QUOTE_BID_ONLY
QUOTE_ASK_ONLY
IMPROVE_BID_ONE_TICK
IMPROVE_ASK_ONE_TICK
CANCEL_QUOTES
PARTIAL_REDUCE_INVENTORY
FULL_CLOSE_INVENTORY
MOMENTUM_BUY
MOMENTUM_SELL
```

## Policy intuition

### Normal state

Если inventory близок к нулю и imbalance нейтральный:

```text
quote both sides
```

### Long inventory + sell pressure

Если позиция длинная и imbalance показывает давление вниз:

```text
stop quoting bid
move ask more aggressively
partial/full reduce inventory
```

### Short inventory + buy pressure

Если позиция короткая и imbalance показывает давление вверх:

```text
stop quoting ask
move bid more aggressively
partial/full reduce inventory
```

### Strong imbalance

Если imbalance экстремальный:

```text
avoid quoting against direction
consider momentum action only in tested regime
```

### Wide spread

Если spread > 1 tick:

```text
consider pinging: improve bid/ask by one tick
```

## Required data

Для полноценной проверки нужны:

```text
order book snapshots / order log
best bid volume
best ask volume
bid/ask prices
spread
trades
market order arrivals if available
order events / queue changes if available
commission model
contract specs
time/session/clearing calendar
```

## Required tests

### Test 1: Predictive power of imbalance

Проверить:

```text
P(next price up | F > threshold)
P(next price down | F < -threshold)
```

на разных горизонтах:

```text
1 tick
5 ticks
10 ticks
100 ms
500 ms
1 sec
5 sec
```

### Test 2: Stationarity / OU fit

Проверить, можно ли моделировать imbalance как OU process:

```text
dF_t = -alpha_F * F_t * dt + sigma_F * dW_t
```

### Test 3: Fill probability

Оценить:

```text
probability of limit order fill depending on F, spread, queue position
```

### Test 4: Strategy simulation

Нужен event-driven backtest/replay:

```text
quotes
cancel/replace
fills
partial fills
inventory
PnL
commission
spread capture
adverse selection loss
```

## Risks

### 1. Signal decay

Imbalance может работать только на очень коротком горизонте, который недоступен при высокой latency.

### 2. Queue problem

Даже правильная лимитная цена не гарантирует исполнение из-за очереди.

### 3. Toxic flow

Лимитные заявки могут исполняться именно тогда, когда цена сейчас пойдет против нас.

### 4. Overfitting

Порог imbalance и параметры могут хорошо работать на одной выборке и ломаться на другой.

### 5. Modern market changes

Материалы относятся к 2015 году. Российский срочный рынок, ликвидность, участники, комиссии и инфраструктура могли сильно измениться.

### 6. Execution realism

Без учета задержек, cancel/replace delay, частичных исполнений и очереди бэктест будет завышать результат.

## Why it could work

- Market making получает spread.
- Inventory skew снижает риск накопления плохой позиции.
- Depth imbalance может фильтровать токсичные состояния.
- Spread state позволяет выбирать между обычным котированием и pinging.
- Policy map можно заранее рассчитать и быстро применять.

## Why it could fail

- Конкуренты быстрее.
- Сигнал imbalance слишком короткий.
- Исполнение лимитных заявок хуже, чем предполагает модель.
- Комиссии и проскальзывание съедают edge.
- Модель не учитывает реальные микроструктурные эффекты.
- Данные недостаточно детальные.

## Implementation path

```text
1. Research note
2. Validate imbalance signal on modern data
3. Estimate fill probabilities
4. Build simple heuristic policy map
5. Build event-driven simulator
6. Compare symmetric MM vs inventory-aware MM vs imbalance-filtered MM
7. Add commission + latency sensitivity
8. Only then consider C++ paper prototype
```

## Current decision

```text
Do not implement live.
Do not connect broker.
Do not assume profitability.
Use as research candidate only.
```

## Related files

```text
strategy_knowledge_base/research_notes/NOTE-20260708-002-market-maker-algorithms-parts-1-4.md
```
