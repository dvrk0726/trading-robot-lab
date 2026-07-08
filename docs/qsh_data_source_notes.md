# QSH Data Source Notes

Status: research note for Trading Lab data layer.

## Current conclusion

QScalp QSH files are a promising historical market data source for this project because they can contain market microstructure streams rather than only candle bars.

The project should treat QSH as a real candidate for the first HFT data pipeline.

## Uploaded sample files reviewed in the working discussion

Real data samples provided by the owner:

```text
RIZ6.2026-07-07.Quotes.qsh
RIZ6.2026-07-07.Deals.qsh
RIZ6.2026-07-07.AuxInfo.qsh
ALRS.2026-07-07.AuxInfo.qsh
```

These files are not committed to git. Raw market data should be stored outside git or under ignored raw-data folders.

## Useful streams

Priority for HFT research:

```text
1. OrdLog.qsh  - best if available, full order log / order events
2. Quotes.qsh  - order book quote updates
3. Deals.qsh   - trade ticks / prints
4. AuxInfo.qsh - auxiliary information
```

## QSH reader/reference material provided by owner

The owner also provided example archives:

```text
QscExample.zip
qsh_example.zip
qsh2qsh.zip
```

Observed purpose:

```text
QscExample.zip - example QScalp connector / data model reference
qsh_example.zip - QSH reader/writer reference implementation in C#
qsh2qsh.zip    - contains qsh2qsh.exe converter utility
```

## qsh2qsh.zip inspection

Archive contents:

```text
qsh2qsh.exe
```

Basic file properties from local inspection:

```text
file type: Windows PE32 console executable, Intel i386, Mono/.NET assembly
sha256: 66c4f487a62bd230adb8fa4b989178df08004210663e316760db1b14df4c1fd1
product/version strings: QScalp / qsh2qsh.exe / version 2.4.6870.23378
copyright strings: Nikolay Moroshkin / 2011-2018
```

Visible usage strings:

```text
QScalp History Data QSHx to QSH4 Converter
Usage: qsh2qsh [options] input_file [output_file]
-e Extract streams from input file.
-c Compress output file(s).
```

Security note: do not run unknown executables inside the project workflow until the owner explicitly decides to trust the source. Do not commit this EXE to git.

## Storage decision draft

Use two-layer storage:

```text
raw data        - original QSH files, never modified
normalized data - parsed internal format for Lab/replay/backtest
```

Suggested local layout:

```text
data/raw/qsh/<instrument>/<date>/
data/normalized/qsh/<instrument>/<date>/
```

The repository should commit only:

```text
parser code
small synthetic samples if needed
docs
schemas
tests
```

The repository should not commit:

```text
large real QSH market data
unknown EXE tools
broker credentials
live trading configs
```

## Next technical step

Create a dedicated task for QSH data research and parser prototype:

```text
M8_QSH_SPEC_AND_PARSER
```

Expected scope:

```text
1. Document QSH streams and normalized schema.
2. Implement minimal Python parser prototype for Deals, Quotes, AuxInfo.
3. Keep raw QSH files out of git.
4. Produce Data Quality output from sample QSH files.
5. Do not implement broker/live trading.
```

## Open questions

```text
1. Can we find matching OrdLog.qsh for RIZ6 on 2026-07-07?
2. What is the license status of qsh_example C# code and qsh2qsh.exe?
3. Should the first parser be pure Python, or should QSH be converted to TXT first for prototyping?
4. What storage should be used after SQLite becomes too small: DuckDB, Parquet, or both?
```
