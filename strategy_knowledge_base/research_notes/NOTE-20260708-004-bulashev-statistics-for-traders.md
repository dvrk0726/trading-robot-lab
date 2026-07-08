# NOTE-20260708-004: Bulashev — Statistics for Traders

Дата обработки: 2026-07-08
Статус: research note / statistical foundation
Источник: пользовательский файл `Книга S.Bulashev.Statistika-dlya-treyderov.pdf`

## Краткий вывод

Эта книга — не готовая торговая стратегия, а фундаментальный справочник для построения статистической дисциплины проекта.

Главная ценность для `trading-robot-lab`:

```text
strategy validation
backtest evaluation
risk metrics
money management
portfolio risk
statistical hypothesis testing
distribution analysis
regression / smoothing / time-series tools
```

Это база, которая должна лечь в основу правил оценки стратегий перед переходом от идеи к backtest, от backtest к replay и от replay к paper.

## Что это за материал

Книга С.В. Булашев `Статистика для трейдеров`, 2003.

По аннотации, книга систематизирует практические методы статистики применительно к финансам и ориентирована на трейдеров/портфельных менеджеров, принимающих решения в условиях неопределенности.

Материал идет от базовой вероятности и случайных величин к более сложным методам анализа инвестиционных рисков.

## Структура книги

Книга состоит из 16 глав.

Ключевые разделы для нашего проекта:

```text
1. Probability and random variables
2. Analytical probability distributions
3. Special distributions
4. Estimation of distribution parameters
5. Statistical inference and hypothesis testing
6. Distribution identification
7. Correlation
8. Regression analysis
9. Fourier analysis
10. Regression for time series
11. Time-series smoothing
12. Adaptive time-series modeling
13. Mechanical trading systems
14. Money management
15. Portfolio risk via covariance
16. Portfolio risk via VaR / Shortfall-at-risk
```

## Самое ценное для торгового робота

### 1. Вероятностное мышление

В книге прямо подчеркивается: цены имеют случайный характер из-за столкновения большого числа участников рынка, поэтому точное предсказание будущей цены невозможно; прогноз возможен только в вероятностном смысле.

Это важный принцип для проекта:

```text
strategy output should be probabilistic / conditional, not deterministic certainty
```

### 2. Распределения и форма доходностей

Книга рассматривает:

```text
normal distribution
lognormal distribution
Laplace distribution
Cauchy distribution
Pareto distribution
generalized exponential distribution
Student t
chi-square
F-distribution
```

Это полезно для анализа:

```text
returns distribution
fat tails
skewness
kurtosis
outliers
PnL distribution
drawdown distribution
trade result distribution
```

Важно: не предполагать нормальность доходностей автоматически.

### 3. Оценка параметров и исключение выбросов

Полезно для:

```text
volatility estimation
mean return estimation
skew/kurtosis estimation
outlier filtering
robust statistics
```

В проекте это нужно применять осторожно: выбросы на рынке часто не являются ошибками данных, а являются реальными risk events.

### 4. Статистические выводы и проверка гипотез

Главный практический слой:

```text
confidence intervals
hypothesis tests
sample mean tests
sample variance tests
```

Для проекта это значит:

```text
strategy performance must be tested statistically
```

Не просто:

```text
PnL > 0
```

а:

```text
Is the result statistically distinguishable from noise?
Is the sample large enough?
Is the effect stable out-of-sample?
```

### 5. Идентификация распределения

Книга содержит разделы по:

```text
data grouping
histogram construction
optimal number of bins
goodness-of-fit tests
RTS log-return histogram
```

Для проекта это применимо к:

```text
return distribution
spread distribution
imbalance distribution
fill delay distribution
trade PnL distribution
latency distribution
slippage distribution
```

### 6. Корреляция и регрессия

Важный блок для:

```text
index/synthetic index models
pair trading
basket trading
factor models
hedge ratio estimation
spread modeling
portfolio covariance
```

Для старой идеи RI vs synthetic index это особенно важно:

```text
synthetic index coefficient
regression stability
covariance structure
correlation breakdown
```

### 7. Fourier analysis / cycles

Полезно для research, но опасно для overfitting.

Возможное применение:

```text
cycle detection
intraday rhythm analysis
periodic liquidity patterns
volume/trade intensity cycles
```

Но нельзя строить стратегию только на красивой гармонике без out-of-sample validation.

### 8. Time-series smoothing and adaptive modeling

Полезно для:

```text
price function
regime detection
trend estimation
noise bands
adaptive filters
```

Это связывается с ранее обработанным материалом `Ценовая функция и режим`.

### 9. Mechanical Trading Systems

Один из самых важных разделов для проекта.

Книга включает:

```text
properties of mechanical trading systems
minimum number of trades
testing MTS
account equity report
trade report
summary report
expected trade return
cumulative trade income curve
probability of loss in a series of trades
probability of ruin
```

Это можно напрямую использовать как чеклист для backtest report.

### 10. Money management

Книга рассматривает:

```text
loss limit per trade
loss percent limit per trade
maximization of average MTS income
optimization of return/risk ratio
moving-average analysis of cumulative trade curve
series criterion
increasing winning position size
```

Для проекта это важно как отдельный модуль:

```text
position sizing != signal generation
```

Стратегия должна выдавать сигнал, а размер позиции должен проходить через risk / capital management layer.

### 11. Portfolio risk via covariance

Полезно для:

```text
multi-strategy portfolio
multi-instrument futures basket
strategy diversification
correlation-aware risk allocation
Monte Carlo constrained portfolio optimization
```

Это применимо не только к портфелю активов, но и к портфелю стратегий.

### 12. VaR and Shortfall-at-risk

Книга рассматривает quantile risk measures:

```text
Value-at-risk
Shortfall-at-risk
portfolio optimization with VaR / SaR
```

Для проекта это важно, но с ограничением: VaR не должен быть единственным risk control. Нужно также использовать:

```text
max daily loss
max position
stress tests
tail risk analysis
kill switch
scenario loss
```

## Что нужно перенести в проект как правила

### Backtest report должен включать минимум

```text
number of trades
win rate
average win
average loss
expected trade return
median trade return
PnL distribution
cumulative equity curve
max drawdown
drawdown duration
profit factor
commission sensitivity
slippage sensitivity
series of losses
probability of ruin estimate
out-of-sample result
parameter stability
```

### Strategy validation must include

```text
hypothesis test for positive expectancy
confidence interval for mean trade result
variance / volatility of results
skewness and kurtosis
tail losses
stationarity/regime checks
```

### Risk engine should use

```text
max loss per trade
max daily loss
max position
max exposure
portfolio covariance
VaR / expected shortfall estimate
series loss control
capital fraction limits
```

## Как это связано с уже обработанными материалами

### Market making materials

Книга помогает формализовать:

```text
whether imbalance signal is statistically significant
whether fill probability h(F) is stable
whether spread filter S > threshold is statistically valid
whether backtest PnL is distinguishable from noise
```

### Regime / price function materials

Книга дает базу для:

```text
regression-based trend estimation
smoothing / adaptive filtering
cycle analysis
confidence bands around price function
```

### Старый RI synthetic index idea

Книга особенно полезна для:

```text
regression coefficient estimation
correlation/covariance analysis
synthetic index validation
spread distribution analysis
mean reversion tests
risk-adjusted backtest report
```

## Практические задачи после обработки книги

### Task 1: Backtest report standard

Создать документ:

```text
docs/backtest_report_standard.md
```

С обязательными метриками и форматом отчета.

### Task 2: Strategy statistical validation checklist

Создать документ:

```text
docs/strategy_statistical_validation_checklist.md
```

Включить:

```text
sample size
hypothesis tests
confidence intervals
out-of-sample
parameter stability
tail risk
series risk
```

### Task 3: Risk metrics module spec

Создать ТЗ для Python Agent:

```text
[PYTHON] Implement baseline strategy metrics module
```

Метрики:

```text
mean trade PnL
median trade PnL
std
skew
kurtosis
max drawdown
drawdown duration
VaR
Expected Shortfall
probability of ruin approximation
loss streak statistics
```

### Task 4: Synthetic index statistical validation

Для RI/synthetic index strategy:

```text
correlation stability
regression stability
residual distribution
spread stationarity
mean reversion check
outlier/tail behavior
```

## Что нельзя делать

Нельзя воспринимать книгу как источник готовых прибыльных стратегий.

Книга дает statistical toolkit, но не market edge.

Нельзя:

```text
оптимизировать стратегию только по истории
верить среднему PnL без доверительного интервала
игнорировать хвосты распределения
оценивать стратегию только по win rate
считать VaR достаточной защитой
использовать сглаживание/Фурье как доказательство предсказуемости
```

## Статус

```text
valuable_for_research: yes
valuable_for_backtest_framework: very_high
valuable_for_risk_framework: very_high
trading_strategy_source: indirect
ready_for_live: no
needs_conversion_to_project_checklists: yes
```

## Bottom line

Книга должна стать не источником одной стратегии, а основой для проектного правила:

```text
No strategy is accepted without statistical validation, risk metrics, and robustness checks.
```
