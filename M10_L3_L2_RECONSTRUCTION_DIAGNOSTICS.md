# M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS

Owner decision date: 2026-07-08
Primary agent: MiMo
Status: ready for implementation

## How to run

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10_L3_L2_RECONSTRUCTION_DIAGNOSTICS.md"
```

After MiMo finishes:

```powershell
.\tools\mimo_save.ps1 "Add L3 L2 reconstruction diagnostics"
```

Then verify locally:

```powershell
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

## Mission

Improve diagnostics for `cpp/qsh_ingest` command `l3-to-l2`.

The first M9 C++ QSH / OrdLog pipeline works:

- C++ CLI builds.
- Tests pass.
- Real historical `OrdLog.qsh` header is read.
- Full quality scan reads 5,362,594 records with 0 rejected records.
- Limited `l3-to-l2` export creates 10,000 L2 snapshots.

But exported L2 can currently contain invalid book state where:

```text
best_bid >= best_ask
```

Observed example:

```text
best_bid = 15266
best_ask = 14062
spread = -1204
```

This must be treated as a reconstruction diagnostics problem. Do not dismiss it as low liquidity. Low liquidity can create wide/empty books, but best bid above best ask means the exported L2 is not strategy-ready.

## Read first

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
cpp/qsh_ingest/README.md
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/src/order_book.cpp
cpp/qsh_ingest/src/ordlog_reader.cpp
cpp/qsh_ingest/include/orderbook/order_book.hpp
cpp/qsh_ingest/include/orderbook/l2_snapshot.hpp
cpp/qsh_ingest/include/quality/data_quality.hpp
```

## Required implementation

### 1. Detect invalid exported L2 snapshots

During `l3-to-l2`, check exported best bid and best ask:

```text
best_bid > 0
best_ask > 0
best_bid >= best_ask
```

Add counters:

```text
l2_crossed_book_snapshots
l2_non_positive_spread_snapshots
l2_empty_bid_snapshots
l2_empty_ask_snapshots
```

### 2. Add diagnostics CSV

Add optional CLI args:

```text
--diagnostics-out <file.csv>
--max-diagnostics N
```

Diagnostics CSV should contain first bad snapshots with at least:

```text
ts,reason,best_bid,best_ask,spread,bid_qty_1,ask_qty_1,snapshots_written,records_processed
```

### 3. Print clear warning

If non-positive spread or crossed-book snapshots are found, print:

```text
WARNING: exported L2 contains invalid best bid / best ask state.
This L2 output is not strategy-ready until reconstruction diagnostics are clean.
```

### 4. Tests

Add small synthetic tests for:

- crossed-book detection;
- non-positive spread detection;
- empty bid/ask detection if implemented.

Do not add real QSH files to git.

### 5. README

Update `cpp/qsh_ingest/README.md` with safe command example:

```powershell
.\build\qsh_ingest\Release\qsh_ingest.exe l3-to-l2 .\data\raw\qsh\RTS-3.21\2021-01-05\RTS-3.21.2021-01-05.OrdLog.qsh --depth 5 --max-records 100000 --max-snapshots 10000 --out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_sample.csv --diagnostics-out .\data\reports\qsh\RTS-3.21\2021-01-05\l2_depth5_diagnostics.csv --max-diagnostics 100
```

Mark clearly:

```text
L2 output is not strategy-ready until diagnostics are clean.
```

## Safety rules

Do not add:

- broker connection;
- live trading;
- real order sending;
- raw QSH files;
- generated CSV/JSON reports;
- EXE/DLL binaries;
- `.env` files;
- secrets or tokens.

Do not claim profitability from historical data.

## Done criteria

```text
1. qsh_ingest builds.
2. Existing tests pass.
3. New diagnostics tests pass.
4. l3-to-l2 reports invalid exported L2 snapshots.
5. diagnostics CSV can be generated.
6. README has safe command.
7. MiMo report is updated.
```

## Report update

Update:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Add section:

```text
M10 diagnostics update
```

Include changed files, commands run, build/test result, sample diagnostic command, observed counters, and remaining limitations.
