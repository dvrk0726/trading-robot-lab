# M10B_CROSSED_BOOK_ROOT_CAUSE

Owner decision date: 2026-07-08
Primary agent: MiMo
Status: ready for implementation

## How to run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10B_CROSSED_BOOK_ROOT_CAUSE.md"
```

After MiMo finishes:

```powershell
.\tools\mimo_save.ps1 "Investigate L3 L2 crossed book root cause"
```

Verify locally:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Investigate and reduce the `CROSSED_BOOK` problem in `l3-to-l2` export.

M10 correctly detects exported L2 snapshots where:

```text
best_bid >= best_ask
```

Observed real sample diagnostics:

```text
crossed_book_snapshots: 921
best_bid = 14100
best_ask = 14062
spread = -38
reason = CROSSED_BOOK
```

This is not strategy-ready. Do not hide it with a filter. Find the likely reconstruction cause.

## Read first

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md
cpp/qsh_ingest/README.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/include/orderbook/l2_snapshot.hpp
cpp/qsh_ingest/include/quality/data_quality.hpp
```

## Current known facts

Real file is local only and ignored by git:

```text
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

Safe sample command:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_sample.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_diagnostics.csv --max-diagnostics 100
```

M10 result:

```text
L2 crossed book snapshots are detected.
But root cause is not solved.
non_positive_spread_snapshots may also be inconsistent with negative spread values.
```

## Required work

### 1. Fix spread diagnostics consistency

If spread is calculated as:

```text
spread = best_ask - best_bid
```

then any spread <= 0 must increment `non_positive_spread_snapshots`.

If `best_bid >= best_ask`, both counters should be consistent:

```text
crossed_book_snapshots += 1
non_positive_spread_snapshots += 1
```

### 2. Add crossed-book trace mode

Add optional CLI args for investigation:

```text
--trace-crossed-out <file.csv>
--max-trace-events N
```

Trace file should help understand why the book crossed. Include best available fields:

```text
ts
snapshot_index
records_processed
reason
best_bid
best_ask
spread
last_event_type
last_order_id
last_side
last_price
last_qty
last_flags
last_repl_act
last_tx_index
```

If some fields are not available yet, include empty values and document limitation.

### 3. Investigate likely root causes

Check at least these areas:

```text
side mapping: buy/sell sign or enum may be reversed
price scaling/tick representation
Add/Fill/Cancel/Move handling
Snapshot/NewSession handling
TxEnd grouping and when snapshots are emitted
order id map updates
removing fully filled orders
moved order old price vs new price
system vs non-system records filtering
```

Do not make a speculative fix without a test or clear report.

### 4. Add small synthetic tests

Add tests for order book behavior:

```text
normal bid < ask
crossed state detection
move order across spread if allowed by bad input
fill/cancel removes depleted order
spread counter consistency
```

Keep tests tiny. Do not add real QSH data.

### 5. Output status

At the end of `l3-to-l2`, print a clear summary:

```text
l2_crossed_book_snapshots
l2_non_positive_spread_snapshots
trace file path if written
L2 strategy-ready: YES/NO
```

If crossed snapshots remain:

```text
L2 strategy-ready: NO
```

## Non-goals

Do not add strategies.
Do not add broker connection.
Do not add live trading.
Do not add real order sending.
Do not commit raw QSH or generated reports.
Do not hide bad snapshots to make the output look clean.

## Done criteria

```text
1. qsh_ingest builds.
2. Tests pass.
3. non_positive_spread counter is consistent with negative/zero spread.
4. crossed-book trace CSV can be generated.
5. Report explains likely cause or states exactly what is still unknown.
6. L2 output is clearly marked strategy-ready NO while crossed snapshots remain.
7. MiMo report is updated.
```

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10B crossed book root cause investigation
```

Include:

```text
changed files
build/test results
sample command
counter result
trace CSV result
hypothesis tested
what was fixed
what remains unresolved
next recommended task
```
