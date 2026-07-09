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

# First crossed-book root cause trace (M10N, enhanced M10O)
# Produces: first_crossed_root_cause.csv, first_crossed_best_orders.csv,
#           first_crossed_lifecycle.csv, first_crossed_bid_order_lifecycle.csv,
#           first_crossed_ask_order_lifecycle.csv, first_crossed_orders_lifecycle_combined.csv,
#           first_crossed_event_window_audit.csv, first_crossed_snapshot_window_audit.csv,
#           first_crossed_orders_raw_audit.csv
qsh-ingest first-crossed-root-cause <OrdLog.qsh> --out-dir data/reports/qsh --context 40

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

## Summary JSON Output

The `--summary-out <file.json>` option writes a machine-readable summary with these fields:

- `book_update_mode`: per-record or tx-grouped
- `snapshot_mode`: event or txend
- `snapshot_records_mode`: ignore, load, or marker
- `fill_semantics`: delta or rest
- `orphan_fill_mode`: strict, ignore, reduce-same-price, or transaction-rest
- `orphan_cancel_mode`: strict or ignore
- `orphan_cancel_ignored`: count of ignored orphan cancels
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

## Real-Sample Validation

Run all validation modes against the local QSH sample:

```powershell
.\tools\run_qsh_real_sample_checks.ps1
```

Options:

```powershell
.\tools\run_qsh_real_sample_checks.ps1 -QshPath "..." -ReportDir "..." -SkipBuild -RunMissingCancelProbe -RunOrphanCancelAudit -RunFirstCrossedProbe -RunSnapshotAudit -RunCrossingWindowAudit -RunCrossedPersistenceAudit
```

The script:
- Builds qsh_ingest and runs tests (unless `-SkipBuild`)
- Runs six `l3-to-l2` modes with `--summary-out`:
  1. per-record strict
  2. per-record reduce-same-price
  3. per-record orphan-cancel-ignore
  4. per-record reduce+orphan-cancel-ignore
  5. snapshot-records-mode load
  6. tx-grouped
- Prints a compact comparison table
- Reports L2 strategy-ready status
- Optionally probes `missing_on_cancel` orders
- Optionally runs orphan cancel/remove audit
- Optionally runs first crossed-book root cause trace
- Optionally runs crossing window audit for records 1966..2136

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
