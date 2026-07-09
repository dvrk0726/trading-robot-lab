# M10J_COMPARE_QSH_RS_L3TOL2_AND_FILL_ORDER_MODEL

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10J_COMPARE_QSH_RS_L3TOL2_AND_FILL_ORDER_MODEL.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Compare qsh-rs L3 to L2 fill order model"
```

Verify:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Compare our OrdLog L3->L2 reconstruction with `qsh-rs`, especially the Fill handling model.

Reference repository:

```text
https://github.com/2dav/qsh-rs
```

Do not copy code blindly. Use it to understand semantics and document differences.

## Current evidence

M10I audit around first missing order showed:

```text
1644: ADD  BUY price=14055 amount=200 amount_rest=200 order_id=...35609
1650: FILL BUY price=14055 amount=6   amount_rest=194 order_id=...35609
1651: FILL BUY price=14055 amount=29  amount_rest=165 order_id=...34789  <-- first missing_order_id
1652: FILL BUY price=14055 amount=1   amount_rest=164 order_id=...33969
1653: FILL BUY price=14055 amount=40  amount_rest=124 order_id=...33149
1654: FILL BUY price=14055 amount=12  amount_rest=112 order_id=...32329
1655: FILL BUY price=14055 amount=26  amount_rest=86  order_id=...31509
1656: FILL BUY price=14055 amount=1   amount_rest=85  order_id=...30689
```

Audit columns showed these FILL rows use:

```text
raw_order_flags_hex=0xd8
raw_side_bits=0x10
raw_event_bits=0x8
has_order_id_field=1
has_price_field=1
has_amount_field=1
order_id_delta=-820
is_add_order_id_path=leb128
```

Interpretation:

```text
The amount_rest sequence looks continuous: 194 -> 165 -> 164 -> 124 -> 112 -> 86 -> 85.
But order_id changes by -820 on every FILL, creating order IDs that were never added.
```

This suggests that orphan FILL records may be normal OrdLog rows or require a different book update model.

## Required work

### 1. Inspect qsh-rs

Clone/read `https://github.com/2dav/qsh-rs` outside this repo or as a temporary local reference. Do not vendor it into this repository.

Find and summarize:

```text
OrderLog parser
L3 book implementation
L3toL2 example/tool
Fill handling
Cancel/Remove handling
orphan Fill behavior
transaction / TxEnd grouping
snapshot handling
```

### 2. Compare models

Write a comparison table in the report:

```text
Topic | our implementation | qsh-rs implementation | match/mismatch/unknown
```

Required topics:

```text
order_id delta decoding
price delta decoding
amount / amount_rest semantics
Fill update target
orphan Fill handling
Cancel handling
Remove handling
Snapshot records
TxEnd grouping
best bid/ask L2 export
```

### 3. Decide how orphan Fill should be handled

Specifically answer:

```text
When a FILL references an order_id that is not in active order map, should we:
A. ignore it;
B. reduce same-side price level;
C. reduce opposite-side price level;
D. use amount_rest to update the most recent/known resting order in the same transaction;
E. group fills by transaction;
F. treat it as decoder bug;
G. something else?
```

Do not change default behavior blindly.

### 4. Add experimental mode if justified

If qsh-rs reveals a different Fill model, add a diagnostic-only option, for example:

```text
--orphan-fill-mode ignore|reduce-same-price|transaction-rest|strict
```

Default should remain current strict behavior unless evidence is strong.

The mode must print counters:

```text
orphan_fill_events
orphan_fill_ignored
orphan_fill_level_reductions
orphan_fill_transaction_rest_updates
crossed_book_snapshots
non_positive_spread_snapshots
missing_order_id
amount_mismatch
negative_level_volume
```

### 5. Synthetic tests

Add focused tests for:

```text
orphan FILL strict mode keeps missing_order_id
orphan FILL ignored mode does not mutate book
orphan FILL same-price reduction mode if implemented
transaction-rest mode if implemented
qsh-rs-inspired behavior is deterministic
```

Do not add real QSH data or generated CSV files.

## Owner real-sample command after implementation

If a new mode is added, report the exact command for owner to run on:

```text
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

Keep command limited:

```text
--max-records 100000 --max-snapshots 10000 --snapshot-mode txend
```

## Expected output

The report must state one of:

```text
A. qsh-rs confirms our Fill model; root cause is elsewhere.
B. qsh-rs uses a different Fill/orphan Fill model and we added diagnostic mode.
C. qsh-rs comparison is inconclusive, but exact differences are documented.
D. qsh-rs cannot be used, and the reason is documented.
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
M10J qsh-rs L3toL2 fill order model comparison
```

Include changed files, build/test result, qsh-rs findings, model comparison table, implemented experimental modes, owner commands, and next recommended task.

## Safety

Do not add broker connection, live trading, real order sending, raw QSH files, generated reports, binaries, env files, keys, credentials, or vendored qsh-rs code.
