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

## QSH specification

The owner provided the official `qsh.pdf` specification for QSH version 4.

Key format points:

```text
QSH stores historical market data required for reconstructing trading in real-time scale.
Data is stored as binary frames.
Supported streams include Quotes, Deals, OwnOrders, OwnTrades, Messages, AuxInfo, OrdLog.
QSH files may be gzip-compressed.
```

Important parser details:

```text
DateTime uses .NET ticks.
GrowDateTime uses growing millisecond timestamps.
Numbers may use LEB128 / ULEB128 variable-length encoding.
Quotes stream contains full book in the first frame and later only changes.
Quote volume is positive for ask, negative for bid, zero means remove level.
Deals stream uses bit flags and carries only changed fields.
AuxInfo stream also uses bit flags and carries only changed fields.
OrdLog stream represents FORTS order log data and is the best source for order-level replay if available.
```

## QSH reader/reference material provided by owner

The owner also provided example archives:

```text
QscExample.zip
qsh_example.zip
qsh2qsh.zip
qsh2txt.zip
txt2qsh.zip
```

Observed purpose:

```text
QscExample.zip - example QScalp connector / data model reference
qsh_example.zip - QSH reader/writer reference implementation in C#
qsh2qsh.zip    - contains qsh2qsh.exe converter utility
qsh2txt.zip    - contains qsh2txt.exe text extractor utility
txt2qsh.zip    - contains txt2qsh.exe text-to-QSH converter utility
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

## qsh2txt.zip inspection

Archive contents:

```text
qsh2txt.exe
```

Basic file properties from local inspection:

```text
file type: Windows PE32 console executable, Intel i386, Mono/.NET assembly
sha256: b516bd227141948c208f02977d267597f703cc2a154d17a717da134fdc88e7a7
visible title: QScalp History Data Text Extractor
visible usage: qsh2txt input_qsh_file
```

Likely use:

```text
Convert or extract QSH frames to text for quick inspection.
Useful for validating our future Python parser output against an official-style converter.
Do not depend on it as the main long-term pipeline.
```

## txt2qsh.zip inspection

Archive contents:

```text
txt2qsh.exe
```

Basic file properties from local inspection:

```text
file type: Windows PE32 GUI executable, Intel i386, Mono/.NET assembly
sha256: 936beba1862ffe0b53f6d69fedc7c3cccb902e889e9e673e643d0e9d781e3a6b
visible title/strings: txt2qsh, QScalp History Data, QScalp (*.qsh)
```

Likely use:

```text
Convert text representation back into QSH.
Useful mostly for tests or synthetic QSH creation, not required for the first parser.
```

Security note: do not run unknown executables inside the project workflow until the owner explicitly decides to trust the source. Do not commit EXE tools to git.

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
5. Optionally compare parser output with qsh2txt output if the owner chooses to run the utility locally.
6. Do not implement broker/live trading.
```

## Open questions

```text
1. Can we find matching OrdLog.qsh for RIZ6 on 2026-07-07?
2. What is the license status of qsh_example C# code and QScalp converter utilities?
3. Should the first parser be pure Python, or should QSH be converted to TXT first for prototyping?
4. What storage should be used after SQLite becomes too small: DuckDB, Parquet, or both?
```
