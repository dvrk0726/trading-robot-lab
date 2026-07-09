# M10K_TXEND_TRANSACTION_GROUPING_AND_CANCEL_REMOVE_MODEL

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10K_TXEND_TRANSACTION_GROUPING_AND_CANCEL_REMOVE_MODEL.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Implement TxEnd transaction grouping diagnostics"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Implement diagnostic TxEnd transaction grouping for OrdLog L3->L2 reconstruction.

M10J showed that simple orphan Fill handling is real but not enough:

```text
strict:
missing_order_id: 319
missing_on_fill: 148
missing_on_cancel: 170
missing_on_remove: 1
orphan_fill_events: 148
crossed_book_snapshots: 7890

reduce-same-price:
missing_order_id: 171
missing_on_fill: 0
missing_on_cancel: 170
missing_on_remove: 1
orphan_fill_events: 148
orphan_fill_level_reductions: 148
crossed_book_snapshots: 7884
```

Conclusion:

```text
orphan Fill handling improves diagnostics but does not solve crossed L2.
Remaining issue is likely transaction-level handling of Fill/Cancel/Remove grouped by TxEnd, not per-record book mutation.
```

L2 remains not strategy-ready.

## Read first

```text
M10J_COMPARE_QSH_RS_L3TOL2_AND_FILL_ORDER_MODEL.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/README.md
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Use qsh-rs as semantic reference if needed:

```text
https://github.com/2dav/qsh-rs
```

Do not vendor/copy qsh-rs into this repository.

## Required work

### 1. Add book update mode

Add CLI option:

```text
--book-update-mode per-record|tx-grouped
```

Default must remain:

```text
per-record
```

`tx-grouped` is diagnostic/experimental until proven on real QSH.

### 2. Implement TxEnd transaction grouping

In `tx-grouped` mode:

```text
collect OrdLog records until TxEnd
apply one transaction batch to the book
write L2 snapshot only after TxEnd
keep existing txend snapshot timing
```

The goal is not speed yet. The goal is semantic correctness.

### 3. Transaction batch behavior

Inside one transaction, inspect and handle:

```text
ADD then FILL chains
ADD then CANCEL chains
ADD then REMOVE chains
FILL/CANCEL/REMOVE that reference orders introduced earlier in same transaction
orphan FILL/CANCEL/REMOVE whose order_id is not in active book before the transaction
```

Do not silently hide errors. Count them.

### 4. Add transaction diagnostics

Print counters:

```text
book_update_mode
transactions_grouped
records_in_grouped_transactions
max_records_in_transaction
orphan_fill_events
orphan_cancel_events
orphan_remove_events
orphan_fill_resolved_in_transaction
orphan_cancel_resolved_in_transaction
orphan_remove_resolved_in_transaction
tx_grouped_missing_order_id
tx_grouped_crossed_book_snapshots
```

Keep existing counters too:

```text
missing_order_id
missing_on_fill
missing_on_cancel
missing_on_remove
crossed_book_snapshots
non_positive_spread_snapshots
```

### 5. Compare per-record vs tx-grouped

Report must include expected owner commands for both modes on the real file:

```text
--max-records 100000 --max-snapshots 10000 --snapshot-mode txend
```

The report must clearly say whether `tx-grouped` improves:

```text
missing_order_id
missing_on_cancel
missing_on_remove
crossed_book_snapshots
non_positive_spread_snapshots
```

### 6. Tests

Add synthetic tests for:

```text
per-record mode unchanged
tx-grouped mode batches until TxEnd
ADD+FILL in same transaction
action on order introduced earlier in same transaction
orphan Cancel/Remove tracked clearly
snapshot only emitted after TxEnd in tx-grouped mode
```

Do not add real QSH data or generated CSV/JSON reports.

## Expected output

The task is done when the report states one of:

```text
A. tx-grouped mode significantly improves crossed L2 and missing order diagnostics;
B. tx-grouped mode improves missing orders but not crossed L2;
C. tx-grouped mode does not help, so root cause is deeper decoder/spec issue;
D. tx-grouped infrastructure is partial, and limitations are clearly documented.
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
M10K TxEnd transaction grouping and Cancel/Remove model
```

Include changed files, build/test result, implementation details, owner commands, and next recommended task.

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, or vendored qsh-rs code.
