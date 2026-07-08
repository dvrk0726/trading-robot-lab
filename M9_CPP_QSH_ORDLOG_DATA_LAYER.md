# M9_CPP_QSH_ORDLOG_DATA_LAYER

Owner decision date: 2026-07-08
Primary agent: MiMo
Status: ready for implementation

## Mission

Build the first C++20 market microstructure data engine for Trading Lab.

This task starts the real historical data layer:

```text
QSH / OrdLog / Quotes / Deals / AuxInfo
  -> C++ parser / normalizer
  -> L3 order book reconstruction
  -> L2 snapshots / event stream
  -> Data Quality report
  -> Python Trading Lab visualization and research
```

This task is not about live trading.

## Read first

MiMo must read these files before coding:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/qsh_data_source_notes.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
apps/lab/backend/README.md
apps/lab/backend/seed_demo_db.py
apps/lab/backend/lab_dashboard.py
apps/lab/frontend/static/lab.css
```

External reference to study, not blindly vendor:

```text
https://github.com/2dav/qsh-rs
```

Use it as a practical reference for:

- QSH v4 header/signature;
- gzip/deflate reading assumptions;
- LEB128 / signed LEB128 / growing integer primitives;
- stream type ids;
- OrdLog entry flags;
- OrdLog order flags;
- TxEnd transaction grouping;
- system/non-system filtering;
- L3 -> L2 reconstruction logic.

Do not copy large code blocks blindly. If implementation logic is reused, document attribution and license note.

## Data context

The owner has an engineering sample:

```text
RTS-3.21.2021-01-05.OrdLog.qsh
```

This is a historical engineering sample, not current trading evidence.

Important rule:

```text
2021 OrdLog = engineering sample, not proof of current profitability.
```

Use historical OrdLog for:

- parser development;
- L3 book reconstruction;
- replay mechanics;
- queue/fill model prototype;
- Data Quality checks;
- historical order book visualization.

Do not use it to claim that a strategy works now.

## Key distinction

`OrdLog.qsh` is not equivalent to `Quotes.qsh + Deals.qsh + AuxInfo.qsh`.

```text
OrdLog.qsh:
  L3 / full order-log level data.
  Contains add/fill/cancel/move/remove-like events, order_id, tx boundaries.
  Best for queue/fill/order book reconstruction.

Quotes.qsh:
  L2 quote/book stream.
  Good for spread/mid/depth, but not full queue reconstruction.

Deals.qsh:
  trade prints.

AuxInfo.qsh:
  auxiliary market/session info.
```

The engine must eventually support both modes:

```text
Mode A: L3 mode from OrdLog.qsh
Mode B: L2 mode from Quotes.qsh + Deals.qsh + AuxInfo.qsh
```

But first priority is `OrdLog.qsh`.

## Language decision

Use C++20 for the low-level data engine.

Do not use C as the main implementation language.

Use modern C++:

```text
RAII
std::vector
std::string
std::optional
std::variant when useful
std::filesystem
enum class
small structs with explicit types
clear error handling
```

Avoid:

```text
raw malloc/free unless justified
unsafe global state
unbounded buffers
silent parser failures
large monolithic files
broker/live code
```

## Target directory

Create:

```text
cpp/qsh_ingest/
  CMakeLists.txt
  README.md
  include/
    qsh/
      qsh_types.hpp
      qsh_header.hpp
      leb128.hpp
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
    data_quality.cpp
  tests/
    test_leb128.cpp
    test_qsh_header.cpp
    test_ordlog_flags.cpp
    test_order_book.cpp
```

Add only safe source code and synthetic/tiny test vectors.

Do not commit real QSH files.

## First CLI

Implement a CLI binary called:

```text
qsh-ingest
```

Required commands:

```bash
qsh-ingest inspect <file.qsh>
qsh-ingest quality <file.qsh>
qsh-ingest convert <file.qsh> --out <dir>
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 20 --out <file.csv>
```

Minimum acceptable first version:

```text
inspect:
  reads QSH signature/header
  prints version, stream type, instrument, recorder, comment, recording time

quality:
  scans records and prints counters

convert:
  exports normalized CSV/JSON metadata for first supported stream

l3-to-l2:
  reconstructs top N L2 snapshots from OrdLog and writes CSV
```

## Step-by-step implementation order

Do not jump directly to full reconstruction.

### Step 1 — C++ skeleton

- create `cpp/qsh_ingest`;
- create CMake project;
- build a CLI that prints help;
- add README with build/run commands;
- add debug build instructions with sanitizers.

### Step 2 — safe file reader and header reader

- implement QSH signature check;
- implement version check;
- implement stream type detection;
- implement instrument/recorder/comment/recording_time read;
- fail clearly if unsupported stream count/version/type.

### Step 3 — numeric primitives

Implement and test:

- unsigned LEB128;
- signed LEB128;
- growing integer;
- little-endian integer readers;
- QSH string reader.

### Step 4 — OrdLog first records

- implement OrdLog structs/enums;
- decode first 100 records from OrdLog;
- print compact sample lines;
- do not reconstruct book yet.

### Step 5 — full OrdLog scan

Add counters:

```text
records_total
records_parsed
records_failed
first_timestamp
last_timestamp
add_count
fill_count
cancel_count
remove_count
new_session_count
tx_end_count
buy_count
sell_count
unknown_side_count
non_system_count
non_zero_repl_act_count
snapshot_count
moved_count
cross_trade_count
```

### Step 6 — TxEnd transaction grouping

- group records by TxEnd;
- count transactions;
- report max transaction size;
- report malformed transaction count.

### Step 7 — L3 order book reconstruction

Implement:

```text
Add
Fill
Cancel
Remove
NewSession clear
```

Track errors:

```text
missing_order_id
level_not_found
amount_mismatch
negative_level_volume
crossed_book_after_update
invalid_side
```

### Step 8 — L2 snapshots

Generate CSV:

```text
ts,bid_price_1,bid_qty_1,ask_price_1,ask_qty_1,...,bid_price_N,bid_qty_N,ask_price_N,ask_qty_N,mid,spread
```

Depth should be configurable.

### Step 9 — Data Quality output

Write:

```text
data_quality.json
metadata.json
```

Include:

```text
source_file_name
source_file_sha256
stream_type
instrument
recording_time
records_total
records_valid
records_rejected
timestamp_min
timestamp_max
warnings
errors
book_reconstruction_errors
```

### Step 10 — Trading Lab integration

Update Python dashboard to show:

- imported file list;
- stream type;
- record counts;
- Data Quality status;
- warnings/errors;
- historical book availability;
- link/section for order book replay placeholder.

Do not build a full animated UI unless the parser/data layer is stable.

## User-facing Trading Lab target

Eventually the user should see:

```text
Data Sources
Data Quality
Historical Order Book
Price / Mid / Spread chart
Trades
Event stream
Strategy Replay
Simulated orders/fills
PnL diagnostics
```

For this task, minimum UI result:

```text
Data Quality section can display C++ generated metadata and quality reports.
```

## Safety constraints

Hard rules:

```text
NO broker connection
NO live trading
NO real order sending
NO real API keys
NO .env files
NO real raw QSH committed
NO EXE/DLL/binary tools committed
NO live_approved=true
NO profitability claims
```

The C++ CLI must only process local historical files supplied by the owner.

## Build requirements

Use CMake.

Add debug build guidance:

```bash
cmake -S cpp/qsh_ingest -B build/qsh_ingest -DCMAKE_BUILD_TYPE=Debug
cmake --build build/qsh_ingest
```

Add optional sanitizer flags for local development:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

Do not require paid dependencies.

Keep dependencies minimal. If a dependency is needed for gzip/deflate reading, document it and keep it cross-platform.

## Report requirement

After implementation, MiMo must create a report:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

Report must include:

```text
1. What files were changed.
2. What commands were run.
3. Build result.
4. Test result.
5. What QSH features are supported.
6. What is not supported yet.
7. Data Quality output example.
8. Any parser assumptions.
9. Any errors/warnings seen on the sample file.
10. Next recommended task.
```

## Done criteria

This task is complete only when:

```text
1. C++ CLI builds successfully.
2. `qsh-ingest inspect` reads QSH header.
3. OrdLog stream type is detected.
4. Numeric primitives have tests.
5. OrdLog records can be scanned.
6. Basic Data Quality counters are produced.
7. L3 -> L2 reconstruction exists at least for a limited safe segment.
8. Output files are written outside git-ignored data directories.
9. Python Trading Lab can show generated quality metadata.
10. MiMo report is committed.
```

If full L3 -> L2 is too large for one pass, stop after safe partial implementation and document exactly what works and what does not.
