# NOTE-20260708-005: Avellaneda-Stoikov — High-Frequency Trading in a Limit Order Book

Дата обработки: 2026-07-08
Статус: research note / primary source
Источник: пользовательский файл `HighFrequencyTrading(1).pdf`
Оригинальная статья: Marco Avellaneda, Sasha Stoikov, `High-frequency trading in a limit order book`, Quantitative Finance, Vol. 8, No. 3, April 2008, 217-224

## Краткий вывод

Это первоисточник для inventory-aware market making.

Главная идея статьи:

```text
Optimal market making = reservation price based on inventory risk + quote distances calibrated to order-arrival intensity.
```

Для нашего проекта статья ценна не как готовая live-стратегия, а как базовая модель:

```text
mid price model
risk-averse utility
inventory-adjusted reservation price
order-arrival intensity by quote distance
optimal bid/ask quote placement
simulation comparison versus symmetric quoting
```

## Что решает статья

Маркетмейкер выставляет bid и ask, но не должен делать это симметрично вокруг mid-price без учета позиции.

Статья строит модель, где dealer:

1. оценивает субъективную reservation / indifference price с учетом текущего inventory;
2. выставляет bid/ask вокруг этой цены;
3. выбирает расстояние котировок от mid-price с учетом вероятности исполнения limit orders.

Это ровно тот принцип, который нужен для будущего policy engine:

```text
quote center depends on inventory
quote width depends on risk and fill intensity
```

## Основные элементы модели

### 1. Mid-price process

В базовой версии mid-price моделируется как Brownian motion без drift:

```text
dS = sigma * dW
```

Практический смысл:

```text
модель не пытается предсказывать направление цены;
она использует случайность mid-price для оценки inventory risk.
```

### 2. Utility objective

Агент максимизирует expected exponential utility of terminal PnL.

Это делает модель risk-averse и позволяет определить reservation prices, не зависящие от текущего wealth.

### 3. Frozen inventory

Сначала рассматривается агент, который держит inventory до terminal time `T`.

Из этого выводится reservation price:

```text
r(s, q, t) = s - q * gamma * sigma^2 * (T - t)
```

Интерпретация:

```text
q > 0 long inventory  -> reservation price below mid-price -> желание продавать
q < 0 short inventory -> reservation price above mid-price -> желание покупать
```

### 4. Limit orders and execution intensity

Агент выставляет:

```text
bid = s - delta_b
ask = s + delta_a
```

Вероятность исполнения зависит от расстояния котировки до mid-price.

Чем дальше котировка от mid-price, тем ниже arrival rate.

В модели используются Poisson intensities:

```text
lambda_a(delta_a)
lambda_b(delta_b)
```

Для практики важна форма:

```text
lambda(delta) = A * exp(-k * delta)
```

или power-law alternative.

## Основной двухшаговый алгоритм

Статья прямо приводит интуитивный two-step procedure:

```text
1. Compute personal reservation / indifference price based on inventory.
2. Calibrate bid/ask distances to the limit order book via execution probability as a function of distance from mid-price.
```

Это должно лечь в архитектуру как:

```text
Inventory Model -> Reservation Price -> Fill Intensity Model -> Quote Generator
```

## Приближенное решение

При symmetric exponential arrival rates:

```text
lambda_a(delta) = lambda_b(delta) = A * exp(-k * delta)
```

получаются простые формулы:

```text
reservation price = s - q * gamma * sigma^2 * (T - t)
```

и bid/ask spread вокруг reservation price зависит от:

```text
gamma
sigma
T - t
k
```

Практическая интерпретация параметров:

```text
gamma  -> risk aversion / inventory sensitivity
sigma  -> market volatility
T-t    -> remaining risk horizon
k      -> how fast execution probability decays with quote distance
A      -> base order-arrival intensity
```

## Simulation results from the paper

В статье сравниваются две стратегии:

```text
inventory strategy  -> quotes around reservation price
symmetric strategy  -> same average spread, but centered around mid-price
```

### gamma = 0.1

Результат:

```text
Inventory:  profit 65.0, std 6.6, final q std 2.9
Symmetric:  profit 68.4, std 12.7, final q std 8.4
```

Вывод:

```text
symmetric has slightly higher average profit, but much higher PnL variance and inventory risk
```

### gamma = 0.01

Risk aversion almost zero.

Результат:

```text
Inventory and symmetric strategies become very similar
```

Это важно: если `gamma -> 0`, inventory penalty исчезает.

### gamma = 1

Very risk-averse case.

Результат:

```text
Inventory: lower profit, much lower variance and much lower final inventory dispersion
Symmetric: higher profit, higher risk
```

Практический вывод:

```text
gamma controls tradeoff between spread capture and inventory risk reduction
```

## Что ценно для проекта

### 1. Разделение signal и inventory control

Статья не пытается предсказывать рынок. Она решает отдельную задачу:

```text
how to quote given inventory and fill probabilities
```

Это правильно архитектурно: signal/regime engine должен быть отдельным слоем.

### 2. Reservation price is core primitive

Нужно ввести в проект понятия:

```text
mid_price
reservation_price
quote_center
bid_distance
ask_distance
inventory_skew
```

### 3. Fill intensity must be estimated from market data

Модель требует `lambda(delta)`, то есть частоту исполнения в зависимости от расстояния от mid.

Для MOEX futures это нужно оценивать отдельно:

```text
by instrument
by session segment
by spread regime
by volatility regime
by queue position approximation
```

### 4. Risk-aversion parameter must be testable

`gamma` нельзя выбирать произвольно.

Его нужно исследовать через:

```text
PnL distribution
inventory distribution
max drawdown
risk-adjusted return
tail losses
probability of ruin
```

### 5. The model is not enough for live trading

Ограничения:

```text
Brownian mid-price assumption
no adverse selection signal
no queue position
no cancel latency
no partial fills
no fees/slippage realism beyond simplified assumptions
no regime switching
no modern exchange-specific microstructure
```

Для нашего проекта статья — theoretical base, не готовый trading system.

## Связь с предыдущими материалами

### Relation to Russian market-maker series

Части 1-5 русской серии фактически развивают/обсуждают эту модель и затем переходят к более сложным попыткам учесть:

```text
adverse selection
order book imbalance
HJB-QVI
policy maps
numerical induction
```

### Relation to `IDEA-20260708-002`

Идея inventory-aware market making должна иметь Avellaneda-Stoikov как base model:

```text
reservation price + quote spread + fill intensity
```

А depth imbalance и regime filter — дополнительные слои поверх.

## Что можно превратить в задачи

### Task 1: Implement Avellaneda-Stoikov baseline simulator

Python prototype:

```text
simulate mid-price Brownian path
simulate Poisson fills with lambda(delta)=A*exp(-k*delta)
compare inventory vs symmetric quoting
output PnL distribution and final inventory distribution
```

Цель: воспроизвести qualitative results from paper, not trade.

### Task 2: Estimate lambda(delta) from real data

Для MOEX futures:

```text
estimate fill / market order arrival intensity by distance from mid
fit exponential and power-law models
compare fit quality
```

### Task 3: Add fees and discrete tick constraints

Реальный рынок требует:

```text
tick size
lot size
commission
exchange fees
minimum price step value
session calendar
```

### Task 4: Compare quote centers

Backtest/replay variants:

```text
symmetric around mid
inventory-adjusted reservation price
reservation + imbalance skew
reservation + regime filter
```

## Статус

```text
valuable_for_research: yes
primary_source: yes
ready_for_baseline_simulation: yes
ready_for_live: no
needs_moex_calibration: yes
needs_replay_extensions: yes
```

## Bottom line

Эта статья должна стать базовым теоретическим reference для market making module.

Проектный принцип:

```text
Never quote symmetrically by default.
Every quote must know inventory, risk horizon, volatility, and estimated execution intensity.
```
