# qsh_ingest — QScalp History Data Engine

C++20 parser and order book reconstruction engine for QSH v4 files.

## Purpose

Parse historical QSH/OrdLog/Quotes/Deals/AuxInfo market data files and produce:
- Normalized event streams (CSV)
- L2 order book snapshots
- Data Quality reports (JSON)
- Metadata for Trading Lab integration

## Safety

- No broker connection
- No live trading
- No real order sending
- Historical files only
- No secrets or API keys

## Requirements

- CMake 3.16+
- C++20 compiler (MSVC 2019+, GCC 10+, Clang 12+)
- zlib (for gzip decompression)

### Windows Setup

Install zlib via vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install zlib:x64-windows
```

Or install via system package manager.

## Build

```powershell
# Configure
cmake -S cpp/qsh_ingest -B build/qsh_ingest -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build/qsh_ingest --config Release
```

### Debug build with sanitizers

```powershell
cmake -S cpp/qsh_ingest -B build/qsh_ingest_debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build/qsh_ingest_debug --config Debug
```

### With vcpkg

```powershell
cmake -S cpp/qsh_ingest -B build/qsh_ingest -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/qsh_ingest
```

## CLI Commands

```bash
# Inspect QSH file header
qsh-ingest inspect <file.qsh>

# Scan records and print Data Quality counters
qsh-ingest quality <file.qsh>

# Export normalized metadata/events
qsh-ingest convert <file.qsh> --out <dir>

# Export decoded OrdLog records to CSV (diagnostic dump)
qsh-ingest dump-records <OrdLog.qsh> --dump-records-out <file.csv> [--dump-records-from N] [--dump-records-to N] [--audit]

# Analyze first missing order backward (root cause investigation)
qsh-ingest check-missing-order <OrdLog.qsh>

# Reconstruct L2 snapshots from OrdLog
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 20 --out <file.csv>

# Test on a small fragment (first 10k records, max 100 snapshots)
qsh-ingest l3-to-l2 <OrdLog.qsh> --max-records 10000 --max-snapshots 100 --out test_l2.csv

# Reconstruct with diagnostics (safe sample)
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --out l2_sample.csv --diagnostics-out l2_diagnostics.csv --max-diagnostics 100

# Reconstruct with crossed-book trace (root cause investigation)
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --out l2_sample.csv --diagnostics-out l2_diagnostics.csv --max-diagnostics 100 \
  --trace-crossed-out l2_trace.csv --max-trace-events 100

# Reconstruct with event-mode snapshots (after every record)
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --snapshot-mode event --out l2_event.csv

# Reconstruct with txend-mode snapshots (after TxEnd only, default)
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --snapshot-mode txend --out l2_txend.csv

# Trace lifecycle of a specific order
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 \
  --trace-order-id 12345 --trace-order-out order_lifecycle.csv

# First-crossed context trace (ring buffer of events around first crossed snapshot)
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --out l2_sample.csv --trace-crossed-context 20

# Trace best-level orders for crossed snapshots
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --snapshot-mode txend --out l2_txend.csv \
  --trace-best-level-orders-out l2_best_orders.csv

# Trace missing order IDs
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --snapshot-mode txend --out l2_txend.csv \
  --trace-missing-order-out l2_missing_orders.csv

# Auto-trace orders from first crossed snapshot
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --snapshot-mode txend --out l2_txend.csv \
  --auto-trace-crossed-orders-out l2_auto_trace.csv

# Full diagnostic command with all traces
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --max-records 100000 --max-snapshots 10000 \
  --snapshot-mode txend --out l2_txend.csv \
  --diagnostics-out l2_txend_diagnostics.csv --max-diagnostics 100 \
  --trace-crossed-out l2_txend_trace.csv --max-trace-events 100 \
  --trace-best-level-orders-out l2_best_orders.csv \
  --trace-missing-order-out l2_missing_orders.csv \
  --auto-trace-crossed-orders-out l2_auto_trace.csv

# Experimental: snapshot records mode (M10H)
# ignore: default behavior, snapshot records applied normally
# load: treat snapshot records as actual order adds (experimental)
# marker: treat snapshot records as markers only, skip apply (experimental)
qsh-ingest l3-to-l2 <OrdLog.qsh> --snapshot-records-mode load --out l2_snapshots.csv

# Dump records with raw decoder state (M10I audit)
# Adds columns: raw_data_offset, raw_entry_flags, raw_order_flags_hex,
# raw_side_bits, raw_event_bits, order_id/price/ts before/after delta,
# amount before/after, is_add_order_id_path
qsh-ingest dump-records <OrdLog.qsh> --dump-records-out audit.csv \
  --dump-records-from 1600 --dump-records-to 1670 --audit

# Experimental: orphan fill mode (M10J)
# strict: default, require order_id in active map, count missing_order_id
# ignore: skip orphan fills entirely, do not mutate book
# reduce-same-price: reduce volume at same price level without order_id lookup
# transaction-rest: use amount_rest to update most recent resting order (not yet implemented)
qsh-ingest l3-to-l2 <OrdLog.qsh> --orphan-fill-mode reduce-same-price --out l2_snapshots.csv

# Experimental: orphan cancel/remove mode (M10N)
# strict: default, count missing_order_id for cancel/remove of unknown order
# ignore: skip cancel/remove of unknown order without mutating book
qsh-ingest l3-to-l2 <OrdLog.qsh> --orphan-cancel-mode ignore --out l2_snapshots.csv

# Combine orphan fill and cancel modes (M10O)
qsh-ingest l3-to-l2 <OrdLog.qsh> --orphan-fill-mode reduce-same-price --orphan-cancel-mode ignore --out l2_snapshots.csv

# Combine all experimental modes (M10S)
qsh-ingest l3-to-l2 <OrdLog.qsh> --orphan-fill-mode reduce-same-price --orphan-cancel-mode ignore --counter-mode ignore-book --out l2_snapshots.csv

# Orphan cancel/remove audit (M10N)
qsh-ingest orphan-cancel-audit <OrdLog.qsh> --out orphan_cancel_audit.csv --max-audits 200

# Snapshot record audit (M10P)
# Dumps snapshot records before first crossing and summarizes initial book state.
# Reports: snapshot record count, buy/sell breakdown, price ranges, initial best bid/ask,
# spread, crossed status, and traces the target bid order 1925033994466246392.
qsh-ingest snapshot-audit <OrdLog.qsh> --out snapshot_audit_before_crossing.csv --max-records 10000

# Crossing window audit (M10Q)
# Dumps records in a specified range with full book state tracking.
# Used to find the exact record that creates a specific order or price level in the active book.
# Reports: best bid/ask before/after, target order active status, mutation path.
qsh-ingest crossing-window-audit <OrdLog.qsh> --from 1966 --to 2136 --out crossing_window_audit.csv
qsh-ingest crossing-window-audit <OrdLog.qsh> --from 1966 --to 2136 --out audit.csv --target-order-id 1925033994466246392 --target-price 14100

# Crossed-state persistence audit (M10R)
# Tracks every transition from a given record, monitors when crossing clears,
# and traces order/level lifecycle for bid_order and ask_order.
# Reports: first_crossing_record_index, first_uncross_record_index, crossed duration,
# order lifecycle (filled/canceled/removed), classification A/B/C/D/E.
qsh-ingest crossed-persistence-audit <OrdLog.qsh> --from 2136 --out crossed_persistence_audit.csv

# Counter flag audit (M10S)
# Counts all Counter-flagged (0x100) events and measures their book impact.
# Reports: counter_records_total, counter_add/fill/cancel/remove/move, counter_buy/sell,
# counter_snapshot/new_session/txend/non_system/cross_trade,
# first_counter_record_index, first_counter_add_record_index,
# counter_events_that_create_new_best_bid/ask, counter_events_that_create_crossed_book,
# counter_events_inside_crossed_state, counter_events_that_uncross_book.
qsh-ingest counter-flag-audit <OrdLog.qsh> --out counter_flag_audit.csv

# Experimental: counter mode (M10S)
# include: default, Counter events mutate book normally
# ignore-book: Counter events skipped for book mutation, counted separately
# NOTE: ignore-book is experimental until remaining crossed snapshots are classified (M10T)
qsh-ingest l3-to-l2 <OrdLog.qsh> --counter-mode ignore-book --out l2_snapshots.csv

# Experimental: non-system mode (M10V)
# include: default, NonSystem events mutate book normally
# ignore-book: NonSystem events decoded/counted but do not mutate visible book
# NOTE: ignore-book is experimental. Default should not be changed without owner approval.
qsh-ingest l3-to-l2 <OrdLog.qsh> --non-system-mode ignore-book --out l2_snapshots.csv

# Combine counter and non-system ignore-book modes (M10V)
qsh-ingest l3-to-l2 <OrdLog.qsh> --counter-mode ignore-book --non-system-mode ignore-book --out l2_snapshots.csv

# Experimental: quote mode (M10X)
# include: default, Quote events mutate book normally
# ignore-book: Quote events decoded/counted but do not mutate visible book
# NOTE: ignore-book is experimental. Default should not be changed without owner approval.
qsh-ingest l3-to-l2 <OrdLog.qsh> --quote-mode ignore-book --out l2_snapshots.csv

# Combine counter, non-system, and quote ignore-book modes (M10X)
qsh-ingest l3-to-l2 <OrdLog.qsh> --counter-mode ignore-book --non-system-mode ignore-book --quote-mode ignore-book --out l2_snapshots.csv

# NonSystem flag audit (M10V)
# Counts all NonSystem-flagged (0x200) events and measures their book impact.
# Reports: non_system_records, non_system_add/fill/cancel/remove/moved,
# non_system_buy/sell, non_system_counter/cross_trade/snapshot/new_session/txend,
# first_non_system_record_index, first_non_system_add_record_index,
# non_system_records_that_create_crossed_book, etc.
qsh-ingest non-system-flag-audit <OrdLog.qsh> --out non_system_flag_audit.csv

# Quote flag audit (M10X)
# Counts all Quote-flagged (0x0080) events and measures their book impact.
# Reports: quote_records, quote_add/fill/cancel/remove/moved,
# quote_buy/sell, quote_counter/non_system/cross_trade/snapshot/txend,
# first_quote_record_index, first_quote_add_record_index,
# quote_records_that_create_crossed_book, etc.
qsh-ingest quote-flag-audit <OrdLog.qsh> --out quote_flag_audit.csv

# Remaining crossed audit (M10T)
# Classifies remaining crossed snapshots after counter-ignore-book.
# Outputs CSV with full event flags, book state, and mutation path for each crossed event.
qsh-ingest remaining-crossed-audit <OrdLog.qsh> --counter-mode ignore-book --out remaining_crossed_after_counter_ignore.csv
qsh-ingest remaining-crossed-audit <OrdLog.qsh> --counter-mode ignore-book --out audit.csv --from 16190 --to 16220 --context 50

# First crossed-book root cause trace (M10N, enhanced M10O)
# Produces: first_crossed_root_cause.csv, first_crossed_best_orders.csv,
#           first_crossed_lifecycle.csv, first_crossed_bid_order_lifecycle.csv,
#           first_crossed_ask_order_lifecycle.csv, first_crossed_orders_lifecycle_combined.csv,
#           first_crossed_event_window_audit.csv, first_crossed_snapshot_window_audit.csv,
#           first_crossed_orders_raw_audit.csv
qsh-ingest first-crossed-root-cause <OrdLog.qsh> --out-dir data/reports/qsh --context 40

# Persistent crossed root cause (M10W)
# Finds the true not-crossed -> crossed transition under counter-ignore-book + non-system-ignore-book.
# Unlike first-crossed-root-cause, this command:
#   1. Uses counter-ignore-book + non-system-ignore-book modes
# 2. Detects the exact transition event (was_crossed=false -> is_crossed_after=true)
# 3. Traces lifecycle of top-of-book orders at the transition
# 4. Reports whether crossing orders are still active at end of file
# Outputs: transition info, context window, top-of-book orders, per-order lifecycle CSV.
qsh-ingest persistent-crossed-root-cause <OrdLog.qsh> --out persistent_crossed_root_cause.csv --context 100

# Experimental: book update mode (M10K)
# per-record: default, apply each record individually
# tx-grouped: group records by TxEnd, apply as transaction batch, emit snapshot after TxEnd
qsh-ingest l3-to-l2 <OrdLog.qsh> --book-update-mode tx-grouped --out l2_snapshots.csv

# Compare per-record vs tx-grouped modes
qsh-ingest l3-to-l2 <OrdLog.qsh> --book-update-mode per-record --max-records 100000 --max-snapshots 10000 --snapshot-mode txend --out l2_per_record.csv
qsh-ingest l3-to-l2 <OrdLog.qsh> --book-update-mode tx-grouped --max-records 100000 --max-snapshots 10000 --snapshot-mode txend --out l2_tx_grouped.csv
```

**L2 output is not strategy-ready until diagnostics are clean.**

During `l3-to-l2`, the engine checks each exported snapshot for:
- Crossed book: `best_bid >= best_ask`
- Empty bid side: `best_bid <= 0`
- Empty ask side: `best_ask <= 0`

Invalid snapshots are counted and optionally written to a diagnostics CSV with columns:
`ts, reason, best_bid, best_ask, spread, bid_qty_1, ask_qty_1, snapshots_written, records_processed`

If any invalid snapshots are found, a warning is printed:
```
WARNING: exported L2 contains invalid best bid / best ask state.
This L2 output is not strategy-ready until reconstruction diagnostics are clean.
```

## Strategy Readiness (M10U)

Each exported L2 snapshot is classified for strategy readiness. Crossed/locked/invalid snapshots are **not silently removed** — they are exported and clearly marked unsafe.

### What strategy_ready means

A snapshot is `strategy_ready=true` only when it is safe for strategy-level research:
- `best_bid > 0` and `best_ask > 0` (both sides exist)
- `best_bid < best_ask` (not crossed, not locked)
- Requested `depth > 0`

### Why crossed/locked rows are exported but marked unsafe

The core principle: **do not hide bad or transitional market states.** Crossed and locked book states are real market events that may be valuable for analysis. They are exported with `strategy_ready=false` and a machine-readable `strategy_reject_reason` so downstream code can filter, count, or study them.

### CSV columns added

Every L2 CSV row now includes:

| Column | Description |
|--------|-------------|
| `best_bid` | Best bid price |
| `best_ask` | Best ask price |
| `is_crossed` | `true` if `best_bid > best_ask` |
| `is_locked` | `true` if `best_bid == best_ask` and both sides exist |
| `strategy_ready` | `true` only if safe for strategy research |
| `strategy_reject_reason` | Machine-readable reason: `ok`, `crossed_book`, `locked_book`, `missing_best_bid`, `missing_best_ask`, `empty_book`, `invalid_price`, `invalid_depth` |
| `snapshot_index` | 1-based snapshot number |
| `record_index` | OrdLog record index at snapshot time |
| `tx_index` | Transaction index at snapshot time |

### How to run l3-to-l2 with summary output

```powershell
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 5 --summary-out summary.json --out l2.csv
```

### How to inspect reason counts

The summary JSON includes a `strategy_reject_reasons` object:

```json
{
  "snapshots_total": 10000,
  "snapshots_strategy_ready": 9093,
  "snapshots_not_strategy_ready": 907,
  "snapshots_crossed": 907,
  "snapshots_locked": 0,
  "l2_strategy_ready": false,
  "strategy_ready_ratio": 0.9093,
  "strategy_reject_reasons": {
    "ok": 9093,
    "crossed_book": 907,
    "locked_book": 0,
    "missing_best_bid": 0,
    "missing_best_ask": 0,
    "empty_book": 0,
    "invalid_price": 0,
    "invalid_depth": 0
  }
}
```

`l2_strategy_ready` is `true` only when `snapshots_not_strategy_ready == 0` and existing critical diagnostics are clean.

### Counter mode

`--counter-mode` default remains `include`. The `ignore-book` mode is experimental and requires owner approval to change as default.

### NonSystem mode (M10V)

`--non-system-mode` controls whether NonSystem-flagged (0x200) events mutate the visible order book.

- `include` (default): NonSystem events mutate the book normally
- `ignore-book`: NonSystem events are decoded and counted but do not mutate the visible book

**How it differs from `--counter-mode`**: Counter mode handles events with the Counter flag (0x100). NonSystem mode handles events with the NonSystem flag (0x200). They are independent — a record can have both flags, in which case Counter takes precedence (checked first).

**M10V findings**: In the RTS-3.21 2021-01-05 sample, only 8 NonSystem records exist among 5.3M total records. None of them create crossed book states. The `ignore-book` mode does NOT reduce the remaining 907 crossed snapshots. NonSystem is not the main remaining cause of crossed book states.

Default should not be changed without owner approval.

### Quote mode (M10X)

`--quote-mode` controls whether Quote-flagged (0x0080) events mutate the visible order book.

- `include` (default): Quote events mutate the book normally
- `ignore-book`: Quote events are decoded and counted but do not mutate the visible book

**How it differs from `--counter-mode` and `--non-system-mode`**: Counter mode handles Counter flag (0x100) events. NonSystem mode handles NonSystem flag (0x200) events. Quote mode handles Quote flag (0x0080) events. They are independent — a record can have multiple flags, and the precedence order is Counter > NonSystem > Quote (checked in that order).

**Flag label correction**: `0x94` = `Add + Buy + Quote` (NOT `Add + Buy + TxEnd`). The official QSH v4 specification maps bit 7 (0x0080) to Quote and bit 10 (0x0400) to TxEnd/EndOfTransaction. Previous reports that labeled `0x94` as containing TxEnd were incorrect.

**M10X purpose**: Investigate whether Quote-flagged records should mutate the normal visible L2 order book. Quote records may represent synthetic top-of-book quote updates rather than real L3 order mutations.

Default should not be changed without owner approval.

## Summary JSON Output

The `--summary-out <file.json>` option writes a machine-readable summary with these fields:

- `book_update_mode`: per-record or tx-grouped
- `snapshot_mode`: event or txend
- `snapshot_records_mode`: ignore, load, or marker
- `fill_semantics`: delta or rest
- `orphan_fill_mode`: strict, ignore, reduce-same-price, or transaction-rest
- `orphan_cancel_mode`: strict or ignore
- `orphan_cancel_ignored`: count of ignored orphan cancels
- `counter_mode`: include or ignore-book
- `counter_records_seen`: count of Counter-flagged records
- `counter_records_ignored_for_book`: count of Counter records skipped for book mutation
- `counter_add_ignored_for_book`: count of Counter ADD records skipped
- `counter_fill_ignored_for_book`: count of Counter FILL records skipped
- `counter_cancel_ignored_for_book`: count of Counter CANCEL records skipped
- `counter_remove_ignored_for_book`: count of Counter REMOVE records skipped
- `counter_move_ignored_for_book`: count of Counter MOVE records skipped
- `quote_mode`: include or ignore-book
- `quote_records_seen`: count of Quote-flagged records
- `quote_records_ignored_for_book`: count of Quote records skipped for book mutation
- `quote_add`: count of Quote ADD records
- `quote_fill`: count of Quote FILL records
- `quote_cancel`: count of Quote CANCEL records
- `quote_remove`: count of Quote REMOVE records
- `quote_moved`: count of Quote MOVED records
- `quote_counter`: count of Quote records also flagged Counter
- `quote_non_system`: count of Quote records also flagged NonSystem
- `quote_cross_trade`: count of Quote records also flagged CrossTrade
- `quote_snapshot`: count of Quote records also flagged Snapshot
- `quote_txend`: count of Quote records also flagged TxEnd
- `quote_records_that_create_crossed_book`: count of Quote records that cause crossed book
- `first_quote_record_index`: record index of first Quote event
- `first_quote_add_record_index`: record index of first Quote ADD event
- `first_quote_crossing_record_index`: record index of first Quote event that creates crossed book
- `orphan_remove_ignored`: count of ignored orphan removes
- `records_processed`: total records processed
- `transactions_seen`: total transactions seen
- `snapshots_written`: total L2 snapshots emitted
- `missing_order_id`: count of missing order ID errors
- `missing_on_fill`, `missing_on_cancel`, `missing_on_remove`, `missing_on_move`: breakdown by event type
- `orphan_fill_events`: count of orphan fill events
- `orphan_fill_level_reductions`: count of reduce-same-price level reductions
- `crossed_book_snapshots`: count of crossed book snapshots
- `non_positive_spread_snapshots`: count of non-positive spread snapshots
- `first_missing_order_record_index`: record index of first missing order ID error
- `first_crossed_book_record_index`: record index of first crossed book snapshot (legacy)
- `first_crossing_event_record_index`: the OrdLog record whose application first makes best_bid >= best_ask
- `first_crossing_snapshot_record_index`: the OrdLog record index where the first crossed L2 snapshot is emitted
- `first_crossing_snapshot_index`: the snapshot number (1-based) that first contains crossed book
- `first_new_session_record_index`: record index of first NewSession event
- `first_valid_book_record_index`: record index of first valid book state (has bid and ask)
- `snapshot_records_before_first_crossing`: count of snapshot records before first crossing
- `tx_index_at_first_crossing`: transaction index at first crossing
- `records_in_first_crossing_tx`: records in the transaction where first crossing occurs
- `records_between_new_session_and_first_crossing`: difference between first crossing and first NewSession
- `l2_strategy_ready`: true if no invalid snapshots found

### M10U Strategy readiness fields

- `snapshots_total`: total snapshots exported
- `snapshots_strategy_ready`: count of snapshots where `strategy_ready=true`
- `snapshots_not_strategy_ready`: count of snapshots where `strategy_ready=false`
- `snapshots_crossed`: count of crossed book snapshots (`best_bid > best_ask`)
- `snapshots_locked`: count of locked book snapshots (`best_bid == best_ask`)
- `snapshots_missing_best_bid`: count with no bid levels
- `snapshots_missing_best_ask`: count with no ask levels
- `snapshots_empty_book`: count with neither side
- `snapshots_invalid_price`: count with invalid price on a side
- `snapshots_invalid_depth`: count with invalid depth
- `strategy_ready_ratio`: `snapshots_strategy_ready / snapshots_total`
- `strategy_reject_reasons`: object with counts per reason (`ok`, `crossed_book`, `locked_book`, `missing_best_bid`, `missing_best_ask`, `empty_book`, `invalid_price`, `invalid_depth`)

## Real-Sample Validation

Run all validation modes against the local QSH sample:

```powershell
.\tools\run_qsh_real_sample_checks.ps1
```

Options:

```powershell
.\tools\run_qsh_real_sample_checks.ps1 -QshPath "..." -ReportDir "..." -SkipBuild -RunMissingCancelProbe -RunOrphanCancelAudit -RunFirstCrossedProbe -RunSnapshotAudit -RunCrossingWindowAudit -RunCrossedPersistenceAudit -RunCounterFlagAudit -RunRemainingCrossedAudit -RunStrategyReadyExport
```

The script:
- Builds qsh_ingest and runs tests (unless `-SkipBuild`)
- Runs eight `l3-to-l2` modes with `--summary-out`:
  1. per-record strict
  2. per-record reduce-same-price
  3. per-record orphan-cancel-ignore
  4. per-record reduce+orphan-cancel-ignore
  5. snapshot-records-mode load
  6. tx-grouped
  7. per-record counter-ignore-book
  8. reduce+orphan-cancel-ignore+counter-ignore-book
- Prints a compact comparison table
- Reports L2 strategy-ready status
- Optionally probes `missing_on_cancel` orders
- Optionally runs orphan cancel/remove audit
- Optionally runs first crossed-book root cause trace
- Optionally runs crossing window audit for records 1966..2136
- Optionally runs counter flag audit (M10S)

Raw QSH must stay under `data/raw/qsh/...`. Generated reports stay under `data/reports/qsh/...`. Both are ignored by Git.

**L2 strategy-ready must remain NO until crossed-book diagnostics are clean.**

## Tests

```powershell
cd build/qsh_ingest
ctest --output-on-failure
```

## Architecture

```
cpp/qsh_ingest/
  include/
    qsh/              QSH format primitives
    orderbook/        L3 book and L2 snapshots
    quality/          Data Quality reports
  src/                Implementation
  tests/              Unit tests
```

## QSH Format Notes

- QSH v4 files are gzip-compressed binary streams
- Uses LEB128/ULEB128 variable-length integer encoding
- OrdLog stream provides L3 order-level data (add/fill/cancel/remove)
- Delta encoding: most fields are incremental from previous record
- TxEnd flag marks transaction boundaries
- .NET ticks timestamp (100ns since 0001-01-01)

## Reference

Implementation informed by [qsh-rs](https://github.com/2dav/qsh-rs) (Apache-2.0).
No code copied verbatim. Format logic reimplemented in C++20.
