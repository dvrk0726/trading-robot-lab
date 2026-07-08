# NOTE-20260708-002: Market Maker Algorithms, Parts 1-4

Дата обработки: 2026-07-08
Статус: research note
Источник: пользовательские файлы `1-4. Часть Алгоритмы маркетмейкера.docx`
Тема: алгоритмы маркетмейкера, inventory risk, adverse selection, depth imbalance, HJB-QVI, numerical solution

## Краткий вывод

Эти четыре материала являются ценными для проекта, потому что они дают не просто идею "ставить лимитки по обе стороны стакана", а полноценную логику управления маркетмейкером:

```text
inventory risk + adverse selection risk + order book imbalance + spread state + dynamic programming
```

Главная практическая ценность: маркетмейкер должен выбирать режим действия в зависимости от состояния:

```text
position / inventory
order book imbalance
spread state
time to liquidation
commission
expected fill probability
short-term directional pressure
```

Это не готовая стратегия для live, но хороший теоретический каркас для будущего research/backtest/replay модуля.

## Часть 1: Avellaneda-Stoikov и inventory risk

### Суть

Описывается базовый market making:

```text
выставлять лимитные ордера одновременно на bid и ask
получать прибыль от spread
```

Но простая постановка на best bid / best ask убыточна из-за двух рисков:

1. `adverse selection` — чаще исполняется та сторона, которая приводит к плохой позиции против движения цены.
2. `inventory risk` — после исполнения одной стороны возникает открытая позиция, которая может начать терять деньги.

### Главная идея

Используется нейтральная цена / indifference price:

```text
Pi(s,q,t) = s - q * gamma * sigma^2 * (T - t)
```

где:

- `s` — цена актива;
- `q` — открытая позиция;
- `gamma` — риск-аверсия / коэффициент риска;
- `sigma` — волатильность;
- `T - t` — остаток времени до завершения торгового периода.

Если позиция длинная, центр котирования сдвигается так, чтобы стимулировать продажу и уменьшить inventory.
Если позиция короткая, центр сдвигается в другую сторону.

### Практический смысл для проекта

Для любого будущего market making / spread strategy нужно разделить:

```text
mid price
reservation / indifference price
bid quote
ask quote
inventory skew
```

Даже если не использовать формулы Avellaneda-Stoikov напрямую, принцип важен:

```text
quotes must depend on inventory
```

### Ограничение

Модель основана на геометрическом броуновском движении цены. В материале прямо указано, что это плохо согласуется с реальными данными, поэтому формулы требуют уточнения.

## Часть 2: учет adverse selection через depth imbalance

### Суть

Вторая часть переходит от чистого inventory control к попытке учитывать adverse selection.

Для краткосрочного предсказания направления цены используется дисбаланс объемов на лучших уровнях стакана:

```text
F = log(Q_best_bid) - log(Q_best_ask)
```

Идея:

```text
F > 0  -> давление в сторону роста
F < 0  -> давление в сторону падения
```

Дисбаланс моделируется как процесс Орнштейна-Уленбека с нулевым средним:

```text
dF_t = -alpha_F * F_t * dt + sigma_F * dW_t
```

### Модель spread state

Спред рассматривается как марковский процесс с тремя состояниями:

```text
S = {delta, 2delta, 3delta}
```

где `delta` — шаг цены.

### Две стратегии действий

#### 1. Make strategy

Лимитные ордера:

```text
quote best bid / best ask
или улучшить цену на 1 tick, если spread > delta
```

#### 2. Take strategy

Маркет-ордера для немедленного исполнения:

```text
закрыть позицию
уменьшить позицию
перевернуться в сторону краткосрочного сигнала
```

### State matrix / policy regions

В материале описаны области политики на плоскости:

```text
inventory level x depth imbalance
```

Режимы:

- `Market Making` — нормальное двустороннее котирование;
- `Momentum Buy/Sell` — агрессивное действие маркет-ордерами по направлению сильного imbalance;
- `Inventory Control Buy/Sell` — полное закрытие позиции;
- `Partial Inventory Control Buy/Sell` — частичное сокращение позиции.

### Практический комментарий по российскому рынку

В комментариях есть ценная проверка на российском срочном рынке:

- инструмент: `Si 06.15`;
- данные: order log + стакан;
- выборка: `220,736` наблюдений;
- интервал: `10:05-16:00`;
- гипотеза OU для imbalance признана достоверной на этой выборке;
- оценка:

```text
dF_t = -0.1608765 * F_t * dt + 1.0266176 * dW_t
```

Также отмечено:

- сигнал imbalance работает только на очень коротких горизонтах;
- полезнее прогнозировать не по времени, а по количеству тиков;
- на совсем коротких интервалах точность низкая, потом растет до максимума, затем падает;
- результаты нужно перепроверять на разных выборках.

Это особенно важно для нашего проекта, потому что речь идет именно о российской срочке.

## Часть 3: функция владения и HJB-QVI

### Суть

Третья часть формализует задачу оптимального контроля.

Цель — максимизировать ожидаемый конечный cash с учетом штрафа за удержание позиции:

```text
maximize E[ X_T - gamma * integral(position^2 * price_variation) ]
```

Вводится liquidation value:

```text
Q(x,y,p,f,s) = x + p*y - |y| * (s/2 + epsilon)
```

где:

- `x` — cash;
- `y` — позиция;
- `p` — mid price;
- `f` — imbalance;
- `s` — spread;
- `epsilon` — комиссия.

Дальше определяется value function:

```text
V(t,x,y,p,f,s)
```

Она максимизируется с учетом:

- изменений цены;
- изменений imbalance;
- изменений spread;
- вероятности исполнения лимитных ордеров;
- возможности использовать маркет-ордера;
- штрафа за inventory.

### Fill probability

Важный элемент: вероятность исполнения лимитного ордера зависит от imbalance.

Функция имеет логистическую форму:

```text
h(u) = 1 / (1 + exp(c0 + c1*u))
```

Это практически важно: в бэктесте нельзя считать, что любая лимитная заявка исполняется автоматически.

### Новая область: pinging

При spread > delta появляется режим:

```text
Pinging on bid / ask side
```

Смысл: ставить лимитный ордер на тик лучше текущего best bid / best ask, чтобы увеличить шанс исполнения при меньшей стоимости, чем market order.

## Часть 4: численное решение методом обратной индукции

### Суть

Четвертая часть переводит теоретическую HJB-QVI модель в численную схему.

Функция владения упрощается:

```text
V(t,x,y,p,f,s) = x + p*y + v(t,y,f,s)
```

Дальше решается сокращенная функция:

```text
v(t,y,f,s)
```

по сетке состояний:

```text
time t
inventory y
imbalance f
spread s
```

### Дискретизация

Создаются сетки:

```text
T_NT = {t_k}
Y_NY = {y_i}
F_NF = {f_j}
```

Также используются матрицы численных производных по imbalance:

```text
D1 — первая производная
D2 — вторая производная
```

### Operator choice

Вводится оператор выбора:

```text
A(t,y,f,s,phi) = max{ L~, M~ ∘ L~ }
```

Смысл:

```text
выбрать, что лучше в текущем состоянии:
лимитные ордера или маркет-ордер
```

### Backward induction

Решение строится от конца торгового периода назад:

1. В момент `T` задать terminal condition:

```text
w(T,y,f,s) = -|y| * (s/2 + epsilon)
```

2. Для каждого предыдущего момента времени перебрать состояния `(y,f,s)`.
3. Посчитать ценность лимитной политики.
4. Посчитать ценность маркет-политики.
5. Выбрать политику с максимальным значением.
6. Получить policy map:

```text
(t, y, f, s) -> action
```

### Практический смысл

Это можно превратить в таблицу политик:

```text
inventory bucket
imbalance bucket
spread state
time to close
=> action
```

Действия:

```text
quote both sides
quote only bid
quote only ask
improve bid / ask by 1 tick
cancel quotes
reduce inventory
close inventory
flip position
```

Это потенциально можно реализовать сначала в Python как research/backtest prototype, а позже в C++ как fast policy lookup.

## Что ценно для нашего проекта

### 1. Market making нельзя делать симметрично

Простое:

```text
bid = best bid
ask = best ask
```

без учета inventory и adverse selection почти наверняка плохая модель.

### 2. Нужен state-based decision engine

Решение должно зависеть от состояния:

```text
inventory
imbalance
spread
volatility
time to session/clearing/end
commission
expected fill probability
```

### 3. Лимитная заявка не равна исполнению

Для бэктеста нужно моделировать:

```text
queue position
fill probability
market order arrivals
spread state
latency
cancel/replace delay
partial fills
```

### 4. Российский рынок требует отдельной проверки

Комментарий по Si показывает, что imbalance-сигнал может существовать, но:

- он короткоживущий;
- зависит от инструмента;
- требует order log / стакана;
- параметры нужно переоценивать на современных данных;
- результаты 2015 года нельзя переносить на текущий рынок без проверки.

### 5. Теория полезна как каркас, но не как готовый код

В проекте нельзя сразу пытаться сделать live market maker.

Правильный путь:

```text
research note -> idea -> formal strategy -> data requirements -> simulator -> backtest -> replay -> paper -> owner gate
```

## Что можно превратить в задачи

### Research task 1

Проверить на современных данных MOEX futures:

```text
Does best-level depth imbalance predict next price move?
```

Для инструментов:

```text
RI / MXI / Si / CNY / BR
```

### Research task 2

Оценить OU-параметры для imbalance:

```text
alpha_F
sigma_F
stationarity
mean reversion speed
```

### Research task 3

Оценить fill probability function:

```text
h(u) = probability(limit order fill | imbalance u)
```

### Research task 4

Построить baseline policy map:

```text
inventory bucket x imbalance bucket x spread state -> action
```

Сначала эвристически, без полноценного HJB-QVI.

### Backtest task

Сделать простой event-driven simulator:

```text
order book events
quote placement
cancel/replace
fills
inventory
PnL
commission
slippage
```

## Предварительная идея для базы стратегий

Из этих материалов выделяется отдельная идея:

```text
Inventory-aware market making with depth imbalance filter
```

Коротко:

- котировать обе стороны, когда inventory нормальный и imbalance не токсичный;
- сдвигать котировки при накоплении inventory;
- не вставать против сильного imbalance;
- использовать partial/ full inventory control при опасном состоянии;
- использовать market order только как защитное действие или momentum-переворот в экстремальных режимах;
- учитывать spread state и time-to-close.

## Статус

```text
valuable_for_research: yes
ready_for_strategy_spec: no
needs_modern_data_validation: yes
needs_order_book_data: yes
safe_for_live: no
```

## Что нельзя делать на основе этих файлов

Нельзя:

- считать стратегию прибыльной;
- сразу писать live execution;
- подключать брокера;
- использовать формулы без проверки на современных данных;
- игнорировать комиссии, очередь, задержки и частичные исполнения;
- считать старые результаты по Si/RI актуальными.

## Рекомендация

Материал сохранить как базовый theoretical/research layer для будущего market-making блока.

Следующий шаг после обработки всей серии: создать формальную стратегию-кандидат:

```text
STRAT-YYYYMMDD-XXX-inventory-imbalance-market-making.md
```

Но перед этим желательно обработать следующие части, особенно если там есть:

- оценка параметров на реальных данных;
- C# код численного решения;
- практическая реализация;
- результаты тестов.
