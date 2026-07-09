# M10R_CROSSED_STATE_PERSISTENCE_AND_MARKET_PHASE_ANALYSIS

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10R_CROSSED_STATE_PERSISTENCE_AND_MARKET_PHASE_ANALYSIS.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Analyze crossed state persistence and market phase"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

M10Q found the direct cause of the first crossed state:

```text
record_index: 2136
event_type: ADD
side: BUY
price: 14100
amount_rest: 111
best_bid_before: 14055
best_ask_before: 14062
best_bid_after: 14100
best_ask_after: 14062
```

So the question is no longer "what created crossing". The new question is:

```text
How long does the crossed state persist, what events clear it, and should this market-data region be marked non-strategy-ready?
```

Do not add crossed-book filtering yet. This task is diagnostics and classification only.

## Known state

```text
first_crossing_event_record_index: 2136
first_crossing_snapshot_record_index: 2210
first_crossing_snapshot_index: 2111
strict crossed_book_snapshots: 7890
reduce-same-price crossed_book_snapshots: 7884
L2 strategy-ready: NO
```

M10Q conclusion:

```text
BUY 14100 is created by add_order at record 2136 from a normal system ADD event.
This is not snapshot init, not orphan cancel, not orphan fill, not tx-grouping.
```

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCrossingWindowAudit -RunFirstCrossedProbe
```

If local QSH is missing, write:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Add crossed-state persistence audit

Add a diagnostic command or script mode:

```text
crossed-persistence-audit <OrdLog.qsh> --from 2136 --max-records N --out <file.csv>
```

Generated output must stay under ignored report path:

```text
data/reports/qsh/RTS-3.21/2021-01-05/crossed_persistence_audit.csv
```

Track every transition after record 2136:

```text
record_index,tx_index,ts,event_type,order_id,side,price,amount,amount_rest,flags_hex,is_snapshot,is_txend,best_bid_before,best_ask_before,best_bid_after,best_ask_after,spread_before,spread_after,crossed_before,crossed_after,crossed_segment_id,mutation_path
```

### 2. Find when crossing clears

Report:

```text
first_crossing_record_index
first_uncross_record_index
records_crossed_duration
transactions_crossed_duration
snapshots_crossed_duration
best_bid_at_cross_start
best_ask_at_cross_start
best_bid_at_uncross
best_ask_at_uncross
```

If crossing never clears within scanned range, state that explicitly.

### 3. Trace relevant orders and levels

Track lifecycle after record 2136 for:

```text
bid_order=1925033994466246392
ask_order=1925033994522131746
price level BUY 14100
price level SELL 14062
```

Answer:

```text
Does bid_order get filled, cancelled, moved, or removed?
Does ask_order get filled, cancelled, moved, or removed?
Which event first restores best_bid < best_ask?
Does crossing persist because ask side is not matched, or because bid side remains stale?
```

### 4. Determine if this is short transition or persistent phase

Classify the first crossing as one of:

```text
A. short raw transition between related events;
B. persistent crossed state over many records/snapshots;
C. auction/session/clearing phase candidate;
D. decoder/bookkeeping issue still suspected;
E. inconclusive.
```

Use evidence only. Do not guess market phase if QSH does not expose phase.

### 5. Check market/session clues in available records

Inspect flags around the crossing window and persistence range:

```text
NewSession
Snapshot
TxEnd
NonSystem
CrossTrade
Moved
Canceled/CanceledGroup
replAct / NonZeroReplAct
```

If there is no explicit trading phase field, write:

```text
No explicit trading phase field available in current OrdLog decoder.
```

### 6. Compare strict vs reduce-same-price persistence

Report for both:

```text
strict
reduce-same-price
reduce+orphan-cancel-ignore
```

Include:

```text
first_crossing_event_record_index
first_crossing_snapshot_record_index
first_uncross_record_index
crossed duration
crossed snapshot count
```

### 7. Update script and README

Update:

```text
tools/run_qsh_real_sample_checks.ps1
cpp/qsh_ingest/README.md
```

Add optional switch:

```powershell
-RunCrossedPersistenceAudit
```

### 8. Update report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10R crossed state persistence and market phase analysis
```

Include:

```text
build/test result
real-sample validation result
crossed persistence summary
first uncross event if found
order/level lifecycle summary
classification A/B/C/D/E
strict vs reduce comparison
files changed
next recommended task
```

## Tests

Existing tests must remain green. Add tests for new command/instrumentation if code changes.

## Done criteria

```text
1. build/test green;
2. real-sample validation run with crossed persistence audit or QSH absence reported;
3. crossing duration is measured or explicitly marked uncleared in scan range;
4. clearing event is identified if it exists;
5. bid/ask order lifecycle after 2136 is summarized;
6. classification A/B/C/D/E is evidence-based;
7. no crossed-book filtering added;
8. generated CSV/JSON reports not committed;
9. L2 strategy-ready remains NO unless crossed diagnostics are genuinely clean.
```

## Safety

No broker connection, no live trading, no raw QSH commit, no generated CSV/JSON commit, no crossed-book skip/ignore shortcut in M10R.
