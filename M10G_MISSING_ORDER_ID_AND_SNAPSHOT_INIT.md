# M10G_MISSING_ORDER_ID_AND_SNAPSHOT_INIT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10G_MISSING_ORDER_ID_AND_SNAPSHOT_INIT.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate missing order ids and snapshot init"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Investigate why stable txend L2 reconstruction still produces crossed-book snapshots.

M10F real-sample result:

```text
fill_semantics=delta: crossed_book_snapshots=7890, missing_order_id=319, amount_mismatch=0
fill_semantics=rest:  crossed_book_snapshots=7890, missing_order_id=319, amount_mismatch=11
```

Conclusion:

```text
Fill amount semantics are probably not the primary cause.
Current better default remains delta.
Next focus: missing_order_id, Snapshot/NewSession initialization, missed ADD records, flags/repl_act decoding.
```

L2 remains not strategy-ready.

## Read first

```text
M10D_STALE_ORDER_AND_MISSING_ID_TRACE.md
M10E_REGRESSION_COMPARE_ORDERBOOK_BEHAVIOR.md
M10F_FILL_AMOUNT_REST_SEMANTICS.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
```

## Known stable counters

```text
snapshot_mode: txend
records_processed: 18030
transactions_seen: 10025
snapshots_written: 10000
missing_order_id: 319
crossed_book_snapshots: 7890
non_positive_spread_snapshots: 7890
```

Known crossed orders:

```text
auto_trace_bid_order: 1925033994466246392
auto_trace_ask_order: 1925033994522131746
best_bid: 14100
best_ask: 14062
spread: -38
```

## Required work

### 1. Missing order timing

Add diagnostics that show:

```text
first missing_order_id record index
first crossed book record/snapshot index
whether missing_order_id starts before or after crossed book
missing_order_id counts by event type
missing_order_id counts by side and price
```

### 2. Snapshot/NewSession handling

Check and report:

```text
whether NewSession clears book
whether Snapshot records are markers or actual book records
whether snapshot orders are loaded into active order map
whether reconstruction starts before valid book init
```

Add counters if useful:

```text
snapshot_records_seen
snapshot_orders_loaded
new_session_records_seen
book_clears_due_to_new_session
first_valid_book_record_index
```

### 3. ADD decoding / filtering

Check whether ADD events are skipped due to flags, repl_act, system/non-system records, side decode, zero price/qty, or unknown event type.

Add counters:

```text
add_records_seen
add_records_applied
add_records_skipped
skip_reason
```

### 4. Selected order lifecycle

Use these known IDs:

```text
1925033994466246392
1925033994522131746
```

Report whether each order was:

```text
added normally
loaded from snapshot
filled/cancelled/removed
still active when crossed book starts
missing ADD event
```

### 5. Tests

Add synthetic tests for:

```text
NewSession behavior
Snapshot init behavior
ADD applied/skipped counters
missing_order_id before/after crossed book
selected order lifecycle trace
```

Do not add real QSH data.

## Expected output

The report must state one of:

```text
A. Snapshot/NewSession initialization was wrong and fixed.
B. ADD records are skipped or misdecoded.
C. missing_order_id events are benign.
D. Root cause remains unknown, but diagnostics are improved.
```

If crossed snapshots remain, keep:

```text
L2 strategy-ready: NO
```

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10G missing_order_id and snapshot init
```

Include build/test result, counters, conclusion, and next recommended task.

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, or credentials.
