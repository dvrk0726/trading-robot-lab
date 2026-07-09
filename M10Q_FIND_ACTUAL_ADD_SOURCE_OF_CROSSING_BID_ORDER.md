# M10Q_FIND_ACTUAL_ADD_SOURCE_OF_CROSSING_BID_ORDER

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for implementation

## Run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10Q_FIND_ACTUAL_ADD_SOURCE_OF_CROSSING_BID_ORDER.md"
```

If MiMo leaves local changes:

```powershell
.\tools\mimo_save.ps1 "Find actual add source of crossing bid order"
```

Owner final check:

```powershell
git status --short
git log --oneline -1
```

## Mission

M10P proved that snapshot initialization is not the crossed-book source. Snapshot state is clean:

```text
snapshot_best_bid=14055
snapshot_best_ask=14062
snapshot_spread=7
snapshot_crossed_at_initial_load=NO
```

Now find the exact real event that creates BUY 14100 in the active book.

Focus:

```text
records 1966..2136
order_id 1925033994466246392
price 14100
side BUY
first crossing event
raw decoder fields
OrderBook mutation path
```

Do not add crossed-book filtering in this task.

## Known state

```text
first_non_snapshot_after_new_session: 1966
first_crossing_event_record_index:    2136
first_crossing_snapshot_record_index: 2210
```

Crossing orders:

```text
bid_order: 1925033994466246392 BUY 14100 qty=111 snapshot_source=NO
ask_order: 1925033994522131746 SELL 14062 qty=30 valid_ADD=YES record=1957
```

M10P conclusion:

```text
1. bid_order is not in snapshot records.
2. crossing is not caused by snapshot initialization.
3. bid_order or BUY level 14100 must enter the book between records 1966 and 2136.
4. M10O lifecycle trace likely missed the earlier entry point or traced the wrong lifecycle boundary.
```

## Mandatory verification

MiMo must run:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
.\tools\run_qsh_real_sample_checks.ps1 -RunSnapshotAudit -RunFirstCrossedProbe
```

If local QSH is missing, write:

```text
local QSH not found, real-sample validation skipped
```

## Required work

### 1. Full audit for records 1966..2136

Add a focused command or reuse `dump-records --audit` through a wrapper. Preferred command:

```text
crossing-window-audit <OrdLog.qsh> --from 1966 --to 2136 --out <file.csv>
```

Generated output must stay under ignored report path:

```text
data/reports/qsh/RTS-3.21/2021-01-05/crossing_window_1966_2136_audit.csv
```

Include at least:

```text
record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,has_order_id_field,has_price_field,has_amount_field,order_id_delta,price_delta,best_bid_before,best_ask_before,best_bid_after,best_ask_after
```

### 2. Find actual entry point of BUY 14100

Identify the earliest record where one of these becomes true:

```text
order_id == 1925033994466246392
price == 14100 and side == BUY
best_bid changes to 14100
active book starts containing order_id 1925033994466246392
active book starts containing any BUY order at price 14100
```

Output:

```text
crossing_bid_order_entry_trace.csv
```

Report:

```text
entry_record_index
event_type
side
price
amount
amount_rest
order_id
flags_hex
raw_order_flags_hex
raw_event_bits
is_snapshot
is_system
is_txend
best_bid/best_ask before and after
qty at 14100 before and after
whether this event directly caused first_crossing_event_record_index=2136
```

### 3. Identify OrderBook mutation path

State exactly which path created BUY 14100:

```text
add_order
move_order
fill_order orphan reduce-same-price
cancel_order
remove_order
other/unknown
```

Expected conclusion:

```text
BUY 14100 is created by <path> at record <N> from <event_type>.
```

### 4. Resolve M10O vs M10P contradiction

M10O said the bid order had no valid ADD. M10P says it must enter between 1966 and 2136. Report one:

```text
A. lifecycle trace started too late and missed ADD;
B. no ADD exists; level enters through MOVE or other mutation;
C. order_id differs but price level 14100 enters from another order;
D. decoder maps another raw event to this order_id later;
E. lifecycle trace implementation is wrong and must be fixed.
```

Fix lifecycle tracing if needed so selected order IDs are traced from file start, not only after first crossed detection.

### 5. Explain strict vs reduce-same-price shift

Explain why:

```text
strict first_crossing_event_record_index = 2136
reduce-same-price first_crossing_event_record_index = 2242
```

Find which record(s) make reduce-same-price delay the crossing.

### 6. Update script and README

Update:

```text
tools/run_qsh_real_sample_checks.ps1
cpp/qsh_ingest/README.md
```

Add optional switch:

```powershell
-RunCrossingWindowAudit
```

Document command usage and generated outputs.

### 7. Update report

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10Q actual add source of crossing bid order
```

Include:

```text
build/test result
real-sample validation result
crossing-window audit summary
actual entry point of BUY 14100
actual mutation path
M10O vs M10P contradiction resolution
strict vs reduce-same-price comparison
files changed
next recommended task
```

## Tests

Add tests if adding commands or instrumentation. Existing tests must remain green.

## Done criteria

```text
1. build/test green;
2. real-sample validation run with crossing-window audit or QSH absence reported;
3. exact record that creates BUY 14100 identified, or instrumentation gap explicitly reported;
4. actual mutation path identified;
5. lifecycle contradiction resolved;
6. strict vs reduce-same-price index shift explained;
7. no crossed-book filtering added;
8. generated CSV/JSON reports not committed;
9. L2 strategy-ready remains NO unless crossed diagnostics are truly clean.
```
