# Shared Contracts

Common data contracts between Trading Lab and Trading Runtime.

## Purpose

Defines the shared structures that both Lab and Runtime use, preventing logic drift between research and execution.

## Contracts

- MarketEvent
- FeatureSnapshot
- StrategySignal
- OrderIntent
- RiskDecision
- OrderState
- TradeEvent
- PositionSnapshot
- StrategyPackage
- BacktestReport
- RuntimeLog

## Rules

- Contracts must be versioned.
- Changes must be documented and must not silently break Runtime.
