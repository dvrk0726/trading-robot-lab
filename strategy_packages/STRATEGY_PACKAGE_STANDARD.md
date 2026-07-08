# Strategy Package Standard

Version: 1.0.0
Date: 2026-07-08
Status: accepted

## 1. Overview

A Strategy Package is the single allowed mechanism to transfer a trading strategy from Trading Lab to Trading Runtime. It is a directory containing configuration files, risk limits, approval state, and a content hash.

## 2. Package Directory Structure

```text
<package_dir>/
  manifest.yaml
  params.yaml
  risk_limits.yaml
  instruments.yaml
  approval.json
  validation_report.json
  package.hash
  README.md
```

Optional:

```text
  test_vectors/
    market_events.csv
    expected_signals.csv
    expected_order_intents.csv
```

## 3. manifest.yaml

Required fields:

| Field | Type | Description |
|---|---|---|
| strategy_id | string | Unique strategy identifier (e.g. `dummy_no_trade`) |
| strategy_version | string | Semver version (e.g. `0.1.0`) |
| created_at | string | ISO 8601 datetime of package creation |
| author | string | Author or agent that created the package |
| allowed_modes | list[string] | Modes the package can run in: `research`, `backtest`, `replay`, `paper`, `live` |
| required_market_data | list[string] | Market data types required (e.g. `tick`, `orderbook_l1`, `orderbook_l2`) |
| required_features | list[string] | Feature names required by the strategy |
| min_runtime_version | string | Minimum Runtime version required |
| validation_report_id | string | Reference to the validation report |
| package_hash | string | SHA-256 hash of all package files (excluding `package.hash` itself) |

Example:

```yaml
strategy_id: dummy_no_trade
strategy_version: 0.1.0
created_at: "2026-07-08T00:00:00Z"
author: mimo-agent
allowed_modes:
  - research
  - backtest
  - replay
  - paper
required_market_data: []
required_features: []
min_runtime_version: "0.1.0"
validation_report_id: "val-dummy-001"
package_hash: "<sha256>"
```

## 4. params.yaml

Contains strategy-specific parameters. Structure depends on the strategy. Must be valid YAML.

For the dummy no-trade package, this file is empty or contains no trading parameters.

## 5. risk_limits.yaml

Required fields:

| Field | Type | Description |
|---|---|---|
| max_position | number | Maximum allowed position size |
| max_order_size | number | Maximum size of a single order |
| max_daily_loss | number | Maximum daily loss (absolute value) |
| max_drawdown | number | Maximum drawdown percentage |
| max_signal_age_ms | integer | Maximum age of a signal before it is rejected (milliseconds) |
| max_orders_per_second | integer | Rate limit for order submission |
| max_market_data_staleness_ms | integer | Maximum allowed staleness of market data (milliseconds) |

Example:

```yaml
max_position: 0
max_order_size: 0
max_daily_loss: 0
max_drawdown: 0.0
max_signal_age_ms: 5000
max_orders_per_second: 0
max_market_data_staleness_ms: 10000
```

## 6. instruments.yaml

Lists instruments the strategy is allowed to trade. Must be valid YAML.

For the dummy no-trade package, this file contains an empty list.

## 7. approval.json

Tracks approval state per execution mode.

Required fields:

| Field | Type | Default | Description |
|---|---|---|---|
| research_approved | boolean | true | Allowed in research mode |
| backtest_approved | boolean | true | Allowed in backtest mode |
| replay_approved | boolean | true | Allowed in replay mode |
| paper_approved | boolean | false | Allowed in paper mode |
| live_approved | boolean | false | Allowed in live mode |
| approved_by | string | `""` | Who approved |
| approved_at | string | `""` | ISO 8601 datetime of approval |
| notes | string | `""` | Approval notes |

Critical rule: `live_approved` must be `false` by default. Only the project owner can set it to `true`.

Example:

```json
{
  "research_approved": true,
  "backtest_approved": true,
  "replay_approved": true,
  "paper_approved": false,
  "live_approved": false,
  "approved_by": "",
  "approved_at": "",
  "notes": "Dummy package. No live approval."
}
```

## 8. validation_report.json

Contains the result of Lab-side validation before the package is exported.

Required fields:

| Field | Type | Description |
|---|---|---|
| report_id | string | Unique report identifier |
| strategy_id | string | Strategy this report belongs to |
| strategy_version | string | Version validated |
| validated_at | string | ISO 8601 datetime |
| validated_by | string | Agent or tool that validated |
| schema_valid | boolean | All files pass schema validation |
| hash_valid | boolean | Package hash matches content |
| signal_parity_passed | boolean | Signal parity test result |
| risk_limits_valid | boolean | Risk limits are within bounds |
| errors | list[string] | Validation errors |
| warnings | list[string] | Validation warnings |

## 9. package.hash

Contains a single line: the SHA-256 hash of all package files (sorted alphabetically by relative path, excluding `package.hash` itself). Computed over the raw bytes of each file concatenated in order.

Format: `sha256:<hex_digest>`

## 10. Runtime Validation

Before loading a package, Trading Runtime must:

1. Verify `manifest.yaml` exists and has all required fields.
2. Verify `approval.json` exists and `live_approved` matches the requested mode.
3. Verify `package.hash` matches the actual content.
4. Verify `risk_limits.yaml` has all required fields.
5. Verify the package is compatible with `min_runtime_version`.

If any check fails, the package must be rejected.

## 11. Security Rules

- No broker connection details in any package file.
- No API keys, tokens, or secrets.
- No live trading code.
- No direct order sending code.
- `live_approved` must be `false` unless explicitly set by the project owner.
