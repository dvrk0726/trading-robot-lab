# Basic Flow Test Vectors

Synthetic test data for the core pipeline: MarketEvent -> FeatureSnapshot -> StrategySignal -> OrderIntent -> RiskDecision.

## Scenario

Replay of a single RI/synthetic-index lead-lag signal on instrument RIU5 during the main trading session.

1. Market data tick arrives with RIU5 at 148250.
2. Feature calculator computes synthetic index at 148180.5, spread z-score 2.35.
3. Strategy emits an entry_short signal (spread is elevated, expect reversion).
4. Position sizer produces an order intent: sell 3 contracts at limit 148240.
5. RiskEngine allows the order (within all limits).

## Files

| File | Contract | Description |
|---|---|---|
| `01_market_event.json` | MarketEvent | RIU5 tick during main session |
| `02_feature_snapshot.json` | FeatureSnapshot | Computed features for lead-lag strategy |
| `03_strategy_signal.json` | StrategySignal | Short entry signal on spread reversion |
| `04_order_intent.json` | OrderIntent | Sell 3 contracts limit |
| `05_risk_decision.json` | RiskDecision | ALLOW |

## Validation

Run `python shared/schemas/validate_examples.py` from the repository root to validate all examples against their schemas.
