# MOEX Source Version Check

Date: 2026-07-09
Status: version alignment partially resolved

## Purpose

This document tracks MOEX public source versions so the project does not accidentally implement against stale protocol files.

This is not a protocol summary and not a copy of MOEX files.

## Checked directories

### FAST SPECTRA test root

Source:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/
```

Observed contents from directory listing:

```text
backup/                  modified 2026-01-26
FAST_8.6/                modified 2025-06-20
FAST_9.0/                modified 2025-09-09
fast_sensor.zip          modified 2022-11-01, size 4,999,380
meta.info                modified 2026-04-10, size 746
spectra_fastgate_en.pdf  modified 2026-04-10, size 556,004
spectra_fastgate_ru.pdf  modified 2026-04-10, size 588,929
templatesT0/             modified 2021-09-24
templatesT1/             modified 2024-04-22
```

Key interpretation:

```text
The Russian/English FAST PDF and root meta.info are newer than templatesT0.
FAST_9.0 is newer than templatesT0.
Therefore, before implementing a decoder, compare the public T0 templates with FAST_9.0/templates.xml and the current PDF.
```

### FAST_9.0 directory

Source:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/FAST_9.0/
```

Observed contents from directory listing:

```text
Spectra_fastgate-1.29.0.0.1188.zip  modified 2025-09-09, size 907,483
templates.xml                       modified 2025-09-09, size 28,575
```

## Owner-uploaded FAST_9.0 package check

Uploaded files checked:

```text
Spectra_fastgate-1.29.0.0.1188.zip
fast_9_templates.xml
spectra_fastgate_ru(1).pdf
```

### ZIP package contents

The uploaded `Spectra_fastgate-1.29.0.0.1188.zip` contains:

```text
Spectra_fastgate/spectra_fastgate_ru.pdf
Spectra_fastgate/spectra_fastgate_en.pdf
Spectra_fastgate/meta.info
```

The ZIP package `meta.info` says:

```text
Package build version: 1.29.0.0.1188
Package build date: 2025-09-04 03-19-59
Jobname: docs.Fast_git
```

Interpretation:

```text
This ZIP is a FAST documentation package, not the market-data runtime feed itself.
It is useful for protocol documentation/version control.
Do not commit ZIP/PDF to Git.
```

### FAST_9.0 templates vs uploaded templatesT0/templates.xml

Result:

```text
fast_9_templates.xml is byte-identical to the previously uploaded templates.xml.
Both files have the same size and SHA-256.
```

SHA-256 observed locally:

```text
dbd50f1e0becc2b2ebd9dac8e4c6609ba1538566811b610cde9b6dd3e7f66a8e
```

Interpretation:

```text
The template mismatch risk between uploaded templatesT0/templates.xml and FAST_9.0/templates.xml is resolved for this file set.
The collector can use the previously extracted template IDs for FAST_9.0/T0 skeleton design.
```

Still important:

```text
A future production collector should load templates.xml dynamically and verify template file hash/version at startup.
Do not hardcode binary layouts without a version check.
```

## Important FAST 1.29.0 document notes

The uploaded `spectra_fastgate_ru(1).pdf` is:

```text
FAST protocol specification
Version 1.29.0
Document date: 2025-09-04
```

Important changes from the document history:

### 1. TradingSessionStatus template changed to id 46

```text
TradingSessionStatus msg id changed from 41 to 46.
Removed:
- TradSesIntermClearingStartTime
- TradSesIntermClearingEndTime
Added:
- SettlSessBegin
- ClrSessBegin
```

Implementation consequence:

```text
Collector/session calendar logic must use template id 46 and must not expect the removed intermediate clearing fields.
```

### 2. SecurityGroupStatus template id 45

```text
SecurityTradingStatus type changed from int32 to uInt32 in SecurityGroupStatus.
SecurityGroupStatus template id changed from 39 to 45.
```

Implementation consequence:

```text
Status decoder must use unsigned status type and template id 45 for the current FAST_9.0/T0 templates.
```

### 3. SecurityDefinition template id 40 and new instrument metadata fields

```text
SecurityDefinition id changed from 38 to 40.
New fields include:
- TradeModeID
- GroupMask
- SectionID
- BaseContractID
- TradePeriodAccess
```

Implementation consequence:

```text
Instrument metadata model must reserve fields for trade mode, group mask, section, base contract and trade-period access.
```

### 4. SecurityDefinition Flags 0x80000 meaning changed

```text
SecurityDefinition Flags 0x80000 was redefined.
New meaning: execution by schedule or execution in clearing session.
0 = clearing session
1 = by schedule
```

Implementation consequence:

```text
Do not reuse older meaning for SecurityDefinition Flags 0x80000.
Version this enum.
```

### 5. MDEntryType additions/changes

FAST 1.29.0 documents changes for Market Data Snapshot/Incremental MDEntryType:

```text
u = funding rate when contract price deviates from underlying beyond configured level
z = dividend adjustment for one-day futures with auto-rollover on index/equity
U = indicative funding rate
Z = current dividend adjustment
m = current market price
```

Implementation consequence:

```text
Decoder should not assume MDEntryType is only bid/ask/trade/book volume.
Unknown MDEntryType must be logged, not silently dropped.
```

### 6. OrdersLogMessage / BookMessage stable IDs

Current relevant template IDs remain:

```text
OrdersLogMessage id=29
BookMessage id=30
DefaultIncrementalRefreshMessage id=31
DefaultSnapshotMessage id=32
SecurityDefinition id=40
SecurityGroupStatus id=45
TradingSessionStatus id=46
```

Implementation consequence:

```text
These IDs match the uploaded FAST_9.0 templates and can be used for skeleton routing.
```

### 7. MDFlags / MDFlags2 are version-sensitive

Important historical changes documented in the PDF:

```text
MDFlags2 was added as an extension bitmask.
MDFlags 0x2000 = passive cross-order removal flag.
MDFlags 0x1000000000000000 = Book-or-cancel / Passive-only order/trade flag.
MDFlags 0x4000000000000000 = discrete-auction order/trade flag.
MDFlags 0x200000000000000 = passive synthetic order flag.
MDFlags 0x80000000 = address order/trade with unique-code matching flag.
```

Implementation consequence:

```text
FAST MDFlags must be a separate enum from historical QSH OrdLog flags.
FAST MDFlags must be versioned and include int64/uint64-safe handling.
```

## Active order-book construction rule

From the FAST specification section on active order-book assembly:

```text
For Incremental OrdersLogMessage, process only orders/trades where MDFlags 0x4 is NOT set.
For Snapshot BookMessage, process only orders where MDFlags 0x4 is NOT set.
```

Project interpretation:

```text
MDFlags 0x4 marks non-system / off-book order or trade in this context.
Default visible-book reconstruction from FAST ORDERS-LOG must ignore records with MDFlags 0x4.
```

## ORDERS-LOG vs FO-BOOK-N comparison rule

The FAST specification warns that a client-built book from ORDERS-LOG can differ from FO-BOOK-N aggregate feeds.

Reason:

```text
FO-BOOK-N may include synthetic liquidity.
ORDERS-LOG client must calculate synthetic liquidity itself if exact comparison is required.
```

Project interpretation:

```text
Use FO-BOOK-N as a sanity check, not as exact equality proof, until synthetic liquidity handling is implemented.
```

## TCP Recovery / Historical Replay limits

For `ORDERS-LOG` TCP Recovery / Historical Replay, important limits include:

```text
maximum active TCP connections per market/instance/IP: 2
maximum TCP connections per day per market/instance/IP: 1000
maximum FAST messages in one request: 1000
Market Data Request timeout after Logon: 1 second
```

Implementation consequence:

```text
Recovery must be conservative.
Do not reconnect in a tight loop.
Do not request more than 1000 messages at once.
Send Market Data Request immediately after TCP Logon.
Log and rate-limit every recovery attempt.
```

## Already parsed sources

### templatesT0/configuration.xml

Status:

```text
parsed from owner-uploaded file
```

Important result:

```text
T0 configuration contains ORDERS-LOG with Incremental A/B, Snapshot A/B and Historical Replay TCP.
```

Related summary:

```text
docs/moex/fast_spectra_t0_templates_notes.md
```

### templatesT0/templates.xml

Status:

```text
parsed from owner-uploaded file
```

Important result:

```text
OrdersLogMessage id=29 and BookMessage id=30 are present.
```

Related summary:

```text
docs/moex/fast_spectra_t0_templates_notes.md
```

### spectra_fastgate_ru.pdf / spectra_fastgate_ru(1).pdf

Status:

```text
first-pass summarized and version-aligned for FAST_9.0 package
```

Related summary:

```text
docs/moex/fast_spectra_notes.md
```

### spectra_fixgate_ru.pdf

Status:

```text
first-pass summarized
```

Related summary:

```text
docs/moex/fix_spectra_notes.md
```

## Not yet parsed / optional later

```text
FAST root meta.info
FIX docs meta.info
templatesT1/configuration.xml
templatesT1/templates.xml
```

## Current implementation rule

```text
The template mismatch between uploaded T0 templates and FAST_9.0/templates.xml is resolved for this file set.
Real decoder implementation is allowed only if it remains template/version-aware and starts with dry-run/synthetic tests.
```

Allowed now:

```text
- continue documentation;
- draft architecture;
- prepare local dry-run config shape;
- create synthetic test messages;
- design FAST collector skeleton;
- implement template loading/inspection before binary decode.
```

Still blocked:

```text
- real network connection without MOEX test access approval;
- production market-data connection without license/agreement;
- any real order sending;
- committing raw captures, PDFs, ZIPs, XML templates or credentials without explicit approval.
```

## Practical conclusion

The project has enough information to confirm the path:

```text
SPECTRA test access -> FAST ORDERS-LOG collector -> normalized data -> paper trading later
```

The next practical implementation should be:

```text
FAST template/config inspector in dry-run mode
```

Not yet:

```text
real multicast network collector
real trading
production order-entry
```
