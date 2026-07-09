# M10I_ORDLOG_FLAGS_REPLACT_AND_DECODER_AUDIT

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10I_ORDLOG_FLAGS_REPLACT_AND_DECODER_AUDIT.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Audit OrdLog flags repl_act and decoder semantics"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Audit the OrdLog decoder itself.

M10H showed that the first missing order appears before the first crossed book, but the current decoder cannot find a prior ADD or Snapshot for that order.

Real-sample result:

```text
first missing order:
order_id:     1925033994522434789
event_type:   FILL
side:         BUY
price:        14055
amount:       29
record_index: 1651

prior_add_found:      NO
prior_snapshot_found: NO

snapshot_records_mode=load:
missing_order_id: 319
crossed_book_snapshots: 7890
snapshot_records_seen: 1964
snapshot_records_loaded: 1924
add_records_seen: 9814
add_records_applied: 9814
add_records_skipped: 0
```

Conclusion so far:

```text
Fill amount semantics are not the cause.
TxEnd is required but not sufficient.
Simple Snapshot loading does not fix the issue.
Ordinary ADD records are not skipped.
The remaining likely problem is deeper OrdLog decoding: flags, repl_act, delta/inherited fields, or order-id lifetime semantics.
```

L2 remains not strategy-ready.

## Read first

```text
M10H_SNAPSHOT_SEMANTICS_AND_ORDERLOG_SPEC_CHECK.md
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/include/qsh/ordlog_reader.hpp
cpp/qsh_ingest/include/qsh/ordlog_flags.hpp
cpp/qsh_ingest/src/main.cpp
docs/qsh_data_source_notes.md
```

Also inspect qsh-rs behavior/spec notes if useful. Do not copy blindly; document differences.

## Required work

### 1. Decoder audit dump

Improve `dump-records` so it can include decoder-state details, not only interpreted fields.

Add columns where available:

```text
record_index
raw_offset_or_frame_index
raw_flags_value
raw_repl_act
raw_side_bits
raw_event_bits
field_presence_bits_if_known
order_id_before_delta
order_id_after_delta
price_before_delta
price_after_delta
amount_before_delta
amount_after_delta
side_before_inherit
side_after_inherit
event_type_before_mapping
event_type_after_mapping
```

If some values are not available yet, leave blank and document limitation.

### 2. Audit record range around first missing order

Add a recommended command to dump at least:

```text
records 1600..1670
records 1500..2250
```

The report must explain what happens around record 1651.

### 3. Verify order_id decoding

Specifically check whether order_id is:

```text
absolute in every record
carried forward from previous record
delta-coded
conditional based on flags
encoded differently for Snapshot/FILL/CANCEL
```

The report must state which interpretation current code uses.

### 4. Verify flags/repl_act mapping

For at least ADD, FILL, CANCEL, REMOVE, Snapshot, NewSession, TxEnd, document:

```text
which flags/repl_act values map to the event
which flags indicate side
which flags indicate system/non-system
which flags mean field is present vs inherited
```

Mark uncertain mappings as uncertain.

### 5. Compare with qsh-rs/spec

Look at qsh-rs or available QSH docs and write a short comparison:

```text
our mapping
qsh-rs/spec mapping
match/mismatch/unknown
```

Focus on OrdLog, not Quotes/Deals.

### 6. Add narrow tests

Add synthetic/unit tests for decoder behavior that is known from code/spec:

```text
flag mapping for event type
side mapping
repl_act mapping
order_id carry-forward or absolute behavior if implemented
unknown flags do not silently become ADD/FILL
```

Do not add real QSH.

## Expected output

The task is done when the report states one of:

```text
A. order_id/price/side/amount was decoded with wrong delta/inheritance semantics;
B. flags/repl_act mapping is incomplete or wrong;
C. first missing order is legitimate under OrdLog semantics and needs different book model;
D. root cause remains unknown, but raw decoder-state dump makes the next check clear.
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
M10I OrdLog flags repl_act and decoder audit
```

Include build/test result, changed files, decoder audit findings, qsh-rs/spec comparison, and next recommended task.

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, or credentials.
