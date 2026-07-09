# M10O_FIRST_CROSSED_ORDER_LIFECYCLE_AND_SESSION_STATE_AUDIT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

Owner starts MiMo:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10O_FIRST_CROSSED_ORDER_LIFECYCLE_AND_SESSION_STATE_AUDIT.md"
```

MiMo must run build, tests, and real-sample validation by itself.

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Audit first crossed order lifecycle and session state"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

M10N showed that orphan cancel/remove is probably not the main crossed-book root cause. The current blocker is that the reconstructed book contains real active orders where best bid is above best ask.

Focus on:

```text
first crossed book root cause
full lifecycle of crossing orders
record index vs snapshot index clarification
session/snapshot/auction state around first crossing
OrdLog decoder semantics
QSH/qsh-rs comparison where useful
L3 -> L2 correctness
```

Do not use old Plaza2/QUIK runtime logic as a development direction. Old code may be used only as historical context, not as a source of production assumptions.

Future runtime connection direction remains FAST/FIX/TWIME-compatible. This task is still offline historical data analysis only.

## Current known state from M10N

Last useful commit:

```text
1adaac2 Investigate orphan cancel and first crossed book root cause
```

M10N real-sample table:

```text
per-record strict:              missing=319, crossed=7890, first_crossed=2210, ready=NO
reduce-same-price:              missing=171, crossed=7884, first_crossed=2242, ready=NO
snapshot-records-mode load:     missing=319, crossed=7890, first_crossed=2210, ready=NO
tx-grouped:                     missing=319, crossed=7890, first_crossed=2210, ready=NO
```

M10N orphan cancel/remove audit:

```text
Cancel events:              198
Remove events:              2
System records:             200
Non-system records:         0
Snapshot records:           0
With prior ADD:             0
With prior Snapshot:        0
With any prior occurrence:  2
With next occurrence:       0
Near NewSession (±50):      0
```

Current interpretation:

```text
A. orphan cancel/remove is likely benign for active book mutation;
B. crossed book is not explained by orphan cancel/remove;
C. the first crossing has real active orders in the reconstructed book.
```

M10N identified crossing orders:

```text
best_bid: 14100
best_ask: 14062

bid_order: 1925033994466246392
side: BUY
price: 14100
qty: 111

ask_order: 1925033994522131746
side: SELL
price: 14062
qty: 30
```

Known discrepancy that must be resolved:

```text
first-crossed-root-cause says first crossed at record 2136
validation summary says first_crossed_book_record_index=2210
```

This may be event index vs exported snapshot/TxEnd index. Do not leave this ambiguous.

## Mandatory MiMo verification

Before reporting success, MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunOrphanCancelAudit -RunFirstCrossedProbe
```

If local QSH is missing, MiMo must write:

```text
local QSH not found, real-sample validation skipped
```

Do not claim real-sample success without running the script on the local QSH.

## Required work

### 1. Add explicit validation modes for orphan cancel ignore

Update `tools/run_qsh_real_sample_checks.ps1` so the main comparison table also includes:

```text
orphan-cancel-ignore
reduce-same-price + orphan-cancel-ignore
```

Recommended modes:

```text
per-record strict
per-record reduce-same-price
per-record orphan-cancel-ignore
per-record reduce-same-price + orphan-cancel-ignore
snapshot-records-mode load
tx-grouped
```

Each mode must write its own CSV and summary JSON under:

```text
data/reports/qsh/RTS-3.21/2021-01-05/
```

Generated files must not be committed.

The markdown table should include:

```text
mode
missing_order_id
missing_on_fill
missing_on_cancel
missing_on_remove
orphan_cancel_mode
orphan_cancel_ignored
orphan_remove_ignored
orphan_fill_events
orphan_fill_level_reductions
crossed_book_snapshots
non_positive_spread_snapshots
first_missing_order_record_index
first_crossed_book_record_index
l2_strategy_ready
```

### 2. Clarify record index vs snapshot index

Add clear definitions to CLI output, summary JSON, and report:

```text
first_crossing_event_record_index
first_crossing_snapshot_record_index
first_crossing_snapshot_index
```

Definitions:

```text
first_crossing_event_record_index:
  the OrdLog record whose application first makes book.best_bid >= book.best_ask.

first_crossing_snapshot_record_index:
  the OrdLog record index where the first exported invalid L2 snapshot is emitted.
  In txend snapshot mode this may be later than the event index.

first_crossing_snapshot_index:
  the number/index of the exported L2 snapshot that first contains crossed book.
```

Keep existing fields if needed for compatibility, but add the unambiguous new fields.

### 3. Trace full lifecycle of the crossing orders

Add or improve command output so it can fully trace:

```text
bid_order=1925033994466246392
ask_order=1925033994522131746
```

For each order, produce lifecycle CSV under ignored reports path:

```text
first_crossed_bid_order_lifecycle.csv
first_crossed_ask_order_lifecycle.csv
first_crossed_orders_lifecycle_combined.csv
```

Minimum columns:

```text
record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,order_id_before_delta,order_id_after_delta,order_id_delta,price_before_delta,price_after_delta,price_delta,ts_before_delta,ts_after_delta,amount_before,amount_after,order_id_path,best_bid_before,best_ask_before,best_bid_after,best_ask_after,active_qty_before,active_qty_after
```

The lifecycle report must answer:

```text
Where was each order ADDed?
Was ADD system or non-system?
Was it snapshot/new-session related?
Was side decoded consistently?
Was price decoded by delta or absolute path?
Did fill/cancel/remove/move affect it before crossing?
Did either order become stale because an event was missed?
```

### 4. Dump raw decoder audit around crossing orders

Create focused audit dumps for the record ranges around:

```text
first crossing event
first exported crossed snapshot
ADD/FILL/CANCEL/REMOVE lifecycle of the two crossing orders
```

Output files under ignored report path:

```text
first_crossed_event_window_audit.csv
first_crossed_snapshot_window_audit.csv
first_crossed_orders_raw_audit.csv
```

Do not commit generated reports.

### 5. Determine session / auction / snapshot state

Add diagnostics answering whether the first crossing happens:

```text
before first NewSession
immediately after NewSession
inside snapshot initialization
before first normal continuous trading state
inside a transaction group
around TxEnd
```

If QSH file does not expose explicit session phase, say so. Do not guess.

Report must include:

```text
new_session_records_seen
first_new_session_record_index
first_valid_book_record_index
first_crossing_event_record_index
first_crossing_snapshot_record_index
records_between_new_session_and_first_crossing
snapshot_records_before_first_crossing
tx_index_at_first_crossing
records_in_that_transaction
```

### 6. qsh-rs comparison, if available

Do not vendor qsh-rs code. Do not copy its implementation.

If local qsh-rs or reference output is available, compare only outputs/semantics:

```text
Does qsh-rs decode the same order IDs?
Does qsh-rs decode the same price values 14100 / 14062?
Does qsh-rs identify the same event types around record 2136/2210?
Does qsh-rs produce the same crossing state, if it has L2 reconstruction?
```

If qsh-rs is not locally available, write:

```text
qsh-rs comparison skipped: not available locally
```

### 7. Update README

Update:

```text
cpp/qsh_ingest/README.md
```

Must document:

```text
-RunOrphanCancelAudit
-RunFirstCrossedProbe
--orphan-cancel-mode strict|ignore
new first crossing index definitions
new lifecycle/audit commands if added
```

### 8. Update report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10O first crossed order lifecycle and session state audit
```

The section must include:

```text
build/test result
real-sample validation table including orphan-cancel-ignore modes
record index vs snapshot index clarification
lifecycle summary for bid_order and ask_order
raw decoder audit summary
session/snapshot/transaction state summary
qsh-rs comparison result or skipped reason
files changed
next recommended task
```

## Tests

Add tests if behavior or summary fields change.

Minimum:

```text
existing C++ tests remain green
summary JSON contains new index fields
orphan-cancel-ignore mode is included in automated validation table
first-crossed commands do not crash on non-crossed data
lifecycle trace handles missing order IDs safely
README command examples remain accurate
```

## Done criteria

Task is done when:

```text
1. build/test are green;
2. real-sample script was run with -RunOrphanCancelAudit and -RunFirstCrossedProbe, or local QSH absence is clearly reported;
3. the table includes orphan-cancel-ignore and reduce+orphan-cancel-ignore modes;
4. 2136 vs 2210 discrepancy is resolved with explicit index definitions;
5. full lifecycle of bid_order=1925033994466246392 and ask_order=1925033994522131746 is documented;
6. raw decoder audit around the crossing orders is documented;
7. session/snapshot/transaction state around first crossing is documented;
8. L2 strategy-ready remains NO unless crossed diagnostics are actually clean;
9. generated CSV/JSON reports are not committed.
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, old Plaza2/QUIK target architecture, or vendored qsh-rs code.
