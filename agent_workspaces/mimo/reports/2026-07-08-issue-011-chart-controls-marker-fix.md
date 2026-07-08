# Report: Chart Controls and Marker Fix

Issue: #11
Task: Improve Trading Lab chart usability and fix price chart trade marker positioning
Agent: mimo-agent (MiMo Code)
Date: 2026-07-08

## Context Read

- AI_CONTEXT.md
- PROJECT_STATE.md
- ROADMAP.md
- SECURITY.md
- docs/current_mimo_workflow_and_state.md
- docs/mimo_developer_workflow.md
- apps/lab/backend/lab_dashboard.py
- apps/lab/backend/seed_demo_db.py
- apps/lab/frontend/static/lab.css
- apps/lab/backend/README.md
- apps/lab/research/demo_lab_database.md

## Summary

Replaced the static SVG price chart with an interactive canvas-based chart featuring full zoom, pan, and display controls. Fixed trade marker positioning to use real time+price coordinates. Updated demo data so all trade entry/exit timestamps align exactly with price series timestamps.

## Files Changed

| File | Change |
|---|---|
| `apps/lab/backend/lab_dashboard.py` | Replaced SVG price chart with interactive canvas chart; added JS for zoom/pan/controls/tooltips |
| `apps/lab/backend/seed_demo_db.py` | Fixed demo trade entry/exit timestamps to match price series timestamps |
| `apps/lab/frontend/static/lab.css` | Added styles for chart controls, canvas wrapper, tooltip, range selector |
| `apps/lab/backend/README.md` | Updated Charts section description |
| `apps/lab/research/demo_lab_database.md` | Updated demo charts data documentation |

## Files Created

None.

## What Was Completed

### 1. Chart Controls (all required controls implemented)

| Control | Implementation |
|---|---|
| Zoom in | Button + W key + scroll up |
| Zoom out | Button + S key + scroll down |
| Pan left | Button + A key + left arrow |
| Pan right | Button + D key + right arrow |
| Reset view | Button + R key |
| Auto-scale Y | Checkbox (default: on), recalculates Y range for visible data |
| Show/hide trades | Checkbox toggle |
| Show/hide exits | Checkbox toggle |
| Show/hide grid | Checkbox toggle |

### 2. Marker Positioning Fix

- Old: markers placed by row index lookup (`price_ts_map[t["entry_ts"]] -> index -> x_scale(index)`)
- New: markers placed at real time+price coordinates via canvas `ts2x(ts)` and `p2y(price)` scaling functions
- Trade entry/exit timestamps now match exactly to price series 5-minute intervals
- No more missing markers due to timestamp mismatch

### 3. Marker Colors

| Marker | Color | Shape |
|---|---|---|
| BUY entry | Green (#33cc66) | Upward triangle |
| SELL entry | Red (#ff4d4d) | Downward triangle |
| EXIT | Orange (#ffaa33) | Downward triangle (smaller) |

### 4. Hover Tooltip

- Shows timestamp, price, and nearest trade info (ID, side, PnL)
- Positioned near cursor, auto-repositions to stay in view
- Appears on mouse hover over chart area

### 5. Demo Data Alignment

Old trade timestamps that were outside the price series range (17:00, 18:00, 19:00, 20:00, 20:30, 16:45) have been moved to timestamps within the 10:00-16:10 range where price data exists. All 8 trades now have entry/exit timestamps that are exact matches to demo_price_series timestamps.

## Commands Run

```powershell
python apps/lab/backend/seed_demo_db.py
# Result: 12 tables populated, 8 demo trades seeded

# Dashboard test via HTTP:
# GET http://127.0.0.1:8000/?section=charts -> 200 OK, 30830 bytes
# Canvas element: YES, Controls: YES, Data embedded: YES
# All other sections (overview, schemas, packages, runtime): 200 OK
```

## Security Check

- No broker connection added
- No live trading code
- No real API keys or secrets
- No external dependencies (still Python standard library only)
- `live_approved` not changed
- All chart code is client-side JavaScript embedded in the dashboard HTML

## Risks

- Canvas chart uses embedded JavaScript; the JS is generated server-side and embedded inline. Not a security risk since all data is demo/synthetic.
- Trade markers now depend on exact timestamp match between demo_trades and demo_price_series. If seed_demo_db.py is modified, timestamps must stay aligned.

## Open Questions

- Equity curve and drawdown charts remain as static SVG. Could be converted to canvas for consistency, but not required by Issue #11.
- Range/window selector: basic range bar UI element added below chart. Full drag-to-select range is a possible future enhancement.

## Handoff

Review needed:
- Verify chart controls work in browser (zoom, pan, reset, tooltips)
- Verify trade markers appear at correct price/time positions
- Verify marker colors: BUY=green, SELL=red, EXIT=orange
- Check that "DEMO DATA ONLY" / "NO BROKER" / "LIVE DISABLED" banners are visible
