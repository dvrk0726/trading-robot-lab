# M6 — Demo Charts for Trading Lab Dashboard

**Date**: 2026-07-08
**Issue**: #10
**Agent**: mimo-agent
**Status**: completed

## Summary

Added demo charts to the Trading Lab dashboard. The dashboard now includes a price chart with trade entry/exit markers, an equity curve chart, a drawdown chart, and a demo trades table. All data is synthetic — no real market data, no broker, no live trading.

## Files Changed

| File | Change |
|---|---|
| `apps/lab/backend/seed_demo_db.py` | Added 4 new tables (demo_price_series, demo_equity_curve, demo_drawdown, demo_trades) with seed functions |
| `apps/lab/backend/lab_dashboard.py` | Added `render_charts()` section renderer with inline SVG chart generation, added "Charts" to nav |
| `apps/lab/frontend/static/lab.css` | Added `.chart-container`, `.chart-title`, `.chart-banner` CSS classes |
| `apps/lab/backend/README.md` | Added Charts section to dashboard sections list |
| `apps/lab/research/demo_lab_database.md` | Documented 4 new tables and demo chart data |

## What Was Built

### New Database Tables (4)
- `demo_price_series` — 75 synthetic price points for RI_demo (random walk, seed=42)
- `demo_equity_curve` — 75 equity points starting at 1,000,000
- `demo_drawdown` — 75 drawdown percentages computed from equity curve
- `demo_trades` — 8 demo trades (6 closed, 2 open) with entry/exit markers

### Dashboard Charts Section
1. **Price Chart** — SVG polyline with green BUY markers (up triangles), red SELL markers (down triangles), and orange exit markers
2. **Equity Curve** — SVG polyline with green area fill
3. **Drawdown Chart** — SVG polyline with red area fill and zero reference line
4. **Trades Table** — HTML table with trade details, color-coded PnL

### Safety Banners
- Red banner at top of Charts section: "DEMO DATA ONLY — NO REAL MARKET DATA — NO BROKER — LIVE DISABLED"
- Header badges already present: LIVE DISABLED, DEMO DATA ONLY, NO BROKER

## Technical Decisions

- **Inline SVG** — no JavaScript, no external charting libraries, pure Python string generation
- **Python standard library only** — zero new dependencies
- **Deterministic data** — random seed=42 ensures reproducible synthetic data
- **Responsive SVG** — viewBox-based, `width:100%`, auto height
- **Dark theme** — chart colors match existing CSS custom properties

## Test Results

- `seed_demo_db.py` runs successfully, all 12 tables seeded with correct row counts
- `render_charts()` produces 14,142 chars of HTML with 3 SVG charts
- All assertions pass: DEMO banner, instrument name, SVG elements, trade IDs present
- Python compile check passes for both modified files

## Risks

None. This is demo-only infrastructure with synthetic data. No real market data, no broker connection, no live trading code.

## Open Questions

- None

## Next Steps

- Charts section is ready for visual inspection at `http://127.0.0.1:8000/?section=charts`
- Future backtest runs can extend `backtest_runs` table and link to chart data
- Real market data integration will replace synthetic data when data source management is ready

## Handoff

Report complete. Issue #10 objective satisfied: demo charts visible in Trading Lab dashboard.
