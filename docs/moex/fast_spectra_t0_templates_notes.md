# FAST SPECTRA T0 Configuration and Templates Notes

Date: 2026-07-09
Status: first-pass summary
Sources:

```text
configuration.xml uploaded from https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/configuration.xml
templates.xml uploaded from https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/templates.xml
```

This document is a derived summary only.

Do not commit the raw XML files unless explicitly approved.

## Main conclusion

T0 test configuration contains the `ORDERS-LOG` feed.

This confirms that the project can prepare a test FAST collector around the same logical feed type needed for full order-log style reconstruction.

Important: this still does not mean we have network access or credentials. It only means the public T0 configuration defines the feed, addresses, ports and templates.

## Configuration root

```text
configuration type: Test
label: Test System
marketId: MOEX
```

## Feed groups in T0 configuration

The uploaded T0 `configuration.xml` defines these market data groups:

```text
FUT-INFO     Futures definition
INDEX        Indexes
NEWS         News feed
OPT-INFO     Options definition
ORDERS-LOG   Full orders log
FO-TRADES    Derivative trades
FO-BOOK-1    Top of book
FO-BOOK-5    Top 5 price levels
FO-BOOK-20   Top 20 price levels
FO-BOOK-50   Top 50 price levels
```

Project priority:

```text
1. FUT-INFO / OPT-INFO  instrument metadata
2. ORDERS-LOG           full anonymous order log
3. FO-BOOK-5/20/50      aggregate book sanity checks
4. FO-TRADES            trades/statistics
5. INDEX                index data if needed for RI/synthetic-index research
```

## ORDERS-LOG T0 channels

`ORDERS-LOG` is labelled `Full orders log` and belongs to marketID `D`.

### Incremental feed A

```text
type: Incremental
protocol: UDP/IP
source IP: 91.203.253.242
multicast group: 239.195.12.40
port: 48040
feed: A
```

### Incremental feed B

```text
type: Incremental
protocol: UDP/IP
source IP: 91.203.255.242
multicast group: 239.195.140.40
port: 49040
feed: B
```

### Snapshot feed A

```text
type: Snapshot
protocol: UDP/IP
source IP: 91.203.253.242
multicast group: 239.195.12.41
port: 48041
feed: A
```

### Snapshot feed B

```text
type: Snapshot
protocol: UDP/IP
source IP: 91.203.255.242
multicast group: 239.195.140.41
port: 49041
feed: B
```

### Historical Replay / TCP recovery

```text
type: Historical Replay
protocol: TCP/IP
IPs: 91.203.253.242, 91.203.255.242
port: 8022
```

Implementation consequence:

```text
The future collector should support both A and B incremental feeds, both A and B snapshot feeds, and TCP recovery/replay on port 8022 for ORDERS-LOG.
```

## Instrument definition feeds

### FUT-INFO

Futures definition feed:

```text
Instrument Replay A:       239.195.12.11:48011, source 91.203.253.242
Instrument Replay B:       239.195.140.11:49011, source 91.203.255.242
Instrument Incremental A:  239.195.12.12:48012, source 91.203.253.242
Instrument Incremental B:  239.195.140.12:49012, source 91.203.255.242
```

### OPT-INFO

Options definition feed:

```text
Instrument Replay A:       239.195.12.27:48027, source 91.203.253.242
Instrument Replay B:       239.195.140.27:49027, source 91.203.255.242
Instrument Incremental A:  239.195.12.28:48028, source 91.203.253.242
Instrument Incremental B:  239.195.140.28:49028, source 91.203.255.242
```

Implementation consequence:

```text
Before processing ORDERS-LOG by SecurityID, the collector needs instrument metadata from FUT-INFO and/or OPT-INFO.
```

## Aggregate book feeds for sanity checks

These are not the main L3 reconstruction feed, but are useful for validation/comparison.

```text
FO-BOOK-1:
  Incremental A 239.195.12.67:48067
  Incremental B 239.195.140.67:49067
  Snapshot A    239.195.12.68:48068
  Snapshot B    239.195.140.68:49068

FO-BOOK-5:
  Incremental A 239.195.12.69:48069
  Incremental B 239.195.140.69:49069
  Snapshot A    239.195.12.70:48070
  Snapshot B    239.195.140.70:49070

FO-BOOK-20:
  Incremental A 239.195.12.71:48071
  Incremental B 239.195.140.71:49071
  Snapshot A    239.195.12.72:48072
  Snapshot B    239.195.140.72:49072

FO-BOOK-50:
  Incremental A 239.195.12.73:48073
  Incremental B 239.195.140.73:49073
  Snapshot A    239.195.12.74:48074
  Snapshot B    239.195.140.74:49074
```

## Trade feed

`FO-TRADES` derivative trades:

```text
Incremental A: 239.195.12.65:48065
Incremental B: 239.195.140.65:49065
Snapshot A:    239.195.12.66:48066
Snapshot B:    239.195.140.66:49066
TCP replay:    91.203.253.242 / 91.203.255.242 port 8027
```

## Template ids in uploaded templates.xml

The uploaded T0 `templates.xml` defines these templates:

```text
31    DefaultIncrementalRefreshMessage
32    DefaultSnapshotMessage
40    SecurityDefinition
4     SecurityDefinitionUpdateReport
5     SecurityStatus
37    SecurityMassStatus
45    SecurityGroupStatus
6     Heartbeat
7     SequenceReset
46    TradingSessionStatus
42    DiscreteAuction
9     News
29    OrdersLogMessage
30    BookMessage
33    OtcMonitorIncrementalRefreshMessage
34    OtcMonitorSnapshotMessage
35    OtcMonitorSecurityDefinition
1000  Logon
1001  Logout
```

Project priority:

```text
40 SecurityDefinition
5  SecurityStatus
37 SecurityMassStatus
46 TradingSessionStatus
29 OrdersLogMessage
30 BookMessage
6  Heartbeat
7  SequenceReset
31 DefaultIncrementalRefreshMessage
32 DefaultSnapshotMessage
```

## ORDERS-LOG templates

### OrdersLogMessage, template id 29

Message type:

```text
MessageType = X
```

Top-level fields:

```text
ApplVerID       tag 1128 constant 9
MessageType     tag 35 constant X
SenderCompID    tag 49 constant MOEX
MsgSeqNum       tag 34
SendingTime     tag 52
LastFragment    tag 893
MDEntries       sequence, length NoMDEntries tag 268
```

MDEntries fields:

```text
MDUpdateAction           tag 279
MDEntryType              tag 269
MDEntryID                tag 278 optional
SecurityID               tag 48 optional
SecurityIDSource         tag 22 constant 8
RptSeq                   tag 83 optional
MDEntryDate              tag 272 optional
MDEntryTime              tag 273
MDEntryPx                tag 270 optional
MDEntrySize              tag 271 optional
LastPx                   tag 31 optional
LastQty                  tag 32 optional
TradeID                  tag 1003 optional
ExchangeTradingSessionID tag 5842 optional
MDFlags                  tag 20017 optional
MDFlags2                 tag 20050 optional
Revision                 tag 20018 optional
```

### BookMessage, template id 30

Message type:

```text
MessageType = W
```

Top-level fields:

```text
ApplVerID                tag 1128 constant 9
MessageType              tag 35 constant W
SenderCompID             tag 49 constant MOEX
MsgSeqNum                tag 34
SendingTime              tag 52
LastMsgSeqNumProcessed   tag 369
RptSeq                   tag 83 optional
LastFragment             tag 893
RouteFirst               tag 7944
ExchangeTradingSessionID tag 5842
SecurityID               tag 48 optional
SecurityIDSource         tag 22 constant 8
MDEntries                sequence, length NoMDEntries tag 268
```

MDEntries fields:

```text
MDEntryType     tag 269
MDEntryID       tag 278 optional
MDEntryDate     tag 272 optional
MDEntryTime     tag 273
MDEntryPx       tag 270 optional
MDEntrySize     tag 271 optional
TradeID         tag 1003 optional
MDFlags         tag 20017 optional
MDFlags2        tag 20050 optional
```

## Key implementation consequences

### 1. ORDERS-LOG exists on T0

The public T0 config contains `ORDERS-LOG` with incremental, snapshot and TCP replay connections.

This is the most important finding.

### 2. Collector should not start from all feeds

Minimum useful MVP feed set:

```text
FUT-INFO Instrument Replay/Incremental
ORDERS-LOG Incremental A/B
ORDERS-LOG Snapshot A/B
ORDERS-LOG Historical Replay TCP
```

Optional validation feeds:

```text
FO-BOOK-5 or FO-BOOK-20
FO-TRADES
```

### 3. Message decoder must be template-driven

The collector should load `templates.xml` and decode by template id.

Hardcoding only one binary layout is risky.

### 4. Separate FAST contracts from QSH contracts

FAST ORDERS-LOG has:

```text
MDUpdateAction
MDEntryType
MDFlags / MDFlags2
SecurityID
RptSeq
MsgSeqNum
```

Historical QSH OrdLog has a different record/flag layout.

Keep separate enums and parsers.

### 5. Recovery is part of the first design

Because configuration includes TCP replay for ORDERS-LOG, the collector design must include sequence-gap detection and replay request capability from the start, even if first MVP only logs gaps.

## Minimum future dry-run config shape

A future local config example should derive from public T0 data but still avoid credentials and local private settings:

```yaml
environment: test
market: spectra
fast:
  t0:
    instrument_replay:
      futures:
        a: { group: "239.195.12.11", port: 48011, source: "91.203.253.242" }
        b: { group: "239.195.140.11", port: 49011, source: "91.203.255.242" }
    orders_log:
      incremental:
        a: { group: "239.195.12.40", port: 48040, source: "91.203.253.242" }
        b: { group: "239.195.140.40", port: 49040, source: "91.203.255.242" }
      snapshot:
        a: { group: "239.195.12.41", port: 48041, source: "91.203.253.242" }
        b: { group: "239.195.140.41", port: 49041, source: "91.203.255.242" }
      historical_replay:
        hosts: ["91.203.253.242", "91.203.255.242"]
        port: 8022
```

Do not put credentials in this config.

## Open questions

```text
1. Does test ORDERS-LOG carry realistic enough activity for collector testing?
2. Does internet access require MOEX-issued login/network permission or multicast routing setup?
3. Are the public T0 addresses reachable only after MOEX test access approval?
4. Should first MVP listen to feed A only, or immediately support A/B deduplication?
5. Which FAST decoder library or internal decoder should be used?
6. Should raw UDP packets be stored as pcap first, or decoded directly into normalized JSONL/Parquet?
```

## Recommended next source

```text
https://ftp.moex.com/pub/FIX/Spectra/test/docs/spectra_fixgate_ru.pdf
```

Read this only after the FAST source notes are accepted, because FIX is for order-entry test flow, not the market-data collector.
