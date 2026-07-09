# M10D_STALE_ORDER_AND_MISSING_ID_TRACE

Owner decision date: 2026-07-08
Primary agent: MiMo
Status: ready for implementation

## How to run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10D_STALE_ORDER_AND_MISSING_ID_TRACE.md"
```

After MiMo finishes:

```powershell
.\tools\mimo_save.ps1 "Trace stale orders and missing order ids"
```

Verify locally:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Investigate the remaining crossed-book states in `--snapshot-mode txend`.

M10C showed that TxEnd snapshot mode reduces crossed-book snapshots but does not remove them:

```text
event mode crossed_book_snapshots: 3656
txend mode crossed_book_snapshots: 921
```

Conclusion:

```text
A large part was intra-transaction temporary state.
The remaining 921 crossed snapshots are likely stale order / missing_order_id / incomplete lifecycle handling.
```

Do not filter bad snapshots out. Find the order lifecycle cause.

## Read first

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md
M10B_CROSSED_BOOK_ROOT_CAUSE.md
M10C_TXEND_AND_ORDER_LIFECYCLE_TRACE.md
cpp/qsh_ingest/README.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/include/orderbook/l2_snapshot.hpp
```

## Known local result

Real file is local only and ignored by git:

```text
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

Event mode:

```text
records_processed: 10025
transactions_seen: 6033
snapshots_written: 10000
missing_order_id: 296
crossed_book_snapshots: 3656
non_positive_spread_snapshots: 3656
L2 strategy-ready: NO
```

TxEnd mode:

```text
records_processed: 18030
transactions_seen: 10025
snapshots_written: 10000
missing_order_id: 319
crossed_book_snapshots: 921
non_positive_spread_snapshots: 921
L2 strategy-ready: NO
```

## Required work

### 1. Add best-level order trace

For crossed snapshots, we need to know which active orders create best bid and best ask.

Add optional CLI arg:

```text
--trace-best-level-orders-out <file.csv>
```

For each crossed snapshot, write at least:

```text
ts
snapshot_index
records_processed
tx_index
best_bid
best_ask
spread
best_bid_total_qty
best_ask_total_qty
best_bid_order_ids
best_ask_order_ids
best_bid_order_count
best_ask_order_count
last_event_type
last_order_id
last_side
last_price
last_qty
last_amount_rest
last_flags
last_repl_act
```

If multiple orders exist at the same best level, output a semicolon-separated list of first N order ids, for example first 20.

### 2. Add missing_order_id trace

Add optional CLI arg:

```text
--trace-missing-order-out <file.csv>
```

Whenever Fill/Cancel/Remove/Move refers to an order id not present in the active order map, write:

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
best_bid_before
best_ask_before
best_bid_after
best_ask_after
reason
```

This should help determine whether missing order ids are benign or cause stale book state.

### 3. Add targeted lifecycle trace from crossed snapshot

When first crossed snapshot is found, automatically identify:

```text
one representative best_bid order_id
one representative best_ask order_id
```

Then optionally trace their lifecycle if feasible:

```text
--auto-trace-crossed-orders-out <file.csv>
```

Output lifecycle rows for those orders if they appear in later records. If past history cannot be reconstructed without a second pass, document this and implement a two-pass mode only if simple.

Acceptable first pass: write the selected order ids and current active order metadata into the report/CSV.

### 4. Check cancel/fill/remove semantics

Investigate these cases carefully:

```text
Fill with amount_rest == 0
Fill with amount_rest > 0
Cancel with amount_rest == 0
Cancel with amount_rest > 0
Remove with amount_rest == 0
Remove with amount_rest > 0
Move with amount_rest changes
NonSystem flag combined with Buy/Sell/TxEnd flags
```

The output report must state whether stale volume is likely caused by:

```text
wrong quantity delta logic
wrong amount_rest semantics
wrong remove/cancel handling
missing add event
wrong side mapping for specific flags
order id decode issue
transaction boundary issue
unknown / needs deeper QSH spec check
```

### 5. Add synthetic tests

Add/update small tests for:

```text
missing order trace is emitted
best-level order ids are visible in crossed state
partial fill reduces active order correctly
full fill removes active order
cancel with rest amount does not leave stale aggregated level
remove removes active order completely
```

Do not add real QSH files.

## Safe run commands to document

README/report should include commands like:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --snapshot-mode txend --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_diagnostics.csv --max-diagnostics 100 --trace-crossed-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_trace.csv --max-trace-events 100 --trace-best-level-orders-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_best_orders.csv --trace-missing-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_missing_orders.csv
```

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
3. Best-level order trace is available for crossed snapshots.
4. missing_order_id trace is available.
5. Report identifies whether stale best bid / best ask comes from specific order ids.
6. Report explains the most likely lifecycle cause or states exactly what remains unknown.
7. L2 remains strategy-ready NO while crossed snapshots remain.
8. MiMo report is updated.
```

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10D stale order and missing id trace
```

Include:

```text
changed files
build/test result
txend diagnostic command/result
best-level order trace summary
missing-order trace summary
representative stale order ids
hypothesis conclusion
remaining limitations
next recommended task
```
