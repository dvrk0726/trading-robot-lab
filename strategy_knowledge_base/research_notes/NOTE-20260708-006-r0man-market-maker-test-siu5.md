# NOTE-20260708-006: r0man — Another Test of the Market Maker Algorithm

Дата обработки: 2026-07-08
Статус: empirical research note
Источник: пользовательский файл `Еще одно тестирование алгоритма Маркет Мэйкера(1).docx`
Тема: practical market maker testing, SIU5, policy maps, execution architecture, real/session statistics, volatility risk

## Краткий вывод

Файл очень ценный, потому что это уже не чистая теория, а практическое описание попытки тестировать алгоритм market making на `SIU5 (09.2015)` с учетом комиссий и с реальным контуром исполнения.

Главная мысль:

```text
market making edge is very thin; execution quality, latency, fees, volatility regime and cancel logic dominate the result.
```

Даже при большом количестве сделок и положительном итоговом PnL стратегия остается хрупкой.

## Что особенно ценно

### 1. Реалистичный взгляд на market making

Автор прямо пишет, что основной режим алгоритма:

```text
market making = liquidity arbitrage = spread trading
```

Средняя прибыль на сделку при идеальном исполнении не превышает spread.

Для `Si` средний spread указан примерно:

```text
2-5 points
```

При сильной волатильности, когда стакан резко двигается на `10-30 points`, алгоритм может стать убыточным даже при большом количестве формально положительных сделок. Основная причина — комиссии и плохое качество исполнения.

### 2. Практическая интерпретация policy zones

Автор объясняет карты политик без сложной математики:

```text
Ba, Bb                 MarketMaking
Ba, Bb+                Pinging Bid Side
Ba-, Bb                Pinging Ask Side
Ba-, Bb+ / variants    Pinging Bid & Ask Sides
BUY AT MARKET          Inventory Control for short position
SELL AT MARKET         Inventory Control for long position
```

Смысл:

- MarketMaking: одновременно ставим лимитки на лучший bid и лучший ask;
- Pinging: ставим заявку внутрь spread / на тик лучше;
- Inventory Control: закрываем накопленную позицию market order.

### 3. Полезный взгляд на imbalance

Автор пишет, что predictive power дисбаланса bid/ask невелика, но практическая польза есть.

Важный microstructure insight:

```text
large volume at best bid/ask may reduce fill probability at that level
```

То есть imbalance можно использовать не только как directional signal, но и как execution/fill-quality signal.

Это важное уточнение для нашего проекта:

```text
imbalance feature should not be treated only as price prediction;
it may also be a fill probability / queue quality feature.
```

### 4. Архитектура расчета и исполнения

Автор описывает двухчастную систему.

#### Расчетная часть

```text
1. за предыдущий день собирается история тиков и лучших bid/ask;
2. считаются рыночные параметры для расчета политик;
3. считаются политики;
4. политики сохраняются в виде массивов;
5. массив по (t, y, f, s) возвращает зону/действие.
```

#### Часть исполнения

```text
1. загрузить рассчитанные массивы;
2. на каждом обновлении лучшего bid/ask рассчитать t, y, f, s;
3. получить политику из массива;
4. сформировать сигналы и выставить заявки;
5. снять неисполненные заявки, если они больше не соответствуют текущей политике.
```

Это хорошо ложится на нашу будущую архитектуру:

```text
Offline Parameter/Policy Builder
  -> Policy Table
  -> Runtime Feature Engine
  -> Policy Lookup
  -> Order Intent
  -> Risk Engine
  -> Execution Engine
```

### 5. Проблема объема и усреднения

Policy maps MarketMaking/Pinging не указывают объем заявки для ловли spread.

В тесте использовался:

```text
1 contract
```

Но если одна сторона лимитной пары не исполняется, а следующая политика снова market-making, позиция может накапливаться до срабатывания Inventory Control.

Автор прямо отмечает, что по сути это превращается в controlled averaging.

Параметры управления:

```text
gamma      risk level / inventory aversion
dzettaMax  allowed market-order quantity / inventory control aggressiveness
```

### 6. Реальная/сессионная статистика SIU5

Дата теста:

```text
03/07/2015
```

Инструмент:

```text
SIU5 (09.2015)
```

Комиссии:

```text
exchange fee = 0.25
broker fee = 0.5 * exchange fee
broker = IT Invest, Forsage tariff
```

Ключевые метрики:

```text
Num.Txns                 1986
Num.Trades               1986
Net.Trading.PL            474.25
Avg.Trade.PL                0.24
Med.Trade.PL               -0.38
Largest.Winner             30.92
Largest.Loser             -20.81
Gross.Profits            2304.09
Gross.Losses            -1829.84
Std.Dev.Trade.PL            3.43
Percent.Positive           36.81
Percent.Negative           63.19
Profit.Factor               1.26
Avg.Win.Trade               3.15
Med.Win.Trade               2.62
Avg.Losing.Trade           -1.46
Med.Losing.Trade           -0.38
Max.Drawdown             -222.62
Profit.To.Max.Draw          2.13
Avg.WinLoss.Ratio           2.16
Med.WinLoss.Ratio           7
End.Equity                474.25
```

### 7. Important interpretation of the statistics

Несмотря на итоговый плюс, статистика показывает хрупкость:

```text
average trade PnL = 0.24
median trade PnL = -0.38
positive trades = 36.81%
negative trades = 63.19%
profit factor = 1.26
```

То есть стратегия держится не на высокой частоте выигрышей, а на редких больших выигрышах и соотношении средний выигрыш / средний проигрыш.

При таком тонком edge комиссии и задержки могут легко уничтожить преимущество.

### 8. MAE/MFE charts

Визуально по MAE/MFE видно, что есть много мелких сделок около нуля и отдельные выбросы.

Это подтверждает:

```text
PnL distribution is asymmetric and sensitive to tails.
```

Для наших будущих отчетов нужно обязательно включить:

```text
MAE
MFE
trade PnL distribution
tail losses
outlier trades
```

### 9. Волатильность — главный нерешенный риск

Финальный вывод автора:

```text
tests look good, but previous week was not very volatile;
current week showed that the algorithm needs serious refinement to account for volatility.
```

Это напрямую подтверждает необходимость regime / volatility filter.

## Что нужно перенести в проект

### Architecture requirement

Добавить concept:

```text
OfflinePolicyBuilder -> PolicyTable -> RuntimePolicyLookup
```

### Execution requirement

Runtime должен уметь:

```text
cancel stale orders when current policy changes
avoid accumulating inventory unintentionally
limit averaging behavior
track policy mismatch time
track order age
track cancel latency
```

### Risk requirement

Обязательные параметры:

```text
gamma
max_inventory
max_market_order_qty / dzettaMax
max_order_age
max_policy_mismatch_duration
max_drawdown
volatility kill switch
commission sensitivity
```

### Backtest/replay requirement

Проверять:

```text
PnL after fees
PnL by volatility regime
PnL by spread regime
PnL by order age
PnL by stale policy events
MAE/MFE
inventory path
cancel latency sensitivity
```

## Новые задачи для проекта

### Task 1: Volatility regime filter

Построить фильтр:

```text
if short-term volatility / book jump speed too high:
    cancel MM quotes
    no new spread capture orders
    only reduce inventory
```

### Task 2: Policy stale-order check

В execution simulator добавить правило:

```text
if active order no longer matches current policy:
    cancel it immediately
```

и измерять задержку отмены.

### Task 3: Averaging risk analysis

Исследовать, как меняется результат при:

```text
gamma values
max_inventory
max active orders per side
dzettaMax
```

### Task 4: MAE/MFE report standard

Включить MAE/MFE в стандарт отчета бэктеста.

## Связь с ранее обработанными материалами

### Relation to Avellaneda-Stoikov

Подтверждает, что inventory risk — не теория, а практическая проблема.

### Relation to market maker parts 5-8

Подтверждает:

```text
policy table can be computed offline
runtime only performs lookup and execution
spread trading edge is thin
volatility can destroy the system
```

### Relation to regime-aware idea

Сильно усиливает аргумент, что market making must be regime-aware.

## Статус

```text
valuable_for_research: yes
valuable_for_execution_architecture: very_high
valuable_for_risk_framework: very_high
ready_for_live: no
needs_modern_retest: yes
needs_replay_with_latency: yes
needs_volatility_filter: yes
```

## Bottom line

Файл показывает, что market making на Si мог давать положительный результат в спокойном режиме, но edge очень тонкий.

Проектный принцип:

```text
A market-making strategy is not acceptable without volatility filter, stale-order cancellation logic, inventory accumulation control, and MAE/MFE reporting.
```
