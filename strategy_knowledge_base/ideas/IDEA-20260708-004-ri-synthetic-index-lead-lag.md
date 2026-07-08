# IDEA-20260708-004: RI / Synthetic Index Lead-Lag Statistical Arbitrage

Дата создания: 2026-07-08
Статус: raw / high priority / needs data validation
Источники:

```text
NOTE-20260708-007-robot-uralpro-hft-context.md
NOTE-20260708-004-bulashev-statistics-for-traders.md
```

## Краткое описание

Идея: исследовать lead-lag relationship между фьючерсом RI и синтетическим индексом, построенным из рыночных цен акций, входящих в индекс РТС.

Старая версия `robot_uralpro` исходила из идеи, что:

```text
RI futures follows synthetic index
```

Современная версия должна начинаться с другого принципа:

```text
Do not assume who leads. Measure who leads.
```

## Рыночная гипотеза

Между RI futures и synthetic index может существовать временное опережение/отставание.

Если один актив статистически стабильно опережает другой, можно использовать это для построения краткосрочного statistical arbitrage signal.

## Почему старая формулировка недостаточна

На современном рынке фьючерс может часто двигаться раньше synthetic index, потому что:

```text
futures is more actively traded
futures has higher short-term volatility
futures has lower entry threshold
futures may incorporate information faster
```

Поэтому слепая логика `RI follows synthetic index` может быть неверной.

## Candidate instruments

```text
RI / RTS index futures
RTS index constituents
synthetic RTS index
FX factor if required
```

Later possible extensions:

```text
MXI / MOEX index futures
index vs ETF/futures
futures vs options
related futures pairs
```

## Required data

```text
RI futures tick / quote / candle data
constituent stock prices
index weights
FX data if synthetic index requires it
contract specs
timestamps with sufficient precision
trading calendar
session segments
commission model
```

## Core features

```text
synthetic_index_value
RI_price
spread = RI_price - synthetic_index_value
normalized_spread
returns_RI
returns_synthetic
cross_correlation_by_lag
lead_lag_direction
lead_lag_strength
lead_lag_stability
session_time_bucket
volatility_regime
liquidity_regime
```

## First research questions

```text
1. Does RI lead synthetic index or does synthetic index lead RI?
2. At what lag is cross-correlation maximal?
3. Is the lag stable intraday?
4. Is the lag stable across volatility regimes?
5. Does spread/residual mean-revert?
6. Does the signal survive commissions and slippage?
7. How many trades per day are realistic?
8. Does using slightly larger calculation intervals reduce microstructure noise?
```

## Candidate signal logic

Not final. Research only.

Possible baseline:

```text
1. Compute lead-lag relation over rolling window.
2. Determine leader and lagger.
3. Detect statistically significant move in leader.
4. Enter lagger only if expected move exceeds costs and risk.
5. Exit on convergence, timeout, stop, or regime invalidation.
```

## Candidate entry filters

```text
lead_lag_strength above threshold
spread/residual deviation above threshold
volatility within acceptable range
liquidity sufficient
session segment allowed
expected edge > commission + slippage + safety margin
no stale data
```

## Candidate exit rules

```text
spread/residual convergence
timeout
loss limit
lead-lag relation breaks
volatility regime changes
session end / clearing proximity
risk engine rejection
```

## Risks

### 1. Direction reversal

Sometimes RI may lead, sometimes synthetic index may lead.

### 2. Latency

If signal horizon is too short, strategy may be impossible without low-latency infrastructure.

### 3. Microstructure noise

Too high frequency may introduce effects not captured in old strategy.

### 4. Data alignment

Synthetic index requires correct timestamp alignment across many instruments.

### 5. Costs

Edge may be smaller than commission/slippage.

### 6. Regime instability

Lead-lag relation may work only in certain market regimes.

### 7. Overfitting

Rolling lag and thresholds can be overfit to history.

## Validation plan

### Step 1: Data sanity

```text
check timestamps
check missing values
check stock/index/futures alignment
check session breaks
```

### Step 2: Lead-lag study

```text
cross-correlation by lag
rolling lead-lag
lag stability heatmap
session-by-session analysis
```

### Step 3: Residual/spread study

```text
spread distribution
mean reversion
half-life
outliers
tail behavior
```

### Step 4: Baseline backtest

```text
simple signal
fees
slippage
trade count
PnL distribution
MAE/MFE
drawdown
out-of-sample
```

### Step 5: Event-driven replay

```text
latency model
order type choice
fill assumptions
stale signal handling
cancel/replace logic
```

## Implementation path

```text
1. Build synthetic index prototype.
2. Build lead-lag analyzer.
3. Produce research report without trading.
4. If relation exists, formalize STRAT file.
5. Build baseline backtest.
6. Add event-driven replay.
7. Only then consider paper mode.
```

## Current decision

```text
High priority research candidate.
No live trading.
No broker connection.
Start with Python research only.
```

## Related files

```text
strategy_knowledge_base/research_notes/NOTE-20260708-007-robot-uralpro-hft-context.md
strategy_knowledge_base/research_notes/NOTE-20260708-004-bulashev-statistics-for-traders.md
docs/trading_robot_vision_and_research_plan.md
```
