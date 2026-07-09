# M10F_FILL_AMOUNT_REST_SEMANTICS

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10F_FILL_AMOUNT_REST_SEMANTICS.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate Fill Cancel Remove amount_rest semantics"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Investigate the remaining crossed-book states in stable `txend` mode.

Current trusted result after M10E:

```text
snapshot_mode: txend
records_processed: 18030
transactions_seen: 10025
snapshots_written: 10000
missing_order_id: 319
crossed_book_snapshots: 7890
non_positive_spread_snapshots: 7890
L2 strategy-ready: NO
```

Tracing is now side-effect-free. The next likely problem is wrong lifecycle semantics for:

```text
Fill
Cancel
Remove
Move
amount_rest
```

Do not hide bad snapshots. Find the semantic error.

## Read first

```text
M10D_STALE_ORDER_AND_MISSING_ID_TRACE.md
M10E_REGRESSION_COMPARE_ORDERBOOK_BEHAVIOR.md
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/src/main.cpp
docs/qsh_data_source_notes.md
```

If needed, compare behavior with `qsh-rs` and QSH/OrdLog documentation, but do not copy code blindly.

## Known real-sample crossed orders

M10D/M10E selected:

```text
auto_trace_bid_order: 1925033994466246392
auto_trace_ask_order: 1925033994522131746
best_bid: 14100
best_ask: 14062
spread: -38
```

Use these IDs as starting points.

## Required work

### 1. Add lifecycle trace for selected order IDs

Use existing `--trace-order-id` if implemented. If not complete, finish it.

It must output all observed events for selected IDs:

```text
ADD
FILL
CANCEL
REMOVE
MOVE
SNAPSHOT
TXEND
```

Fields:

```text
ts
record_index
tx_index
event_type
order_id
side
price
qty
amount_rest
flags
repl_act
book_best_bid_before
book_best_ask_before
book_best_bid_after
book_best_ask_after
active_order_qty_before
active_order_qty_after
active_order_price_before
active_order_price_after
```

### 2. Clarify amount_rest semantics

Determine whether `amount_rest` means:

```text
remaining quantity after the event
quantity delta in the event
original quantity
unknown / depends on event type
```

Check separately for:

```text
ADD
FILL
CANCEL
REMOVE
MOVE
```

The report must say exactly how the current implementation interprets it and why.

### 3. Add alternative experimental mode if needed

If semantics are unclear, add a diagnostic-only option:

```text
--fill-semantics delta|rest
```

or similar.

Run both modes on the limited sample and compare:

```text
crossed_book_snapshots
missing_order_id
negative_level_volume
amount_mismatch
```

Do not make experimental mode the default unless tests and report justify it.

### 4. Correlate missing_order_id with crossed book

Use `l2_missing_orders.csv` and crossed context to answer:

```text
Do missing_order_id events happen before crossed book starts?
Do they refer to prices near best_bid/best_ask?
Are they mostly FILL, CANCEL, REMOVE, or MOVE?
Are they benign duplicate removals or evidence of missing ADD events?
```

### 5. Tests

Add focused tests for:

```text
partial fill with amount_rest as remaining quantity
partial fill with amount_rest as delta quantity
full fill removes order
cancel/remove semantics
move semantics with amount_rest
missing_order_id does not leave stale active volume
```

Keep tests synthetic. Do not add real QSH.

## Expected output

The task is complete when report states one of:

```text
A. amount_rest semantics were wrong and fixed;
B. amount_rest semantics appear correct, issue is elsewhere;
C. semantics remain unknown, but experiments narrow the cause.
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
M10F Fill Cancel Remove amount_rest semantics
```

Include:

```text
changed files
build/test result
lifecycle trace summary for bid/ask order ids
amount_rest interpretation by event type
baseline vs experimental counters if added
root cause hypothesis
what remains unresolved
next recommended task
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, or credentials.
