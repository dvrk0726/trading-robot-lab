# QSH / OrdLog Data Layer Handoff through M10S

Date: 2026-07-09
Repo: `dvrk0726/trading-robot-lab`
Audience: another AI agent continuing the project

## Project scope

This repository is a research lab for a future trading robot architecture. Current work is offline historical-data research only.

Do not add:

```text
broker connections
live trading
real order sending
secrets / .env
raw QSH files
generated CSV/JSON reports
old Plaza2/QUIK runtime architecture as the new direction
```

Current technical focus:

```text
QSH v4 / OrdLog parsing
L3 -> L2 order book reconstruction
Data Quality diagnostics
strategy-ready gating
future FAST/FIX/TWIME-compatible data layer
```

## Local environment expected by owner

Repo path:

```powershell
C:\ProjectsHFT\trading-robot-lab
```

Real sample path, ignored by Git:

```powershell
.\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh
```

Build/test:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

Real-sample validation script:

```powershell
.\tools\run_qsh_real_sample_checks.ps1
```

Useful extended validation switches now include:

```powershell
-RunMissingCancelProbe
-RunOrphanCancelAudit
-RunFirstCrossedProbe
-RunSnapshotAudit
-RunCrossingWindowAudit
-RunCrossedPersistenceAudit
-RunCounterFlagAudit
```

Generated outputs must stay under:

```text
data/reports/qsh/...
```

They must not be committed.

## Key files to read first

```text
AI_CONTEXT.md
PROJECT_STATE.md
README.md
cpp/qsh_ingest/README.md
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

The long report contains the full M10A-M10S investigation trail.

## Known QSH sample facts

`quality` on the real sample showed:

```text
Records total: 5,362,594
Records valid: 5,362,594
Records rejected: 0
Add: 2,568,449
Fill: 587,076
Cancel: 1,700,849
Remove: 0
NewSession: 2
TxEnd: 4,268,048
Snapshot: 1,965
Moved: 497,113
CrossTrade: 63
```

Initial broad issue: exported L2 contained many crossed snapshots and was not strategy-ready.

## Investigation timeline summary

### M10F-M10G

`--fill-semantics delta|rest` was tested.

Result:

```text
delta: better, amount_mismatch=0
rest: worse, amount_mismatch>0
```

Snapshot loading alone did not fix missing order IDs or crossed book.

### M10H-M10I

First missing order was audited. Decoder appeared to decode IDs/prices consistently, but orphan fills/cancels remained.

Important early chain around record 1651 showed FILL records with order_id deltas and amount_rest continuation.

### M10J

qsh-rs comparison was added conceptually. qsh-rs exists at `https://github.com/2dav/qsh-rs`; do not vendor code.

Added:

```text
--orphan-fill-mode strict|ignore|reduce-same-price|transaction-rest
```

`reduce-same-price` removed missing_on_fill but did not fix crossed book.

### M10K

Added transaction grouping:

```text
--book-update-mode per-record|tx-grouped
```

Tx grouping did not fix crossed book.

### M10L-M10M

Automation was fixed with:

```text
tools/run_qsh_real_sample_checks.ps1
```

This script is now the standard way to validate the local real sample.

### M10N

Added orphan cancel/remove diagnostics and:

```text
--orphan-cancel-mode strict|ignore
orphan-cancel-audit
first-crossed-root-cause
```

Finding: orphan cancel/remove is not the primary crossed-book cause.

### M10O

Resolved index ambiguity:

```text
first_crossing_event_record_index: 2136
first_crossing_snapshot_record_index: 2210
first_crossing_snapshot_index: 2111
```

Meaning:

```text
2136 = record whose application first makes best_bid >= best_ask
2210 = record where first crossed L2 snapshot is emitted in txend mode
2111 = exported snapshot index
```

M10O initially suspected snapshot origin for `bid_order=1925033994466246392`, but this was later disproven.

### M10P

Snapshot audit proved snapshot initialization is clean:

```text
snapshot_records_processed: 1964
snapshot_best_bid: 14055
snapshot_best_ask: 14062
snapshot_spread: 7
snapshot_crossed_at_initial_load: NO
```

The target order:

```text
bid_order=1925033994466246392 BUY 14100 qty=111
```

was not in snapshot records.

### M10Q

Actual source of first crossing was found.

Record 2136:

```text
event_type: ADD
side: BUY
price: 14100
amount: 111
amount_rest: 111
flags_hex: 0x114
is_snapshot: 0
is_system: 1
best_bid_before: 14055
best_ask_before: 14062
best_bid_after: 14100
best_ask_after: 14062
mutation_path: add_order
```

Conclusion: first crossing is caused by a normal-looking ADD event at record 2136.

The earlier M10O lifecycle contradiction was caused by lifecycle tracing starting too late.

### M10R

Crossed state persistence audit showed the crossing does not clear through the file.

```text
first_crossing_record_index: 2136
first_uncross_record_index: NOT FOUND
crossed_records_count: 5,359,671
crossed_snapshots_count: 4,265,129
```

`bid_order=1925033994466246392`:

```text
ADD at 2136 qty=111
partial FILL at 2137: 111 -> 81
remains active at qty=81 to end
not canceled
not removed
not fully filled
```

`ask_order=1925033994522131746`:

```text
ADD at 1957 SELL 14062 qty=30
remains active to end
not filled
not canceled
not removed
```

Critical clue: record 2136 and subsequent fill records have Counter flag `0x100`.

### M10S

Counter flag semantics were tested.

Added:

```text
counter-flag-audit
--counter-mode include|ignore-book
```

Counter audit summary:

```text
counter_records_total: 724,888
counter_add: 316,582
counter_fill: 148,929
counter_remove: 259,377
counter_cancel: 0
counter_cross_trade: 63
first_counter_record_index: 2136
first_counter_add_record_index: 2136
```

Record 2136:

```text
flags: 0x114 = Counter + Add + Buy
```

Record 2137:

```text
flags: 0x118 = Counter + Fill + Buy
```

Mode comparison:

```text
include/default crossed_book_snapshots: 7890
counter ignore-book crossed_book_snapshots: 907
reduction: 87%
```

Most important conclusion:

```text
Counter events are the primary cause of early crossed-book reconstruction.
Counter events probably should not mutate the normal visible order book.
```

But M10S did not fully solve everything:

```text
crossed_book_snapshots after counter-ignore-book: 907
L2 strategy-ready: false
```

## Current status after M10S

Strong evidence supports keeping Counter events as diagnostics but excluding them from normal visible book mutation.

However, there are still 907 crossed snapshots after `--counter-mode ignore-book`. Do not declare L2 strategy-ready yet.

Current status:

```text
L2 strategy-ready: NO
```

## Recommended next task

Create/execute:

```text
M10T_REMAINING_CROSSED_AFTER_COUNTER_IGNORE
```

Purpose:

```text
1. Use --counter-mode ignore-book as experimental mode.
2. Find the first remaining crossed snapshot/event, currently around first_crossed_book_record_index=16215.
3. Inspect raw flags around that point.
4. Check for CrossTrade, Moved, CanceledGroup, NonSystem, Snapshot, replAct, TxEnd, or another special flag.
5. Determine whether remaining 907 crossed snapshots are:
   A. another special event class to exclude from visible book;
   B. short transitions to mark non-strategy-ready;
   C. true decoder/orderbook bug;
   D. market phase not exposed by current decoder;
   E. inconclusive.
6. Do not switch default counter mode until remaining crossed states are understood, unless owner explicitly chooses that direction.
```

## Important commands for the next AI

Run current validation:

```powershell
.\tools\run_qsh_real_sample_checks.ps1 -RunCounterFlagAudit -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe
```

Test a specific reconstruction mode:

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

## Non-negotiable safety rules

```text
Never commit data/raw/
Never commit data/reports/
Never commit .env or secrets
Never connect broker/live trading
Never mark L2 strategy-ready while crossed_book_snapshots > 0
Do not hide crossed book with generic filtering before proving the cause
Keep all experimental modes explicit until evidence is complete
```
