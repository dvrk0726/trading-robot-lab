# M10H_SNAPSHOT_SEMANTICS_AND_ORDERLOG_SPEC_CHECK

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10H_SNAPSHOT_SEMANTICS_AND_ORDERLOG_SPEC_CHECK.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Check Snapshot semantics and OrdLog decoding"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Find why `missing_order_id` starts before the first crossed book and why L2 remains crossed.

M10G real-sample result:

```text
first_missing_order_record_index: 1651
first_crossed_book_record_index: 2210
missing_order_id starts BEFORE crossed book
missing_order_id: 319
crossed_book_snapshots: 7890
add_records_seen: 9814
add_records_applied: 9814
add_records_skipped: 0
snapshot_records_seen: 1964
new_session_records_seen: 1
```

Conclusion so far:

```text
Fill amount semantics are not the cause.
ADD records are not skipped by current filters.
TxEnd mode is required but not sufficient.
The next likely issue is Snapshot semantics, flags/repl_act decoding, or incomplete OrdLog interpretation.
```

L2 remains not strategy-ready.

## Read first

```text
M10G_MISSING_ORDER_ID_AND_SNAPSHOT_INIT.md
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/include/qsh/ordlog_reader.hpp
cpp/qsh_ingest/include/qsh/ordlog_flags.hpp
cpp/qsh_ingest/README.md
docs/qsh_data_source_notes.md
```

If useful, compare behavior with qsh-rs and QSH/OrdLog documentation. Do not copy code blindly.

## Required work

### 1. Raw OrdLog record dump

Add a diagnostic command or option that exports decoded records to CSV:

```text
record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,flags,flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system
```

It should support a safe limited dump:

```text
--dump-records-out <file.csv>
--dump-records-from N
--dump-records-to N
```

Main target range:

```text
records 1500..2250
```

This range covers first missing order at 1651 and first crossed book at 2210.

### 2. First missing order backward check

For the first missing order id, check if the same order id appears earlier as:

```text
ADD
Snapshot record
different side
different price
non-system/system record
non-zero repl_act
unknown event type
```

Add summary output:

```text
first_missing_order_id
first_missing_order_event_type
first_missing_order_side
first_missing_order_price
first_prior_occurrence_index
first_prior_occurrence_type
prior_add_found: YES/NO
prior_snapshot_found: YES/NO
```

### 3. Snapshot semantics check

Determine whether Snapshot records in this OrdLog stream are:

```text
actual active order rows that must initialize the book
markers only
transaction boundary-like records
unknown
```

Do not change default reconstruction unless evidence is clear. Add an experimental flag if needed:

```text
--snapshot-records-mode ignore|load|marker
```

Compare counters only if simple.

### 4. QSH / OrdLog spec notes

Update docs or report with a short note explaining the current interpretation of:

```text
Snapshot
NewSession
TxEnd
repl_act
system vs non-system
side flags
```

Mark uncertain items clearly.

### 5. Tests

Add synthetic tests for:

```text
record dump range works
first missing order backward check finds prior ADD
first missing order backward check reports no prior ADD
snapshot mode does not silently change default behavior
```

Do not add real QSH or generated CSV files.

## Expected output

The task is done when the report states one of:

```text
A. first missing order has no prior ADD/Snapshot -> decoder may miss records;
B. prior ADD/Snapshot exists but was not loaded -> book init/lifecycle bug;
C. Snapshot semantics were wrong and an experimental mode proves it;
D. root cause remains unknown, but decoded record dump makes next step clear.
```

If crossed snapshots remain, keep:

```text
L2 strategy-ready: NO
```

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10H Snapshot semantics and OrdLog spec check
```

Include build/test result, dump command, first missing order analysis, Snapshot/NewSession conclusion, and next recommended task.

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, or credentials.
