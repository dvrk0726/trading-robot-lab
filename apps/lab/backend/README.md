# Trading Lab — Backend

Backend service for Trading Lab.

## Purpose

Provides the local Trading Lab dashboard and database seed logic. Currently serves a read-only demo dashboard with a local SQLite database.

## Stack

- Python standard library only (http.server, sqlite3, json, pathlib)
- No external dependencies

## Quick Start

```powershell
python apps/lab/backend/seed_demo_db.py
python apps/lab/backend/lab_dashboard.py
```

Then open: http://127.0.0.1:8000

## Files

| File | Description |
|---|---|
| `seed_demo_db.py` | Creates and populates `data/lab/trading_lab_demo.sqlite` with demo data |
| `lab_dashboard.py` | Local HTTP server with dark-theme dashboard reading from SQLite |

## Dashboard Sections

- Overview — project status, stats, pipeline diagram, safety rules
- Schemas — shared contract schemas (MarketEvent, FeatureSnapshot, etc.)
- Strategy Packages — loaded strategy packages with approval status
- Test Vectors — test vector files with validation status
- MiMo Reports — completed agent work reports
- Data Sources — available data feeds (demo/synthetic only)
- Backtests — backtest run history (empty for now)
- Charts — interactive canvas price chart with zoom/pan/controls, equity curve, drawdown, trades table (synthetic data only)
- Runtime Status — runtime component status (all not implemented)

## Rules

- No broker connection.
- No live trading.
- No real API keys.
- No secrets.
- Uses only Python standard library.
- No real order sending code.
- No external paid dependencies.
