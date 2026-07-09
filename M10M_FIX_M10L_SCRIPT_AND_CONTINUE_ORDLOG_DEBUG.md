# M10M_FIX_M10L_SCRIPT_AND_CONTINUE_ORDLOG_DEBUG

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

Owner only starts MiMo:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10M_FIX_M10L_SCRIPT_AND_CONTINUE_ORDLOG_DEBUG.md"
```

MiMo must run build, tests, and local real-sample validation by itself.

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Fix M10L validation script and continue OrdLog debug"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

Fix the incomplete part of M10L and continue debugging the current QSH/OrdLog reconstruction.

Do not add old Plaza2/QUIK logic as a development direction. Old code may be used only as historical context. Current data-layer work must stay focused on:

```text
QSH / OrdLog decoding
L3 -> L2 reconstruction
Data Quality
future FAST/FIX/TWIME-compatible architecture
```

## Why this task exists

M10L commit `d433004 Automate real QSH sample validation` partially completed the task:

Done:

```text
- MiMo ran build/test locally.
- MiMo found the local QSH sample.
- MiMo added real-sample comparison results to the report.
- MiMo added machine-readable `--summary-out` support.
- MiMo added `missing-cancel-probe` command.
- MiMo fixed tx-grouped `first_missing_order_record_index: 0` diagnostic propagation.
```

Not done:

```text
- tools/run_qsh_real_sample_checks.ps1 was required by M10L but was not created/committed.
```

This task must close that gap.

## Mandatory MiMo verification

MiMo must run before reporting success:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

MiMo must also run the new validation script if the local QSH exists.

## Required work

### 1. Create the missing validation script

Create and commit:

```text
tools/run_qsh_real_sample_checks.ps1
```

The script must be owner-friendly: one command, clear output.

Default run:

```powershell
.\tools\run_qsh_real_sample_checks.ps1
```

Optional parameters:

```powershell
.\tools\run_qsh_real_sample_checks.ps1 -QshPath "..." -ReportDir "..." -SkipBuild
```

Default QSH path:

```text
.\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh
```

Default report dir:

```text
.\data\reports\qsh\RTS-3.21\2021-01-05
```

The script must:

```text
1. Check that it is run from repo root or safely detect repo root.
2. Check whether the QSH file exists.
3. Build qsh_ingest unless -SkipBuild is passed.
4. Run ctest unless -SkipBuild is passed.
5. If QSH does not exist, print local QSH not found and exit without failure.
6. If QSH exists, run real-sample checks.
7. Write generated CSV/JSON only under data/reports/qsh/RTS-3.21/2021-01-05/.
8. Print a compact comparison table.
9. Print final L2 strategy-ready status.
10. Never add generated files to Git.
```

### 2. Modes the script must run

Run these four modes using `--summary-out`:

```text
per-record strict
per-record reduce-same-price
snapshot-records-mode load
tx-grouped
```

All must use:

```text
--depth 5
--max-records 100000
--max-snapshots 10000
--snapshot-mode txend
```

Recommended output files:

```text
l2_per_record_strict.csv
l2_per_record_strict.summary.json
l2_reduce_same_price.csv
l2_reduce_same_price.summary.json
l2_snapshot_load.csv
l2_snapshot_load.summary.json
l2_tx_grouped.csv
l2_tx_grouped.summary.json
```

### 3. Compact table requirements

The script must print a table with at least:

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

The table must be easy for MiMo to copy into the report.

### 4. Missing cancel probe

Use existing `missing-cancel-probe` support and make the script optionally run it:

```powershell
.\tools\run_qsh_real_sample_checks.ps1 -RunMissingCancelProbe
```

Expected output:

```text
missing_cancel_probe.csv
```

If this is not fully implemented yet, document the limitation clearly and do not pretend it is complete.

### 5. Continue current OrdLog debugging, not old Plaza2 logic

After the script exists, continue the current investigation using the script output.

The report must focus on:

```text
missing_on_cancel=170
missing_on_remove=1
system vs non-system records
snapshot/new-session initialization
hidden/inherited fields
qsh-rs comparison for cancel/remove lifecycle
why crossed_book remains 7890 or 7884
```

Do not introduce tasks or docs that promote old Plaza2/QUIK architecture as the new target.

### 6. Update README or qsh_ingest README

Add a short section to:

```text
cpp/qsh_ingest/README.md
```

Include:

```powershell
.\tools\run_qsh_real_sample_checks.ps1
```

and explain:

```text
- raw QSH must stay under data/raw/qsh/...
- generated reports stay under data/reports/qsh/...
- both are ignored by Git
- L2 strategy-ready must remain NO until crossed-book diagnostics are clean
```

### 7. Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10M validation script fix and OrdLog debug continuation
```

The section must include:

```text
build/test result from MiMo's own run
whether tools/run_qsh_real_sample_checks.ps1 now exists
whether local QSH was found
compact table produced by the script
missing-cancel-probe status
files changed
next recommended task
```

## Tests

Add tests only if useful and not artificial. Minimum requirement:

```text
existing C++ tests must remain green
summary-out fields must remain stable
```

If adding PowerShell script tests is too heavy, document manual validation in the report.

## Expected result

The task is done only when:

```text
1. tools/run_qsh_real_sample_checks.ps1 exists in Git.
2. MiMo ran build/test and pasted result into report.
3. MiMo ran the script if local QSH exists.
4. The report includes the script's compact table.
5. Generated CSV/JSON reports are not committed.
6. L2 strategy-ready remains NO unless crossed-book diagnostics are clean.
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, or vendored qsh-rs code.
