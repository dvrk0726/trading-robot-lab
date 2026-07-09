# M10V_NON_SYSTEM_FLAG_SEMANTICS_AND_CROSSED_IMPACT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10V_NON_SYSTEM_FLAG_SEMANTICS_AND_CROSSED_IMPACT.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate NonSystem flag semantics and crossed impact"
```

Owner final check:

```powershell
git status --short
git log --oneline -3
```

## Mission

M10S showed that `Counter=0x100` events are the primary cause of the early crossed-book reconstruction problem.

M10T showed that after:

```text
--counter-mode ignore-book
```

there are still 907 crossed snapshots.

M10U added row-level Data Quality gating:

```text
strategy_ready
strategy_reject_reason
```

and confirmed:

```text
snapshots_total = 10000
snapshots_strategy_ready = 9093
snapshots_not_strategy_ready = 907
snapshots_crossed = 907
l2_strategy_ready = false
```

Now investigate a suspicious clue from M10T:

```text
first remaining crossed trigger:
record_index = 16195
event_type = ADD
side = BUY
price = 14120
flags = 0x94 = Add + Buy + NonSystem
counter = false
```

The task is to determine whether `NonSystem` records should mutate the normal visible order book.

Do not assume `NonSystem` is a normal visible book event.

Do not assume it is non-visible either.

Measure it.

## Why this matters

A normal visible limit order book should not remain crossed for a long time:

```text
best_bid > best_ask
```

Short crossing inside a transaction/sweep may be acceptable as a transitional state.

But if crossing persists for thousands or millions of records, the parser/reconstruction may be applying some event class incorrectly.

M10T classified remaining crossed snapshots as B / normal trading transitional states, but the trigger event has the `NonSystem` flag. That means the conclusion is not fully proven.

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCounterFlagAudit -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe -RunRemainingCrossedAudit -RunStrategyReadyExport
```

If local QSH is missing, write exactly:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Add NonSystem audit

Add a focused audit command or extend an existing diagnostics command.

Preferred command:

```text
non-system-flag-audit <OrdLog.qsh> --out <file.csv> [--max-records N]
```

The audit should count:

```text
total_records
non_system_records
non_system_add
non_system_fill
non_system_cancel
non_system_remove
non_system_moved
non_system_buy
non_system_sell
non_system_counter
non_system_cross_trade
non_system_snapshot
non_system_new_session
non_system_txend
non_system_unknown_event
non_system_records_that_create_new_best_bid
non_system_records_that_create_new_best_ask
non_system_records_that_create_crossed_book
non_system_records_inside_crossed_state
non_system_records_that_uncross_book
first_non_system_record_index
first_non_system_add_record_index
first_non_system_crossing_record_index
```

CSV output should include at least the first N suspicious NonSystem events:

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
is_counter
is_non_system
is_cross_trade
is_moved
is_add
is_fill
is_cancel
is_remove
best_bid_before
best_ask_before
best_bid_after
best_ask_after
is_crossed_before
is_crossed_after
mutation_path
```

### 2. Add experimental NonSystem book mode

Add explicit experimental mode:

```text
--non-system-mode include|ignore-book
```

Rules:

```text
include       = current behavior; NonSystem records mutate book normally
ignore-book  = NonSystem records are decoded and counted, but do not mutate visible book
```

Do not change the default behavior silently.

Default for now should remain:

```text
--non-system-mode include
```

unless the code structure requires no default argument. In that case document exact behavior clearly.

When `ignore-book` is used:

```text
increment non_system_records_ignored_for_book
increment non_system_add_ignored_for_book
increment non_system_fill_ignored_for_book
increment non_system_cancel_ignored_for_book
increment non_system_remove_ignored_for_book
```

### 3. Compare reconstruction modes

Run and compare at least these modes on the real sample:

```text
A. counter-mode include, non-system-mode include
B. counter-mode ignore-book, non-system-mode include
C. counter-mode include, non-system-mode ignore-book
D. counter-mode ignore-book, non-system-mode ignore-book
E. reduce+orphan-cancel-ignore + counter-mode ignore-book + non-system-mode ignore-book
```

Comparison table must include:

```text
mode
counter_mode
non_system_mode
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
l2_strategy_ready
classification
```

Critical question:

```text
Does --non-system-mode ignore-book reduce the remaining 907 crossed snapshots?
```

### 4. Re-check the M10T trigger event

Specifically re-check record 16195 under these modes:

```text
--counter-mode ignore-book --non-system-mode include
--counter-mode ignore-book --non-system-mode ignore-book
```

Report:

```text
Does record 16195 still mutate the visible book?
Does it still create best_bid=14120 while best_ask=14062?
Does first remaining crossed snapshot move to a later event?
Does crossing disappear entirely in first 100k records?
```

### 5. Distinguish short transitional crossing from persistent reconstruction error

If crossings remain under `non-system-mode ignore-book`, classify remaining states:

```text
A. another flag/event class still mutates visible book incorrectly
B. genuine short transactional/sweep transition
C. persistent crossed state caused by lifecycle/order deletion mismatch
D. decoder/orderbook bug
E. market/session phase not exposed
F. inconclusive
```

Do not call a crossed state “short transitional” if it persists through most of the file without proof of a later uncrossing event.

### 6. Add summary JSON fields

Extend summary JSON with NonSystem stats.

Minimum fields:

```text
non_system_mode
non_system_records_seen
non_system_records_ignored_for_book
non_system_add
non_system_fill
non_system_cancel
non_system_remove
non_system_moved
non_system_cross_trade
non_system_counter
non_system_records_that_create_crossed_book
first_non_system_record_index
first_non_system_add_record_index
first_non_system_crossing_record_index
```

If counts already exist elsewhere, reuse names instead of duplicating.

### 7. Update validation script

Update:

```text
tools/run_qsh_real_sample_checks.ps1
```

Add optional switch:

```powershell
-RunNonSystemFlagAudit
```

Add validation modes for:

```text
counter-ignore-book + non-system-ignore-book
reduce+orphan-cancel-ignore + counter-ignore-book + non-system-ignore-book
```

The table should print:

```text
counter_mode
non_system_mode
crossed_book_snapshots
snapshots_strategy_ready
snapshots_not_strategy_ready
non_system_records_seen
non_system_records_ignored_for_book
l2_strategy_ready
```

### 8. Add deterministic tests

Add tests for NonSystem behavior.

Minimum tests:

```text
non_system_mode_include_allows_add_to_mutate_book
non_system_mode_ignore_book_skips_add_mutation
non_system_mode_ignore_book_counts_skipped_add
non_system_mode_include_can_create_crossed_book_when_add_crosses
non_system_mode_ignore_book_prevents_that_specific_crossing
counter_ignore_and_non_system_ignore_are_independent
```

Existing tests must remain green.

### 9. Update README

Update:

```text
cpp/qsh_ingest/README.md
```

Document:

```text
what --non-system-mode means
that it is experimental
how it differs from --counter-mode
how to run non-system-flag-audit
how to compare crossed snapshots under include vs ignore-book
that default should not be changed without owner approval
```

### 10. Update long report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10V NonSystem flag semantics and crossed impact
```

Include:

```text
build/test result
real-sample validation result
NonSystem audit counts
record 16195 re-check
mode comparison table
crossed snapshot reduction or no reduction
classification A/B/C/D/E/F
files changed
next recommended task
```

## Decision rules

### If NonSystem ignore-book reduces 907 to 0 or near 0

Then conclusion should be:

```text
NonSystem events likely should not mutate normal visible L2 book.
```

Next task:

```text
M10W_NON_SYSTEM_REFERENCE_CHECK_AND_DEFAULT_DECISION
```

Do not change default yet without owner approval.

### If NonSystem ignore-book does not materially reduce 907

Then conclusion should be:

```text
NonSystem is not the main remaining cause.
```

Next task should inspect:

```text
transaction grouping
persistent top order lifecycle
market/session phase
Fill/Remove semantics
external reference implementation/spec comparison
```

### If results are mixed

Keep both modes experimental and document exactly where they differ.

## Done criteria

```text
1. build/test green;
2. real-sample validation run or QSH absence explicitly reported;
3. NonSystem audit implemented or existing diagnostics extended equivalently;
4. --non-system-mode include|ignore-book implemented as explicit experimental mode;
5. record 16195 behavior compared under include vs ignore-book;
6. crossed snapshot counts compared across counter/non-system modes;
7. summary JSON includes NonSystem stats;
8. validation script updated with NonSystem audit/modes;
9. deterministic tests added;
10. README and long report updated;
11. no raw QSH, generated CSV/JSON, .env, exe, dll, or secrets committed;
12. no live trading, broker connection, or real order sending.
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

## Expected next step after M10V

If NonSystem explains the remaining crossed snapshots:

```text
M10W — NonSystem reference check and default-mode decision
```

If NonSystem does not explain them:

```text
M10W — Persistent crossed-state lifecycle / transaction semantics investigation
```

Only after this should Trading Lab UI work resume:

```text
M10X — Trading Lab Data Quality UI
```
