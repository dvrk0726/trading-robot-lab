# Contract Flow

Pipeline: MarketEvent -> FeatureSnapshot -> StrategySignal -> OrderIntent -> RiskDecision

## Flow Diagram

```
┌─────────────┐
│ MarketEvent │  Exchange / Replay / Paper
└──────┬──────┘
       │
       v
┌──────────────────┐
│ FeatureSnapshot  │  FeatureCalculator
└──────┬───────────┘
       │
       v
┌──────────────────┐
│ StrategySignal   │  TradeAgent
└──────┬───────────┘
       │
       v
┌──────────────────┐
│ OrderIntent      │  PositionSizer / OrderBuilder
└──────┬───────────┘
       │
       v
┌──────────────────┐
│ RiskDecision     │  RiskEngine
└──────┬───────────┘
       │
       ├── ALLOW ──────────> OrderManager -> ExecutionGateway
       ├── REDUCE ─────────> OrderManager (adjusted qty)
       ├── REJECT ─────────> no action, logged
       ├── CANCEL_ALL ─────> cancel all active orders
       ├── STOP_STRATEGY ──> stop strategy, cancel orders
       └── SAFE_MODE ──────> safe mode, cancel all, no new orders
```

## Stage Details

### 1. MarketEvent
- Source: exchange feed, replay engine, paper simulator
- Contains: price, volume, bid/ask, session state
- Produced by: MarketDataAdapter or ReplayEngine

### 2. FeatureSnapshot
- Input: one or more MarketEvents
- Contains: computed features (spread, volatility, signals, etc.)
- Produced by: FeatureCalculator
- Note: features are strategy-specific

### 3. StrategySignal
- Input: FeatureSnapshot
- Contains: signal type, strength, confidence, target price
- Produced by: TradeAgent (from Strategy Package)
- Note: signal does NOT directly create orders

### 4. OrderIntent
- Input: StrategySignal + current position
- Contains: side, quantity, price, order type
- Produced by: PositionSizer / OrderBuilder
- Note: must always go through RiskEngine

### 5. RiskDecision
- Input: OrderIntent
- Contains: ALLOW / REDUCE / REJECT / CANCEL_ALL / STOP_STRATEGY / SAFE_MODE
- Produced by: RiskEngine
- Note: every OrderIntent must receive exactly one RiskDecision

## Safety Invariant

No order can reach ExecutionGateway without passing through RiskEngine.
Trading Lab cannot bypass this pipeline.
