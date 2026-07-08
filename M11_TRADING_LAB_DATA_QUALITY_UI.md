# M11_TRADING_LAB_DATA_QUALITY_UI

Owner decision date: 2026-07-08
Primary agent: MiMo
Status: draft / start after M10 diagnostics

## How to run

Run only after `M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md` is complete.

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M11_TRADING_LAB_DATA_QUALITY_UI.md"
```

After MiMo finishes:

```powershell
.\tools\mimo_save.ps1 "Add Trading Lab QSH Data Quality UI"
```

## Mission

Make the first user-visible Trading Lab screen for real historical QSH/OrdLog processing results.

This is not a live trading UI.

The goal is to let the owner open the local dashboard and see:

```text
QSH data source
quality counters
L3->L2 diagnostics
warning if order book is not strategy-ready
basic L2 snapshot preview
```

## Read first

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/user_visible_program_plan.md
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md
apps/lab/backend/lab_dashboard.py
apps/lab/backend/seed_demo_db.py
apps/lab/frontend/static/lab.css
```

## Required UI behavior

### 1. Keep safety banners

Dashboard must clearly show:

```text
LIVE DISABLED
HISTORICAL DATA ONLY
NO BROKER CONNECTED
NO REAL ORDER SENDING
```

### 2. Data Sources section

Show local historical QSH metadata if available from generated JSON reports:

```text
file name
instrument
stream type
recorder/comment
recording time
records total
records valid
records rejected
```

### 3. Data Quality section

Show counters from generated quality report:

```text
Add
Fill
Cancel
Moved
Snapshot
CrossTrade
Buy
Sell
TxEnd
Transactions
Book reconstruction errors
```

### 4. L3->L2 Diagnostics section

After M10, show diagnostics counters:

```text
l2 snapshots
non-positive spread snapshots
crossed-book snapshots
empty bid snapshots
empty ask snapshots
missing_order_id
```

If diagnostics are not clean, show warning:

```text
Стакан восстановлен с предупреждениями. Нельзя использовать для проверки стратегии, пока диагностика не чистая.
```

### 5. Static order book preview

If a generated L2 CSV exists, show first/last or selected snapshot as a simple table:

```text
ASK levels
MID
BID levels
spread
```

Do not implement animation/replay yet.

## Non-goals

Do not add:

```text
Buy/Sell buttons
broker connection
live mode
real order sending
strategy execution
account controls
```

Do not commit generated CSV/JSON reports or raw QSH.

## Done criteria

```text
1. Dashboard opens locally.
2. Safety banners are visible.
3. Data Sources section can show generated QSH metadata.
4. Data Quality section can show generated counters.
5. L3->L2 diagnostics warning is visible when diagnostics are not clean.
6. Static L2 preview is visible if local CSV exists.
7. No live/broker/order buttons are added.
8. MiMo report is updated.
```
