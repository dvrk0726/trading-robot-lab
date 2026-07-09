# M10C_TXEND_AND_ORDER_LIFECYCLE_TRACE

Owner decision date: 2026-07-08
Primary agent: MiMo
Status: ready for implementation

## How to run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10C_TXEND_AND_ORDER_LIFECYCLE_TRACE.md"
```

After MiMo finishes:

```powershell
.\tools\mimo_save.ps1 "Investigate TxEnd and order lifecycle for L2 reconstruction"
```

Verify locally:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Investigate whether crossed-book L2 snapshots are caused by snapshot emission inside unfinished OrdLog transactions or by stale/missing order lifecycle handling.

Do not hide bad snapshots. The goal is to understand why exported L2 has:

```text
best_bid >= best_ask
```

## Read first

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md
M10B_CROSSED_BOOK_ROOT_CAUSE.md
cpp/qsh_ingest/README.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/include/orderbook/l2_snapshot.hpp
```

## Known result after M10B

Real local sample:

```text
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

M10B trace showed:

```text
Records processed: 18030
System records: 18030
L2 snapshots: 10000
Moved events: 0
missing_order_id: 319
crossed_book_snapshots: 921
non_positive_spread_snapshots: 921
L2 strategy-ready: NO
```

First crossed trace rows start near snapshot index 2111 / records processed 2210:

```text
CROSSED_BOOK, best_bid=14100, best_ask=14062, spread=-38, last_event_type=FILL
CROSSED_BOOK, best_bid=14100, best_ask=14062, spread=-38, last_event_type=REMOVE
```

This suggests the next investigation must focus on:

```text
TxEnd / transaction boundary
snapshot emission timing
order lifecycle around bid 14100 and ask 14062
missing_order_id events
```

## Required work

### 1. Add transaction-boundary snapshot mode

Add CLI option:

```text
--snapshot-mode event|txend
```

Default can remain current behavior if needed, but support:

```text
--snapshot-mode txend
```

In `txend` mode, write L2 snapshots only after TxEnd / transaction boundary, not after every individual OrdLog event.

At the end print both:

```text
snapshot_mode
records_processed
transactions_seen
snapshots_written
crossed_book_snapshots
non_positive_spread_snapshots
L2 strategy-ready: YES/NO
```

### 2. Compare event mode vs txend mode

README and report must include two safe commands:

```text
l3-to-l2 ... --snapshot-mode event ...
l3-to-l2 ... --snapshot-mode txend ...
```

Compare counters. If txend mode reduces or removes crossed-book snapshots, document that crossed states were likely intra-transaction temporary states.

If txend mode does not reduce them, continue investigating stale orders.

### 3. Add order lifecycle trace

Add optional CLI args:

```text
--trace-order-id <id>
--trace-order-out <file.csv>
```

For the traced order id, write lifecycle rows:

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
book_best_bid_after
book_best_ask_after
spread_after
```

If the parser cannot easily expose some fields, include empty values and document limitation.

### 4. Add automatic first-crossed context trace

When `--trace-crossed-out` is provided, improve rows around first crossed snapshot if feasible:

```text
last 20 events before first crossed snapshot
first crossed event
next 20 events after first crossed snapshot
```

If a ring buffer is easier, implement small ring buffer of last N decoded records and dump it when first crossed snapshot is detected.

### 5. Tests

Add small synthetic tests for:

```text
snapshot emitted after each event can see temporary crossed state
snapshot emitted only after TxEnd can avoid temporary crossed state
order lifecycle trace captures add/fill/cancel/remove sequence
```

Keep tests small. Do not add real QSH.

## Safety rules

Do not add:

```text
broker connection
live trading
real order sending
raw QSH to git
generated CSV/JSON reports to git
EXE/DLL binaries
.env files
secrets or tokens
```

Do not claim strategy profitability.

## Done criteria

```text
1. qsh_ingest builds.
2. Tests pass.
3. --snapshot-mode txend exists.
4. event vs txend counters can be compared.
5. trace-order-id or first-crossed context trace exists.
6. Report states whether crossed book is intra-transaction or still unexplained.
7. L2 remains strategy-ready NO if crossed snapshots remain.
8. MiMo report is updated.
```

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10C TxEnd and order lifecycle trace
```

Include:

```text
changed files
build/test result
event-mode command/result
txend-mode command/result
crossed-book comparison
order lifecycle trace result
hypothesis conclusion
remaining limitations
next recommended task
```
