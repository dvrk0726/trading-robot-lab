# EVAL-20260708-001: SIU5 Market Maker Test by r0man

Дата обработки: 2026-07-08
Статус: historical empirical evaluation / not reproducible yet
Источник: `NOTE-20260708-006-r0man-market-maker-test-siu5.md`

## Summary

Исторический тест market-making алгоритма на SIU5 (09.2015), дата 03/07/2015.

Результат положительный, но edge очень тонкий и сильно зависит от режима рынка, комиссий, скорости данных, исполнения и volatility filter.

## Instrument

```text
Symbol: SIU5
Contract: 09.2015
Date: 03/07/2015
```

## Fees mentioned

```text
exchange fee = 0.25
broker fee = 0.5 * exchange fee
broker = IT Invest
 тариф = Forsage
```

## Core metrics

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
End.Equity                474.25
```

## Interpretation

### Positive points

```text
Net PnL positive after stated fees
Profit Factor > 1
Max drawdown smaller than final net PnL
Large number of trades
Realistic focus on fees and execution
```

### Weak points

```text
Avg.Trade.PL = 0.24 only
Median trade result is negative
More losing than winning trades
Edge can be destroyed by small latency/fee/slippage changes
Single session result is not enough
Volatility regime not solved
```

## Critical conclusion

This is not proof of a robust strategy.

It is evidence that:

```text
market making may work in calm conditions,
but requires strict volatility filtering and execution realism.
```

## Required follow-up before accepting strategy

```text
1. Reproduce on historical data.
2. Test multiple sessions and regimes.
3. Compare calm vs volatile periods.
4. Add cancel latency simulation.
5. Add queue/fill approximation.
6. Add MAE/MFE reporting.
7. Add volatility kill switch.
8. Run commission and slippage sensitivity.
9. Run out-of-sample validation.
```

## Verdict

```text
historically_interesting: yes
strategy_edge_proven: no
ready_for_modern_backtest: yes
ready_for_live: no
```
