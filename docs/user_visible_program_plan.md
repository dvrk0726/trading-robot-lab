# User-visible Trading Lab plan

Date: 2026-07-08
Status: draft / safe historical mode only

## Purpose

This document defines what the owner/user should be able to open and see in Trading Lab while the project is still in historical research mode.

Trading Lab must stay safe:

```text
NO broker connection
NO live trading
NO real order sending
NO manual buy/sell buttons
NO live order controls
```

## What can be opened now

The current Python dashboard can already be opened locally:

```powershell
python apps/lab/backend/lab_dashboard.py
```

Then open:

```text
http://127.0.0.1:8000
```

Current dashboard is still mostly demo/status UI. It is useful to confirm that Trading Lab runs, but it is not yet a real order book viewer.

## What should be visible next

### 1. Data Sources

Show imported historical data sources:

```text
file name
instrument
stream type
source/recorder
recording time
records total
quality status
```

For example:

```text
RTS-3.21.2021-01-05.OrdLog.qsh
Stream: OrderLog
Instrument: Plaza2:RTS-3.21::1502592:10
Recorder: QshWriter.7561
Comment: Zerich QSH Service
Records total: 5,362,594
```

### 2. Data Quality

Show quality counters from C++ `qsh-ingest quality` output:

```text
records total
records valid
records rejected
Add / Fill / Cancel / Moved / Snapshot / CrossTrade
Buy / Sell
TxEnd / Transactions
book reconstruction errors
```

### 3. L3 to L2 Diagnostics

Before showing the historical стакан as trusted, the UI must show reconstruction diagnostics:

```text
L2 snapshots generated
non-positive spread snapshots
crossed-book snapshots
empty bid snapshots
empty ask snapshots
missing_order_id
negative_level_volume
invalid_side
```

If `best_bid >= best_ask` appears, the UI must show a warning:

```text
Стакан восстановлен с предупреждениями. Нельзя использовать для проверки стратегии, пока диагностика не чистая.
```

### 4. Historical Order Book Preview

Only after diagnostics are visible, add a simple preview of historical L2 snapshots:

```text
bid levels
ask levels
mid
spread
timestamp
```

First version can be table-based, not animated.

### 5. Historical Order Book Replay

Later version:

```text
Play / Pause
step forward / step backward
speed 1x / 10x / 100x
depth 5 / 10 / 20
linked mid/spread chart
```

## What must not be shown yet

Do not show:

```text
Buy button
Sell button
Start live button
Connect broker button
Send order button
Real account controls
Live trading status as active
```

Current stage is historical research only.

## Current answer to owner question

The owner can already open the Python dashboard, but it will not yet show the real historical стакан.

The real historical order book should be exposed after M10 diagnostics, because current exported L2 snapshots can contain negative spread / crossed-book states.

Recommended order:

```text
1. Finish M10 diagnostics.
2. Show C++ quality + diagnostics reports in Trading Lab.
3. Add static historical order book preview from CSV.
4. Add replay controls later.
```
