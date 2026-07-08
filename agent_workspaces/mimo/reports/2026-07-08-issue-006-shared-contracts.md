# Report: Issue #6 — Shared Contracts

Agent: MiMo (Implementation Agent)
Date: 2026-07-08
Task: Define first Shared Contracts for Trading Lab and Trading Runtime

## Context Read

- AI_CONTEXT.md
- PROJECT_STATE.md
- ROADMAP.md
- SECURITY.md
- docs/mimo_developer_workflow.md
- decisions/ADR-0002-two-system-lab-runtime-architecture.md
- docs/system_architecture_and_user_interface_requirements.md
- shared/contracts/README.md
- shared/schemas/README.md

## Summary

Created 5 minimal JSON schemas for the core contract pipeline:
MarketEvent -> FeatureSnapshot -> StrategySignal -> OrderIntent -> RiskDecision

## Files Created

| File | Description |
|---|---|
| `shared/schemas/market_event.schema.json` | Market data event schema (tick/trade/quote/orderbook) |
| `shared/schemas/feature_snapshot.schema.json` | Computed feature values schema |
| `shared/schemas/strategy_signal.schema.json` | TradeAgent output signal schema |
| `shared/schemas/order_intent.schema.json` | Order intent schema (must pass RiskEngine) |
| `shared/schemas/risk_decision.schema.json` | RiskEngine decision schema |

## Files Updated

| File | Description |
|---|---|
| `shared/contracts/README.md` | Updated with contract table, flow reference, rules |
| `shared/schemas/README.md` | Updated with schema table, validation info, versioning policy |

## Files Created (Optional)

| File | Description |
|---|---|
| `shared/contracts/contract_flow.md` | Pipeline diagram and stage details |

## What Was Completed

- 5 JSON schemas covering the core pipeline
- All schemas use JSON Schema draft-07
- All schemas are versioned (1.0.0)
- All schemas have `$id` with `trading-robot-lab://schemas/<name>/v1`
- All schemas have `additionalProperties: false` for strictness
- Documentation updated with contract tables, flow, and rules
- Contract flow diagram created with safety invariants

## What Was Not Completed

- Additional contracts (OrderState, TradeEvent, PositionSnapshot, StrategyPackage, BacktestReport, RuntimeLog) — not in scope for Issue #6
- Python/JS validation code — not in scope
- Test vectors — not in scope
- Schema integration tests — not in scope

## Schema Design Decisions

1. **timestamps in nanoseconds** — standard for HFT, avoids precision loss
2. **nullable fields** — bid/ask/price can be null when not available
3. **enum constraints** — signal_type, decision, event_type use strict enums
4. **signal_id in OrderIntent** — traceability from signal to order
5. **order_id in RiskDecision** — traceability from order to risk decision
6. **reason + reason_code** — human and machine readable explanations

## Commands Run

None required (schema definition only, no code to test).

## Risks

- Schemas are minimal; fields may need adjustment after first integration test
- FeatureSnapshot.features is a free-form map; may need per-strategy schema later

## Open Questions

- Should MarketEvent include full order book depth or just top-of-book?
- Should RiskDecision include a list of all checked rules or just the triggered one?
- Should StrategySignal include a reference to the FeatureSnapshot used?

## Next Steps

- Validate schemas with a JSON Schema validator
- Create test vectors for signal parity checks
- Define remaining contracts (OrderState, TradeEvent, PositionSnapshot, etc.)
- Create Python/JS SDK classes that match these schemas

## Handoff

Review needed: Architecture Agent
