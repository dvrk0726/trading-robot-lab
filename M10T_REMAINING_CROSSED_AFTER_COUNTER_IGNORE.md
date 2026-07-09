# M10T_REMAINING_CROSSED_AFTER_COUNTER_IGNORE

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10T_REMAINING_CROSSED_AFTER_COUNTER_IGNORE.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate remaining crossed snapshots after counter ignore"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

M10S showed that Counter-flagged records are the primary cause of the early crossed-book reconstruction problem.

Experimental mode:

```text
--counter-mode ignore-book
```

reduced crossed snapshots:

```text
include/default:       crossed_book_snapshots = 7890
counter ignore-book:   crossed_book_snapshots = 907
reduction:             87%
```

However, the L2 output is still not strategy-ready because 907 crossed snapshots remain.

This task must explain the remaining 907 crossed snapshots after Counter events are ignored for visible book mutation.

Do not add generic crossed-book filtering. Do not hide the problem. First prove the cause.

## Known state after M10S

Real sample:

```powershell
.\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh
```

Known result table:

```text
mode                                                    missing_order_id   crossed_book_snapshots   first_crossed_book_record_index   l2_strategy_ready
per-record strict / include                            319                7890                     2210                              false
reduce+orphan-cancel-ignore / include                  0                  7884                     2242                              false
per-record counter-ignore-book                         272                907                      16215                             false
reduce+orphan-cancel-ignore+counter-ignore-book        0                  907                      16215                             false
```

Important prior finding:

```text
Counter=0x100 events should likely not mutate the normal visible order book.
```

But this is not enough. Remaining crossed states must be classified before changing defaults or moving to strategy research.

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCounterFlagAudit -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe
```

Then MiMo must run or reproduce the current counter-ignore-book mode:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 `
  --max-records 100000 `
  --max-snapshots 10000 `
  --snapshot-mode txend `
  --counter-mode ignore-book `
  --summary-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_counter_ignore.summary.json `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_counter_ignore.csv
```

If local QSH is missing, write exactly:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Locate the first remaining crossed event under counter-ignore-book

Use `--counter-mode ignore-book` as the experimental mode.

Find and document:

```text
first_remaining_crossing_event_record_index
first_remaining_crossing_snapshot_record_index
first_remaining_crossing_snapshot_index
best_bid_before
best_ask_before
best_bid_after
best_ask_after
last_event_type
last_order_id
last_side
last_price
last_amount
last_amount_rest
last_flags_hex
last_repl_act
tx_index
records_in_tx
```

Known starting clue from M10S:

```text
first_crossed_book_record_index around 16215 after counter-ignore-book
```

Do not assume 16215 is the cause. Prove the exact crossing event, snapshot record, and snapshot index.

### 2. Add a focused remaining-crossed diagnostic

Add a command or script-supported diagnostic, for example:

```text
remaining-crossed-audit <OrdLog.qsh> --counter-mode ignore-book --out <file.csv> [--from N] [--to N] [--context N]
```

Acceptable alternative: extend an existing command if cleaner.

Generated output must stay under ignored report path:

```text
data/reports/qsh/RTS-3.21/2021-01-05/remaining_crossed_after_counter_ignore.csv
```

The CSV should include enough raw and interpreted fields to classify the cause:

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
entry_flags_hex
repl_act
is_counter
is_cross_trade
is_moved
is_cancel
is_cancel_group
is_remove
is_add
is_fill
is_snapshot
is_new_session
is_txend
is_non_system
is_system
is_non_zero_repl_act
best_bid_before
best_ask_before
best_bid_after
best_ask_after
spread_before
spread_after
mutation_path
```

### 3. Inspect special flags around the remaining crossing

For the first remaining crossed region, inspect at least a context window around it:

```text
record_index - 50 .. record_index + 100
```

Check whether the crossing is related to any of:

```text
Counter
CrossTrade
Moved
Canceled
CanceledGroup
Remove / amount_rest == 0
Snapshot
NewSession
TxEnd
NonSystem
NonZeroReplAct
Quote
FillOrKill
unknown side
unknown event
price delta anomaly
order_id delta anomaly
```

Do not collapse these into one generic category. Report each one explicitly.

### 4. Trace both crossing orders

For the first remaining crossed snapshot under counter-ignore-book, identify:

```text
best_bid_order_id(s)
best_ask_order_id(s)
```

Then trace lifecycle of the top bid and ask orders:

```text
ADD record
FILL records
CANCEL records
REMOVE records
MOVE records
whether still active at end
final active qty
```

If the exact order IDs cannot be identified, explain why and add the missing diagnostic needed to identify them.

### 5. Classify remaining 907 crossed snapshots

Give one evidence-based classification:

```text
A. Another special event class should not mutate normal visible book.
B. Crossed states are short transitional states; mark affected snapshots non-strategy-ready.
C. Decoder or OrderBook bug.
D. Market/session phase not exposed by current decoder.
E. Inconclusive; more QSH spec/reference comparison needed.
```

If classification is A, state exactly which flag/event class and why.

If classification is C, state exact bug hypothesis and the smallest failing test to add.

If classification is D, state what field or external reference is needed to expose phase.

### 6. Compare modes, but do not change default yet

Compare at least:

```text
include/default
counter-ignore-book
reduce+orphan-cancel-ignore+counter-ignore-book
```

Comparison table must show:

```text
mode
counter_mode
orphan_fill_mode
orphan_cancel_mode
missing_order_id
crossed_book_snapshots
first_crossing_event_record_index
first_crossing_snapshot_record_index
first_crossing_snapshot_index
counter_records_seen
counter_records_ignored_for_book
l2_strategy_ready
remaining_crossing_classification
```

Do not change default `--counter-mode` in this task unless the remaining 907 crossed snapshots are fully understood and all tests are updated.

### 7. Add tests if behavior changes

If MiMo changes reconstruction logic, add deterministic tests.

Minimum test requirements if new logic is added:

```text
existing tests remain green
counter-mode include still preserves old behavior
counter-mode ignore-book still skips Counter mutation and counts skipped events
remaining-crossed audit works on synthetic records
classification/summary output is deterministic
no generic crossed-book hiding is introduced
```

### 8. Update script and README

Update if new command/switch is added:

```text
tools/run_qsh_real_sample_checks.ps1
cpp/qsh_ingest/README.md
```

Suggested script switch:

```powershell
-RunRemainingCrossedAudit
```

Document that `ignore-book` is still experimental until remaining crossed snapshots are classified.

### 9. Update long report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10T Remaining crossed snapshots after counter-ignore-book
```

Include:

```text
build/test result
real-sample validation result
first remaining crossed event/snapshot
raw flags around crossing
order lifecycle traces
mode comparison table
A/B/C/D/E classification
files changed
next recommended task
```

## Done criteria

```text
1. build/test green;
2. real-sample validation run or QSH absence explicitly reported;
3. first remaining crossed event under counter-ignore-book is identified;
4. raw flags and special event classes around the remaining crossing are audited;
5. top bid/ask crossing order lifecycle is traced or missing diagnostic is added;
6. remaining 907 crossed snapshots are classified A/B/C/D/E;
7. no generic crossed-book filtering is added;
8. default counter mode is not changed unless remaining crossed states are fully understood;
9. generated CSV/JSON reports are not committed;
10. L2 strategy-ready remains NO unless crossed_book_snapshots is actually 0 and evidence supports it.
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
No generic crossed-book skip/hide shortcut.
```

## Expected next step after M10T

If remaining crossed snapshots are explained and solved:

```text
M10U — normalize strategy-ready L2 export and Data Quality gating
```

If they are not solved:

```text
M10U — external reference/spec comparison for remaining OrdLog crossed states
```
