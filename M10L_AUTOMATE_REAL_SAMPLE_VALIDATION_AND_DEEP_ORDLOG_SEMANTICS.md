# M10L_AUTOMATE_REAL_SAMPLE_VALIDATION_AND_DEEP_ORDLOG_SEMANTICS

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

Owner only needs to start MiMo:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10L_AUTOMATE_REAL_SAMPLE_VALIDATION_AND_DEEP_ORDLOG_SEMANTICS.md"
```

MiMo must run build, tests, and real-sample validation by itself. The owner should not have to manually run the standard verification commands unless MiMo reports a failure or asks for help.

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Automate real QSH sample validation"
```

Owner final check after MiMo:

```powershell
git status --short
git log --oneline -1
```

## MiMo mandatory local verification

MiMo must run these commands itself before considering the task complete:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

MiMo must paste the build/test result into the report.

If build/test fails, MiMo must fix the issue or clearly report the failure and stop. Do not claim success without green tests.

## Mission

Automate local real-sample validation and continue the deeper OrdLog semantics investigation.

M10K showed that `tx-grouped` mode does not improve the real sample:

```text
per-record:
missing_order_id: 319
missing_on_fill: 148
missing_on_cancel: 170
missing_on_remove: 1
crossed_book_snapshots: 7890
non_positive_spread_snapshots: 7890

reduce-same-price from M10J:
missing_order_id: 171
missing_on_fill: 0
missing_on_cancel: 170
missing_on_remove: 1
crossed_book_snapshots: 7884
non_positive_spread_snapshots: 7884

tx-grouped from M10K:
missing_order_id: 319
missing_on_fill: 148
missing_on_cancel: 170
missing_on_remove: 1
crossed_book_snapshots: 7890
non_positive_spread_snapshots: 7890
orphan_fill_resolved_in_transaction: 0
orphan_cancel_resolved_in_transaction: 0
orphan_remove_resolved_in_transaction: 0
```

Conclusion:

```text
Simple orphan Fill handling helps one diagnostic dimension but does not fix crossed L2.
TxEnd grouping does not help this real sample.
The root cause is likely deeper OrdLog semantics, decoder/spec mismatch, initial book state, hidden/inherited fields, or system/non-system interpretation.
```

L2 remains not strategy-ready.

## Important workflow change

From this task onward, MiMo must run local real-sample validation automatically when the real QSH exists at:

```text
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

If the file exists:

```text
MiMo must run the validation script/commands and paste the compact comparison table into its report.
```

If the file does not exist:

```text
MiMo must write: local QSH not found, real-sample validation skipped.
```

Do not commit the raw QSH file or generated CSV/JSON reports.

## Required work

### 1. Add local validation script

Create:

```text
tools/run_qsh_real_sample_checks.ps1
```

The script must:

```text
1. Check whether the local QSH file exists.
2. Build qsh_ingest or clearly fail with instructions.
3. Run the standard CTest suite.
4. Run a compact set of real-sample validation modes when QSH exists.
5. Print a small comparison table to the console.
6. Store generated outputs under data/reports/qsh/RTS-3.21/2021-01-05/ only.
7. Never add generated files to Git.
```

Default QSH path:

```text
.\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh
```

Default report dir:

```text
.\data\reports\qsh\RTS-3.21\2021-01-05
```

### 2. Validation modes

The script must run at least:

```text
per-record strict baseline
per-record reduce-same-price
per-record snapshot-records-mode load
book-update-mode tx-grouped
```

All with:

```text
--depth 5
--max-records 100000
--max-snapshots 10000
--snapshot-mode txend
```

### 3. Compact table

The script output must include a table with columns:

```text
mode
missing_order_id
missing_on_fill
missing_on_cancel
missing_on_remove
orphan_fill_events
orphan_fill_level_reductions
crossed_book_snapshots
non_positive_spread_snapshots
first_missing_order_record_index
first_crossed_book_record_index
l2_strategy_ready
```

If possible, parse from CLI output. If parsing is fragile, add a safe machine-readable summary option to `qsh_ingest`, for example:

```text
--summary-out <file.json>
```

or:

```text
--summary-out <file.csv>
```

Generated summary files must stay under ignored report directories.

### 4. Fix tx-grouped diagnostic propagation

M10K showed this suspicious output:

```text
book_update_mode: tx-grouped
missing_order_id: 319
first_missing_order_record_index: 0
```

This is likely a diagnostic propagation bug. Fix it so:

```text
if missing_order_id > 0 then first_missing_order_record_index must be a real record index, not 0.
```

Add a test for this.

### 5. Deep OrdLog semantics next probe

After automation exists, continue semantic investigation by adding diagnostics for the remaining likely root causes:

```text
system vs non-system records around missing order ids
hidden/inherited fields around cancel/remove
first 20 missing_on_cancel order ids and prior occurrence checks
whether missing_on_cancel orders appear as Snapshot records before NewSession/book init
whether qsh-rs ignores some system records or handles them as non-book actions
```

Add a command or script section that can produce:

```text
missing_cancel_probe.csv
```

Columns should include:

```text
record_index,ts,order_id,event_type,side,price,amount,amount_rest,prior_add_found,prior_snapshot_found,prior_any_occurrence_found,raw_flags,raw_repl_act,is_system,is_non_system
```

### 6. Report requirements

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10L automated real-sample validation and deep OrdLog semantics
```

The report must include:

```text
build/test result from MiMo's own run
whether local QSH was found
compact validation table from MiMo's own run
whether tx-grouped first_missing_order_record_index bug was fixed
what missing_on_cancel probe shows
next recommended task
```

## Tests

Add tests for:

```text
summary output contains required fields
tx-grouped first_missing_order_record_index is propagated
script path constants are documented or configurable
missing cancel probe reports prior occurrence state
```

Do not add real QSH data or generated CSV/JSON reports.

## Expected output

The task is done when the report states one of:

```text
A. automation works and real-sample table is included in report;
B. local QSH was not found but script and tests are ready;
C. tx-grouped diagnostic propagation bug was fixed;
D. missing_on_cancel probe identifies a new likely root cause;
E. root cause remains unknown, but future manual command-running is no longer needed.
```

If crossed snapshots remain, keep:

```text
L2 strategy-ready: NO
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, or vendored qsh-rs code.
