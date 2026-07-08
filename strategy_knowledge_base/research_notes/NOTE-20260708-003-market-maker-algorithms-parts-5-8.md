# NOTE-20260708-003: Market Maker Algorithms, Parts 5-8

Дата обработки: 2026-07-08
Статус: research note
Источники: пользовательские файлы `5-8. Часть ...docx`
Темы: estimation from market data, corrected HJB-QVI implementation, empirical backtest results, price regime and price function

## Краткий вывод

Файлы 5-8 сильно повышают практическую ценность предыдущих материалов.

Если части 1-4 дали теорию:

```text
inventory risk + adverse selection + HJB-QVI + policy map
```

то части 5-8 добавляют:

```text
как оценивать параметры из реальных данных
какой C# код использовать осторожно
почему нужна проверка исправлений
какие результаты были получены на Si
почему backtest может расходиться с реальной торговлей
зачем нужен detector рыночного режима
зачем нужна price function для выбора между market making / momentum / cycles
```

Главный practical lesson:

```text
market making strategy cannot be evaluated only by theoretical policy maps.
It needs real order book data, corrected implementation, realistic replay, latency model, and market regime filter.
```

## Файл 5: Algorithm Market Maker, Part 5

### Что добавляет

Часть 5 завершает цикл по HJB-QVI market making и показывает, как получать параметры модели из рыночных данных.

Ключевые параметры:

```text
lambda_J1    intensity of half-tick mid-price jumps
lambda_J2    intensity of one-tick mid-price jumps
lambda_S     spread jump intensity
rho_ij       spread state transition matrix
psi_1(F)     probability of half-tick price jump conditional on imbalance
psi_2(F)     probability of one-tick price jump conditional on imbalance
h(F)         probability of limit order fill conditional on imbalance
alpha_F      OU mean reversion parameter for imbalance
sigma_F      OU volatility parameter for imbalance
```

### Что важно для проекта

Параметры не должны подбираться вручную вслепую. Их нужно оценивать из данных:

- считать скачки spread по состояниям;
- считать переходы spread state;
- считать скачки mid-price на half-tick / one-tick;
- бинить imbalance `F`;
- оценивать `psi_1`, `psi_2` через логистическую аппроксимацию;
- оценивать fill probability `h(F)` отдельно для bid и ask;
- оценивать OU-параметры для imbalance методом максимального правдоподобия.

### Практическая структура реализации

В коде фигурируют основные массивы политики:

```text
w[t,y,f,s]       value function
polmk[t,y,f,s]   true = limit orders, false = market orders
thtkq[t,y,f,s]   market order quantity
thmka[t,y,f,s]   ask-side limit policy: best / improve one tick
thmkb[t,y,f,s]   bid-side limit policy: best / improve one tick
```

Это важная архитектурная идея: результат сложной математики можно хранить как lookup table:

```text
(t, inventory, imbalance, spread) -> action
```

### Важное предупреждение

В конце части 5 прямо указывается, что изначальный код имел ошибки, а исправленный код предоставил Eskalibur. Это критично: не использовать первый вариант как эталон.

## Файл 6: Corrections in Market Maker Algorithm

### Главный смысл

Короткий, но очень важный файл.

В нем подтверждается:

```text
original C# implementation contained several errors
errors significantly affected results
Eskalibur corrected algorithm to match the original paper
users should use corrected listing from the end of part 5
```

### Что важно для проекта

Любой перенос старого C#/псевдокода должен проходить через:

```text
1. mathematical review
2. unit tests
3. replication of known policy maps
4. comparison with reference output
5. no direct live use
```

Иначе можно получить красивый, но неправильный policy engine.

## Файл 7: Research Results of Market-Making Algorithm

### Самый практический файл в блоке

Автор Eskalibur описывает практические исследования алгоритма market making.

Ценные выводы:

1. Простые intraday strategies часто недолговечны.
2. Mean reversion на простых фильтрах почти не работал.
3. Жестко алгоритмизированные системы рано или поздно перестают работать.
4. Нужна диверсификация по параметрам и по движку.
5. Парный трейдинг не является граалем без капитала и широкой диверсификации.

### Почему был выбран Si

Автор считает, что на ФОРТС сигнал best bid / best ask imbalance в целом слабый, поэтому исследования проводились на `Si` как наиболее ликвидном фьючерсе.

### Настройки исследования

Использовались:

```text
max inventory = 10 futures
gamma = 1
number of spread states = 10
imbalance discretization step = 50
calculation at t = 0
policy horizon = several seconds
```

### Интерпретация policy maps

При нулевом spread основная политика — market making.

При spread = 1 появляются pinging zones:

```text
best ask - minstep
best bid + minstep
```

Идея: вместо market order встать лимитником внутрь spread, что дешевле.

При росте spread политика качественно не меняется, но увеличивается inventory exposure. Strategy пытается:

```text
stand inside spread
wait for spread collapse
reverse on opposite side
```

### Backtest result 1

Без фильтра:

```text
trades = 7563
profit = 2110
conclusion: cannot trade as is
```

### Backtest result 2

С фильтром:

```text
use Make Strategy only when S > 4
trades = 2531
profit = 1886
exchange fee = -632 rub
estimated net after broker = 500-600 rub
used contracts <= 3
```

Это не доказательство работоспособности, но показывает, что фильтр по spread резко улучшает качество.

### Risk sensitivity

Уменьшение `gamma` до 0.1, то есть повышение риска, вывело доходность в отрицательную зону.

Вывод:

```text
weak signal + high risk = likely negative result
```

### Очень важное практическое предупреждение

Автор прямо пишет, что реализовать стратегию с прибылью не удалось.

Причины:

- уникальность бэктеста;
- в бэктесте появляются сигналы, которых нет в реальности;
- вероятная причина — данные приходят пакетами;
- в реальной торговле появляются сделки, которых нет в backtest;
- заявки не успевают сниматься, когда условия размещения исчезают;
- реальные результаты обычно хуже backtest;
- direct Plaza + colocation могут существенно улучшить применимость.

Это очень важный lesson для нашего проекта:

```text
backtest without event timing and cancel latency is not enough
```

## Файл 8: Price Function and Regime, Part 1

### Что добавляет

Файл 8 расширяет взгляд: стратегия должна не только оптимально управлять заявками, но и понимать текущий ценовой режим.

Классификация HFT algorithms:

```text
market making
microstructure exploitation
short-term arbitrage
large order execution algorithms
```

Среднесрочные стратегии:

```text
trend following
cycle following
long-term arbitrage
```

### Режим рынка

Для market making важно понимать, когда режим подходит для котирования, а когда стратегия почти гарантированно будет терять.

Опасные состояния:

```text
news gap
strong momentum
one-sided order flow
trend breakout
regime shift
```

При сильном momentum market maker получает токсичный поток:

```text
market up   -> sell orders filled -> short inventory against rising price
market down -> buy orders filled  -> long inventory against falling price
```

### Price function

Price function должна давать:

```text
expected mean price over future horizon
derivatives
noise estimate
mean reversion activity
upper/lower noise bands
trend/cycle segment amplitude and period
```

В market making контексте нужна почти линейная и монотонная price function, которая дает среднее значение цены в шуме.

В momentum/cycle контексте нужна функция с кривизной, которая следует за импульсом или циклом и помогает ловить разворот.

### Подходы к price function

Упомянуты:

- signal processing filters;
- signal decomposition and reconstruction;
- stochastic systems with Kalman filter;
- direct spline calibration by least squares;
- possible Bayesian SDE approach.

Практический warning: Kalman filter оказался сложным в настройке ковариаций и нестабильным при неожиданных шумах.

## Главные выводы для нашего проекта

### 1. Нужно отделить policy engine от signal engine

Market maker HJB/policy map — это одно.

Сигнал regime / price function — отдельный слой.

Архитектурно:

```text
Market Data
  -> Feature Engine
      -> imbalance features
      -> spread state
      -> price function
      -> regime classifier
  -> Strategy Policy Engine
      -> inventory-aware market making
      -> momentum / cycle mode
  -> Risk Engine
  -> Execution / Backtest / Replay
```

### 2. Нужен regime filter до market making

Не каждый момент подходит для market making.

Перед котированием нужно проверять:

```text
is regime balanced / range-like?
is order flow one-sided?
is momentum strong?
is spread wide enough?
is volatility abnormal?
is session near clearing/end?
```

### 3. Spread filter важен

Из исследования Eskalibur:

```text
Make Strategy only when S > 4
```

значительно улучшила результат.

Это не значит, что `S > 4` будет работать сейчас. Но принцип важен:

```text
do not quote when spread does not compensate execution risk
```

### 4. Backtest must model missing real-world effects

Нужно обязательно моделировать:

```text
packetized data arrival
cancel latency
stale signals
orders that remain active after condition disappears
real fills not present in simplified backtest
queue position
partial fills
```

### 5. Correct implementation matters

Ошибки в коде C# materially changed results.

Нельзя переносить код без проверки.

## Новые research tasks

### Task 1: Parameter estimation from data

Создать Python модуль для оценки:

```text
lambda_J1
lambda_J2
lambda_S
rho_ij
psi_1(F)
psi_2(F)
h_bid(F)
h_ask(F)
alpha_F
sigma_F
```

### Task 2: Regime classifier prototype

Признаки:

```text
short-term slope
curvature
realized volatility
spread width
order flow imbalance
trade intensity
range / trend score
cycle amplitude estimate
```

Выход:

```text
MARKET_MAKING_OK
MOMENTUM_RISK
CYCLE_MODE
NO_TRADE
UNKNOWN
```

### Task 3: Spread filter study

Проверить на современных данных:

```text
PnL by spread bucket
fill quality by spread bucket
adverse selection by spread bucket
net PnL after fees by spread bucket
```

### Task 4: Replay realism gap study

Сравнить:

```text
simple backtest
event-driven backtest
replay with cancel latency
replay with queue approximation
```

## Candidate strategy additions

На основе файлов 5-8 нужно обновить/расширить идею:

```text
IDEA-20260708-002-inventory-imbalance-market-making.md
```

Добавить:

- spread filter;
- regime classifier;
- corrected implementation requirement;
- parameter estimation module;
- explicit warning about backtest/live mismatch.

Также можно создать новую идею:

```text
Regime-aware market making with price function filter
```

## Статус

```text
valuable_for_research: yes
practical_importance: high
ready_for_live: no
ready_for_backtest_spec: partly
needs_modern_data_validation: yes
needs_event_driven_replay: yes
needs_corrected_reference_implementation: yes
```

## Bottom line

Эти материалы подтверждают: market making strategy должна быть не одиночной формулой, а системой:

```text
parameter estimation
feature engine
regime filter
policy map
risk engine
realistic replay
execution-latency model
```

Без этого backtest может выглядеть хорошо, но реальная торговля будет хуже.
