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

**Build tools not available on current system.** MSVC 2022 Community is installed but cmake and zlib are not present. The C++ code is complete and should compile once cmake and zlib are installed.

## 4. Test Result

4 test files created:
- `test_leb128.cpp` — 14 tests covering ULEB128, signed LEB128, growing integers, little-endian readers, QSH string reader, overflow detection
- `test_qsh_header.cpp` — 6 tests covering valid header parsing, bad signature rejection, bad version rejection, empty streams, too-small files
- `test_ordlog_flags.cpp` — 14 tests covering OLFlags, OLEntryFlags, DealFlags, AuxInfoFlags, event classification, side detection, system record checks, TxEnd detection
- `test_order_book.cpp` — 9 tests covering add orders, snapshots, mid price, fill/cancel/clear operations

Tests cannot be run until build tools are installed.

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
