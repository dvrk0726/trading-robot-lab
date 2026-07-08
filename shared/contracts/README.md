# Shared Contracts

Common data contracts between Trading Lab and Trading Runtime.

## Purpose

Defines the shared structures that both Lab and Runtime use, preventing logic drift between research and execution.

## Core Pipeline Contracts

| Contract | Schema | Description |
|---|---|---|
| MarketEvent | `shared/schemas/market_event.schema.json` | Single market data event from exchange or replay stream |
| FeatureSnapshot | `shared/schemas/feature_snapshot.schema.json` | Computed features at a point in time |
| StrategySignal | `shared/schemas/strategy_signal.schema.json` | Output signal from a TradeAgent |
| OrderIntent | `shared/schemas/order_intent.schema.json` | Order intent derived from a signal, must pass RiskEngine |
| RiskDecision | `shared/schemas/risk_decision.schema.json` | RiskEngine output for an OrderIntent |

## Additional Contracts (defined later)

| Contract | Description |
|---|---|
| OrderState | Current state of an order in the order lifecycle |
| TradeEvent | Executed trade (fill) |
| PositionSnapshot | Current position state |
| StrategyPackage | Strategy package manifest and metadata |
| BacktestReport | Backtest result summary |
| RuntimeLog | Runtime audit log entry |

## Data Flow

See [contract_flow.md](contract_flow.md) for the full pipeline diagram.

```
MarketEvent -> FeatureSnapshot -> StrategySignal -> OrderIntent -> RiskDecision
```

## Rules

- Contracts are versioned (see `$id` in each schema).
- All schemas use JSON Schema draft-07.
- Changes must be documented and must not silently break Runtime.
- Every OrderIntent must receive exactly one RiskDecision.
- Trading Lab cannot generate RiskDecision directly.
- RiskDecision.ALLOW is required before any order execution.
