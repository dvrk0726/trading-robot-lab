# M10P_SNAPSHOT_RECORD_HANDLING_AND_INITIAL_BOOK_AUDIT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

Owner starts MiMo:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10P_SNAPSHOT_RECORD_HANDLING_AND_INITIAL_BOOK_AUDIT.md"
```

MiMo must run build, tests, and real-sample validation by itself.

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Audit snapshot records and initial book state"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

M10O traced the first crossed book to real active orders in the reconstructed book. The next task is to audit snapshot record handling and initial book state before adding any skip/ignore behavior.

Focus only on:

```text
QSH / OrdLog decoding
Snapshot records
NewSession / initial book state
L3 -> L2 reconstruction correctness
Data Quality evidence
future FAST/FIX/TWIME-compatible data layer
```

Do not introduce old Plaza2/QUIK runtime logic as a development direction.

## Current known state from M10O

M10O resolved the index ambiguity:

```text
first_crossing_event_record_index: 2136
first_crossing_snapshot_record_index: 2210
first_crossing_snapshot_index: 2111
```

Interpretation:

```text
2136 = the OrdLog record whose application first makes best_bid >= best_ask.
2210 = the OrdLog record where the first invalid L2 snapshot is emitted in txend mode.
2111 = exported snapshot number/index.
```

M10O identified the first crossing active orders:

```text
best_bid: 14100
best_ask: 14062

bid_order: 1925033994466246392
side: BUY
price: 14100
qty: 111
valid ADD path: NO
likely source: snapshot initialization

ask_order: 1925033994522131746
side: SELL
price: 14062
qty: 30
valid ADD path: YES
ADD at record 1957
```

M10O also reported:

```text
records_after_NewSession_at_first_crossing: 2135
snapshot_records_processed_before_crossing: 1964
crossed snapshots remain: 7890 strict / 7884 reduce-same-price
L2 strategy-ready: NO
```

Current interpretation:

```text
1. orphan cancel/remove is not the main blocker.
2. first crossed book is caused by a real active bid order at 14100.
3. that bid order has no normal ADD lifecycle and likely enters through snapshot initialization.
4. therefore the next root cause is snapshot record handling / initial book state / snapshot decoding semantics.
```

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

### 1. Audit snapshot record decoding

Add a focused diagnostic command or extend existing audit output to dump snapshot records before the first crossing.

Recommended command:

```text
snapshot-audit <OrdLog.qsh> --out <file.csv> --max-records N
```

If adding a new command is too heavy, extend existing dump/audit commands with a snapshot filter.

Output file under ignored report path only:

```text
data/reports/qsh/RTS-3.21/2021-01-05/snapshot_audit_before_crossing.csv
```

Recommended columns:

```text
record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,order_id_before_delta,order_id_after_delta,order_id_delta,price_before_delta,price_after_delta,price_delta,amount_before,amount_after,amount_rest_before,amount_rest_after,order_id_path,price_path,amount_path
```

The report must answer:

```text
How many snapshot records are processed before first crossing?
How many BUY vs SELL snapshot records?
What is the min/max price by side in snapshot records?
Does bid_order=1925033994466246392 appear in snapshot records?
If yes, at which record_index, with what side, price, qty, flags, replAct?
Does the snapshot itself already contain bid >= ask?
```

### 2. Build initial book state summary

Add a command/report section to summarize book state after snapshot initialization but before normal event processing.

Minimum output:

```text
snapshot_records_loaded
snapshot_buy_orders_loaded
snapshot_sell_orders_loaded
snapshot_best_bid
snapshot_best_ask
snapshot_spread
snapshot_crossed_at_initial_load
snapshot_crossed_order_count
snapshot_top_bid_orders
snapshot_top_ask_orders
```

If there is no explicit end-of-snapshot marker, define and document the chosen boundary:

```text
first non-snapshot record after NewSession
first TxEnd after snapshot records
first event record after snapshot streak
```

Do not guess silently.

### 3. Trace the snapshot source of bid_order

Specifically trace:

```text
order_id: 1925033994466246392
side: BUY
price: 14100
qty: 111
```

The report must state one of:

```text
A. order enters via a snapshot record decoded as BUY 14100 qty=111;
B. order enters through an incorrectly decoded snapshot record;
C. order enters due to stale inherited fields from previous record;
D. order enters due to an unknown decoder path;
E. order does not actually enter snapshot records, earlier conclusion was wrong.
```

Include raw fields/evidence.

### 4. Validate snapshot-records-mode behavior

Compare these modes explicitly:

```text
--snapshot-records-mode ignore
--snapshot-records-mode load
--snapshot-records-mode marker
```

Use:

```text
--depth 5
--max-records 100000
--max-snapshots 10000
--snapshot-mode txend
```

The main table must show whether each mode changes:

```text
missing_order_id
crossed_book_snapshots
first_crossing_event_record_index
first_crossing_snapshot_record_index
l2_strategy_ready
```

If `marker` does not exist or has different semantics, document actual behavior and do not invent.

### 5. Do not hide crossed book yet

Do not add `skip-initial-crossed` or `ignore-crossed` mode in this task.

Reason:

```text
Crossed book may indicate decoder/spec mismatch or an auction/session state. We must prove which before filtering it away.
```

If evidence suggests crossed snapshot initialization is legitimate auction/session data, propose a future guarded task, but do not implement filtering in M10P.

### 6. qsh-rs / external semantic comparison if available

Do not vendor qsh-rs code.

If local qsh-rs or reference output is available, compare snapshot handling only:

```text
Does qsh-rs load snapshot records?
Does qsh-rs include order_id 1925033994466246392?
Does qsh-rs produce best bid 14100 before/at first crossing?
Does qsh-rs treat the same period as crossed/auction/non-strategy-ready?
```

If not available, report:

```text
qsh-rs snapshot comparison skipped: not available locally
```

### 7. Update validation script

Update:

```text
tools/run_qsh_real_sample_checks.ps1
```

Add optional switch:

```powershell
-RunSnapshotAudit
```

When enabled, it should create snapshot audit outputs under ignored report path and print a compact snapshot summary.

Keep default script output concise.

### 8. README update

Update:

```text
cpp/qsh_ingest/README.md
```

Document:

```text
-RunSnapshotAudit
snapshot-audit command or equivalent
snapshot-records-mode behavior
that L2 strategy-ready remains NO while crossed-book diagnostics are present
```

### 9. Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10P snapshot record handling and initial book audit
```

The section must include:

```text
build/test result
real-sample validation table
snapshot-records-mode comparison
snapshot audit summary before first crossing
initial book state summary
bid_order snapshot source conclusion A/B/C/D/E
qsh-rs snapshot comparison result or skipped reason
files changed
next recommended task
```

## Tests

Add tests if behavior or summary fields change.

Minimum:

```text
existing C++ tests remain green
snapshot audit command does not crash on empty/non-snapshot data
snapshot summary fields are deterministic
summary JSON keeps backward-compatible fields
README commands remain accurate
```

## Done criteria

Task is done when:

```text
1. build/test are green;
2. real-sample script was run with -RunSnapshotAudit, or local QSH absence is clearly reported;
3. snapshot audit identifies whether bid_order=1925033994466246392 enters via snapshot records;
4. initial book state after snapshot loading is summarized;
5. snapshot-records-mode comparison is documented;
6. 2136 vs 2210 remains clearly defined;
7. no skip/ignore crossed-book filter is added;
8. L2 strategy-ready remains NO unless crossed diagnostics are truly clean;
9. generated CSV/JSON reports are not committed.
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, old Plaza2/QUIK target architecture, skip/ignore crossed-book shortcuts, or vendored qsh-rs code.
