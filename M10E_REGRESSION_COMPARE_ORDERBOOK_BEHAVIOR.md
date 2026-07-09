# M10E_REGRESSION_COMPARE_ORDERBOOK_BEHAVIOR

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10E_REGRESSION_COMPARE_ORDERBOOK_BEHAVIOR.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Fix M10D order book regression"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Fix the regression introduced around M10D.

M10C real local txend result:

```text
crossed_book_snapshots: 921
non_positive_spread_snapshots: 921
```

M10D real local txend result on the same limited sample:

```text
crossed_book_snapshots: 7890
non_positive_spread_snapshots: 7890
```

This is not acceptable. M10D tracing must be side-effect-free. It must not change reconstructed L2 state.

## Read first

```text
M10C_TXEND_AND_ORDER_LIFECYCLE_TRACE.md
M10D_STALE_ORDER_AND_MISSING_ID_TRACE.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/tests/test_best_level_orders.cpp
cpp/qsh_ingest/tests/test_snapshot_mode.cpp
```

## Known useful M10D output

M10D identified representative crossed orders:

```text
auto_trace_bid_order: 1925033994466246392
auto_trace_ask_order: 1925033994522131746
best_bid: 14100
best_ask: 14062
spread: -38
```

Keep this diagnostic ability, but do not let tracing change book behavior.

## Required work

1. Compare M10C vs M10D logic in `OrderBook` and `l3-to-l2`.
2. Find why adding best-level order-id tracing changed crossed-book count.
3. Make trace/read methods read-only:
   - no `operator[]` on read paths;
   - no insertion into maps during diagnostics;
   - no mutation while collecting order ids;
   - use `find()`/const access where possible.
4. Add regression tests proving:
   - baseline txend and traced txend produce the same L2 counters on synthetic data;
   - best-level tracing does not mutate the book;
   - missing-order tracing does not mutate the book;
   - auto-trace does not change crossed-book count.
5. Keep diagnostics, but make them side-effect-free.
6. Do not filter bad snapshots just to make counters clean.

## Expected result

This task is successful if tracing no longer changes reconstruction counters.

It is acceptable if the real sample still has crossed snapshots after the fix. The goal here is not to solve all crossed books; the goal is to remove the M10D regression.

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10E regression compare order book behavior
```

Include:

```text
changed files
build/test result
what caused the regression
what was fixed
whether tracing is now side-effect-free
whether owner must rerun real-sample baseline/traced commands
next recommended task
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, or credentials.
