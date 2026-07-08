# NOTE-20260708-007: robot_uralpro HFT Context and Modernization Notes

Дата обработки: 2026-07-08
Статус: research note / historical strategy context / modernization input
Источник: пользовательский файл `HFT робот.docx`

## Краткий вывод

Материал очень важен, потому что связывает старый `robot_uralpro`, реальные результаты ЛЧИ 2010, последующие комментарии автора, модернизацию под современный рынок, инфраструктуру, скорость, lead-lag relationship и практический подход к развитию робота.

Главный вывод:

```text
Old robot_uralpro is not a live-ready robot.
It is a historical strategy/architecture source.
Modern project should rebuild the idea as a research platform with lead-lag detection, statistical validation, realistic replay, risk engine and modular multi-strategy architecture.
```

## Исторический результат robot_uralpro

В материале указано:

```text
Competition: LCHI 2010
Rank: 25
Initial capital: 104,941.27 RUB
Return: 257.10%
Profit: 269,805.67 RUB
Approximate trades per day: ~2000
```

Принцип работы старого робота:

```text
RI futures follows synthetic index dynamically composed from prices of RTS index constituents.
```

Автор прямо отмечает:

```text
currently the algorithm does not work in the old form;
adaptation to modern exchange parameters is required.
```

## Что реально ценно в старом роботе

Не сам старый код как боевой инструмент, а:

```text
HFT robot structure from 2010
RI / synthetic index statistical arbitrage idea
order placement logic
connector examples
market data collection approach
old implementation of high-frequency event loop
historical evidence that algorithmic trading was possible
```

Автор подчеркивает, что код написан примитивно, но это может быть плюсом для понимания.

## Технологии старого робота

Указано:

```text
C#
.NET Framework 3.5
Visual Studio 2010
QUIK DDE
QUIK API / Trans2QUIK-like order sending
Plaza2 ClientGate
```

В комментариях уточняется:

```text
Plaza2 ClientGate was used, not CGate.
ClientGate is old COM-based technology.
```

Также автор считает Python слишком медленным для такого алгоритма; C# лучше, C++ еще лучше.

## Современная инфраструктура по мнению автора на 2015

В блоке вопросов автор дает ориентиры:

```text
ideal tick-to-trade: 4-7 microseconds
some algorithms can work at ~100 microseconds, but less effectively
market data protocol: FAST
order sending: TWIME / FIX
colocation: needed
server: modern, fast CPU, at least 8 cores
```

Указанные стоимости колокации и брокера относятся к старому периоду и не должны использоваться как актуальные.

## Важный блок: modernization through lead-lag relationship

Отдельная часть материала посвящена модернизации `robot_uralpro` через `lead-lag relationship`.

Исходная идея:

```text
old strategy depends on relationship between synthetic index and RI futures.
```

Современная проблема:

```text
in many cases futures price moves earlier than synthetic index.
```

Причины, указанные в материале:

```text
futures trades more actively
futures has higher volatility
futures has lower entry threshold
```

Ключевой принцип модернизации:

```text
do not assume which asset leads;
measure lead-lag relationship from data.
```

Это применимо не только к RI/synthetic index, но и к:

```text
pair trading
basket trading
futures vs options
index vs futures
correlated futures
```

## Практическая рекомендация по частоте

Очень важная рекомендация автора для тех, кто улучшает `robot_uralpro`:

```text
do not chase extremely high trade frequency;
increase calculation intervals slightly;
many microstructure effects that were ignored in 2010 will disappear;
work becomes easier;
but too large interval reduces statistical reliability.
```

Ориентир из его тестов на современных данных:

```text
~500 trades per day
volume: 5 RI contracts
```

Это важно для нашего проекта: стартовать не с ultra-low-latency HFT, а с более медленного исследуемого режима.

## Новая архитектура автора после robot_uralpro

В материале по результатам роботорговли за март автор пишет, что новая архитектура основана на `robot_uralpro`, но значительно улучшена по гибкости.

Ключевые принципы:

```text
main robot skeleton allows adding new algorithms without rebuilding core
possible to add even options strategies
several strategies implemented
not all strategies trade immediately; some wait for enough statistics
parameter diversification: 10 parameter sets per algorithm
multiple virtual robots trade simultaneously
strategies are based on observations from mathematical model testing, not price patterns
```

Пример из материала:

```text
3 strategies implemented
2 participate in live trading
1 waits for enough statistics
10 parameter sets per algorithm
around 20 virtual robots simultaneously
```

## Infrastructure in author's live setup 2015

Автор пишет:

```text
Plaza2 connection
no colocation
ordinary hosting with minimal ping to Plaza IP
ping around 3 ms
average order roundtrip around 10 ms
```

Roundtrip defined as:

```text
time from order sent by robot to callback that order was placed on exchange
```

Также указано, что strategies with ~200 trades/day were not truly HFT and robustness was doubtful because available history started only from November 2014.

## Testing philosophy

В комментариях автор говорит:

```text
FORTS data for all instruments are recorded from Plaza.
timestamps are in milliseconds.
backtest uses timestamps but runs faster than real time.
200 trades/day is too little to evaluate algorithm.
confidence appears only from algorithms with several thousand trades.
he tests on all available history.
```

Это важно для нашего validation standard.

## Volatility and stop logic

Автор указывает:

```text
volatility affects result;
robot can be manually stopped;
automatic stop is built in at critical equity drawdown;
standard deviation can be used as simple volatility estimate.
```

Но из предыдущих market maker materials видно, что одного std недостаточно: нужен regime/volatility filter.

## Brownian motion and mathematical models

Автор объясняет, что Brownian motion is a model for concept explanation.

Практическая мысль:

```text
real market is more complex than model;
model helps see a currently expressed imbalance/skew;
then this skew must be statistically detected and tested.
```

Это согласуется с нашей исследовательской методологией:

```text
theory -> feature -> statistical detection -> backtest -> replay -> paper
```

## Что нужно взять в наш проект

### 1. Первое направление стратегии: RI / synthetic index / lead-lag

Старая идея не должна переноситься напрямую.

Нужно формализовать:

```text
which asset leads: RI futures or synthetic index?
lead-lag stability by time/session/regime
spread/residual distribution
entry/exit logic only after lead-lag validation
```

### 2. Второе направление: inventory-aware market making

Из предыдущих материалов:

```text
Avellaneda-Stoikov
HJB-QVI
depth imbalance
policy maps
volatility filter
```

Из этого файла добавляется:

```text
do not overfocus on ultra-high frequency at the beginning;
use modular multi-strategy architecture;
validate on many trades and long sample.
```

### 3. Architecture target

Нужна архитектура:

```text
core skeleton
strategy plugins
feature engine
statistical research modules
policy tables
risk engine
execution abstraction
backtest/replay/paper/live modes
```

### 4. Validation target

Не принимать стратегию без:

```text
large number of trades
out-of-sample
multiple regimes
commission sensitivity
latency sensitivity
parameter diversification check
live/paper shadow comparison
```

### 5. Infrastructure target

Live low-latency stage is not current task.

But architecture should later allow:

```text
FAST market data
TWIME/FIX order sending
colocation deployment
C++ execution core
```

Current safe stage:

```text
historical data
research
backtest
replay
paper
```

## Practical project implications

### Do not start with broker connection

Because:

```text
strategy not validated
current exchange conditions changed
microstructure changed since 2010/2015
real execution is decisive
```

### Start with data and research platform

First useful system should be able to:

```text
load historical market data
build synthetic index
calculate lead-lag cross-correlation
estimate spread/residual
simulate execution
produce standardized reports
```

### Modular multi-strategy architecture is mandatory

Because the author explicitly moved from one robot/one algorithm to flexible skeleton + multiple strategies + parameter diversification.

## New project tasks

### Task 1: Lead-lag research note and formal idea

Create:

```text
strategy_knowledge_base/ideas/IDEA-YYYYMMDD-XXX-ri-synthetic-index-lead-lag.md
```

### Task 2: Lead-lag Python prototype

Inputs:

```text
RI futures price series
synthetic index price series
timestamps
candidate lags
```

Outputs:

```text
cross-correlation by lag
leader/lagger classification
stability by session segment
visual report
```

### Task 3: Synthetic index builder spec

Need specification for:

```text
RTS constituent weights
stock prices
FX factor if required
rebalancing/weights
missing data
latency alignment
```

### Task 4: Strategy plugin architecture

Design interface:

```text
Strategy.on_market_event(event) -> OrderIntent[]
```

But live order sending stays disabled.

### Task 5: Backtest/replay standard

Must include:

```text
timestamps
latency
fees
slippage
order queue approximation
trade count
PnL distribution
MAE/MFE
drawdown
regime breakdown
```

## Status

```text
valuable_for_research: yes
valuable_for_project_vision: very_high
valuable_for_strategy_direction: very_high
ready_for_live: no
ready_for_lead_lag_research_spec: yes
ready_for_architecture_spec: yes
```

## Bottom line

`HFT робот.docx` changes the project emphasis:

```text
We should not build one cloned old robot.
We should build a modular trading research and execution laboratory.
```

The first serious strategy direction should be:

```text
RI / synthetic index lead-lag statistical arbitrage
```

The second serious strategy direction should be:

```text
regime-aware inventory market making
```

Both must pass the same pipeline:

```text
idea -> statistical validation -> backtest -> event-driven replay -> paper -> owner gate -> live only later
```
