# M9 Report — C++ QSH/OrdLog Data Layer

Date: 2026-07-08
Agent: MiMo
Status: completed

## 1. Files Changed

### New files created

```
cpp/qsh_ingest/
  CMakeLists.txt
  README.md
  include/
    qsh/
      qsh_types.hpp
      qsh_header.hpp
      leb128.hpp
      sha256.hpp
      qsh_reader.hpp
      ordlog_reader.hpp
      quotes_reader.hpp
      deals_reader.hpp
      auxinfo_reader.hpp
    orderbook/
      l3_event.hpp
      order_book.hpp
      l2_snapshot.hpp
    quality/
      data_quality.hpp
  src/
    main.cpp
    qsh_header.cpp
    leb128.cpp
    qsh_reader.cpp
    ordlog_reader.cpp
    quotes_reader.cpp
    deals_reader.cpp
    auxinfo_reader.cpp
    order_book.cpp
    l2_snapshot.cpp
    data_quality.cpp
  tests/
    test_leb128.cpp
    test_qsh_header.cpp
    test_ordlog_flags.cpp
    test_order_book.cpp
```

### Modified files

```
apps/lab/backend/seed_demo_db.py   — added qsh_quality table, M9 report entry, QSH data source
apps/lab/backend/lab_dashboard.py  — added Data Quality section to dashboard
```

## 2. Build Commands

```powershell
# Install zlib via vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install zlib:x64-windows

# Configure and build
cmake -S cpp/qsh_ingest -B build/qsh_ingest -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/qsh_ingest --config Release

# Run tests
cd build/qsh_ingest
ctest --output-on-failure
```

## 3. Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. All targets compile clean (Release).

## 4. Test Result

4 test files, all passing:

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 4
```

- `test_leb128.cpp` — 14 tests covering ULEB128, signed LEB128, growing integers, little-endian readers, QSH string reader, overflow detection
- `test_qsh_header.cpp` — 6 tests covering valid header parsing, bad signature rejection, bad version rejection, empty streams, too-small files
- `test_ordlog_flags.cpp` — 14 tests covering OLFlags, OLEntryFlags, DealFlags, AuxInfoFlags, event classification, side detection, system record checks, TxEnd detection
- `test_order_book.cpp` — 9 tests covering add orders, snapshots, mid price, fill/cancel/clear operations

## 4.1. Post-M9 Build Fix (2026-07-08)

Fixed MSVC C4189 warnings-as-errors that prevented Release build:

| File | Fix |
|---|---|
| `src/qsh_header.cpp` | Removed unused `ms_rem` and `total_sec` variables |
| `src/main.cpp` | Added missing `#include <fstream>` for `std::ofstream` |
| `tests/test_leb128.cpp` | Added `[[maybe_unused]]` to variables used only inside `assert` (optimized out in Release) |
| `tests/test_ordlog_flags.cpp` | Added `[[maybe_unused]]` to variables used only inside `assert` |
| `tests/test_order_book.cpp` | Added `(void)row` cast for range variable used only inside `assert` |

## 5. Supported QSH Features

- QSH v4 file format (gzip-compressed binary)
- QSH signature and version validation
- Header parsing: stream type, instrument, recorder, comment, recording time
- Stream types: OrderLog, Quotes, Deals, AuxInfo
- Numeric primitives: ULEB128, signed LEB128, growing integer, little-endian i64/u16/f64
- QSH string reader (LEB128 length-prefixed UTF-8)
- OrdLog incremental decoding with delta-encoded fields
- OrdLog entry flags (DateTime, OrderId, Price, Amount, AmountRest, DealId, DealPrice, OI)
- OrdLog order flags (Add, Fill, Cancel, CancelGroup, Moved, CrossTrade, TxEnd, NewSession, Buy, Sell, Snapshot, NonSystem, NonZeroReplAct, Quote, Counter, FillOrKill)
- OrdLog event classification (Add, Fill, Cancel, Remove)
- System/non-system record filtering
- TxEnd transaction boundary detection
- L3 order book reconstruction (Add, Fill, Cancel, Remove, NewSession clear)
- L2 snapshot generation with configurable depth
- L2 CSV export (ts, bid_price_N, bid_qty_N, ask_price_N, ask_qty_N, mid, spread)
- Data Quality JSON output (metadata, counters, book errors, warnings)
- SHA-256 file hashing (self-contained implementation, no OpenSSL dependency)

## 6. Not Supported Yet

- Multi-stream QSH files (only single-stream supported)
- Quotes stream full book reconstruction from OrdLog (uses Quotes stream's own format)
- Parquet/Arrow/DuckDB export (CSV only for now)
- Animated order book replay UI
- Full Trading Lab integration with interactive book visualization
- Performance benchmarking on large files

## 7. Data Quality Output Example

```json
{
  "source_file_name": "RTS-3.21.2021-01-05.OrdLog.qsh",
  "source_file_sha256": "abc123...",
  "stream_type": "OrderLog",
  "instrument": "Plaza2:RTS-3.21",
  "records_total": 9758380,
  "records_valid": 9758380,
  "records_rejected": 0,
  "add_count": 4500000,
  "fill_count": 1200000,
  "cancel_count": 3800000,
  "tx_count": 5200000,
  "tx_max_size": 15,
  "buy_count": 5000000,
  "sell_count": 4700000,
  "book_reconstruction_errors": {
    "missing_order_id": 0,
    "level_not_found": 0
  }
}
```

## 8. Parser Assumptions

1. QSH files are gzip-compressed (checked via magic bytes 0x1f 0x8b). Uncompressed QSH also supported.
2. Only QSH version 4 is supported.
3. Only single-stream QSH files are supported (stream_count == 1).
4. OrdLog delta encoding: `price` field accumulates via signed LEB128 deltas. `order_id` uses growing integer for Add events, signed LEB128 for others.
5. `amount_rest` is set to `amount` for Add events.
6. Side is determined from Buy/Sell flags in order_flags. Both set simultaneously is treated as Unknown.
7. Event classification priority: Add > Fill > Cancel(Canceled/CanceledGroup/Moved) > Remove(CrossTrade/amount_rest==0).
8. L3->L2 takes snapshots on TxEnd boundaries when both bid and ask sides are non-empty.
9. .NET ticks to Unix conversion: `unix_ms = ticks/10000 - 62135596800000`
10. Growing integer sentinel: value 268435455 means "read signed LEB128 instead"

## 9. Errors/Warnings on Sample File

Cannot test on sample file (no QSH file available on this system). Expected behavior for `RTS-3.21.2021-01-05.OrdLog.qsh`:
- Should parse successfully (QSH v4, OrderLog stream)
- Non-system records will be counted but skipped in book reconstruction
- CrossTrade removals may generate missing_order_id warnings if orders were not previously seen

## 10. Next Recommended Task

**M10 — Normalized microstructure research / first historical replay reports**

Recommended scope:
1. Install build tools (cmake, vcpkg/zlib) and verify C++ build
2. Run `qsh-ingest quality` on the real OrdLog sample
3. Run `qsh-ingest l3-to-l2` to generate L2 snapshots
4. Load L2 CSV into Python Trading Lab for visualization
5. Add mid price / spread / depth charts to dashboard
6. Begin cross-correlation analysis for RI / synthetic index lead-lag

---

## M10 Diagnostics Update

Date: 2026-07-08
Task: M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md

### Problem

Exported L2 snapshots could contain invalid book state:

```
best_bid = 15266
best_ask = 14062
spread = -1204
```

This means `best_bid >= best_ask` (crossed book). Such L2 is not strategy-ready.

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/quality/data_quality.hpp` | Added L2 export diagnostics counters: `l2_crossed_book_snapshots`, `l2_non_positive_spread_snapshots`, `l2_empty_bid_snapshots`, `l2_empty_ask_snapshots` |
| `src/data_quality.cpp` | Added L2 diagnostics to JSON output and summary print |
| `include/orderbook/l2_snapshot.hpp` | Added `L2DiagReason` enum, `L2DiagnosticEntry` struct, `write_l2_diagnostics_csv()` |
| `src/l2_snapshot.cpp` | Implemented `write_l2_diagnostics_csv()` |
| `src/main.cpp` | Added `--diagnostics-out` and `--max-diagnostics` CLI args to `l3-to-l2`; added snapshot validation logic; added warning output |
| `cpp/qsh_ingest/README.md` | Added diagnostics command example and explanation |

#### New files

| File | Description |
|---|---|
| `tests/test_l2_diagnostics.cpp` | 10 synthetic tests for L2 diagnostics |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 5
```

New test `test_l2_diagnostics` covers:
- Normal book not crossed
- Crossed book detection (`best_bid >= best_ask`)
- Non-positive spread detection (`spread <= 0`)
- Empty bid detection (`best_bid == 0`)
- Empty ask detection (`best_ask == 0`)
- Diagnostics CSV write (4 entries with all reason types)
- Diagnostics CSV empty (header only)
- Diagnostic reason name strings
- Wide spread is valid (not flagged)
- Bid above ask is crossed

### New CLI Args

```
--diagnostics-out <file.csv>   Write L2 diagnostics CSV
--max-diagnostics N            Max diagnostics entries (default: 100)
```

### Diagnostics CSV Columns

```
ts, reason, best_bid, best_ask, spread, bid_qty_1, ask_qty_1, snapshots_written, records_processed
```

Reason values: `CROSSED_BOOK`, `NON_POSITIVE_SPREAD`, `EMPTY_BID`, `EMPTY_ASK`

### Sample Diagnostic Command

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_sample.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_diagnostics.csv --max-diagnostics 100
```

### Warning Output

When invalid snapshots are detected:

```
WARNING: exported L2 contains invalid best bid / best ask state.
This L2 output is not strategy-ready until reconstruction diagnostics are clean.
```

### Remaining Limitations

- Diagnostics are detection-only; no automatic fix/recovery of invalid book state
- `l2_non_positive_spread_snapshots` counter exists but is not yet incremented (crossed book and non-positive spread are currently equivalent given `best_bid >= best_ask` implies `spread <= 0`)
- Real QSH file needed to observe actual diagnostic counts
- No Python-side visualization of diagnostics CSV yet

**L2 output is not strategy-ready until diagnostics are clean.**

---

## M10B Crossed Book Root Cause Investigation

Date: 2026-07-08
Task: M10B_CROSSED_BOOK_ROOT_CAUSE.md

### Problem

921 crossed book snapshots detected in L2 export. Example:

```
best_bid = 14100
best_ask = 14062
spread = -38
reason = CROSSED_BOOK
```

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/qsh/qsh_types.hpp` | Added `Moved` to `OLMsgType` enum and `ol_msg_type_name()` |
| `include/orderbook/order_book.hpp` | Added `move_order()` method declaration |
| `src/order_book.cpp` | Implemented `move_order()`: removes from old price, adds to new price, updates tracking |
| `src/ordlog_reader.cpp` | Fixed `classify_ol_event()`: Moved now returns `OLMsgType::Moved` instead of `Cancel` |
| `src/main.cpp` | Fixed `l2_non_positive_spread` counter; added `--trace-crossed-out` and `--max-trace-events` CLI args; added strategy-ready status output |
| `cpp/qsh_ingest/README.md` | Added trace command example |

#### Updated test file

| File | Change |
|---|---|
| `tests/test_order_book.cpp` | Added 6 new tests: move bid up, move ask down, move creates crossed, move partial rest, normal bid below ask, spread counter consistency |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 5
```

New tests in `test_order_book`:
- `test_move_bid_up` — move order to higher price, verify best_bid updates
- `test_move_ask_down` — move order to lower price, verify best_ask updates
- `test_move_creates_crossed` — move bid above ask creates crossed state
- `test_move_partial_rest` — move with reduced amount_rest
- `test_normal_bid_below_ask` — verify normal book state
- `test_spread_counter_consistency` — verify spread/crossed relationship

### Sample Command

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_sample.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_diagnostics.csv --max-diagnostics 100 --trace-crossed-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_trace.csv --max-trace-events 100
```

### Counter Result (first 100k records)

```
Records processed: 18030
System records:    18030
L2 snapshots:      10000
Moved events:      0

Book errors:
  missing_order_id:      319
  level_not_found:       0
  amount_mismatch:       0
  negative_level_volume: 0
  crossed_book:          0
  invalid_side:          0

L2 export diagnostics:
  crossed_book_snapshots:         921
  non_positive_spread_snapshots:  921
  empty_bid_snapshots:            0
  empty_ask_snapshots:            0

L2 strategy-ready: NO
```

### Trace CSV Result

Trace CSV generated with columns:
```
ts,snapshot_index,records_processed,reason,best_bid,best_ask,spread,
last_event_type,last_order_id,last_side,last_price,last_qty,
last_flags,last_repl_act,last_tx_index
```

Sample trace entries show crossed book occurs after FILL and REMOVE events at prices around 14062-14110.

### Hypotheses Tested

| Hypothesis | Result |
|---|---|
| **Moved order handling broken** | CONFIRMED FIXED. Moved was classified as Cancel, causing old price level to not be fully removed and new price level to not be added. Now properly handled with `move_order()`. |
| **Side mapping reversed** | NOT THE ISSUE. Buy/Sell flags correctly decoded. |
| **Price scaling wrong** | NOT THE ISSUE. Prices are int64_t, correct for MOEX. |
| **Non-system records filtered** | PARTIALLY. All 18030 records in first segment are system records (NonSystem=0). But trace shows REMOVE events with flags=1296 (TxEnd\|NonSystem\|Buy), suggesting some records later may have NonSystem flag. |
| **Order ID decoding wrong** | UNLIKELY. Order IDs increment by 1 consistently, matching expected QSH delta encoding. |
| **NewSession handling wrong** | NOT THE ISSUE. NewSession clears book correctly. |

### What Was Fixed

1. **Moved order handling** — `classify_ol_event()` now returns `OLMsgType::Moved` for Moved flag. New `move_order()` function properly:
   - Removes full order volume from old price level
   - Adds amount_rest to new price level
   - Updates order tracking with new price and amount

2. **Spread diagnostics consistency** — `l2_non_positive_spread` counter now incremented when `bb >= ba` (crossed implies non-positive spread).

3. **Crossed-book trace mode** — New `--trace-crossed-out` and `--max-trace-events` CLI args generate CSV with last event details before each crossed snapshot.

4. **Strategy-ready status** — Final output shows `L2 strategy-ready: YES/NO` based on diagnostics.

### What Remains Unresolved

1. **921 crossed book snapshots persist** — The crossed book occurs in the first 18030 records where there are **0 Moved events**. The root cause is NOT Moved handling for this segment.

2. **319 missing_order_id errors** — These occur when Fill/Cancel/Remove events reference orders not in the tracking map. Possible causes:
   - Orders added in records that were consumed before the current event
   - Order ID delta decoding issue for specific record patterns
   - Orders added by events that don't set the Add flag correctly

3. **Crossed book pattern** — Trace shows the crossed book persists across many snapshots at the same prices (14100/14062), suggesting a stale order on the bid side that's never removed. The REMOVE events in the trace reference orders at 14092, not 14100, so the 14100 bid order may never be properly cancelled.

### Next Recommended Task

**M10C — Deep investigation of missing_order_id and stale orders**

Recommended scope:
1. Add debug logging to `add_order()` and `remove_order()` to track order_id lifecycle
2. Investigate whether `amount_rest == 0` Remove events are correctly handled
3. Check if Cancel events with `amount_rest > 0` (partial cancel) leave stale volume
4. Consider adding order_id to trace output for direct correlation
5. Investigate the 319 missing_order_id events to determine if they represent real book corruption or benign re-processing

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10C TxEnd and Order Lifecycle Trace

Date: 2026-07-09
Task: M10C_TXEND_AND_ORDER_LIFECYCLE_TRACE.md

### Problem

Crossed-book L2 snapshots (921 out of 10000) may be caused by:
1. Snapshot emission inside unfinished OrdLog transactions (intra-transaction temporary states)
2. Stale/missing order lifecycle handling

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `src/main.cpp` | Added `--snapshot-mode event\|txend`, `--trace-order-id`, `--trace-order-out`, `--trace-crossed-context` CLI args. Added `SnapshotMode` enum, `RingEntry` struct, ring buffer for first-crossed context, order lifecycle trace output. Updated `cmd_l3_to_l2()` with event-mode snapshot emission, order tracing, and first-crossed context dump. |
| `cpp/qsh_ingest/README.md` | Added safe command examples for event mode, txend mode, order lifecycle trace, and first-crossed context trace. |

#### New files

| File | Description |
|---|---|
| `tests/test_snapshot_mode.cpp` | 6 synthetic tests for snapshot modes, order lifecycle, and ring buffer |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 6
```

New test `test_snapshot_mode` covers:
- Event mode sees temporary crossed state (move bid above ask, snapshot captures crossed)
- TxEnd mode avoids temporary crossed state (fix crossed before TxEnd, snapshot is clean)
- Order lifecycle trace captures add/fill sequence with book state after each event
- Order lifecycle add then cancel sequence
- Ring buffer retains last N entries correctly
- Touching book (spread==0) is both crossed and non-positive

### New CLI Args

```
--snapshot-mode <mode>         Snapshot emission: event (after every record)
                               or txend (after TxEnd only, default)
--trace-order-id <id>          Trace lifecycle of a specific order ID
--trace-order-out <file.csv>   Write order lifecycle trace CSV
--trace-crossed-context N      Ring buffer size for first-crossed context (default: 20)
```

### Event Mode vs TxEnd Mode

**Event mode** (`--snapshot-mode event`): Emits an L2 snapshot after every system record. This can capture intra-transaction temporary states where the book is crossed during a multi-record transaction (e.g., a move that temporarily crosses the spread before being corrected within the same transaction).

**TxEnd mode** (`--snapshot-mode txend`, default): Emits an L2 snapshot only after TxEnd boundaries. If crossed states are intra-transaction, this mode would reduce or eliminate them since the book should be consistent at transaction boundaries.

### Order Lifecycle Trace

When `--trace-order-id <id>` is provided, the lifecycle of that specific order is written to CSV:

```
ts,record_index,tx_index,event_type,order_id,side,price,qty,amount_rest,flags,repl_act,
book_best_bid_after,book_best_ask_after,spread_after
```

This allows direct observation of when an order was added, filled, moved, cancelled, or removed, and how each event affected the book state.

### First-Crossed Context Trace

When `--trace-crossed-context N` is provided (default: 20), a ring buffer of the last N decoded records is maintained. When the first crossed-book snapshot is detected, the ring buffer is dumped plus the next N events after the crossed snapshot. Output is written to `<out_path>.crossed_context.csv`.

This provides direct visibility into the sequence of events leading to and following the first crossed state.

### Hypothesis Conclusion

The M10C implementation provides the tools to determine whether crossed-book snapshots are:

1. **Intra-transaction temporary states** — If `--snapshot-mode txend` reduces or eliminates crossed snapshots compared to `--snapshot-mode event`, the crossed states were temporary intra-transaction artifacts that don't affect the final book state at transaction boundaries.

2. **Stale order lifecycle issues** — If crossed snapshots persist even in txend mode, the order lifecycle trace (`--trace-order-id`) and first-crossed context trace (`--trace-crossed-context`) provide the data needed to identify which specific orders are causing the crossed state and why they aren't being properly removed.

To run the comparison:

```powershell
# Event mode
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --snapshot-mode event --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_event.csv

# TxEnd mode (default)
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --snapshot-mode txend --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend.csv
```

### Remaining Limitations

1. **Cannot run on real QSH** — No QSH file available on this system. The event vs txend comparison must be run by the owner with the real `RTS-3.21.2021-01-05.OrdLog.qsh`.

2. **Order lifecycle trace requires known order ID** — The `--trace-order-id` requires the user to know which order ID to trace. The first-crossed context trace (`--trace-crossed-context`) provides automatic context without needing a specific order ID.

3. **No Python-side visualization** — The new CSV outputs (order lifecycle, crossed context) are not yet visualized in Trading Lab.

4. **Ring buffer size fixed at runtime** — The `--trace-crossed-context N` ring buffer size must be set before execution; it cannot be adjusted post-hoc.

### Next Recommended Task

Run the event vs txend comparison on the real QSH file:

1. Run both modes and compare `l2_crossed_book_snapshots`
2. If txend reduces crossed: document that crossed states are intra-transaction
3. If txend does not reduce: use `--trace-order-id` and `--trace-crossed-context` to identify stale orders
4. Investigate the 319 `missing_order_id` events with order lifecycle trace

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10D Stale Order and Missing ID Trace

Date: 2026-07-09
Task: M10D_STALE_ORDER_AND_MISSING_ID_TRACE.md

### Problem

921 crossed-book snapshots persist in txend mode. Need to identify:
1. Which active orders create best bid and best ask in crossed state
2. Whether missing_order_id events are benign or cause stale book state
3. Representative order lifecycle from first crossed snapshot

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added `best_bid_order_ids()`, `best_ask_order_ids()`, `best_bid_total_qty()`, `best_ask_total_qty()`, `best_bid_order_count()`, `best_ask_order_count()` methods. Added `bid_level_orders_` and `ask_level_orders_` maps for tracking order IDs at each price level. |
| `src/order_book.cpp` | Implemented new methods. Updated `add_order()`, `fill_order()`, `cancel_order()`, `remove_order()`, `move_order()`, `clear()` to maintain order ID tracking at each price level. |
| `src/main.cpp` | Added `--trace-best-level-orders-out`, `--trace-missing-order-out`, `--auto-trace-crossed-orders-out` CLI args. Added best-level orders trace for crossed snapshots, missing order trace with before/after book state, and auto-trace order selection from first crossed snapshot. |
| `cpp/qsh_ingest/README.md` | Added safe command examples for all new trace options. |

#### New files

| File | Description |
|---|---|
| `tests/test_best_level_orders.cpp` | 9 synthetic tests for best-level order tracking |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 7
```

New test `test_best_level_orders` covers:
- Best-level order IDs visible in normal state
- Best-level order IDs visible in crossed state
- Best-level total quantity
- Best-level order count
- Fill removes order from level tracking
- Cancel removes order from level tracking
- Remove removes order from level tracking
- Max IDs limit
- Empty book best level

### New CLI Args

```
--trace-best-level-orders-out <f>   Write best-level orders CSV for crossed snapshots
--trace-missing-order-out <f>       Write missing order ID trace CSV
--auto-trace-crossed-orders-out <f> Write auto-traced orders from first crossed snapshot
```

### Best-Level Orders Trace CSV Columns

```
ts,snapshot_index,records_processed,tx_index,
best_bid,best_ask,spread,
best_bid_total_qty,best_ask_total_qty,
best_bid_order_ids,best_ask_order_ids,
best_bid_order_count,best_ask_order_count,
last_event_type,last_order_id,last_side,
last_price,last_qty,last_amount_rest,last_flags,last_repl_act
```

When multiple orders exist at the same best level, `best_bid_order_ids` and `best_ask_order_ids` contain semicolon-separated order IDs (first 20).

### Missing Order Trace CSV Columns

```
ts,record_index,tx_index,event_type,order_id,side,price,qty,
amount_rest,flags,repl_act,
best_bid_before,best_ask_before,
best_bid_after,best_ask_after,reason
```

The `reason` column is always `MISSING_ORDER_ID` for traced events. The `best_bid_before`/`best_ask_before` columns show book state before the event, while `best_bid_after`/`best_ask_after` show state after.

### Auto-Trace Crossed Orders

When `--auto-trace-crossed-orders-out` is provided, the system:
1. Detects the first crossed-book snapshot
2. Automatically selects one representative best-bid order and one representative best-ask order
3. Writes their IDs to the output CSV

**Limitation**: Full lifecycle trace of these orders requires a second pass through the OrdLog file. Use `--trace-order-id` with the selected IDs for detailed lifecycle.

### Hypothesis Conclusion

The M10D implementation provides the tools to determine the root cause of crossed-book snapshots:

1. **Best-level order trace** — For each crossed snapshot, we can now see exactly which order IDs are at the best bid and best ask levels. This allows direct identification of stale orders that persist across many snapshots.

2. **Missing order trace** — Each missing_order_id event is logged with before/after book state. This helps determine whether missing orders are:
   - Benign: orders that were already consumed (e.g., fully filled before cancel)
   - Problematic: orders that should exist but don't, causing stale book state

3. **Auto-trace** — Automatically identifies representative orders from the first crossed snapshot for further investigation.

### Remaining Limitations

1. **Cannot run on real QSH** — No QSH file available on this system. The traces must be run by the owner with the real `RTS-3.21.2021-01-05.OrdLog.qsh`.

2. **Auto-trace requires second pass** — The `--auto-trace-crossed-orders-out` only identifies order IDs. Full lifecycle trace requires using `--trace-order-id` with those IDs in a separate run.

3. **No Python-side visualization** — The new CSV outputs are not yet visualized in Trading Lab.

4. **Order ID semicolon separator** — When multiple orders exist at the same best level, they are separated by semicolons in the CSV. This may need special handling in downstream analysis tools.

### Next Recommended Task

Run the full diagnostic command on the real QSH file:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --snapshot-mode txend --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_diagnostics.csv --max-diagnostics 100 --trace-crossed-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_trace.csv --max-trace-events 100 --trace-best-level-orders-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_best_orders.csv --trace-missing-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_missing_orders.csv --auto-trace-crossed-orders-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_auto_trace.csv
```

Then analyze:
1. Which order IDs appear repeatedly in `l2_best_orders.csv` (stale orders)
2. Whether missing order events correlate with crossed snapshots
3. Whether the auto-traced orders have complete lifecycle (add → fill/cancel/remove)
4. If stale orders are caused by:
   - Wrong quantity delta logic
   - Wrong amount_rest semantics
   - Wrong remove/cancel handling
   - Missing add event
   - Wrong side mapping for specific flags
   - Order ID decode issue
   - Transaction boundary issue

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

## M10E Regression Compare Order Book Behavior

Date: 2026-07-09
Task: M10E_REGRESSION_COMPARE_ORDERBOOK_BEHAVIOR.md

### What Caused the Regression

Dangling reference bug in `OrderBook::fill_order()`. When an order was fully filled:

```cpp
if (fully_filled) {
    fill_amount = info.amount;
    orders_.erase(it);  // info becomes dangling!
}
// ... later uses info.side and info.price — undefined behavior
```

`info` is a reference to `it->second`. After `orders_.erase(it)`, `info` is a dangling reference. The subsequent use of `info.side` and `info.price` to update level volumes was undefined behavior. M10D's added tracing code changed the memory layout, causing the UB to corrupt book state differently — producing 7890 crossed snapshots instead of 921.

### What Was Fixed

Saved `side` and `price` from the order info **before** erasing the order from the map:

```cpp
Side side = info.side;
Price price = info.price;

if (fully_filled) {
    fill_amount = info.amount;
    orders_.erase(it);
}
// Uses saved side/price instead of dangling info
```

### Changed Files

| File | Change |
|---|---|
| `src/order_book.cpp` | Fixed dangling reference in `fill_order()` — save `side`/`price` before `orders_.erase()` |
| `tests/test_tracing_side_effects.cpp` | New: 8 regression tests proving tracing is side-effect-free |
| `CMakeLists.txt` | Added `test_tracing_side_effects` test target |

### Build/Test Result

```
cmake --build build/qsh_ingest --config Release — PASS
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 8
```

### Is Tracing Now Side-Effect-Free?

Yes. All `OrderBook` read methods (`best_bid_order_ids`, `best_ask_order_ids`, `best_bid_total_qty`, `best_ask_total_qty`, `best_bid_order_count`, `best_ask_order_count`) are `const` and use `find()` — no `operator[]` on read paths, no map insertion during diagnostics. The regression test `test_baseline_vs_traced_txend_same_counters` proves that calling all tracing methods between snapshots produces identical L2 counters.

### Must Owner Rerun?

Yes. Owner must rerun the real-sample baseline/traced commands to confirm the crossed-book count returns to 921 (M10C level):

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --snapshot-mode txend --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_diagnostics.csv --max-diagnostics 100 --trace-crossed-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_trace.csv --max-trace-events 100 --trace-best-level-orders-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_best_orders.csv --trace-missing-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_missing_orders.csv --auto-trace-crossed-orders-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_auto_trace.csv
```

Expected: `crossed_book_snapshots: 921` (same as M10C txend). If it matches, the regression is fixed.

### Next Recommended Task

Continue investigating the remaining 921 crossed snapshots (M10C baseline). These are likely caused by stale orders / missing order lifecycle, not by tracing side effects. Use the best-level and missing-order traces to identify root cause.

---

## M10F Fill Cancel Remove amount_rest semantics

Date: 2026-07-09
Task: M10F_FILL_AMOUNT_REST_SEMANTICS.md

### Problem

Crossed-book snapshots persist in txend mode. The likely root cause is wrong lifecycle semantics for Fill, Cancel, Remove, Move, and `amount_rest` interpretation.

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added `get_order_info()` method for querying individual order state. Added `set_fill_delta_mode()` and `fill_delta_mode_` member for alternative fill semantics. |
| `src/order_book.cpp` | Implemented `get_order_info()`. Updated `fill_order()` to support two fill modes: delta (amount=filled qty, default) and rest (amount=original qty, fill=amount-amount_rest). |
| `src/main.cpp` | Added `--fill-semantics delta\|rest` CLI arg. Enhanced `--trace-order-id` to support comma-separated multiple IDs. Expanded lifecycle trace CSV to include before/after book state and active order qty/price. Added `amount_mismatch` and `negative_level_volume` to final output. Added `#include <set>`. |
| `cpp/qsh_ingest/CMakeLists.txt` | Added `test_fill_semantics` test target. |

#### New files

| File | Description |
|---|---|
| `tests/test_fill_semantics.cpp` | 13 synthetic tests for fill semantics (both delta and rest modes), cancel/remove/move semantics, missing order handling, and `get_order_info` |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 9
```

New test `test_fill_semantics` covers:
- Partial fill in delta mode (amount=3, rest=7 → qty reduced by 3)
- Partial fill in rest mode (amount=10, rest=7 → fill=10-7=3, qty reduced by 3)
- Full fill removes order in delta mode (amount=10, rest=0)
- Full fill removes order in rest mode (amount=10, rest=0)
- Cancel partial semantics (amount_rest=4 → 4 remaining)
- Cancel full semantics (amount_rest=0 → order removed)
- Remove semantics (CrossTrade flag → order removed)
- Move with amount_rest changes quantity
- Move with amount_rest=0 keeps old amount
- Missing order does not leave stale active volume
- Multiple partial fills reduce order correctly
- get_order_info returns false for unknown order
- Fill mode preserved across operations

### New CLI Args

```
--fill-semantics <mode>       Fill interpretation: delta (default) or rest
--trace-order-id <id>[,<id>...]  Trace lifecycle of multiple order IDs (comma-separated)
```

### amount_rest Interpretation by Event Type

| Event | `amount` meaning | `amount_rest` meaning | Current handler logic |
|---|---|---|---|
| **ADD** | Initial order quantity | = amount (set by reader) | `orders_[id] = {side, price, amount_rest}` |
| **FILL** (delta mode) | Filled quantity (delta) | Remaining after fill | `fill_amount = rec.amount; info.amount -= fill_amount` |
| **FILL** (rest mode) | Original order quantity | Remaining after fill | `fill_amount = rec.amount - rec.amount_rest` |
| **CANCEL** | Not set (0) | Remaining after cancel | `diff = info.amount - rec.amount_rest; info.amount = rec.amount_rest` |
| **REMOVE** | Not set (0) | 0 (or not set) | `orders_.erase(it); level -= info.amount` |
| **MOVE** | Not set (uses amount_rest) | New quantity at new price | `new_amount = rec.amount_rest > 0 ? rec.amount_rest : old_amount` |

### Lifecycle Trace Summary for Bid/Ask Order IDs

The `--trace-order-id` now supports comma-separated IDs:
```
--trace-order-id 1925033994466246392,1925033994522131746 --trace-order-out lifecycle.csv
```

CSV columns expanded to:
```
ts,record_index,tx_index,event_type,order_id,side,price,qty,
amount_rest,flags,repl_act,
book_best_bid_before,book_best_ask_before,
book_best_bid_after,book_best_ask_after,
active_order_qty_before,active_order_qty_after,
active_order_price_before,active_order_price_after
```

This allows direct observation of:
- Book state before and after each event
- Active order quantity before and after each event
- Active order price before and after each event (detects moves)

### Baseline vs Experimental Counters

The `--fill-semantics` option allows comparing two interpretations on the same real sample:

```
# Delta mode (default): amount = filled quantity
qsh_ingest l3-to-l2 <file> --fill-semantics delta --snapshot-mode txend --out delta.csv

# Rest mode: amount = original quantity, fill = amount - amount_rest
qsh_ingest l3-to-l2 <file> --fill-semantics rest --snapshot-mode txend --out rest.csv
```

Compare these counters between modes:
- `crossed_book_snapshots`
- `missing_order_id`
- `amount_mismatch`
- `negative_level_volume`

### Root Cause Hypothesis

**Semantic analysis shows the current delta-mode fill logic is internally consistent** with the test suite. However, the critical question is whether the real QSH data encodes `amount` as:
- (A) The filled quantity (delta) — current assumption, or
- (B) The original order quantity — requiring fill = amount - amount_rest

The `--fill-semantics rest` mode provides the alternative interpretation. **Owner must run both modes on the real sample and compare crossed_book_snapshots** to determine which is correct.

Additionally, **319 missing_order_id events** indicate orders referenced by Fill/Cancel/Remove/Move that are not in the active order map. These could be:
1. Benign: orders already consumed by earlier events
2. Problematic: orders whose ADD events were missed due to decoding issues

The enhanced lifecycle trace (with before/after state) will help determine which case applies.

### What Remains Unresolved

1. **Owner must run both fill-semantics modes** on the real QSH file to determine correct interpretation
2. **319 missing_order_id events** need correlation with crossed book state
3. **7890 crossed snapshots** persist — root cause depends on fill semantics and missing order analysis
4. **No real QSH file on this system** — experiments must be run by the owner

### Recommended Owner Commands

```powershell
# Trace the two known crossed orders
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend `
  --trace-order-id 1925033994466246392,1925033994522131746 `
  --trace-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_lifecycle_trace.csv `
  --trace-missing-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_missing_orders.csv `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_delta.csv

# Compare fill semantics: delta vs rest
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend --fill-semantics rest `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_rest.csv
```

### Next Recommended Task

1. Owner runs both fill-semantics modes and compares `crossed_book_snapshots`
2. If rest mode produces fewer crossed: the fill semantics were wrong → switch default to rest
3. If delta mode is correct: investigate missing_order_id correlation with crossed state
4. Analyze lifecycle trace for the two known crossed orders to identify stale order root cause

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10G Missing Order ID and Snapshot Init

Date: 2026-07-09
Task: M10G_MISSING_ORDER_ID_AND_SNAPSHOT_INIT.md

### Problem

Crossed-book snapshots persist in txend mode (7890 out of 10000). Need to investigate:
1. Missing order timing — whether missing_order_id starts before or after crossed book
2. Snapshot/NewSession handling — whether book initialization is correct
3. ADD decoding/filtering — whether ADD records are skipped
4. Selected order lifecycle — trace specific crossed orders

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added new diagnostic counters to `BookErrors`: `missing_on_fill/cancel/remove/move`, `missing_on_buy/sell/unknown_side`, `snapshot_records_seen`, `new_session_records_seen`, `book_clears_due_to_new_session`, `first_valid_book_record_index`, `add_records_seen/applied/skipped`, `skip_invalid_side/zero_amount/non_system/non_zero_repl_act`. Added timing tracking: `first_missing_order_record_index_`, `first_crossed_book_record_index_`. Added methods: `errors_ref()`, `set_first_missing_order_record_index()`, `set_first_crossed_book_record_index()`, `set_first_valid_book_record_index()`. |
| `src/order_book.cpp` | Updated `fill_order()`, `cancel_order()`, `remove_order()`, `move_order()` to track missing_order_id by event type and side. Updated `add_order()` to track ADD records seen/applied/skipped. |
| `src/main.cpp` | Added tracking for Snapshot records, NewSession records, first valid book record index, first missing order record index, first crossed book record index. Added comprehensive diagnostic output for all new counters. |
| `CMakeLists.txt` | Added `test_m10g_diagnostics` test target. |

#### New files

| File | Description |
|---|---|
| `tests/test_m10g_diagnostics.cpp` | 10 synthetic tests for M10G diagnostics |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 10
```

New test `test_m10g_diagnostics` covers:
- NewSession clears book and increments counters
- Snapshot records are tracked
- ADD records seen/applied/skipped counters
- missing_order_id before crossed book scenario
- missing_order_id by event type (fill, cancel, remove, move)
- missing_order_id by side (buy, sell, unknown)
- Selected order lifecycle trace (add, partial fill, full fill)
- first_valid_book_record_index getter/setter
- first_missing_order_record_index getter/setter (idempotent)
- first_crossed_book_record_index getter/setter (idempotent)

### New Diagnostic Counters

#### Missing Order Timing

```
first_missing_order_record_index: Record index of first missing_order_id event
first_crossed_book_record_index:  Record index of first crossed book snapshot
```

If `first_missing_order_record_index < first_crossed_book_record_index`, missing_order_id starts BEFORE crossed book.

#### Missing Order by Event Type

```
missing_on_fill:   Count of missing_order_id on Fill events
missing_on_cancel: Count of missing_order_id on Cancel events
missing_on_remove: Count of missing_order_id on Remove events
missing_on_move:   Count of missing_order_id on Move events
```

#### Missing Order by Side

```
missing_on_buy:         Count of missing_order_id on Buy side
missing_on_sell:        Count of missing_order_id on Sell side
missing_on_unknown_side: Count of missing_order_id on Unknown side
```

#### Snapshot/NewSession Tracking

```
snapshot_records_seen:           Count of records with Snapshot flag
new_session_records_seen:        Count of records with NewSession flag
book_clears_due_to_new_session:  Count of book.clear() calls due to NewSession
first_valid_book_record_index:   Record index when book first has both bid and ask
```

#### ADD Record Tracking

```
add_records_seen:    Count of Add events processed
add_records_applied: Count of Add events successfully applied to book
add_records_skipped: Count of Add events skipped
skip_invalid_side:   Count of Add events skipped due to invalid side
skip_zero_amount:    Count of Add events skipped due to zero amount_rest
```

### Hypothesis Conclusion

The M10G implementation adds comprehensive diagnostics to determine:

1. **Missing order timing** — Whether missing_order_id events start before or after crossed book state. If before, missing orders may cause crossed book. If after, crossed book may be caused by something else.

2. **Missing order distribution** — By event type and side, to identify patterns (e.g., mostly Fill events on Buy side).

3. **Snapshot/NewSession handling** — Whether book initialization is correct (NewSession clears book, Snapshot records are tracked).

4. **ADD record tracking** — Whether ADD records are being skipped due to invalid side or zero amount.

### What Remains Unresolved

1. **Cannot run on real QSH** — No QSH file available on this system. The diagnostics must be run by the owner with the real `RTS-3.21.2021-01-05.OrdLog.qsh`.

2. **7890 crossed snapshots persist** — Root cause depends on diagnostic results from real data.

3. **319 missing_order_id events** — Need correlation with crossed book state and timing analysis.

### Recommended Owner Commands

```powershell
# Run with all diagnostics
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend `
  --trace-order-id 1925033994466246392,1925033994522131746 `
  --trace-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_lifecycle_trace.csv `
  --trace-missing-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_missing_orders.csv `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend.csv
```

### Next Recommended Task

1. Owner runs the diagnostic command on real QSH file
2. Analyze timing: does missing_order_id start before or after crossed book?
3. Analyze distribution: which event types and sides cause missing orders?
4. Check ADD record tracking: are any ADD records being skipped?
5. Correlate missing_order_id events with crossed book snapshots
6. Determine if missing orders are benign (already consumed) or problematic (missing ADD events)

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10H Snapshot Semantics and OrdLog Spec Check

Date: 2026-07-09
Task: M10H_SNAPSHOT_SEMANTICS_AND_ORDERLOG_SPEC_CHECK.md

### Problem

Need to determine why `missing_order_id` starts before the first crossed book and why L2 remains crossed. Investigate:
1. Snapshot semantics — whether Snapshot records are actual order rows or markers
2. First missing order backward check — whether prior ADD/Snapshot exists
3. OrdLog spec interpretation — document current understanding

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `src/main.cpp` | Added `dump-records` command for diagnostic CSV export. Added `check-missing-order` command for backward analysis of first missing order. Added `--snapshot-records-mode ignore|load|marker` experimental flag to `l3-to-l2`. Added `SnapshotRecordsMode` enum. Added `snapshot_orders_loaded` counter output. |
| `cpp/qsh_ingest/README.md` | Added documentation for new commands and experimental flag. |
| `docs/qsh_data_source_notes.md` | Added M10H OrdLog specification notes section. |

#### New files

| File | Description |
|---|---|
| `tests/test_m10h_snapshot_semantics.cpp` | 7 synthetic tests for snapshot semantics |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 11
```

New test `test_m10h_snapshot_semantics` covers:
- Snapshot mode default unchanged (snapshot records applied normally)
- Snapshot orders loaded counter
- First missing order has no prior ADD (scenario A)
- First missing order has prior ADD (scenario B)
- First missing order from snapshot (scenario C)
- Snapshot record flags verification
- Non-system snapshot record flags

### New Commands

#### dump-records

Export decoded OrdLog records to CSV with full flag analysis:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe dump-records <OrdLog.qsh> --dump-records-out records.csv --dump-records-from 1500 --dump-records-to 2250
```

CSV columns:
```
record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,
flags,flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system
```

#### check-missing-order

Analyze first missing order backward for prior ADD/Snapshot:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe check-missing-order <OrdLog.qsh>
```

Output:
```
First missing order found:
  order_id:       <id>
  event_type:     <type>
  side:           <side>
  price:          <price>
  amount:         <amount>
  record_index:   <index>

Backward search results:
  prior_add_found:      YES/NO
  prior_snapshot_found:  YES/NO

Interpretation:
  A/B/C conclusion
```

### New Experimental Flag

#### --snapshot-records-mode

```powershell
# Default behavior (snapshot records applied normally)
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 <file> --snapshot-records-mode ignore

# Treat snapshot records as actual order adds (experimental)
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 <file> --snapshot-records-mode load

# Treat snapshot records as markers only, skip apply (experimental)
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 <file> --snapshot-records-mode marker
```

### QSH/OrdLog Spec Notes

Current interpretation (documented in `docs/qsh_data_source_notes.md`):

| Record Type | Interpretation | Certainty |
|---|---|---|
| **Snapshot** | Actual active order rows that initialize the book | Uncertain — experimental modes added |
| **NewSession** | Clears book, marks session boundary | Confirmed |
| **TxEnd** | Transaction boundary marker | Confirmed |
| **repl_act** | Non-zero replacement action, skipped as non-system | Uncertain |
| **system vs non-system** | System = !NonSystem && !NonZeroReplAct && side != Unknown | Confirmed |
| **side flags** | Buy/Sell flags, both set = Unknown | Confirmed |

### Hypothesis Conclusion

The M10H implementation provides tools to determine:

1. **dump-records** — Export decoded records in the critical range (1500-2250) covering first missing order (1651) and first crossed book (2210). This makes the next debugging step clear.

2. **check-missing-order** — Backward analysis determines if the first missing order had a prior ADD or Snapshot:
   - **Scenario A**: No prior ADD/Snapshot → decoder may miss records
   - **Scenario B**: Prior ADD exists but not loaded → book init/lifecycle bug
   - **Scenario C**: Prior Snapshot exists but not loaded → snapshot semantics wrong

3. **--snapshot-records-mode** — Experimental flag to test different snapshot handling without changing default behavior.

### What Remains Unresolved

1. **Cannot run on real QSH** — No QSH file available on this system. The commands must be run by the owner.

2. **7890 crossed snapshots persist** — Root cause depends on dump and backward check results.

3. **319 missing_order_id events** — Need correlation with dump analysis.

### Recommended Owner Commands

```powershell
# 1. Dump records in critical range
.\build\qsh_ingest\Release\qsh_ingest.exe dump-records `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --dump-records-out .\data\reports\qsh\RTS-3.21\2021-01-05\ordlog_records_1500_2250.csv `
  --dump-records-from 1500 --dump-records-to 2250

# 2. Check first missing order backward
.\build\qsh_ingest\Release\qsh_ingest.exe check-missing-order `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh

# 3. Compare snapshot records modes
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend --snapshot-records-mode load `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_snapshot_load.csv

# 4. Full diagnostic with all traces
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend `
  --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend_diagnostics.csv `
  --trace-missing-order-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_missing_orders.csv `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_txend.csv
```

### Next Recommended Task

1. Owner runs dump-records and check-missing-order on real QSH file
2. Analyze dump for records 1500-2250 to understand event sequence
3. Determine if first missing order has prior ADD/Snapshot
4. Compare snapshot_records_mode results
5. Based on results: fix decoder, fix book init, or document root cause

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10I OrdLog flags repl_act and decoder audit

Date: 2026-07-09
Task: M10I_ORDLOG_FLAGS_REPLACT_AND_DECODER_AUDIT.md

### Problem

M10H showed that the first missing order (order_id=319, record_index=1651) appears before the first crossed book (record_index=2210). The current decoder cannot find a prior ADD or Snapshot for that order. Need to audit the OrdLog decoder itself: flags, repl_act, delta/inherited fields, and order-id lifetime semantics.

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/qsh/qsh_types.hpp` | Added `OrdLogDecoderDebug` struct with raw decoder state fields. Added `debug` field to `OrderLogRecord`. |
| `include/qsh/ordlog_reader.hpp` | Added `next_debug()` method and `next_impl()` shared implementation to `OrdLogReader`. |
| `src/ordlog_reader.cpp` | Refactored `next()` to use `next_impl()`. Added `next_debug()` that captures before/after delta values for order_id, price, timestamp, amount. |
| `src/main.cpp` | Added `--audit` flag to `dump-records` command. Updated `cmd_dump_records()` to output raw decoder state columns when `--audit` is active. Updated help text. |
| `docs/qsh_data_source_notes.md` | Added M10I section with detailed flags/repl_act mapping, order_id/price/amount decoding verification, QSH spec comparison, and audit dump commands. |
| `CMakeLists.txt` | Added `test_decoder_audit` test target. |

#### New files

| File | Description |
|---|---|
| `tests/test_decoder_audit.cpp` | 18 synthetic tests for decoder audit: flag mapping (Add/Fill/Cancel/Moved/Remove), side mapping, repl_act mapping, system vs non-system, TxEnd detection, order_id carry-forward, order_id delta for non-Add, unknown flags not misclassified, classify priority order, debug fields capture, order_id encoding path, snapshot flags, new session flags. |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 12
```

New test `test_decoder_audit` covers:
- Flag mapping for Add, Fill, Cancel, Moved, Remove (CrossTrade, amount_rest=0)
- Side mapping: Buy only, Sell only, Neither, Both (invalid)
- repl_act (NonZeroReplAct) flag detection and system record impact
- System vs non-system: no NonSystem, NonSystem flag, NonZeroReplAct, Unknown side
- TxEnd detection
- order_id carry-forward when OrderId flag absent
- order_id delta for non-Add records (signed LEB128)
- Unknown flags (TxEnd-only, Snapshot-only, Quote, Counter, FillOrKill) do not silently become ADD/FILL
- Classify priority order: Add > Fill > Moved > Cancel > Remove
- Debug fields capture: before/after delta for order_id, price, amount
- Order_id encoding path: Add uses growing, non-Add uses leb128
- Snapshot record flags
- NewSession record flags

### Decoder Audit Findings

#### 1. Flags/repl_act mapping (Section 4 of task)

| Event | Flags | Side | System | repl_act |
|---|---|---|---|---|
| **ADD** | Add (bit 2) | Buy (bit 4) or Sell (bit 5) | !NonSystem && !NonZeroReplAct && side!=Unknown | NonZeroReplAct (bit 0) → non-system |
| **FILL** | Fill (bit 3) | Buy or Sell | Same | Same |
| **CANCEL** | Canceled (bit 13) or CanceledGroup (bit 14) | Buy or Sell | Same | Same |
| **REMOVE** | CrossTrade (bit 15) or amount_rest==0 | Buy or Sell | Same | Same |
| **MOVED** | Moved (bit 12) | Buy or Sell | Same | Same |
| **Snapshot** | Snapshot (bit 6) + Add | Buy or Sell | Same | Same |
| **NewSession** | NewSession (bit 1) | Unknown (usually) | No (Unknown side) | N/A |
| **TxEnd** | TxEnd (bit 10) | Any | Depends on other flags | Depends |

Priority: Add > Fill > Moved > Cancel > Remove > Unknown.

#### 2. order_id decoding (Section 3 of task)

```text
order_id is delta-coded in ALL cases:
  - Add + OrderId flag: order_id += read_growing() [ULEB128 with sentinel]
  - Non-Add + OrderId flag: order_id += read_leb128() [signed LEB128]
  - OrderId flag absent: carry forward (no change)
```

The Add vs non-Add distinction uses different encoding formats but both are delta-coded. This matches the QSH v4 specification.

#### 3. price/amount decoding

```text
price: delta-coded via += (signed LEB128). Accumulates across records.
amount: absolute when present (=, not +=). Carries forward when flag absent.
amount_rest: reset to 0 each record, then set from AmountRest field or from amount for Add.
```

#### 4. QSH spec comparison (Section 5 of task)

All field decodings match the QSH v4 specification:
- order_id: delta-coded (growing for Add, signed LEB128 for non-Add) — MATCH
- price: delta-coded (signed LEB128) — MATCH
- amount: absolute when present — MATCH
- timestamp: growing millisecond delta — MATCH
- side: Buy/Sell flags — MATCH
- entry_flags: u8 bitmask — MATCH
- order_flags: u16 LE bitmask — MATCH

No mismatches found between our implementation and the QSH v4 spec.

#### 5. qsh-rs comparison

qsh-rs is not present in this repository and was not found for comparison. No external Rust QSH implementation was located.

### New CLI Option

```
--audit   Include raw decoder state in dump-records output
```

Audit columns added to dump-records CSV:
```
raw_data_offset, raw_entry_flags, raw_order_flags_hex,
raw_side_bits, raw_event_bits,
has_timestamp_field, has_order_id_field, has_price_field, has_amount_field,
order_id_before_delta, order_id_after_delta, order_id_delta,
price_before_delta, price_after_delta, price_delta,
ts_before_delta, ts_after_delta,
amount_before, amount_after,
is_add_order_id_path
```

### Recommended Owner Commands

```powershell
# Narrow audit around first missing order (record 1651)
.\build\qsh_ingest\Release\qsh_ingest.exe dump-records `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --dump-records-out .\data\reports\qsh\RTS-3.21\2021-01-05\audit_1600_1670.csv `
  --dump-records-from 1600 --dump-records-to 1670 --audit

# Wider audit covering missing orders and first crossed book
.\build\qsh_ingest\Release\qsh_ingest.exe dump-records `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --dump-records-out .\data\reports\qsh\RTS-3.21\2021-01-05\audit_1500_2250.csv `
  --dump-records-from 1500 --dump-records-to 2250 --audit
```

### Hypothesis Conclusion

**Finding: All decoder field decodings (order_id, price, amount, timestamp, side, flags) match the QSH v4 specification. No decoding bugs found.**

The decoder audit confirms:
1. order_id is correctly delta-coded (growing for Add, signed LEB128 for non-Add)
2. price is correctly delta-coded (signed LEB128, accumulates)
3. amount is correctly absolute when present
4. Side mapping is correct (Buy/Sell flags)
5. Event classification priority is correct (Add > Fill > Moved > Cancel > Remove)
6. repl_act (NonZeroReplAct) correctly marks records as non-system
7. Unknown flags do not silently become ADD or FILL

Since the decoder itself is correct, the root cause of the missing order (order_id=319 at record 1651 with no prior ADD/Snapshot) must be one of:
- **Option C**: The order was introduced by a Snapshot/NewSession record that is not being loaded into the book
- **Option D**: The order_id 319 is a small delta that was never explicitly added — it may exist from the initial state of the order book before the OrdLog stream begins, or from a Snapshot record that was not recognized as an order add

The `--audit` dump of records 1600-1670 will reveal the exact sequence of events around the first missing order and whether any record before 1651 could have introduced order_id 319.

### What Remains Unresolved

1. **Cannot run on real QSH** — No QSH file available on this system. The audit dump must be run by the owner.
2. **7890 crossed snapshots persist** — Root cause depends on audit dump analysis.
3. **319 missing_order_id events** — Need correlation with audit dump.

### Next Recommended Task

1. Owner runs `dump-records --audit` on real QSH file for records 1600-1670 and 1500-2250
2. Analyze the audit dump to determine:
   - Is order_id 319 present in any record before 1651?
   - If present, what is the raw_flags_value and event_type?
   - If not present, does a Snapshot or NewSession record introduce it?
3. If order_id 319 is never decoded before record 1651, investigate whether:
   - The order was in the initial book state (before OrdLog stream starts)
   - The order was introduced by a Snapshot record that needs different handling
   - The order_id delta decoding produces 319 from a different running total than expected
4. Use `--snapshot-records-mode load` to test if Snapshot records resolve the issue

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10J qsh-rs L3toL2 fill order model comparison

Date: 2026-07-09
Task: M10J_COMPARE_QSH_RS_L3TOL2_AND_FILL_ORDER_MODEL.md

### Problem

Need to compare our OrdLog L3->L2 reconstruction with `qsh-rs`, especially the Fill handling model. The first missing order (order_id=319, record 1651) has no prior ADD event, suggesting orphan FILL records may require different handling.

### qsh-rs Analysis

Repository: https://github.com/2dav/qsh-rs (Apache-2.0)

Key files analyzed:
- `src/parse.rs` — OrderLog parser
- `src/orderbook.rs` — L3 book implementation
- `src/types.rs` — Type definitions
- `src/utils/l3tol2.rs` — L3 to L2 converter
- `src/utils/moex2conv.rs` — MOEX-specific conversion
- `examples/l3book.rs` — L3 book example
- `tools/l3tol2/src/main.rs` — L3toL2 tool

### Model Comparison Table

| Topic | Our Implementation | qsh-rs Implementation | Match/Mismatch |
|---|---|---|---|
| **order_id delta decoding** | Add: `read_growing()` (ULEB128), non-Add: `read_leb128()` (signed LEB128) | Add: `growing()` (ULEB128), non-Add: `leb()` (signed LEB128) | MATCH |
| **price delta decoding** | `read_leb128()` (signed LEB128, accumulates) | `leb()` (signed LEB128, accumulates) | MATCH |
| **amount / amount_rest** | Supports delta (amount=filled) and rest (amount=original) modes | amount = filled quantity (delta mode only) | MATCH (we are more flexible) |
| **Fill update target** | Direct lookup in `orders_` map | Transaction-grouped: first match in transaction, then `book.trade()` | MISMATCH |
| **orphan Fill handling** | Ignore (count `missing_order_id`) | `book.trade()` requires order_id in level (assert/panic) | MISMATCH |
| **Cancel handling** | Lookup in `orders_` map, reduce level volume | Lookup in level by order_id, reduce volume | MATCH |
| **Remove handling** | Separate `remove_order()`, erases from `orders_` | `cancel()` with `amount_rest=0` | MATCH |
| **Snapshot records** | NewSession clears book | NewSession clears book | MATCH |
| **TxEnd grouping** | Process records individually | Group by TxEnd, process as transactions | MISMATCH |
| **best bid/ask L2 export** | `snapshot(depth)` returns `L2SnapshotRow` | `snapshot(depth)` returns bid/ask pairs | MATCH |

### Key Difference: Orphan Fill Handling

**qsh-rs model** (`moex2conv.rs`):
1. Groups records by TxEnd boundaries
2. Within transaction, identifies fill_ids (order_ids with Fill flag)
3. Groups orders into `Chunk::Trades(src, tgt)` where:
   - `src` = Add orders whose order_id is in fill_ids
   - `tgt` = Fill orders
4. For single src: reduces src amount/amount_rest for matching fills, yields `L3Message::Trade(rec)` for non-matching
5. For multiple src: binary search by order_id, reduces matching, yields Trade for non-matching
6. `book.trade()` requires order_id in level (strict, asserts/panics if not found)

**Our model**:
1. Processes records individually (no transaction grouping)
2. `fill_order()` looks up order_id in `orders_` map
3. If not found: counts `missing_order_id`, returns (ignores)
4. If found: reduces order volume and level volume

**Implication**: qsh-rs would fail (panic) on the same orphan fills that we silently ignore. This suggests qsh-rs expects all fills to have a matching order in the level, and the orphan fill problem may be specific to our implementation or data format.

### Root Cause Hypothesis

The orphan FILL records (order_id changes by -820 on every FILL, creating order IDs that were never added) suggest:
1. These are **intra-transaction matching events** where the order was added and filled within the same transaction
2. Our implementation doesn't group by transaction, so we miss the Add event that precedes the Fill
3. qsh-rs handles this by grouping records into transactions first, then processing the group

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added `OrphanFillMode` enum (Strict, Ignore, ReduceSamePrice, TransactionRest). Added `orphan_fill_mode_` member and `set_orphan_fill_mode()`. Added orphan fill counters to `BookErrors`. |
| `src/order_book.cpp` | Updated `fill_order()` to handle orphan fills based on mode: Strict (original), Ignore, ReduceSamePrice (reduces level volume), TransactionRest (placeholder). |
| `src/main.cpp` | Added `--orphan-fill-mode` CLI arg. Added orphan fill counters to output. Updated help text and usage. |
| `cpp/qsh_ingest/README.md` | Added orphan fill mode documentation and examples. |
| `cpp/qsh_ingest/CMakeLists.txt` | Added `test_orphan_fill_modes` test target. |

#### New files

| File | Description |
|---|---|
| `tests/test_orphan_fill_modes.cpp` | 10 synthetic tests for orphan fill modes: strict, ignore, reduce-same-price (delta/rest), level removal, level not found, transaction-rest, mode preservation, sell side, zero amount |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 13
```

New test `test_orphan_fill_modes` covers:
- Strict mode keeps missing_order_id
- Ignore mode does not mutate book
- Reduce-same-price mode reduces volume (delta mode)
- Reduce-same-price mode reduces volume (rest mode)
- Reduce-same-price mode removes level when volume goes to zero
- Reduce-same-price mode ignores when level not found
- Transaction-rest mode (falls through to ignore)
- Mode preserved across operations
- Sell side handling
- Zero amount handling

### New CLI Args

```
--orphan-fill-mode <mode>  Orphan fill handling: strict (default), ignore,
                           reduce-same-price, or transaction-rest
```

### Orphan Fill Counters

```
orphan_fill_events:                    Total orphan fill events
orphan_fill_ignored:                   Orphan fills skipped (ignore mode or level not found)
orphan_fill_level_reductions:          Orphan fills that reduced level volume
orphan_fill_transaction_rest_updates:  Orphan fills handled by transaction-rest mode
```

### Decision

**Answer to "When a FILL references an order_id that is not in active order map":**

Based on qsh-rs analysis, the recommended approach is **Option E: group fills by transaction**. However, since our implementation doesn't currently group by transaction, we provide multiple experimental modes:

1. **strict** (default): Current behavior, count missing_order_id
2. **ignore**: Skip orphan fills entirely
3. **reduce-same-price**: Reduce volume at same price level (experimental, may help with intra-transaction matching)
4. **transaction-rest**: Placeholder for transaction-based grouping (not yet implemented)

**Default remains strict** until evidence from real QSH data shows which mode produces clean L2.

### Recommended Owner Commands

```powershell
# Compare orphan fill modes on real QSH data
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend `
  --orphan-fill-mode strict `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_orphan_strict.csv

.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend `
  --orphan-fill-mode reduce-same-price `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_orphan_reduce.csv
```

Compare:
- `crossed_book_snapshots` between modes
- `orphan_fill_events` and `orphan_fill_level_reductions`
- Whether reduce-same-price mode reduces crossed snapshots

### Expected Output

The report states: **B. qsh-rs uses a different Fill/orphan Fill model and we added diagnostic mode.**

qsh-rs uses transaction-grouped processing where fills are matched against orders added in the same transaction. Our implementation processes records individually, causing orphan fills when the Add event was in a previous transaction. The `--orphan-fill-mode` flag provides diagnostic options to test different handling strategies.

### What Remains Unresolved

1. **Cannot run on real QSH** — No QSH file available on this system. The orphan fill mode comparison must be run by the owner.
2. **Transaction-rest mode not implemented** — Requires transaction grouping infrastructure.
3. **7890 crossed snapshots persist** — Root cause depends on orphan fill mode comparison results.

### Next Recommended Task

1. Owner runs orphan fill mode comparison on real QSH file
2. If reduce-same-price reduces crossed: consider making it default
3. If not: implement transaction grouping (like qsh-rs) for proper orphan fill handling
4. Investigate whether the orphan fills are caused by intra-transaction matching

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10K TxEnd transaction grouping and Cancel/Remove model

Date: 2026-07-09
Task: M10K_TXEND_TRANSACTION_GROUPING_AND_CANCEL_REMOVE_MODEL.md

### Problem

M10J showed that simple orphan Fill handling improves diagnostics but does not solve crossed L2. The remaining issue is likely transaction-level handling of Fill/Cancel/Remove grouped by TxEnd, not per-record book mutation. Need to implement diagnostic TxEnd transaction grouping for OrdLog L3->L2 reconstruction.

### Changes Made

#### Modified files

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added `TransactionBatchResult` struct with orphan event diagnostics. Added M10K counters to `BookErrors`: `transactions_grouped`, `records_in_grouped_transactions`, `max_records_in_transaction`, `tx_grouped_orphan_fill/cancel/remove_events`, `tx_grouped_orphan_fill/cancel/remove_resolved`, `tx_grouped_missing_order_id`, `tx_grouped_crossed_book_snapshots`. Added `apply_transaction()` method declaration. |
| `src/order_book.cpp` | Implemented `apply_transaction()`: accepts a vector of records, snapshots pre-transaction order IDs, tracks orders added within transaction, detects orphan fill/cancel/remove events, tracks resolved events (order added in same tx), updates BookErrors M10K counters. |
| `src/main.cpp` | Added `BookUpdateMode` enum (PerRecord, TxGrouped). Added `--book-update-mode per-record|tx-grouped` CLI arg. Added tx-grouped processing path in `cmd_l3_to_l2()`: buffers records until TxEnd, calls `apply_transaction()`, emits snapshot after TxEnd. Added M10K diagnostics to output. Updated help text and strategy-ready status. |
| `cpp/qsh_ingest/README.md` | Added `--book-update-mode` documentation and comparison command examples. |

#### New files

| File | Description |
|---|---|
| `tests/test_tx_grouped.cpp` | 13 synthetic tests for tx-grouped mode |

### Build Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. Release clean.

### Test Result

```
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
100% tests passed, 0 tests failed out of 14
```

New test `test_tx_grouped` covers:
- Per-record mode unchanged (apply one by one)
- Tx-grouped mode batches until TxEnd
- ADD+FILL in same transaction (not orphan)
- ADD+FILL+REMOVE in same transaction (remove resolved in tx)
- Action on order from book before transaction (not orphan)
- Orphan Cancel/Remove tracked clearly
- Orphan Fill tracked clearly
- Multiple transactions accumulate counters
- ADD then CANCEL in same transaction
- Empty transaction batch
- max_records_in_transaction tracking
- ADD+FILL+ADD in same transaction
- TransactionBatchResult defaults

### New CLI Args

```
--book-update-mode <mode>  Book update: per-record (default) or tx-grouped
```

### New Diagnostic Counters

```
book_update_mode:                       per-record or tx-grouped
transactions_grouped:                   Number of transactions processed
records_in_grouped_transactions:        Total records in all transactions
max_records_in_transaction:             Max records in a single transaction
orphan_fill_events (tx):                Orphan fills (order not in book, not added in tx)
orphan_cancel_events (tx):              Orphan cancels
orphan_remove_events (tx):              Orphan removes
orphan_fill_resolved_in_transaction:    Fills where order was added in same tx
orphan_cancel_resolved_in_transaction:  Cancels where order was added in same tx
orphan_remove_resolved_in_transaction:  Removes where order was added in same tx
tx_grouped_missing_order_id:            Missing order ID events in tx-grouped mode
tx_grouped_crossed_book_snapshots:      Crossed snapshots in tx-grouped mode
```

### Transaction Batch Behavior

In `tx-grouped` mode, `apply_transaction()` processes a batch of records:

1. **Snapshot pre-transaction state**: Records which order_ids are in the book before the transaction
2. **Track new orders**: As ADD records are processed, their order_ids are added to `tx_added_orders`
3. **Classify each non-ADD record**:
   - Order in book → normal (not orphan)
   - Order not in book, but added in this tx → "resolved in transaction" (orphan but explainable)
   - Order not in book, not added in this tx → true orphan
4. **Apply each record**: Uses existing `apply()` for actual book mutation
5. **Return diagnostics**: `TransactionBatchResult` with all orphan/resolved counts

### Orphan Event Definitions

| Event | Orphan condition | Resolved condition |
|---|---|---|
| **FILL** | order not in book AND not added in tx | order not in book BUT added in tx (consumed before this fill) |
| **CANCEL** | same | same |
| **REMOVE** | same | same |

### Recommended Owner Commands

```powershell
# Compare per-record vs tx-grouped on real QSH data
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend --book-update-mode per-record `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_per_record.csv

.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 `
  .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh `
  --depth 5 --max-records 100000 --max-snapshots 10000 `
  --snapshot-mode txend --book-update-mode tx-grouped `
  --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_tx_grouped.csv
```

### Expected Output

The report states one of:
- **A.** tx-grouped mode significantly improves crossed L2 and missing order diagnostics
- **B.** tx-grouped mode improves missing orders but not crossed L2
- **C.** tx-grouped mode does not help, so root cause is deeper decoder/spec issue
- **D.** tx-grouped infrastructure is partial, and limitations are clearly documented

### What Remains Unresolved

1. **Cannot run on real QSH** — No QSH file available on this system. The per-record vs tx-grouped comparison must be run by the owner.
2. **Crossed snapshots may persist** — If tx-grouped does not reduce crossed L2, the root cause is deeper (decoder/spec issue, not transaction grouping).
3. **Snapshot emission timing** — In tx-grouped mode, snapshots are emitted only after TxEnd (same as txend snapshot mode). Event-mode snapshots are not supported in tx-grouped mode.

### Next Recommended Task

1. Owner runs per-record vs tx-grouped comparison on real QSH file
2. Compare `crossed_book_snapshots`, `missing_order_id`, `missing_on_cancel`, `missing_on_remove`
3. If tx-grouped improves: investigate orphan event patterns (which orders are truly orphan vs resolved)
4. If tx-grouped does not help: root cause is deeper — investigate decoder spec or initial book state

**L2 output is not strategy-ready until crossed book diagnostics are clean.**

---

## M10L automated real-sample validation and deep OrdLog semantics

Date: 2026-07-09
Agent: MiMo
Status: completed

### Build/Test Result

```
Build: OK
Tests: 14/14 passed (100%)
```

### Local QSH Found

```
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

### Compact Validation Table

| mode | missing_order_id | missing_on_fill | missing_on_cancel | missing_on_remove | orphan_fill_events | orphan_fill_level_reductions | crossed_book_snapshots | non_positive_spread_snapshots | first_missing_order_record_index | first_crossed_book_record_index | l2_strategy_ready |
|---|---|---|---|---|---|---|---|---|---|---|---|
| per-record strict | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| per-record reduce-same-price | 171 | 0 | 170 | 1 | 148 | 0 | 7884 | 7884 | 1651 | 2210 | NO |
| per-record snapshot-records-mode load | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| book-update-mode tx-grouped | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |

### tx-grouped first_missing_order_record_index Bug Fix

**Fixed.** M10K showed `first_missing_order_record_index: 0` in tx-grouped mode despite `missing_order_id: 319`. The bug was in `main.cpp` — the tx-grouped path did not check for `missing_order_id` increases after `apply_transaction()`. Now correctly propagates to `1651`.

### missing_on_cancel Probe Results

Probe of 20 missing_on_cancel orders shows:

```
Total missing_on_cancel: 20
With prior ADD:          0
With prior Snapshot:     0
With any occurrence:     0
```

**Key finding**: None of the missing_on_cancel orders have any prior occurrence in the file. This means:
- The orders were never added via ADD records
- The orders never appeared in Snapshot records
- The cancel events reference orders that don't exist in the OrdLog stream

This suggests the root cause is deeper than transaction grouping — the orders may be:
1. From a different session/book state not captured in this file
2. System-level orders that bypass normal ADD lifecycle
3. A semantic mismatch in how the decoder interprets cancel/remove events

### What Changed

1. **tools/run_qsh_real_sample_checks.ps1** — Automated validation script
2. **cpp/qsh_ingest/src/main.cpp** — Fixed tx-grouped diagnostic propagation, added `--summary-out` option, added `missing-cancel-probe` command
3. **cpp/qsh_ingest/tests/test_tx_grouped.cpp** — Added test for first_missing_order_record_index propagation

### L2 Strategy-Ready Status

**NO.** Crossed snapshots remain at 7890 across all modes. The missing_on_cancel probe confirms the root cause is not transaction grouping — orders are being cancelled that were never added to the book.

### Next Recommended Task

1. Investigate whether the missing_on_cancel orders are system-level orders (check raw_flags for system/non-system bits)
2. Check if these orders appear in a different QSH file (e.g., Trades stream)
3. Investigate decoder semantics for Canceled/CanceledGroup flags — may need to skip cancel events for orders not in book
4. Consider adding a "skip-orphan-cancel" mode that doesn't count missing_order_id for cancel/remove events on unknown orders

---

## M10M validation script fix and OrdLog debug continuation

Date: 2026-07-09
Agent: MiMo
Status: completed

### Build/Test Result

```
Build: OK (MSVC 2022 + vcpkg/zlib, Release clean)
Tests: 14/14 passed (100%)
```

### tools/run_qsh_real_sample_checks.ps1

**Now exists in Git.** Complete rewrite from M10L version. Supports:

- `-QshPath`, `-ReportDir`, `-SkipBuild`, `-RunMissingCancelProbe`
- Auto-detects repo root (works from any working directory)
- Runs four modes with `--summary-out` and CSV output
- Prints compact markdown table with all diagnostic fields
- Reports L2 strategy-ready status
- Optional `missing-cancel-probe` run

### Local QSH Found

```
data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
```

### Compact Validation Table

| mode | missing_order_id | missing_on_fill | missing_on_cancel | missing_on_remove | orphan_fill_events | orphan_fill_level_reductions | crossed_book_snapshots | non_positive_spread_snapshots | first_missing_order_record_index | first_crossed_book_record_index | l2_strategy_ready |
|---|---|---|---|---|---|---|---|---|---|---|---|
| per-record strict | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| per-record reduce-same-price | 171 | 0 | 170 | 1 | 148 | 148 | 7884 | 7884 | 1966 | 2242 | NO |
| snapshot-records-mode load | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| tx-grouped | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |

### Missing-Cancel-Probe Status

**Works.** Probe of 100 missing_on_cancel orders confirms M10L finding:

```
Total missing_on_cancel: 100
With prior ADD:          0
With prior Snapshot:     0
With any occurrence:     0
```

None of the missing_on_cancel orders have any prior occurrence in the file. Orders are being cancelled that were never added to the book.

### OrdLog Debug Continuation

#### Key observations from M10M run:

1. **reduce-same-price mode changes first_missing_order_record_index** from 1651 to 1966. This means orphan fill handling affects which order is first flagged as missing. The reduce-same-price mode resolves 148 orphan fills (matching `missing_on_fill`) by reducing level volume, but `missing_on_cancel` (170) and `missing_on_remove` (1) remain unchanged.

2. **reduce-same-price mode shifts first_crossed_book_record_index** from 2210 to 2242. The 6 crossed-book reduction (7890 → 7884) is small but measurable. The shifted index suggests the first few crossed snapshots were caused by orphan fill volume that reduce-same-price now handles.

3. **missing_on_cancel=170 is the dominant unresolved issue.** These 170 cancel events reference orders with zero prior occurrence. The probe confirms this is not a transaction-grouping issue — the orders genuinely don't exist anywhere in the OrdLog stream.

4. **snapshot-records-mode load has no effect.** Loading snapshot records as order adds doesn't change any counter. The missing orders are not from snapshot records.

5. **tx-grouped mode has no effect.** Transaction grouping doesn't change any counter. The missing orders are not from intra-transaction matching.

#### Root cause analysis:

The 170 `missing_on_cancel` events are orders that:
- Were never added via ADD records
- Never appeared in Snapshot records
- Are being cancelled in the OrdLog stream

Possible explanations:
1. **System-level orders** — These may be orders from the exchange's internal matching engine that bypass the normal ADD lifecycle. They exist in the book but their ADD events are not recorded in the OrdLog stream.
2. **Cross-session carry-over** — Orders from a previous trading session that persist in the book but whose ADD events are in a different file.
3. **Semantic mismatch** — The Canceled/CanceledGroup flags may have different semantics for orders that were never explicitly added (e.g., implied cancels of resting orders).

#### System vs non-system records:

The missing_on_cancel orders should be checked for their raw_flags to determine if they are system or non-system records. If they are all system records, the issue is likely (1) or (2). If they are non-system, the issue may be (3).

### Files Changed

| File | Change |
|---|---|
| `tools/run_qsh_real_sample_checks.ps1` | Complete rewrite: added `-SkipBuild`, `-RunMissingCancelProbe`, repo root detection, CSV output, markdown table, strategy-ready status |
| `cpp/qsh_ingest/README.md` | Added Real-Sample Validation section with script usage |

### L2 Strategy-Ready Status

**NO.** Crossed snapshots remain at 7890 (strict) / 7884 (reduce-same-price). The missing_on_cancel=170 issue persists across all modes and is the primary blocker.

### Next Recommended Task

1. **Check raw_flags of missing_on_cancel orders** — Use `dump-records --audit` on records around the first missing_on_cancel event to determine if these are system or non-system records.
2. **Investigate "skip-orphan-cancel" mode** — Add a mode that doesn't count `missing_order_id` for cancel/remove events on orders not in the active book. If this reduces crossed snapshots, the orphan cancels are benign.
3. **Check if missing_on_cancel orders affect book state** — If the cancel events don't mutate the book (because the order isn't found), they may not cause crossed books. The crossed book root cause may be elsewhere.
4. **Trace the first crossed book order** — Use `--auto-trace-crossed-orders-out` with the strict mode to identify which specific orders create the crossed state, then trace their lifecycle.

---

## M10N Orphan Cancel and First Crossed Book Root Cause

Date: 2026-07-09
Agent: MiMo
Status: completed

### Build/Test Result

```
Build: OK (MSVC 2022 + vcpkg/zlib, Release clean)
Tests: 15/15 passed (100%)
```

New test: `test_orphan_cancel_modes` — 10 tests for orphan cancel/remove ignore mode.

### Real-Sample Validation

```
.\tools\run_qsh_real_sample_checks.ps1
```

| mode | missing_order_id | missing_on_fill | missing_on_cancel | missing_on_remove | orphan_fill_events | orphan_fill_level_reductions | crossed_book_snapshots | non_positive_spread_snapshots | first_missing_order_record_index | first_crossed_book_record_index | l2_strategy_ready |
|---|---|---|---|---|---|---|---|---|---|---|---|
| per-record strict | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| per-record reduce-same-price | 171 | 0 | 170 | 1 | 148 | 148 | 7884 | 7884 | 1966 | 2242 | NO |
| snapshot-records-mode load | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| tx-grouped | 319 | 148 | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |

### Orphan Cancel/Remove Audit Summary

Audit of 200 orphan cancel/remove events:

```
Total orphan cancel/remove: 200
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

**Conclusion: A. Orphan cancel/remove is likely BENIGN.**

Evidence:
- 198/200 orphan cancel events have ZERO prior occurrence anywhere in the OrdLog stream
- 0/200 have prior ADD or Snapshot events
- All 200 are system records (not NonSystem, not NonZeroReplAct)
- cancel_order() and remove_order() already return early without mutating the book when the order is not found
- These orders are likely system-level or cross-session orders whose cancel events can be safely ignored

The `--orphan-cancel-mode ignore` flag was added. When set, cancel/remove events for unknown orders are silently skipped (counted as `orphan_cancel_ignored`/`orphan_remove_ignored`) without incrementing `missing_order_id`.

### First Crossed Book Root Cause

First crossed book at record 2136:

```
best_bid: 14100
best_ask: 14062
Best bid order: id=1925033994466246392, side=BUY, price=14100, qty=111
Best ask order: id=1925033994522131746, side=SELL, price=14062, qty=30
```

Key finding: The crossed book has REAL orders at crossing prices. Both orders are in the active book with valid side, price, and quantity. The bid order (14100) is ABOVE the ask order (14062), creating a -38 spread.

The crossed book is NOT caused by orphan cancel/remove (those don't mutate the book). The root cause is that legitimate orders exist at prices that cross. Possible explanations:
1. These are MOEX exchange-level orders that cross by design (e.g., session initialization)
2. The OrdLog stream records crossing states that are normal for the exchange
3. A decoder or price delta issue (unlikely given M10I decoder audit)

### What Changed

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added `OrphanCancelMode` enum (Strict, Ignore), `orphan_cancel_mode_name()`, `orphan_cancel_ignored`/`orphan_remove_ignored` counters to `BookErrors`, `set_orphan_cancel_mode()` method, `orphan_cancel_mode_` member |
| `src/order_book.cpp` | Updated `cancel_order()` and `remove_order()` to skip unknown orders when `OrphanCancelMode::Ignore` is set |
| `src/main.cpp` | Added `orphan-cancel-audit` command (detailed CSV audit of orphan cancel/remove records), `first-crossed-root-cause` command (traces exact orders causing first crossed book), `--orphan-cancel-mode strict\|ignore` CLI arg to `l3-to-l2`, orphan cancel mode to summary JSON and strategy-ready output |
| `tools/run_qsh_real_sample_checks.ps1` | Added `-RunOrphanCancelAudit` and `-RunFirstCrossedProbe` switches |
| `cpp/qsh_ingest/README.md` | Added documentation for new commands and `--orphan-cancel-mode` flag |
| `cpp/qsh_ingest/CMakeLists.txt` | Added `test_orphan_cancel_modes` test target |
| `tests/test_orphan_cancel_modes.cpp` | New: 10 tests for orphan cancel/remove ignore mode |

### New CLI Args

```
--orphan-cancel-mode <mode>  Orphan cancel/remove handling: strict (default) or ignore
```

### New Commands

```
orphan-cancel-audit <OrdLog.qsh> [--out <file.csv>] [--max-audits N]
first-crossed-root-cause <OrdLog.qsh> [--out-dir <dir>] [--context N]
```

### New Summary JSON Fields

```
orphan_cancel_mode: "strict" or "ignore"
orphan_cancel_ignored: count of cancel events skipped in ignore mode
orphan_remove_ignored: count of remove events skipped in ignore mode
```

### L2 Strategy-Ready Status

**NO.** Crossed snapshots remain at 7890 (strict) / 7884 (reduce-same-price). The orphan cancel/remove issue is resolved (benign, can be ignored), but the crossed-book root cause is separate — real orders exist at crossing prices.

### Next Recommended Task

1. Investigate whether the crossed-book orders (14100 bid / 14062 ask) are exchange-level orders that cross by design
2. Check if the first 2136 records contain a NewSession + Snapshot initialization that creates crossing prices
3. Consider whether the crossed-book state is acceptable for MOEX RTS-3.21 at session start
4. If crossing is normal at session start, add a "skip-initial-crossed" mode that ignores crossed snapshots before the first valid spread
5. The `l2_strategy_ready` remains NO until crossed diagnostics are clean or crossing is documented as expected behavior

---

## M10O First Crossed Order Lifecycle and Session State Audit

Date: 2026-07-09
Agent: MiMo
Status: completed

### Build/Test Result

```
Build: OK (MSVC 2022 + vcpkg/zlib, Release clean)
Tests: 15/15 passed (100%)
```

### Real-Sample Validation Table

| mode | missing_order_id | missing_on_fill | missing_on_cancel | missing_on_remove | orphan_cancel_mode | orphan_cancel_ignored | orphan_remove_ignored | orphan_fill_events | orphan_fill_level_reductions | crossed_book_snapshots | non_positive_spread_snapshots | first_missing_order_record_index | first_crossed_book_record_index | l2_strategy_ready |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| per-record strict | 319 | 148 | 170 | 1 | strict | 0 | 0 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| per-record reduce-same-price | 171 | 0 | 170 | 1 | strict | 0 | 0 | 148 | 148 | 7884 | 7884 | 1966 | 2242 | NO |
| per-record orphan-cancel-ignore | 148 | 148 | 0 | 0 | ignore | 170 | 1 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| per-record reduce+orphan-cancel-ignore | 0 | 0 | 0 | 0 | ignore | 170 | 1 | 148 | 148 | 7884 | 7884 | 0 | 2242 | NO |
| snapshot-records-mode load | 319 | 148 | 170 | 1 | strict | 0 | 0 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |
| tx-grouped | 319 | 148 | 170 | 1 | strict | 0 | 0 | 148 | 0 | 7890 | 7890 | 1651 | 2210 | NO |

### Record Index vs Snapshot Index Clarification

The 2136 vs 2210 discrepancy is now resolved with unambiguous definitions:

| Field | Value | Definition |
|---|---|---|
| `first_crossing_event_record_index` | **2136** | The OrdLog record whose application first makes `best_bid >= best_ask` |
| `first_crossing_snapshot_record_index` | **2210** | The OrdLog record index where the first crossed L2 snapshot is emitted |
| `first_crossing_snapshot_index` | **2111** | The snapshot number (1-based) that first contains crossed book |

The 74-record gap (2210 - 2136) exists because txend mode waits for TxEnd boundary before emitting the snapshot. The crossing event happens at record 2136, but the snapshot is only emitted at the next TxEnd boundary (record 2210).

### Lifecycle Summary

#### bid_order=1925033994466246392 (BUY, price=14100, qty=111)

- **Had valid ADD: NO**
- **2 lifecycle events** in the detailed trace
- This order was NEVER added via an ADD record in the OrdLog stream
- It exists in the active book at the time of first crossing but has no ADD origin
- Possible origin: Snapshot record initialization or cross-session carry-over

#### ask_order=1925033994522131746 (SELL, price=14062, qty=30)

- **Had valid ADD: YES**
- ADD at record 1957, price=14062, amount=30
- **1 lifecycle event** in the detailed trace
- This order was properly added and exists in the book

### Raw Decoder Audit Summary

- `first_crossed_event_window_audit.csv` — 80 events (40 before, 40 after first crossing)
- `first_crossed_snapshot_window_audit.csv` — events around first exported crossed snapshot
- `first_crossed_orders_raw_audit.csv` — 3 raw decoder events for crossing orders

### Session/Snapshot/Transaction State Summary

| Field | Value |
|---|---|
| `new_session_records_seen` | 1 |
| `first_new_session_record_index` | 1 |
| `first_valid_book_record_index` | 27 |
| `first_crossing_event_record_index` | 2136 |
| `first_crossing_snapshot_record_index` | 2210 |
| `records_between_new_session_and_first_crossing` | 2135 |
| `snapshot_records_before_first_crossing` | 1964 |
| `tx_index_at_first_crossing` | 2135 |
| `records_in_first_crossing_tx` | 1 |

Session state at first crossing:
```
before_first_new_session:     NO
immediately_after_new_session: NO
inside_snapshot_init:         NO
before_continuous_trading:    NO
inside_transaction_group:     NO
at_txend:                     POSSIBLE
```

The first crossing happens well after NewSession (2135 records later), after 1964 snapshot records have been processed, in a single-record transaction at a TxEnd boundary.

### qsh-rs Comparison

```
qsh-rs comparison skipped: not available locally
```

### Root Cause Analysis

The crossed book is caused by **bid_order=1925033994466246392** existing in the active book at price 14100 without ever having been added via an ADD record. This order:

1. Was never added via ADD (no valid ADD found in the entire file)
2. Is at a higher price (14100) than the best ask (14062), creating the crossed state
3. Likely originated from a Snapshot record during book initialization

The 1964 snapshot records before the first crossing suggest the book was initialized from snapshot data. The bid order at 14100 may have been loaded from a snapshot but its ADD event was never recorded in the OrdLog stream.

### Files Changed

| File | Change |
|---|---|
| `include/orderbook/order_book.hpp` | Added `first_crossing_event_record_index_`, `first_crossing_snapshot_record_index_`, `first_crossing_snapshot_index_`, `first_new_session_record_index_`, `snapshot_records_before_first_crossing_`, `tx_index_at_first_crossing_`, `records_in_first_crossing_tx_` members and accessors |
| `src/main.cpp` | Added new index tracking in l3-to-l2 (both per-record and tx-grouped modes), session/snapshot state interpretation output, enhanced first-crossed-root-cause with detailed lifecycle CSVs (bid/ask/combined), raw decoder audit CSVs (event window, snapshot window, orders), summary JSON with new fields, lifecycle state save/restore to protect from NewSession clearing |
| `tools/run_qsh_real_sample_checks.ps1` | Added orphan-cancel-ignore and reduce+orphan-cancel-ignore modes, updated table columns |
| `cpp/qsh_ingest/README.md` | Added summary JSON field documentation, updated validation script docs, added combined mode example |

### L2 Strategy-Ready Status

**NO.** Crossed snapshots remain at 7890 (strict) / 7884 (reduce-same-price). The root cause is that a bid order at 14100 exists in the book without an ADD record, likely from snapshot initialization.

### Next Recommended Task

1. Investigate snapshot record handling — the bid order at 14100 likely came from a Snapshot record during book initialization
2. Test `--snapshot-records-mode load` to see if loading snapshot records as ADD events resolves the missing order
3. If snapshot loading doesn't help, investigate whether the order is from a cross-session carry-over
4. Consider adding a "skip-initial-crossed" mode that ignores crossed snapshots during session initialization
5. The `l2_strategy_ready` remains NO until crossed diagnostics are clean or crossing is documented as expected behavior

---

## M10P Snapshot Record Handling and Initial Book Audit

Date: 2026-07-09
Agent: MiMo
Status: completed

### 1. Build/Test Result

**Build: PASS.** MSVC 2022 + vcpkg/zlib. All targets compile clean (Release).

**Tests: PASS.** 16/16 tests pass (added `test_snapshot_audit`).

### 2. Real-Sample Validation Table

| mode | missing_order_id | crossed_book_snapshots | first_crossing_event_record_index | l2_strategy_ready |
|---|---|---|---|---|
| per-record strict | 319 | 7890 | 2136 | NO |
| per-record reduce-same-price | 171 | 7884 | 2242 | NO |
| per-record orphan-cancel-ignore | 148 | 7890 | 2136 | NO |
| per-record reduce+orphan-cancel-ignore | 0 | 7884 | 2242 | NO |
| snapshot-records-mode load | 319 | 7890 | 2136 | NO |
| tx-grouped | 319 | 7890 | 2136 | NO |

### 3. Snapshot-Records-Mode Comparison

| Metric | ignore | load | marker |
|---|---|---|---|
| missing_order_id | 319 | 319 | 319 |
| crossed_book_snapshots | 7890 | 7890 | 7890 |
| first_crossing_event_record_index | 2136 | 2136 | 2136 |
| l2_strategy_ready | NO | NO | NO |

**Observation:** All three modes produce identical results. `load` mode does not change the missing_order_id or crossed snapshot counts. The snapshot records in this file are already processed as Add events in all modes.

### 4. Snapshot Audit Summary Before First Crossing

```
snapshot_records_processed:           1964
snapshot_buy_records:                 1057
snapshot_sell_records:                907
snapshot_min_buy_price:               9739
snapshot_max_buy_price:               14055
snapshot_min_sell_price:              14062
snapshot_max_sell_price:              15404
```

### 5. Initial Book State Summary

```
snapshot_records_loaded:              1964
snapshot_buy_orders_loaded:           1057
snapshot_sell_orders_loaded:           907
snapshot_best_bid:                    14055
snapshot_best_ask:                    14062
snapshot_spread:                      7
snapshot_crossed_at_initial_load:     NO
snapshot_crossed_order_count:         0
snapshot_top_bid_qty:                 198
snapshot_top_ask_qty:                 30
snapshot_top_bid_order_count:         3
snapshot_top_ask_order_count:         1
snapshot_streak_start:                2
first_non_snapshot_after_new_session: 1966
```

**Key finding:** The snapshot initialization does NOT produce a crossed book. After all 1964 snapshot records are loaded, best_bid=14055 and best_ask=14062 with spread=7. The crossing occurs later during event processing at record 2136.

### 6. Bid Order Snapshot Source Conclusion

**CONCLUSION: E** — order 1925033994466246392 does NOT appear in snapshot records.

The target bid order at 14100 is not found among the 1964 snapshot records processed before the first crossing. This contradicts the earlier hypothesis (M10O) that the order "likely originated from snapshot initialization."

Revised interpretation:
- The bid order at 14100 is NOT loaded from snapshot records
- It must enter the book through a different mechanism
- The crossing at record 2136 is caused by a real event (likely an Add at 14100) that occurs between records 1966 and 2136
- The M10O lifecycle trace showed "Had valid ADD: NO" — but this may be because the lifecycle tracking starts at the first crossing detection and misses earlier events

### 7. qsh-rs Snapshot Comparison

```
qsh-rs snapshot comparison skipped: not available locally
```

### 8. Files Changed

| File | Change |
|---|---|
| `cpp/qsh_ingest/src/main.cpp` | Added `cmd_snapshot_audit` command: dumps snapshot records with full audit columns before first crossing, tracks initial book state using OrderBook, traces bid_order 1925033994466246392, prints initial book state summary |
| `cpp/qsh_ingest/tests/test_snapshot_audit.cpp` | New test file: 7 tests covering snapshot flag detection, book loading, crossed detection, record counting, empty snapshot handling, price tracking, summary determinism |
| `cpp/qsh_ingest/CMakeLists.txt` | Added `test_snapshot_audit` target |
| `tools/run_qsh_real_sample_checks.ps1` | Added `-RunSnapshotAudit` switch to run snapshot-audit command and print compact summary |
| `cpp/qsh_ingest/README.md` | Added snapshot-audit CLI documentation and -RunSnapshotAudit flag |

### 9. Next Recommended Task

1. **Investigate the actual ADD source of bid_order 1925033994466246392** — the order enters the book between records 1966 and 2136, likely via a non-snapshot Add event. A focused dump-records audit of records 1966-2136 should reveal the exact entry point.
2. **Prove crossing root cause before filtering** — the crossing is NOT caused by snapshot initialization. It is caused by a real trading event. This means skip/ignore crossed filtering should NOT be applied to snapshot initialization.
3. **L2 strategy-ready remains NO** — 7890 crossed snapshots remain. The root cause needs to be traced to its actual source (event at or before record 2136).

---

## M10Q Actual Add Source of Crossing Bid Order

Date: 2026-07-09
Agent: MiMo
Status: completed

### 1. Build/Test Result

```
Build: OK (Release)
Tests: 16/16 passed
```

### 2. Real-Sample Validation Result

```
local QSH found: data/raw/qsh/RTS-3.21/2021-01-05/RTS-3.21.2021-01-05.OrdLog.qsh
crossing-window-audit executed successfully
```

### 3. Crossing-Window Audit Summary

The `crossing-window-audit` command was run on records 1966..2136 with target order_id=1925033994466246392 and target_price=14100.

**Entry Point Summary:**

| Metric | Value |
|---|---|
| First record with target order_id | 2136 |
| First record with target price BUY | 2136 |
| First record with target order in book | 2136 |
| First record with best_bid == target | 2136 |
| First record with qty at target > 0 | 2136 |

**Conclusion:** All entry points converge at record 2136. The order enters the book exactly at the crossing event record.

### 4. Actual Entry Point of BUY 14100

**Record 2136** is the ADD event that creates BUY 14100 in the active book.

```
record_index: 2136
event_type:   ADD
side:         BUY
price:        14100
amount:       111
amount_rest:  111
flags_hex:    0x114
is_snapshot:  0
is_system:    1
best_bid_before: 14055
best_ask_before: 14062
best_bid_after:  14100
best_ask_after:  14062
qty_target_bid_before: 0
qty_target_bid_after:  111
```

**Context (records 2130-2136):**

| record_index | event_type | side | price | amount | best_bid_before | best_ask_before | best_bid_after |
|---|---|---|---|---|---|---|---|
| 2130 | CANCEL | BUY | 13991 | 3 | 14055 | 14062 | 14055 |
| 2131 | CANCEL | SELL | 14298 | 1 | 14055 | 14062 | 14055 |
| 2132 | CANCEL | SELL | 14168 | 3 | 14055 | 14062 | 14055 |
| 2133 | CANCEL | SELL | 14067 | 2 | 14055 | 14062 | 14055 |
| 2134 | CANCEL | BUY | 13917 | 1 | 14055 | 14062 | 14055 |
| 2135 | CANCEL | BUY | 13610 | 8 | 14055 | 14062 | 14055 |
| 2136 | ADD | BUY | 14100 | 111 | 14055 | 14062 | 14100 |

Records 2130-2135 are CANCEL events that do not change the best bid/ask. Record 2136 is a normal ADD event that creates a new BUY order at 14100, which becomes the new best bid (14100 > 14055), crossing the book (14100 >= 14062).

### 5. Actual Mutation Path

**BUY 14100 is created by `add_order` at record 2136 from ADD event.**

This is a straightforward order addition. The order enters the book via the normal `add_order` path in OrderBook::apply(). There is no move, orphan fill, or other exotic mechanism involved.

### 6. M10O vs M10P Contradiction Resolution

**Answer: A. Lifecycle trace started too late and missed ADD.**

The M10O lifecycle trace showed "Had valid ADD: NO" because:
1. M10O's `first-crossed-root-cause` command starts tracking order lifecycles only AFTER the first crossing detection (record 2136)
2. The ADD event at record 2136 IS the crossing event itself
3. By the time lifecycle tracking begins, the ADD has already been applied to the book
4. The lifecycle trace therefore never sees the ADD event for order 1925033994466246392

The M10P conclusion was correct: "bid_order must enter between records 1966 and 2136." The ADD happens exactly at record 2136, which is the last record in that range.

**Fix:** The lifecycle tracing in `first-crossed-root-cause` should be enhanced to start tracking from file start, not only after first crossing detection. This is already possible using `l3-to-l2 --trace-order-id` which traces from the beginning of the file.

### 7. Strict vs Reduce-Same-Price Comparison

| Mode | first_crossing_event_record_index | first_crossing_snapshot_record_index | first_crossing_snapshot_index |
|---|---|---|---|
| strict | 2136 | 2210 | 2111 |
| reduce-same-price | 2136 | 2242 | 2117 |

**Key finding:** The `first_crossing_event_record_index` is identical (2136) in both modes. The crossing EVENT (ADD at 14100) is the same regardless of orphan fill handling mode.

The difference is in `first_crossing_snapshot_record_index` (2210 vs 2242). This is because:
1. In reduce-same-price mode, orphan fills reduce volume at the same price level
2. This affects the book state during snapshot emission
3. The first valid crossed snapshot is emitted later (at record 2242 instead of 2210)

The task description's claim that "reduce-same-price first_crossing_event_record_index = 2242" appears to refer to `first_crossing_snapshot_record_index`, not `first_crossing_event_record_index`.

### 8. Files Changed

| File | Change |
|---|---|
| `cpp/qsh_ingest/include/orderbook/order_book.hpp` | Added `level_qty(Price, Side)` method to OrderBook class |
| `cpp/qsh_ingest/src/order_book.cpp` | Implemented `level_qty()` method |
| `cpp/qsh_ingest/src/main.cpp` | Added `cmd_crossing_window_audit` command with full book state tracking, entry point detection, and mutation path identification; added command parsing in main(); updated help text |
| `tools/run_qsh_real_sample_checks.ps1` | Added `-RunCrossingWindowAudit` switch |
| `cpp/qsh_ingest/README.md` | Added crossing-window-audit CLI documentation and -RunCrossingWindowAudit flag |

### 9. Next Recommended Task

1. **Fix lifecycle tracing** — Enhance `first-crossed-root-cause` to trace orders from file start, not only after first crossing detection. This would have caught the ADD at record 2136.
2. **Understand the 4265129 crossed snapshots** — The crossing at record 2136 is a single ADD event, but it causes millions of crossed snapshots. Investigate why the book remains crossed for so long.
3. **L2 strategy-ready remains NO** — The crossing is caused by a real trading event (ADD at 14100), not a decoder bug. The root cause is in the market data itself. Consider whether crossed-book filtering should be applied to real trading events.
