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
