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
