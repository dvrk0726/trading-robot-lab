# ADR-0003: C++ QSH / OrdLog Data Engine

Date: 2026-07-08
Status: accepted

## Context

The project is moving from general Trading Lab scaffolding to the first real market microstructure data layer.

Available historical QSH data includes at least one full `OrdLog.qsh` sample:

```text
RTS-3.21.2021-01-05.OrdLog.qsh
```

The QScalp archive also contains sources with different depth of market data:

```text
Zerich / Finam / Techcap  -> historical OrdLog when available
scalping.pro             -> Quotes.qsh, Deals.qsh, AuxInfo.qsh
```

`OrdLog.qsh` is not equivalent to `Quotes.qsh + Deals.qsh + AuxInfo.qsh`.

- `OrdLog.qsh` is L3/order-log level data: add, fill, cancel, move/remove, order id, transaction boundaries.
- `Quotes.qsh` is L2/book quote updates or reconstructed book state.
- `Deals.qsh` is trade prints.
- `AuxInfo.qsh` is auxiliary market/session information.

For HFT and microstructure replay, `OrdLog.qsh` is the preferred engineering source because it allows L3 order book reconstruction and later queue/fill model experiments.

However, the currently available OrdLog archive appears old, with last available records around 2021-01-05 for some sources. Therefore, these files are accepted as engineering samples, not proof of current strategy profitability.

## Decision

Use **C++20** for the low-level QSH/OrdLog data engine.

Use **Python** for the research and laboratory layer.

```text
C++20:
- QSH binary reading
- OrdLog parser
- Quotes / Deals / AuxInfo parser
- TxEnd transaction grouping
- L3 order book reconstruction
- L3 -> L2 snapshots/events
- Data Quality counters
- normalized export for Python

Python:
- Trading Lab dashboard
- research scripts
- lead-lag analysis
- charts
- reports
- strategy experiments
- backtest reports
```

This replaces the earlier default assumption that the first QSH ingest layer could be Python-first or Rust-first.

Rust `qsh-rs` remains useful as an external reference implementation, but the project will not depend on it as the primary implementation.

## Rationale

The owner explicitly wants to build C++ experience now because future real-time order log processing, replay, risk, and potential execution/runtime code will likely require a fast systems language.

C++20 is selected over C because:

- it is fast enough for large QSH/OrdLog files;
- it is closer to a future low-latency runtime core;
- it supports RAII and safer resource management than raw C;
- it gives access to strong types, standard containers, tests, sanitizers, and modern build tooling;
- it avoids C-style manual memory handling unless there is a measured reason.

## Scope

The first C++ component must be narrow:

```text
cpp/qsh_ingest
```

It must read historical files only and produce normalized data/quality reports.

It must not contain:

- broker connection;
- real order sending;
- live trading mode;
- API keys;
- account IDs;
- secrets;
- raw real QSH committed to GitHub;
- `.env` files;
- unknown external EXE/DLL binaries.

## Target architecture

```text
Raw QSH files outside git
  ↓
C++ qsh_ingest CLI
  ↓
Normalized events + Data Quality report
  ↓
Python Trading Lab
  ↓
Research / charts / replay / backtest reports
```

## Required C++ modules

```text
cpp/qsh_ingest/
  CMakeLists.txt
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

## CLI commands

The first CLI should support:

```bash
qsh-ingest inspect <file.qsh>
qsh-ingest quality <file.qsh>
qsh-ingest convert <file.qsh> --out <dir>
qsh-ingest l3-to-l2 <OrdLog.qsh> --depth 20 --out <file.csv>
```

Initial output can be CSV/JSON for simplicity. Parquet/Arrow/DuckDB can be added later after the parser and reconstruction are verified.

## Important HFT assumptions

Historical `OrdLog.qsh` from 2021 is accepted for engineering validation:

- parser development;
- transaction grouping;
- L3 book reconstruction;
- replay mechanics;
- queue/fill model prototype;
- Data Quality page.

It must not be treated as:

- current market evidence;
- proof of profitability;
- final execution quality estimate;
- live-trading validation.

Modern strategy validation must later use current data, current instrument specifications, current weights, current commissions, and current market regime.

## User-facing result

Trading Lab should eventually show:

- imported QSH files;
- data quality status;
- historical order book replay;
- bid/ask depth;
- mid price;
- spread;
- trades;
- event stream;
- strategy replay decisions;
- simulated order/fill result;
- PnL and diagnostics.

No user-facing live trading controls should be added at this stage.

## Consequences

Positive:

- builds C++ skill and future runtime foundation;
- avoids slow Python parsing of large binary streams;
- creates reusable low-level market data engine;
- keeps Trading Lab flexible in Python;
- separates historical data engineering from strategy research.

Negative:

- slower initial development than Python-only;
- more build/test discipline required;
- MiMo must use small steps and produce detailed reports;
- C++ mistakes can corrupt parsing silently unless Data Quality and tests are strict.

## Implementation rule

MiMo and any future coding agent must implement this incrementally:

1. C++ project skeleton builds.
2. Header/signature reader works.
3. Stream type detection works.
4. LEB128/Growing integer readers are tested.
5. First 100 OrdLog records can be decoded.
6. Whole file can be scanned with counters.
7. TxEnd grouping works.
8. L3 order book reconstruction works on a limited segment.
9. L2 snapshots are exported.
10. Data Quality report is visible in Trading Lab.

No broker, no live mode, no real order sending.
