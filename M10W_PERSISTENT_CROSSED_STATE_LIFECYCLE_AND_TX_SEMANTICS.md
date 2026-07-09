# M10W_PERSISTENT_CROSSED_STATE_LIFECYCLE_AND_TX_SEMANTICS

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10W_PERSISTENT_CROSSED_STATE_LIFECYCLE_AND_TX_SEMANTICS.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate persistent crossed-state lifecycle and transaction semantics"
```

Owner final check:

```powershell
git status --short
git log --oneline -3
```

## Mission

M10S proved that `Counter=0x100` events are the primary cause of the early crossed-book reconstruction problem:

```text
crossed_book_snapshots: 7890 -> 907
```

after:

```text
--counter-mode ignore-book
```

M10U added row-level strategy readiness gating:

```text
strategy_ready
strategy_reject_reason
```

and confirmed that the remaining bad rows are all:

```text
strategy_reject_reason = crossed_book
```

M10V proved that `NonSystem` is not the remaining cause:

```text
--counter-mode ignore-book + --non-system-mode ignore-book
crossed_book_snapshots: 907 -> 907
```

Now investigate the real cause of the persistent crossed state.

This task must answer:

```text
Which exact event first creates the remaining crossed state under counter-ignore-book/non-system-ignore-book?
Why does that crossed state persist?
Which lifecycle event is missing, misapplied, or incorrectly interpreted?
```

## Important correction from M10V

M10T reported record 16195 as the first remaining crossing trigger.

M10V showed that this was incomplete/wrong:

```text
record 16195 under counter-ignore-book:
best_bid_before = 14110
best_ask_before = 14062
crossed_before = YES
```

Therefore record 16195 is not the true first cause. It occurs after the book is already crossed.

M10W must locate the true first event that changes the book from:

```text
not crossed -> crossed
```

under the current experimental cleanest mode:

```text
--counter-mode ignore-book --non-system-mode ignore-book
```

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCounterFlagAudit -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe -RunRemainingCrossedAudit -RunStrategyReadyExport -RunNonSystemFlagAudit
```

If local QSH is missing, write exactly:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Fix or replace first-crossing diagnostics

Existing first-crossing diagnostics produced conflicting results:

```text
M10T: first_remaining_crossing_event_record_index = 16195
M10V: record 16195 already has crossed_before = YES
```

This means the diagnostic is not locating the true transition event correctly.

Add or fix a diagnostic that explicitly detects the transition:

```text
was_crossed == false && is_crossed_after == true
```

Preferred command:

```text
persistent-crossed-root-cause <OrdLog.qsh> --counter-mode ignore-book --non-system-mode ignore-book --out <file.csv> [--from N] [--to N] [--context N]
```

The diagnostic must output:

```text
transition_record_index
transition_snapshot_index
transition_tx_index
event_type
order_id
side
price
amount
amount_rest
flags_hex
entry_flags_hex
is_counter
is_non_system
is_cross_trade
is_moved
is_add
is_fill
is_cancel
is_remove
is_snapshot
is_new_session
is_txend
is_system
is_non_zero_repl_act
best_bid_before
best_ask_before
best_bid_after
best_ask_after
spread_before
spread_after
crossed_before
crossed_after
mutation_path
```

It must also write a context window around the transition:

```text
transition_record_index - 100 .. transition_record_index + 200
```

### 2. Trace top-of-book order lifecycle

At the true transition event, identify the active top bid and top ask orders that make the book crossed.

For each of these orders, trace complete lifecycle:

```text
order_id
side
price
ADD record(s)
FILL record(s)
CANCEL record(s)
REMOVE record(s)
MOVED record(s)
Counter/NonSystem flags on each lifecycle event
amount before/after each event
amount_rest reported by the record
book quantity before/after
is still active after transition window
is still active at end of 100k-record validation window
is still active at end of file if full scan is feasible
```

Expected key question:

```text
Is there a missing/misinterpreted Fill/Cancel/Remove/Move that should have removed the crossing top bid or ask?
```

### 3. Check Fill semantics in detail

The previous work tested `fill_semantics delta|rest`, but M10W must focus on the actual persistent crossing orders.

For the crossing orders, compare:

```text
fill_semantics = delta
fill_semantics = rest
```

Report for each mode:

```text
does the transition still happen?
does the crossed state persist?
what is final remaining qty of crossing orders?
which Fill record changes the qty?
does amount_rest represent remaining size, executed size, or delta for this specific lifecycle?
```

Do not generalize from global counts only. Use the actual crossing orders.

### 4. Check transaction grouping / TxEnd semantics

Run and compare:

```text
--book-update-mode per-record
--book-update-mode tx-grouped
--snapshot-mode event
--snapshot-mode txend
```

Under:

```text
--counter-mode ignore-book --non-system-mode ignore-book
```

Determine:

```text
does the true transition occur inside a multi-record transaction?
what records are in that transaction?
does TxEnd ordering matter?
do intermediate records cross but final TxEnd book state uncrosses?
or does the final transaction state remain crossed?
```

If `tx-grouped` does not currently apply all mutations atomically before snapshot/gating, explain the exact implementation and whether it is correct.

### 5. Check market/session phase clues

Audit the transition area for:

```text
NewSession
Snapshot
System records
trading halt/session boundary
large time gap
unusual timestamp jump
CrossTrade
CanceledGroup
FillOrKill
NonZeroReplAct
unknown flags
```

If there are unknown flags, list them in hex and count them near the transition.

### 6. Compare external/reference behavior if possible

If there is any existing qsh-rs note, QScalp note, old research note, or code reference in the repo, use it.

Do not browse random internet unless necessary. Prefer repository notes first.

If no local reference is available, write:

```text
no local external reference found
```

Then state exactly what external reference is needed:

```text
QSH OrdLog flag semantics for Fill/Counter/CrossTrade/System/NonSystem
QScalp qsh2txt output for the transition window
independent qsh-rs reconstruction for the same records
MOEX ASTS/FAST OrdLog semantics for comparable fields
```

### 7. Add summary JSON fields

Extend summary JSON or diagnostics output with persistent crossed lifecycle fields:

```text
persistent_crossed_transition_record_index
persistent_crossed_transition_snapshot_index
persistent_crossed_transition_tx_index
persistent_crossed_transition_event_type
persistent_crossed_transition_order_id
persistent_crossed_transition_flags_hex
persistent_crossed_transition_best_bid_before
persistent_crossed_transition_best_ask_before
persistent_crossed_transition_best_bid_after
persistent_crossed_transition_best_ask_after
persistent_crossed_top_bid_order_id
persistent_crossed_top_bid_price
persistent_crossed_top_bid_final_qty
persistent_crossed_top_ask_order_id
persistent_crossed_top_ask_price
persistent_crossed_top_ask_final_qty
persistent_crossed_persists_to_end_of_window
persistent_crossed_persists_to_end_of_file_if_checked
```

Do not overload existing fields if the meaning is different.

### 8. Update validation script

Update:

```text
tools/run_qsh_real_sample_checks.ps1
```

Add optional switch:

```powershell
-RunPersistentCrossedRootCause
```

The script should run the new diagnostic under:

```text
--counter-mode ignore-book --non-system-mode ignore-book
```

and write output under:

```text
data/reports/qsh/RTS-3.21/2021-01-05/
```

Do not commit generated CSV/JSON reports.

### 9. Add deterministic tests

Add tests for the new transition detector and lifecycle tracing.

Minimum tests:

```text
transition_detector_finds_not_crossed_to_crossed
transition_detector_does_not_report_already_crossed_event
transition_detector_handles_counter_ignored_events
transition_detector_handles_non_system_ignored_events
lifecycle_trace_records_add_fill_cancel_remove_for_top_order
lifecycle_trace_marks_persistent_order_active_when_not_removed
```

If full lifecycle tracing is too hard to unit-test directly, test the helper-level logic used by the command.

Existing tests must remain green.

### 10. Update README

Update:

```text
cpp/qsh_ingest/README.md
```

Document:

```text
persistent-crossed-root-cause command
how it differs from first-crossed-root-cause and remaining-crossed-audit
how to run it under counter-ignore-book + non-system-ignore-book
what persistent crossed state means
why l2_strategy_ready remains false
```

### 11. Update long report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10W Persistent crossed-state lifecycle and transaction semantics
```

Include:

```text
build/test result
real-sample validation result
true transition record
why earlier diagnostics pointed to 16195 incorrectly
top bid/ask lifecycle traces
Fill semantics comparison
TxEnd / transaction grouping analysis
market/session phase clues
external/reference check result
classification
files changed
next recommended task
```

## Classification required

At the end, classify the remaining crossed state as one of:

```text
A. Fill semantics bug
B. Cancel/Remove lifecycle bug
C. Move semantics bug
D. Transaction grouping / TxEnd application bug
E. Market/session phase not represented in current decoder
F. Special flag/event class still missing
G. QSH decoder field/flag interpretation bug
H. Genuine market crossed state that should remain gated, not fixed
I. Inconclusive; external reference comparison required
```

Do not choose H unless there is proof that the final visible book legitimately remains crossed according to an external/reference interpretation.

## Done criteria

```text
1. build/test green;
2. real-sample validation run or QSH absence explicitly reported;
3. true not-crossed -> crossed transition found under counter-ignore-book + non-system-ignore-book;
4. diagnostic no longer mislabels an already-crossed event as first cause;
5. top bid and top ask lifecycle traced;
6. Fill semantics checked on the actual crossing orders;
7. TxEnd / transaction grouping checked on the actual transition;
8. market/session/unknown-flag clues audited;
9. persistent crossed classification A-I written with evidence;
10. README and long report updated;
11. no raw QSH, generated CSV/JSON, .env, exe, dll, or secrets committed;
12. no broker connection, live trading, or real order sending.
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

## Expected next step after M10W

If a concrete parser/orderbook bug is found:

```text
M10X_FIX_PERSISTENT_CROSSED_ROOT_CAUSE_AND_REGRESSION_TESTS
```

If external reference is required:

```text
M10X_QSH_REFERENCE_COMPARISON_FOR_PERSISTENT_CROSSED_STATE
```

If the remaining crossed state is proven genuine and safely gated:

```text
M10X_TRADING_LAB_DATA_QUALITY_UI
```

Only after Data Quality is understood and visible should the project move toward:

```text
M11 — normalized microstructure research dataset / first RI-Synthetic lead-lag preparation
```
