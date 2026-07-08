# Shared Schemas

Formal JSON Schema definitions for shared contracts between Trading Lab and Trading Runtime.

## Schemas

| Schema | File | Version |
|---|---|---|
| MarketEvent | `market_event.schema.json` | 1.0.0 |
| FeatureSnapshot | `feature_snapshot.schema.json` | 1.0.0 |
| StrategySignal | `strategy_signal.schema.json` | 1.0.0 |
| OrderIntent | `order_intent.schema.json` | 1.0.0 |
| RiskDecision | `risk_decision.schema.json` | 1.0.0 |

## Validation

All schemas use JSON Schema draft-07 (`http://json-schema.org/draft-07/schema#`).

Schemas can be validated with any standard JSON Schema validator, e.g.:
- Python: `jsonschema` library
- JavaScript: `ajv` library
- CLI: `ajv-cli` or `check-jsonschema`

## Naming Convention

- Files: `<snake_case>.schema.json`
- Title: `<PascalCase>`
- IDs: `trading-robot-lab://schemas/<snake_case>/v<N>`

## Versioning Policy

- Breaking changes require a new major version.
- Optional fields can be added in a minor version.
- Each schema has a `version` field in its metadata.
