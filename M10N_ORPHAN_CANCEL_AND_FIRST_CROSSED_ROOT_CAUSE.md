# M10N_ORPHAN_CANCEL_AND_FIRST_CROSSED_ROOT_CAUSE

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

Owner only starts MiMo:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10N_ORPHAN_CANCEL_AND_FIRST_CROSSED_ROOT_CAUSE.md"
```

MiMo must run build, tests, and local real-sample validation by itself.

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Investigate orphan cancel and first crossed book root cause"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

Continue the current QSH/OrdLog investigation. Do not introduce old Plaza2/QUIK logic as a development direction.

Focus only on:

```text
QSH / OrdLog decoding
orphan cancel/remove semantics
first crossed-book root cause
L3 -> L2 reconstruction correctness
Data Quality evidence
future FAST/FIX/TWIME-compatible data layer
```

M10M fixed the automation gap and created:

```text
tools/run_qsh_real_sample_checks.ps1
```

Use it for all real-sample validation.

## Current known real-sample state

From M10M:

```text
per-record strict:              missing=319, crossed=7890, ready=NO
reduce-same-price:              missing=171, crossed=7884, ready=NO
snapshot-records-mode load:     missing=319, crossed=7890, ready=NO
tx-grouped:                     missing=319, crossed=7890, ready=NO
```

Missing-cancel probe over 100 events:

```text
With prior ADD:      0
With prior Snapshot: 0
With any occurrence: 0
```

Interpretation so far:

```text
1. orphan FILL can be partly handled by reduce-same-price;
2. orphan CANCEL remains unresolved;
3. tx-grouped does not help;
4. snapshot load does not help;
5. crossed-book remains the true blocker;
6. L2 is not strategy-ready.
```

## Mandatory MiMo verification

Before reporting success, MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1
```

If local QSH is missing, MiMo must write:

```text
local QSH not found, real-sample validation skipped
```

Do not claim real-sample success without running the script on the local QSH.

## Required work

### 1. Inspect orphan CANCEL / REMOVE raw semantics

Extend existing diagnostics or add a focused command for orphan cancel/remove audit.

The output should answer:

```text
Are missing_on_cancel records system or non-system?
What are raw_flags and replAct values?
Do missing_on_cancel records carry price/amount/amount_rest fields?
Do missing_on_cancel orders ever appear as ADD, Snapshot, Fill, Cancel, Remove, or TxEnd anywhere else?
Do missing_on_remove records have the same pattern?
Are these records clustered around NewSession / snapshot initialization?
```

Add an output file under ignored report path only, for example:

```text
data/reports/qsh/RTS-3.21/2021-01-05/orphan_cancel_audit.csv
```

Recommended columns:

```text
record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,flags_hex,repl_act,is_system,is_non_system,is_snapshot,is_new_session,is_txend,has_order_id_field,has_price_field,has_amount_field,prior_add_found,prior_snapshot_found,prior_any_occurrence_found,next_any_occurrence_found,near_new_session
```

Generated files must not be committed.

### 2. Decide whether orphan CANCEL / REMOVE is book-impacting

Do not blindly ignore orphan cancel/remove.

Add evidence-based conclusion:

```text
A. orphan cancel/remove is likely benign and can be ignored for active-book mutation; or
B. orphan cancel/remove indicates decoder/spec mismatch and must stay an error; or
C. inconclusive, keep strict behavior.
```

The report must state which of A/B/C is supported by data.

### 3. Optional experimental mode: orphan cancel

If evidence supports that orphan cancel/remove is benign, add experimental mode:

```text
--orphan-cancel-mode strict|ignore
```

Rules:

```text
strict: default, current behavior, count missing_order_id for cancel/remove of unknown order.
ignore: skip cancel/remove of unknown order without mutating book and count separately as orphan_cancel_ignored/orphan_remove_ignored.
```

If evidence does not support this, do not add the mode. Instead document why.

If added, update summary JSON with:

```text
orphan_cancel_ignored
orphan_remove_ignored
orphan_cancel_mode
```

Add tests.

### 4. Trace first crossed-book root cause separately

Do not assume missing_on_cancel causes crossed-book, because unknown cancel/remove may not mutate active book.

Add or improve diagnostics to identify the exact orders/levels that produce the first crossed state:

```text
first_crossed_book_record_index: 2210 strict / 2242 reduce-same-price
best_bid
best_ask
best_bid_order_ids
best_ask_order_ids
last 40 events before first crossed
lifecycle of best bid / best ask orders
whether the best bid / best ask orders had valid ADD path
whether fill/cancel/remove/move affected those order ids
```

Expected output under ignored report path only:

```text
first_crossed_root_cause.csv
first_crossed_best_orders.csv
first_crossed_lifecycle.csv
```

### 5. Use the automated script

Update `tools/run_qsh_real_sample_checks.ps1` if needed so it can optionally run the new probes:

```powershell
.\tools\run_qsh_real_sample_checks.ps1 -RunMissingCancelProbe
```

or add clearly named switches if needed:

```powershell
-RunOrphanCancelAudit
-RunFirstCrossedProbe
```

Keep default output concise.

### 6. Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10N orphan cancel and first crossed book root cause
```

The section must include:

```text
build/test result from MiMo's own run
run_qsh_real_sample_checks.ps1 result table
orphan cancel/remove audit summary
whether orphan cancel/remove is book-impacting: A/B/C
first crossed root cause summary
files changed
next recommended task
```

### 7. README update

If new CLI flags or script switches are added, update:

```text
cpp/qsh_ingest/README.md
```

Do not add old Plaza2/QUIK architecture guidance.

## Tests

Add tests if implementation changes behavior.

Minimum:

```text
existing C++ tests remain green
new flags are covered by tests if added
summary JSON fields remain stable
orphan cancel/remove behavior is deterministic
first crossed diagnostics do not crash on empty or non-crossed data
```

## Done criteria

Task is done when:

```text
1. build/test are green;
2. automated real-sample script was run or local QSH absence was clearly reported;
3. orphan cancel/remove semantics have an evidence-based conclusion;
4. first crossed-book root cause is traced separately from missing_order_id;
5. generated CSV/JSON reports are not committed;
6. L2 strategy-ready remains NO unless crossed diagnostics are clean.
```

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, old Plaza2/QUIK target architecture, or vendored qsh-rs code.
