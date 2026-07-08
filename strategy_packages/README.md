# Strategy Packages

Strategy Package is the only allowed way to move a strategy from Trading Lab to Trading Runtime.

## Purpose

A Strategy Package is a self-contained, versioned, validated bundle that describes a trading strategy for Runtime execution. It contains configuration, risk limits, instruments, approval state, and a content hash.

Trading Lab prepares packages. Trading Runtime only loads approved and validated packages.

## Standard

See [STRATEGY_PACKAGE_STANDARD.md](STRATEGY_PACKAGE_STANDARD.md) for the full specification.

## Package Layout

```text
strategy_package/
  manifest.yaml            # package metadata and identity
  params.yaml              # strategy parameters
  risk_limits.yaml         # strategy-specific risk limits
  instruments.yaml         # instruments the strategy can trade
  approval.json            # approval state per mode
  validation_report.json   # Lab validation result
  package.hash.example     # example hash (real hash computed at build time)
  README.md                # package description
```

## Examples

- [dummy_no_trade_v001](examples/dummy_no_trade_v001/) — a no-trade package for testing Runtime loading, validation, and rejection logic.

## Rules

- Every package must have a valid `manifest.yaml`.
- Every package must have an `approval.json` with `live_approved: false` by default.
- Every package must have a `package.hash` that matches the content.
- Trading Runtime must reject packages with invalid hash, missing approval, or schema violations.
- No broker connection, no live trading code, no secrets inside packages.
