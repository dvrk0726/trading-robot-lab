# Report: Issue #9 — Trading Lab Dashboard with Demo SQLite Database

- Issue: #9
- Task: Create the first visible local Trading Lab dashboard with a local demo SQLite database
- Agent: mimo-agent
- Date: 2026-07-08

## Context Read

- AI_CONTEXT.md
- PROJECT_STATE.md
- ROADMAP.md
- SECURITY.md
- docs/current_mimo_workflow_and_state.md
- docs/mimo_developer_workflow.md
- apps/lab/backend/README.md
- apps/lab/frontend/README.md
- shared/contracts/README.md
- shared/schemas/README.md
- strategy_packages/README.md
- strategy_packages/STRATEGY_PACKAGE_STANDARD.md
- shared/schemas/*.schema.json (all 5 schemas)
- shared/test_vectors/basic_flow/* (all 5 test vectors)
- strategy_packages/examples/dummy_no_trade_v001/* (all package files)
- agent_workspaces/mimo/reports/*.md (all existing reports)

## Summary

Created the first visible local Trading Lab dashboard with a demo SQLite database. The dashboard runs locally using only Python standard library (http.server + sqlite3). No external dependencies. No broker connection. No live trading. Demo data only.

## Architecture Decision

- Implemented one local Lab DB (SQLite) now
- Runtime DB will be designed later after Lab workflow and market data needs are clear
- Database location: `data/lab/trading_lab_demo.sqlite`
- Database file excluded from git (*.sqlite in .gitignore)

## Files Created

| File | Description |
|---|---|
| apps/lab/backend/seed_demo_db.py | Creates and populates demo SQLite database with 8 tables |
| apps/lab/backend/lab_dashboard.py | Local HTTP server with dark-theme dashboard |
| apps/lab/frontend/static/lab.css | Dashboard CSS with dark theme styling |
| apps/lab/research/demo_lab_database.md | Documentation for the demo database |
| data/lab/.gitkeep | Preserves data/lab directory in git |

## Files Updated

| File | Description |
|---|---|
| apps/lab/backend/README.md | Updated with dashboard usage, files, and rules |

## Database Tables Seeded

| Table | Rows | Description |
|---|---|---|
| project_status | 1 | Overall project status, live=DISABLED, runtime=NOT_IMPLEMENTED |
| schemas | 5 | MarketEvent, FeatureSnapshot, StrategySignal, OrderIntent, RiskDecision |
| strategy_packages | 1 | dummy_no_trade_v001 (live_approved=0) |
| test_vectors | 1 | basic_flow with 5 validated files |
| mimo_reports | 5 | Initial context + M1-M4 issue reports |
| data_sources | 4 | demo_synthetic, test_vectors, moex (not connected), paper (not implemented) |
| backtest_runs | 0 | No backtests yet |
| runtime_status | 7 | All components NOT_IMPLEMENTED or DISABLED |

## Dashboard Sections

1. **Overview** — project status cards, stats grid, pipeline diagram, safety rules
2. **Schemas** — shared contract schemas with pipeline order and validation status
3. **Strategy Packages** — loaded packages with approval status (live_approved=false)
4. **Test Vectors** — test vector files with schema validation status
5. **MiMo Reports** — completed agent work reports
6. **Data Sources** — available data feeds (demo/synthetic only)
7. **Backtests** — backtest history (empty)
8. **Runtime Status** — component status (all NOT_IMPLEMENTED)

## UI Design

- Simple readable dark theme
- Color palette: dark bg (#0f1117), cards (#1a1d27), accent blue (#4a9eff), danger red (#ff4d4d)
- Monospace font throughout
- Status badges with color coding (valid=green, disabled=red, demo=yellow)
- Three header badges always visible: LIVE DISABLED / DEMO DATA ONLY / NO BROKER
- Sidebar navigation with active state highlighting
- Pipeline flow visualization

## Commands to Run

```powershell
python apps/lab/backend/seed_demo_db.py
python apps/lab/backend/lab_dashboard.py
```

Open: http://127.0.0.1:8000

## What Was Completed

1. Seed script (seed_demo_db.py):
   - Creates 8 tables with proper schema
   - Seeds demo data for all tables
   - Creates data/lab/ directory automatically
   - Drops and recreates on re-run
   - Prints row counts for verification

2. Dashboard (lab_dashboard.py):
   - Pure Python standard library (http.server, sqlite3)
   - GET-based routing with query parameters (?section=...)
   - Serves CSS as static file
   - 8 dashboard sections with data from SQLite
   - Error handling for missing database

3. CSS (lab.css):
   - Dark theme with CSS variables
   - Status badge system
   - Responsive card grid layout
   - Pipeline flow visualization
   - Table styling with hover effects

4. Documentation (demo_lab_database.md):
   - Database architecture
   - Table descriptions
   - Demo data summary
   - Usage instructions
   - Security notes

5. Updated README.md for backend

## What Was Not Completed

- No Runtime DB (deferred per architecture decision)
- No backtest run data (no backtests executed yet)
- No real market data integration
- No broker connection (by design)
- No live trading (by design)

## Rules Compliance

- [x] No broker connection
- [x] No live trading
- [x] No real API keys
- [x] No secrets
- [x] No real order sending code
- [x] No bypassing RiskEngine
- [x] No live_approved=true
- [x] No external paid dependencies
- [x] Python standard library only
- [x] Database file excluded from git

## Risks

None. This is local demo infrastructure only. No real market data. No broker connection. No live trading.

## Open Questions

- Should the dashboard support real-time updates in the future?
- Should there be a REST API in addition to the HTML dashboard?
- What data format should be used for historical market data when available?
- Should backtest results be stored in the same SQLite database or a separate one?

## Handoff

Review needed: Architecture Agent / Owner
Next step: Runtime skeleton can load and display data from this dashboard. Research data and backtest results can be added to the database when available.
