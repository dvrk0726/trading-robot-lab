# M10X_QUOTE_FLAG_SEMANTICS_AND_CROSSED_IMPACT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10X_QUOTE_FLAG_SEMANTICS_AND_CROSSED_IMPACT.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate Quote flag semantics and crossed impact"
```

Owner final check:

```powershell
git status --short
git log --oneline -3
```

## Mission

M10W classified the remaining persistent crossed state as:

```text
H — Genuine market crossed state that should remain gated, not fixed.
```

Do not accept that conclusion as final yet.

A new external clue from the public `AndrewKhodyakov/qsh_parser` issue discussion points back to the official QSH v4 specification:

```text
https://www.qscalp.ru/store/qsh.pdf
```

The QSH v4 OrdLog flag layout says:

```text
bit 2  / 0x0004 = Add
bit 3  / 0x0008 = Fill
bit 4  / 0x0010 = Buy
bit 5  / 0x0020 = Sell
bit 6  / 0x0040 = Snapshot
bit 7  / 0x0080 = Quote
bit 8  / 0x0100 = Counter
bit 9  / 0x0200 = NonSystem
bit 10 / 0x0400 = EndOfTransaction / TxEnd
bit 11 / 0x0800 = FillOrKill
bit 12 / 0x1000 = Moved
bit 13 / 0x2000 = Canceled
bit 14 / 0x4000 = CanceledGroup
bit 15 / 0x8000 = CrossTrade
```

Therefore:

```text
0x94 = 0x80 + 0x10 + 0x04 = Quote + Buy + Add
```

NOT:

```text
Add + Buy + TxEnd
```

M10W reported record 16195 as:

```text
flags_hex = 0x94 (Add + Buy + TxEnd)
```

This label is incorrect if the official flag mapping is used.

The real transition record is more likely:

```text
record_index = 16195
event_type = ADD
side = BUY
price = 14120
flags_hex = 0x94 = Add + Buy + Quote
```

The purpose of this task is to investigate whether `Quote`-flagged OrdLog records should mutate the normal visible L2 order book.

## Why this matters

The current crossed state appears when a BUY ADD at 14120 is applied while best ask is 14062.

If this record is a plain passive order-book ADD, crossed book may be a real state.

If this record is `Quote`-flagged and Quote means a special book/quote update, synthetic quote, aggressive/marketable quote, or non-normal L3 mutation, then our current reconstruction may be wrong.

Do not hide crossed states.

Do not change defaults without evidence.

Measure `Quote` behavior first.

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCounterFlagAudit -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe -RunRemainingCrossedAudit -RunStrategyReadyExport -RunNonSystemFlagAudit -RunPersistentCrossedRootCause
```

If local QSH is missing, write exactly:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Fix all flag labels and reporting

Audit every place that converts `order_flags` to human-readable text.

Ensure:

```text
0x0080 is Quote
0x0400 is TxEnd / EndOfTransaction
0x0094 is Add + Buy + Quote
0x0414 is Add + Buy + TxEnd
```

Search and fix report/docs/output labels that incorrectly describe `0x94` as `TxEnd`.

Required files to inspect:

```text
cpp/qsh_ingest/include/qsh/qsh_types.hpp
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/README.md
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

If code enum is already correct, explicitly write:

```text
OLFlags enum is correct; previous report label was wrong.
```

### 2. Add Quote flag audit

Add a focused audit command or extend existing diagnostics.

Preferred command:

```text
quote-flag-audit <OrdLog.qsh> --out <file.csv> [--max-records N]
```

Audit counts:

```text
total_records
quote_records
quote_add
quote_fill
quote_cancel
quote_remove
quote_moved
quote_buy
quote_sell
quote_counter
quote_non_system
quote_cross_trade
quote_snapshot
quote_new_session
quote_txend
quote_fill_or_kill
quote_canceled
quote_canceled_group
quote_records_that_create_new_best_bid
quote_records_that_create_new_best_ask
quote_records_that_create_crossed_book
quote_records_inside_crossed_state
quote_records_that_uncross_book
first_quote_record_index
first_quote_add_record_index
first_quote_crossing_record_index
```

CSV output should include suspicious Quote events:

```text
record_index
ts
tx_index
event_type
order_id
side
price
amount
amount_rest
flags_hex
is_quote
is_counter
is_non_system
is_cross_trade
is_snapshot
is_txend
is_add
is_fill
is_cancel
is_remove
is_moved
best_bid_before
best_ask_before
best_bid_after
best_ask_after
is_crossed_before
is_crossed_after
mutation_path
```

### 3. Add experimental Quote book mode

Add explicit experimental mode:

```text
--quote-mode include|ignore-book
```

Rules:

```text
include      = current behavior; Quote records mutate book normally
ignore-book = Quote records are decoded and counted, but do not mutate visible book
```

Default for now:

```text
--quote-mode include
```

Do not silently change default behavior.

When `ignore-book` is used, count:

```text
quote_records_ignored_for_book
quote_add_ignored_for_book
quote_fill_ignored_for_book
quote_cancel_ignored_for_book
quote_remove_ignored_for_book
quote_moved_ignored_for_book
```

### 4. Re-check record 16195 under Quote modes

Specifically inspect record 16195 under:

```text
--counter-mode ignore-book --non-system-mode ignore-book --quote-mode include
--counter-mode ignore-book --non-system-mode ignore-book --quote-mode ignore-book
```

Report:

```text
flags_hex
flag_names
is_quote
is_txend
best_bid_before
best_ask_before
best_bid_after
best_ask_after
crossed_before
crossed_after
mutation_path
whether 16195 mutates the visible book
whether 16195 creates the persistent crossed state
```

Expected sanity check:

```text
0x94 must be reported as Add + Buy + Quote, not TxEnd.
```

### 5. Compare reconstruction modes

Run and compare at least these modes on the real sample:

```text
A. counter include + non-system include + quote include
B. counter ignore-book + non-system include + quote include
C. counter ignore-book + non-system ignore-book + quote include
D. counter ignore-book + non-system ignore-book + quote ignore-book
E. reduce+orphan-cancel-ignore + counter ignore-book + non-system ignore-book + quote ignore-book
```

Comparison table must include:

```text
mode
counter_mode
non_system_mode
quote_mode
orphan_fill_mode
orphan_cancel_mode
missing_order_id
crossed_book_snapshots
snapshots_total
snapshots_strategy_ready
snapshots_not_strategy_ready
snapshots_crossed
strategy_ready_ratio
first_crossing_event_record_index
first_crossing_snapshot_record_index
first_crossing_snapshot_index
counter_records_seen
counter_records_ignored_for_book
non_system_records_seen
non_system_records_ignored_for_book
quote_records_seen
quote_records_ignored_for_book
quote_records_that_create_crossed_book
l2_strategy_ready
classification
```

Critical question:

```text
Does --quote-mode ignore-book reduce the remaining 907 crossed snapshots?
```

### 6. Check whether Quote records should be treated like top-of-book Quotes, not L3 order mutations

Investigate internal semantics:

```text
Are Quote-flagged records always ADD/FILL?
Do Quote-flagged records carry stable order_id lifecycle?
Are Quote order_ids later removed/canceled/filled normally?
Do Quote records have TxEnd?
Do Quote records correlate with crossed states?
Do Quote records look like synthetic top-of-book quote updates rather than real orders?
```

If Quote records do not have a clean L3 lifecycle, do not treat them as normal visible L3 orders without evidence.

### 7. Marketable ADD hypothesis check

If `quote-mode ignore-book` does not reduce 907, add a small audit only, not a full implementation, for marketable ADDs:

```text
marketable_add_audit
```

Count:

```text
buy_add_price_gte_best_ask
sell_add_price_lte_best_bid
marketable_add_that_creates_crossed_book
marketable_add_with_quote_flag
marketable_add_without_quote_flag
marketable_add_with_counter_flag
marketable_add_with_cross_trade_flag
```

This is only a fallback audit in M10X. Do not implement matching logic yet unless the Quote audit clearly proves it is needed.

### 8. Summary JSON fields

Extend summary JSON or diagnostics output with Quote stats:

```text
quote_mode
quote_records_seen
quote_records_ignored_for_book
quote_add
quote_fill
quote_cancel
quote_remove
quote_moved
quote_counter
quote_non_system
quote_cross_trade
quote_snapshot
quote_txend
quote_records_that_create_crossed_book
first_quote_record_index
first_quote_add_record_index
first_quote_crossing_record_index
```

### 9. Update validation script

Update:

```text
tools/run_qsh_real_sample_checks.ps1
```

Add optional switch:

```powershell
-RunQuoteFlagAudit
```

Add validation modes for:

```text
counter-ignore-book + non-system-ignore-book + quote-ignore-book
reduce+orphan-cancel-ignore + counter-ignore-book + non-system-ignore-book + quote-ignore-book
```

The table should print:

```text
counter_mode
non_system_mode
quote_mode
crossed_book_snapshots
snapshots_strategy_ready
snapshots_not_strategy_ready
quote_records_seen
quote_records_ignored_for_book
l2_strategy_ready
```

### 10. Add deterministic tests

Add tests for Quote behavior.

Minimum tests:

```text
quote_mode_include_allows_add_to_mutate_book
quote_mode_ignore_book_skips_add_mutation
quote_mode_ignore_book_counts_skipped_add
quote_mode_include_can_create_crossed_book_when_quote_add_crosses
quote_mode_ignore_book_prevents_that_specific_crossing
counter_ignore_non_system_ignore_quote_ignore_are_independent
flag_names_decode_0x94_as_add_buy_quote
flag_names_decode_0x414_as_add_buy_txend
```

Existing tests must remain green.

### 11. Update README

Update:

```text
cpp/qsh_ingest/README.md
```

Document:

```text
what --quote-mode means
that it is experimental
how it differs from --counter-mode and --non-system-mode
how to run quote-flag-audit
how to compare crossed snapshots under include vs ignore-book
that 0x94 is Add + Buy + Quote, not TxEnd
that default must not be changed without owner approval
```

### 12. Update long report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10X Quote flag semantics and crossed impact
```

Include:

```text
build/test result
real-sample validation result
flag label correction for 0x94
Quote audit counts
record 16195 re-check
mode comparison table
crossed snapshot reduction or no reduction
marketable ADD audit result if needed
classification
files changed
next recommended task
```

## Decision rules

### If Quote ignore-book reduces 907 to 0 or near 0

Conclusion:

```text
Quote-flagged records likely should not mutate normal visible L2/L3 book as ordinary orders.
```

Next task:

```text
M10Y_QUOTE_REFERENCE_CHECK_AND_DEFAULT_DECISION
```

Do not change default yet without owner approval.

### If Quote ignore-book does not materially reduce 907

Conclusion:

```text
Quote flag is not the main remaining cause.
```

Next task should inspect:

```text
marketable ADD matching semantics
Fill/Remove lifecycle
transaction grouping
external reference/spec comparison
```

### If results are mixed

Keep Quote mode experimental and document exact cases where it changes reconstruction.

## Classification required

At the end classify the remaining crossed state as one of:

```text
A. Quote records should not mutate normal visible book
B. Quote records are normal and not the cause
C. Marketable ADD semantics likely needed
D. Fill/Remove lifecycle bug remains
E. Transaction grouping / TxEnd bug remains
F. Decoder field/flag interpretation bug remains
G. Genuine market crossed state, but only after Quote semantics checked
H. Inconclusive; external reference required
```

Do not choose G unless Quote semantics are explicitly checked and the flag labels are corrected.

## Done criteria

```text
1. build/test green;
2. real-sample validation run or QSH absence explicitly reported;
3. all 0x94/TxEnd mislabels corrected;
4. quote-flag-audit implemented or equivalent diagnostics extended;
5. --quote-mode include|ignore-book implemented as explicit experimental mode;
6. record 16195 compared under quote include vs quote ignore-book;
7. crossed snapshot counts compared across counter/non-system/quote modes;
8. summary JSON includes Quote stats;
9. validation script updated with Quote audit/modes;
10. deterministic tests added, including flag-name decoding tests;
11. README and long report updated;
12. no raw QSH, generated CSV/JSON, .env, exe, dll, or secrets committed;
13. no broker connection, live trading, or real order sending.
```

## Safety

```text
No broker connection.
No live trading.
No real order sending.
No raw QSH commit.
No generated CSV/JSON report commit.
No .env or secrets.
No profitability claims from the 2021 engineering sample.
No default mode change without evidence and owner approval.
No strategy research until remaining data semantics are understood or safely gated.
```

## Expected next step after M10X

If Quote explains the remaining crossed snapshots:

```text
M10Y_QUOTE_REFERENCE_CHECK_AND_DEFAULT_DECISION
```

If Quote does not explain them but marketable ADD audit points to aggressive order semantics:

```text
M10Y_MARKETABLE_ADD_MATCHING_SEMANTICS_EXPERIMENT
```

If Quote does not explain them and marketable ADD is not enough:

```text
M10Y_QSH_REFERENCE_OR_SPEC_SEMANTICS_ESCALATION
```

Only after this should UI resume:

```text
M10Z_TRADING_LAB_DATA_QUALITY_UI
```
