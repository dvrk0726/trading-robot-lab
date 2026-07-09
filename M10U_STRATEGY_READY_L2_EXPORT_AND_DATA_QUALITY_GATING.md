# M10U_STRATEGY_READY_L2_EXPORT_AND_DATA_QUALITY_GATING

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10U_STRATEGY_READY_L2_EXPORT_AND_DATA_QUALITY_GATING.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Add strategy-ready L2 export and data quality gating"
```

Owner final check:

```powershell
git status --short
git log --oneline -3
```

## Mission

M10S proved that `Counter=0x100` events are a primary cause of early crossed-book reconstruction.

M10T proved that after `--counter-mode ignore-book`, remaining crossed snapshots are caused by non-Counter aggressive trading / transitional book states, not by the same Counter bug.

Current status:

```text
counter-ignore-book crossed_book_snapshots = 907
l2_strategy_ready = false
remaining crossed classification = B / transitional normal trading states
```

The next job is not strategy research yet.

The next job is to make the L2 export explicit and safe for downstream research by adding strategy-readiness gating to exported snapshots and summary metadata.

## Core principle

Do not hide bad or transitional market states.

Do not delete crossed/locked/bad snapshots silently.

Do not mark the whole dataset strategy-ready while any exported snapshot has invalid strategy conditions.

Instead:

```text
export the snapshot
mark it clearly
explain why it is not strategy-ready
make the Data Quality summary count it
make Trading Lab able to display this status later
```

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunCounterFlagAudit -RunCrossedPersistenceAudit -RunCrossingWindowAudit -RunFirstCrossedProbe -RunRemainingCrossedAudit
```

Then MiMo must run at least one direct export with gating enabled:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 `
  --max-records 100000 `
  --max-snapshots 10000 `
  --snapshot-mode txend `
  --counter-mode ignore-book `
  --summary-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_gated.summary.json `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_gated.csv
```

If local QSH is missing, write exactly:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Add explicit row-level strategy readiness to L2 CSV

Extend L2 CSV output with row-level strategy-readiness fields.

Minimum columns to add:

```text
is_crossed
is_locked
strategy_ready
strategy_reject_reason
```

Recommended additional columns if easy and useful:

```text
spread
spread_ticks
best_bid
best_ask
snapshot_mode
counter_mode
book_update_mode
snapshot_index
record_index
tx_index
```

Rules:

```text
is_crossed = true when best_bid > best_ask
is_locked  = true when best_bid == best_ask and both sides exist
strategy_ready = false when is_crossed == true
strategy_ready = false when is_locked == true unless there is a clear reason to allow locked books
strategy_ready = false when either best side is missing and depth requires both sides
strategy_ready = true only when the row is safe for strategy-level research
```

`strategy_reject_reason` should be stable and machine-readable.

Use values like:

```text
ok
crossed_book
locked_book
missing_best_bid
missing_best_ask
empty_book
invalid_price
invalid_depth
```

Do not use long free-text prose in the CSV reason column.

### 2. Add Data Quality summary fields

Extend summary JSON with strategy-readiness counts.

Minimum fields:

```text
snapshots_total
snapshots_strategy_ready
snapshots_not_strategy_ready
snapshots_crossed
snapshots_locked
snapshots_missing_best_bid
snapshots_missing_best_ask
snapshots_empty_book
snapshots_invalid_price
snapshots_invalid_depth
l2_strategy_ready
strategy_ready_ratio
strategy_reject_reasons
```

`l2_strategy_ready` rule:

```text
true only if snapshots_not_strategy_ready == 0 and existing critical diagnostics are clean
false otherwise
```

If the previous summary already has similar fields, reuse them instead of duplicating names.

### 3. Keep crossed snapshots visible by default

Do not add a default filter that removes bad snapshots.

Allowed optional export modes:

```text
--strategy-ready-only
--include-non-strategy-ready
```

If such modes are added, the default must be conservative and explicit.

Preferred default for now:

```text
export all snapshots and mark strategy_ready per row
```

If `--strategy-ready-only` is implemented, summary JSON must still report:

```text
original snapshots total
filtered snapshots exported
filtered snapshots dropped
reason counts for dropped rows
```

### 4. Decide counter-mode default carefully

Do not silently change `--counter-mode` default in this task.

Current default may remain:

```text
include
```

Experimental mode remains:

```text
--counter-mode ignore-book
```

If MiMo believes default should change to `ignore-book`, do not change it yet. Instead document a recommendation and require owner approval.

### 5. Add deterministic tests

Add or update tests for L2 gating.

Minimum synthetic cases:

```text
normal_book -> strategy_ready=true, reason=ok
crossed_book -> strategy_ready=false, reason=crossed_book
locked_book -> strategy_ready=false, reason=locked_book
missing_bid -> strategy_ready=false, reason=missing_best_bid
missing_ask -> strategy_ready=false, reason=missing_best_ask
empty_book -> strategy_ready=false, reason=empty_book
```

If the code structure makes direct L2 CSV tests hard, create small unit tests around a helper/class, for example:

```text
L2SnapshotQuality classify_l2_snapshot(...)
```

The gating logic should not be buried only in CSV string-writing code.

### 6. Update validation script

Update:

```text
tools/run_qsh_real_sample_checks.ps1
```

The script should print Data Quality / strategy readiness counts for each validation mode if summary JSON has them.

Expected table should include:

```text
mode
counter_mode
missing_order_id
crossed_book_snapshots
snapshots_strategy_ready
snapshots_not_strategy_ready
snapshots_crossed
snapshots_locked
strategy_ready_ratio
l2_strategy_ready
```

Add an optional switch if useful:

```powershell
-RunStrategyReadyExport
```

### 7. Update README

Update:

```text
cpp/qsh_ingest/README.md
```

Document:

```text
what strategy_ready means
why crossed/locked rows are exported but marked unsafe
how to run l3-to-l2 with summary output
how to inspect reason counts
that ignore-book is still explicit/experimental unless owner approves default switch
```

### 8. Update long report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10U Strategy-ready L2 export and Data Quality gating
```

Include:

```text
build/test result
real-sample validation result
new CSV columns
new summary JSON fields
strategy readiness rules
mode comparison table
first rows/reason examples if available
files changed
next recommended task
```

### 9. Optional Trading Lab integration

Only if fast and safe, update Trading Lab to read the new summary fields.

Possible files:

```text
apps/lab/backend/lab_dashboard.py
apps/lab/frontend/static/css/lab.css
```

Add display fields such as:

```text
strategy-ready snapshots
not strategy-ready snapshots
strategy-ready ratio
reject reason counts
```

Do not spend too much time on UI styling in this task. The core task is the data/export gate.

## Done criteria

```text
1. build/test green;
2. real-sample validation run or QSH absence explicitly reported;
3. L2 CSV has row-level strategy_ready and strategy_reject_reason;
4. crossed snapshots are exported and marked unsafe, not silently removed;
5. summary JSON counts strategy-ready and non-strategy-ready snapshots;
6. l2_strategy_ready remains false when any critical unsafe snapshots exist;
7. deterministic tests cover normal/crossed/locked/missing/empty cases;
8. validation script prints strategy readiness counts;
9. README and long report updated;
10. no raw QSH, generated CSV/JSON, .env, exe, dll, or secrets committed.
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
No strategy research until Data Quality gating is explicit and tested.
```

## Expected next step after M10U

If M10U is clean:

```text
M10V — Trading Lab Data Quality UI: load summary JSON and display strategy-ready status/reason counts
```

Then after the UI clearly shows Data Quality:

```text
M11 — Normalized microstructure research dataset / first RI-Synthetic lead-lag preparation
```
