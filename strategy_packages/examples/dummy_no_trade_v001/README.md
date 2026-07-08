# Dummy No-Trade Strategy Package v001

A no-trade strategy package for testing Trading Runtime's package loading, validation, and rejection logic.

## Purpose

This package contains no trading logic. It exists to verify that:

- Runtime can discover and parse a Strategy Package.
- Runtime can validate `manifest.yaml` fields.
- Runtime can check `package.hash` against content.
- Runtime can read `approval.json` and enforce `live_approved: false`.
- Runtime can read `risk_limits.yaml` fields.
- Runtime rejects invalid or tampered packages.

## What This Package Does

Nothing. The strategy produces no signals and no order intents. It is a placeholder for infrastructure testing.

## Files

| File | Description |
|---|---|
| manifest.yaml | Package metadata |
| params.yaml | Empty parameters |
| risk_limits.yaml | Zero risk limits |
| instruments.yaml | Empty instrument list |
| approval.json | All approvals false except research/backtest/replay |
| validation_report.json | Pre-filled validation report |
| package.hash.example | Example hash (real hash computed at build time) |

## Rules

- `live_approved` is `false` and must remain `false`.
- No broker connection.
- No secrets.
- No real order sending code.
