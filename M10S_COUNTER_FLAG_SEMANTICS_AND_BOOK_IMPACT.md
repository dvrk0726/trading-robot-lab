# M10S_COUNTER_FLAG_SEMANTICS_AND_BOOK_IMPACT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10S_COUNTER_FLAG_SEMANTICS_AND_BOOK_IMPACT.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate Counter flag semantics and book impact"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

M10R proved that the crossed state is persistent and begins at a normal-looking ADD event:

```text
record_index: 2136
event_type: ADD
side: BUY
price: 14100
amount_rest: 111
flags include Counter=0x100
best_bid_before: 14055
best_ask_before: 14062
best_bid_after: 14100
best_ask_after: 14062
```

The next hypothesis is that `Counter=0x100` events may not belong to the normal visible limit order book reconstruction path, or require special handling.

Do not add generic crossed-book filtering yet. First test whether Counter events are the actual cause.

## Known state

```text
first_crossing_event_record_index: 2136
crossed state persists to end of file
crossed_records_count: 5,359,671
crossed_snapshots_count: 4,265,129
classification: B, persistent crossed state
no explicit trading phase field available in current OrdLog decoder
```

M10R key clue:

```text
Counter flag (0x100) is set on the crossing ADD and subsequent fill events.
```

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe
```

If local QSH is missing, write:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Audit all Counter flag records

Add a diagnostic command or script switch:

```text
counter-flag-audit <OrdLog.qsh> --out <file.csv> [--max-records N]
```

Generated output must stay under ignored report path:

```text
data/reports/qsh/RTS-3.21/2021-01-05/counter_flag_audit.csv
```

Report summary:

```text
counter_records_total
counter_add
counter_fill
counter_cancel
counter_remove
counter_move
counter_buy
counter_sell
counter_snapshot
counter_new_session
counter_txend
counter_non_system
counter_cross_trade
first_counter_record_index
first_counter_add_record_index
first_counter_crossing_record_index
```

### 2. Determine book impact of Counter records

For Counter events, measure whether they create or sustain crossed states:

```text
counter_events_that_create_new_best_bid
counter_events_that_create_new_best_ask
counter_events_that_create_crossed_book
counter_events_inside_crossed_state
counter_events_that_uncross_book
```

Specifically inspect record 2136 and record 2137.

### 3. Add experimental Counter handling mode

Add an experimental mode to `l3-to-l2`:

```text
--counter-mode include|ignore-book
```

Rules:

```text
include: current behavior, default.
ignore-book: do not mutate OrderBook with Counter-flagged ADD/FILL/CANCEL/REMOVE/MOVE records, but count them separately.
```

Do not delete Counter records from diagnostics. They should remain visible as separate events.

Add summary JSON fields:

```text
counter_mode
counter_records_seen
counter_records_ignored_for_book
counter_add_ignored_for_book
counter_fill_ignored_for_book
counter_cancel_ignored_for_book
counter_remove_ignored_for_book
counter_move_ignored_for_book
```

### 4. Compare reconstruction modes

Update validation script to include at least:

```text
per-record strict
per-record reduce-same-price
per-record reduce+orphan-cancel-ignore
per-record counter-ignore-book
per-record reduce+orphan-cancel-ignore+counter-ignore-book
```

Main comparison table must show:

```text
mode
counter_mode
missing_order_id
crossed_book_snapshots
first_crossing_event_record_index
first_crossing_snapshot_record_index
counter_records_seen
counter_records_ignored_for_book
l2_strategy_ready
```

Most important question:

```text
Does --counter-mode ignore-book remove or reduce crossed_book_snapshots?
```

### 5. Evaluate whether Counter should be excluded from normal L2

Report one evidence-based conclusion:

```text
A. Counter events should not mutate normal visible book; ignore-book fixes crossed state.
B. Counter events are normal book events; ignore-book does not help.
C. Counter events require special transform, not simple ignore.
D. Inconclusive; keep include default.
```

Do not change default to ignore unless evidence is strong. Keep experimental mode explicit.

### 6. qsh-rs / external comparison if available

If local qsh-rs or reference output exists, compare how it treats Counter flag. If not available, write:

```text
qsh-rs Counter comparison skipped: not available locally
```

Do not vendor qsh-rs code.

### 7. Update script and README

Update:

```text
tools/run_qsh_real_sample_checks.ps1
cpp/qsh_ingest/README.md
```

Add optional switch:

```powershell
-RunCounterFlagAudit
```

Document:

```text
counter-flag-audit
--counter-mode include|ignore-book
that ignore-book is experimental and not default
```

### 8. Update report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10S Counter flag semantics and book impact
```

Include:

```text
build/test result
real-sample validation table
counter flag audit summary
book impact summary
counter-mode comparison
A/B/C/D conclusion
files changed
next recommended task
```

## Tests

Add tests if implementation changes behavior.

Minimum:

```text
existing tests remain green
counter-mode include preserves current behavior
counter-mode ignore-book skips book mutation but counts skipped events
summary JSON fields are deterministic
counter audit handles no-counter files safely
```

## Done criteria

```text
1. build/test green;
2. real-sample validation run with Counter audit or QSH absence reported;
3. Counter flag event statistics are documented;
4. experimental counter-mode include|ignore-book exists if implemented;
5. effect on crossed_book_snapshots is measured;
6. A/B/C/D conclusion is evidence-based;
7. no generic crossed-book filtering added;
8. generated CSV/JSON reports are not committed;
9. L2 strategy-ready remains NO unless diagnostics are actually clean.
```

## Safety

No broker connection, no live trading, no raw QSH commit, no generated CSV/JSON commit, no generic crossed-book skip/ignore shortcut in M10S.
