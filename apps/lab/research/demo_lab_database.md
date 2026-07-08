# Demo Lab Database

Date: 2026-07-08
Status: active
Issue: #9

## Purpose

This document describes the demo SQLite database used by the Trading Lab dashboard. The database is a local, read-only demo store for visualizing project status, schemas, strategy packages, test vectors, reports, and runtime state.

## Architecture Decision

Only one local Lab DB is implemented now. Runtime DB will be designed later after Lab workflow and market data needs are clear.

## Database Location

```text
data/lab/trading_lab_demo.sqlite
```

This file is excluded from git via `.gitignore` (`*.sqlite` pattern). Only source code and seed logic are committed.

## Tables

### project_status

Single-row table with overall project status.

| Column | Type | Description |
|---|---|---|
| project_name | TEXT | Project name |
| project_version | TEXT | Current version |
| live_enabled | INTEGER | 0 = disabled, 1 = enabled |
| live_status | TEXT | DISABLED / ENABLED |
| runtime_implemented | INTEGER | 0 = not implemented |
| runtime_status | TEXT | NOT_IMPLEMENTED / IMPLEMENTED |
| database_type | TEXT | SQLite |
| database_status | TEXT | DEMO |
| environment | TEXT | development |

### schemas

Shared contract schemas used in the core pipeline.

| Column | Type | Description |
|---|---|---|
| name | TEXT | Schema name (e.g. MarketEvent) |
| version | TEXT | Schema version |
| file_path | TEXT | Path to .schema.json file |
| description | TEXT | What the schema represents |
| pipeline_order | INTEGER | Order in the core pipeline (1-5) |
| required_fields | TEXT | Comma-separated required field names |
| validation_status | TEXT | VALID / INVALID |

### strategy_packages

Strategy packages loaded into the system.

| Column | Type | Description |
|---|---|---|
| strategy_id | TEXT | Unique package identifier |
| strategy_version | TEXT | Semver version |
| author | TEXT | Package author |
| allowed_modes | TEXT | Comma-separated allowed modes |
| live_approved | INTEGER | 0 = not approved |
| backtest_approved | INTEGER | 1 = approved |
| status | TEXT | demo / active / rejected |

### test_vectors

Test vector sets for schema validation.

| Column | Type | Description |
|---|---|---|
| name | TEXT | Test vector set name |
| description | TEXT | What is tested |
| schema_validated | INTEGER | 1 = validated |
| file_count | INTEGER | Number of test files |
| files | TEXT | Comma-separated file names |
| status | TEXT | ALL_VALID / PARTIAL / FAILED |

### mimo_reports

Agent work reports from MiMo.

| Column | Type | Description |
|---|---|---|
| report_id | TEXT | Report identifier |
| title | TEXT | Report title |
| issue | TEXT | GitHub issue number |
| agent | TEXT | Agent that produced the report |
| completed_at | TEXT | Completion date |
| status | TEXT | completed / in_progress / failed |
| description | TEXT | Brief description of work done |

### data_sources

Available data sources.

| Column | Type | Description |
|---|---|---|
| name | TEXT | Source identifier |
| source_type | TEXT | synthetic / exchange / simulated |
| status | TEXT | active / not_connected / not_implemented |
| description | TEXT | What the source provides |
| instruments | TEXT | Instruments available |
| date_range | TEXT | Date range or N/A |

### backtest_runs

Backtest run history.

| Column | Type | Description |
|---|---|---|
| run_id | TEXT | Run identifier |
| strategy_id | TEXT | Strategy tested |
| started_at | TEXT | Start timestamp |
| completed_at | TEXT | Completion timestamp |
| status | TEXT | completed / failed / running |
| total_trades | INTEGER | Number of trades |
| pnl | REAL | Profit/loss |
| max_drawdown | REAL | Maximum drawdown |
| win_rate | REAL | Win rate percentage |
| notes | TEXT | Run notes |

### runtime_status

Runtime component status.

| Column | Type | Description |
|---|---|---|
| component | TEXT | Component name |
| status | TEXT | DISABLED / NOT_IMPLEMENTED / ACTIVE |
| description | TEXT | Current state description |

## Demo Data

### Live Status

- live_enabled = 0
- live_status = DISABLED
- No broker connection
- No real order sending code

### Runtime Status

- runtime_implemented = 0
- All components: NOT_IMPLEMENTED or DISABLED

### Schemas (5)

1. MarketEvent v1.0.0
2. FeatureSnapshot v1.0.0
3. StrategySignal v1.0.0
4. OrderIntent v1.0.0
5. RiskDecision v1.0.0

### Strategy Packages (1)

- dummy_no_trade_v001 — demo no-trade package for infrastructure testing

### Test Vectors (1 set, 5 files)

- basic_flow — full pipeline test with 5 validated JSON files

### MiMo Reports (5)

1. Initial context read
2. Issue #2 — Project skeleton
3. Issue #6 — Shared contracts
4. Issue #7 — Test vectors and validation
5. Issue #8 — Strategy Package standard

### Data Sources (4)

1. demo_synthetic — synthetic demo data
2. test_vectors — schema validation vectors
3. moex_historical — not connected
4. paper_feed — not implemented

### Backtest Runs

None. Table is empty.

## How to Use

### Seed the database

```powershell
python apps/lab/backend/seed_demo_db.py
```

### Run the dashboard

```powershell
python apps/lab/backend/lab_dashboard.py
```

Open: http://127.0.0.1:8000

### Reset the database

```powershell
python apps/lab/backend/seed_demo_db.py
```

Re-running the seed script drops and recreates the database.

## Security

- No real market data in the database.
- No API keys, tokens, or secrets.
- No broker connection data.
- No real order sending data.
- Database file is excluded from git.
- Only demo/synthetic data.

## Dependencies

- Python standard library only (sqlite3, http.server, pathlib, json)
- No external packages required

## Future Work

- Runtime DB design after Lab workflow needs are clear
- Real market data integration (with proper data source management)
- Backtest result storage
- Strategy performance tracking
- Research report integration
