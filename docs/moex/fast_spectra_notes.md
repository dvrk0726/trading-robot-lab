# FAST SPECTRA Notes

Date: 2026-07-09
Status: first-pass summary
Source: https://ftp.moex.com/pub/FAST/Spectra/test/spectra_fastgate_ru.pdf
Source version: FAST protocol specification 1.30.2, 2026-04-10

## Purpose

This document extracts project-relevant implementation notes from the MOEX SPECTRA FAST Gate Russian specification.

It is not a full copy of the manual.

Use the official PDF as the source of truth when implementing protocol details.

## Scope for this project

FAST SPECTRA is used for market data, not order entry.

Project use:

```text
FAST -> realtime market data collector -> normalized market events -> replay/paper/live-data research
```

Not project use:

```text
FAST -> real order sending
```

Order entry belongs to FIX/TWIME later and must remain behind Runtime + RiskGate + owner gate.

## Important source structure

The FAST specification covers:

```text
1. Market Data Multicast interaction scenarios
2. UDP multicast channels and feeds
3. UDP/TCP recovery
4. Public FIX-style message definitions used inside FAST feeds
5. Anonymous orders and trades stream / ORDERS-LOG
6. Synthetic matching notes
7. TCP Recovery limitations
```

## Connection/bootstrap sequence

The spec defines the client startup process for full market data:

```text
1. Download channel/feed configuration XML from the FTP server.
2. Download FAST templates XML from the FTP server.
3. Listen to Instruments Incremental and buffer messages.
4. Listen to Instrument Replay, receive instrument list, then apply buffered instrument updates.
5. Listen to Incremental feeds and buffer market updates.
6. Listen to Snapshot feeds, receive current snapshots, apply them.
7. For each instrument, discard buffered incremental messages up to LastMsgSeqNumProcessed and apply remaining updates.
8. Stop listening to Snapshot feeds.
9. Continue normal Incremental processing.
```

Implementation consequence:

```text
Collector cannot just subscribe to incremental UDP and start processing.
It needs a bootstrap state machine: config -> templates -> instrument replay -> snapshots -> buffered incrementals -> steady state.
```

## Transport and channel model

FAST Gate uses UDP multicast for market data distribution.

Missing data recovery is supported through:

```text
- UDP Recovery feeds;
- TCP request/replay sessions.
```

Channels group instruments. Each channel has multiple UDP feeds and a TCP recovery port.

Feeds A and B carry identical messages. The duplicate feeds exist to reduce packet-loss risk.

Implementation consequence:

```text
Collector must support A/B duplicate handling and sequence-based deduplication.
```

## Main stream types

The spec lists main UDP incremental streams including:

```text
FO-TRADES
FO-BOOK-5
FO-BOOK-20
FO-BOOK-50
INDEX
NEWS
ORDERS-LOG
```

For this project, priority is:

```text
1. ORDERS-LOG    full anonymous order/trade event stream
2. FO-BOOK-N     aggregate book comparison / sanity checks
3. FO-TRADES     trades/statistics
4. instruments   instrument metadata
```

## ORDERS-LOG message templates

The full anonymous orders/trades stream uses special templates:

```text
OrdersLogMessage, template id 29
BookMessage, template id 30
```

OrdersLogMessage is used for updates and TCP Recovery.

BookMessage is used for snapshots.

Important fields:

```text
MsgSeqNum                tag 34
SendingTime              tag 52
LastFragment             tag 893
NoMDEntries              tag 268
MDUpdateAction           tag 279
MDEntryType              tag 269
MDEntryID                tag 278
SecurityID               tag 48
RptSeq                   tag 83
MDEntryTime              tag 273
MDEntryPx                tag 270
MDEntrySize              tag 271
LastPx                   tag 31
LastQty                  tag 32
TradeID                  tag 1003
ExchangeTradingSessionID tag 5842
MDFlags                  tag 20017
MDFlags2                 tag 20050
Revision                 tag 20018
```

## ORDERS-LOG update semantics

### Add order

```text
MDUpdateAction = 0 New
MDEntryType    = 0 Bid or 1 Ask
MDEntryID      = unique order id
SecurityID     = instrument id
RptSeq         = per-instrument update sequence
MDEntryPx      = order price
MDEntrySize    = order size
MDFlags        = bit mask
```

### Delete order

```text
MDUpdateAction = 2 Delete
MDEntryID      = unique order id
MDEntrySize    = size in delete operation
```

### Partial fill

```text
MDUpdateAction = 1 Change
MDEntryID      = unique order id
MDEntrySize    = remaining order quantity
LastPx         = trade price
LastQty        = trade quantity
TradeID        = trade id
```

Important consequence:

```text
For live FAST ORDERS-LOG, partial fill MDEntrySize means remaining quantity, not executed delta.
```

### Full fill

```text
MDUpdateAction = 2 Delete
MDEntryID      = unique order id
LastPx         = trade price
LastQty        = trade quantity
TradeID        = trade id
MDEntrySize    = absent
```

Important consequence:

```text
Full fill removes the order from active order container.
```

## MDFlags mapping for live FAST ORDERS-LOG

Important: FAST ORDERS-LOG MDFlags are not the same bit layout as historical QSH OrdLog order_flags.

For Add/Delete in FAST ORDERS-LOG, key MDFlags include:

```text
0x01              quote order / quoted order
0x02              counter order / opposite order
0x04              non-system order/trade
0x1000            last record in transaction
0x80000           Fill-or-kill
0x100000          result of order move
0x200000          result of order delete
0x400000          result of group delete
0x4000000         negotiated/address order flag
0x8000000         multileg/bundle order flag
0x200000000000    synthetic order
0x400000000000    RFS order
0x1000000000000000 Book-or-cancel / Passive only
0x4000000000000000 discrete-auction order/trade flag
```

Project rule:

```text
Do not reuse QSH order_flags enum for FAST MDFlags.
Create a separate FAST MDFlags enum/table.
```

This matters because in QSH v4 historical OrdLog we saw:

```text
0x94 = Add + Buy + Quote
```

But in live FAST ORDERS-LOG, Add/Buy are not encoded the same way in MDFlags. Side is MDEntryType and action is MDUpdateAction.

## Active order book reconstruction rule

The spec says that to assemble the active order book from ORDERS-LOG incremental stream, the client should process only orders/trades where the non-system flag is not set:

```text
ignore if MDFlags has 0x04 non-system
```

Same rule applies to active-order snapshots from BookMessage.

Implementation consequence:

```text
For FAST live collector, default visible-book reconstruction should exclude MDFlags 0x04 non-system records.
```

## Empty book / clearing messages

There are two important Empty Book cases:

```text
1. Clear active orders by trading session.
2. Full clear of all active orders.
```

For full clear, client must clean all active orders and then repeat bootstrap steps 4-7 from the connection process.

Implementation consequence:

```text
Collector needs explicit handling for MDEntryType = J Empty Book.
Do not treat it as a normal price level.
It is a state-reset/control event.
```

## Snapshot handling

Book snapshots may be split into multiple messages.

Relevant fields:

```text
LastMsgSeqNumProcessed tag 369
RptSeq                 tag 83
LastFragment           tag 893
RouteFirst             tag 7944
SecurityID             tag 48
MDEntryType            tag 269
MDEntryID              tag 278
MDEntryPx              tag 270
MDEntrySize            tag 271
MDFlags                tag 20017
MDFlags2               tag 20050
```

Implementation consequence:

```text
Snapshot application must support fragmentation and per-instrument state completion.
A snapshot is complete only after the required first/last fragment logic is satisfied.
```

## ORDERS-LOG vs FO-BOOK-N difference

The spec warns that a client-built book from ORDERS-LOG and MOEX aggregate books FO-BOOK-1/5/20/50 may differ.

Reason:

```text
FO-BOOK-N may include synthetic liquidity.
ORDERS-LOG client must calculate synthetic liquidity itself if it needs exact comparison.
```

Implementation consequence:

```text
Do not expect L3 ORDERS-LOG reconstructed book to match FO-BOOK-N exactly until synthetic liquidity handling is implemented.
FO-BOOK-N should be used as a sanity check, not as an exact equality oracle.
```

## Synthetic matching

The spec describes synthetic matching as matching between orders in different books/instruments to increase liquidity.

Synthetic liquidity can appear in aggregate books and may require separate calculation.

Project implication:

```text
For MVP collector, record synthetic flags and synthetic sizes.
Do not attempt full synthetic matching in the first implementation unless needed for validation.
```

## TCP Recovery

FAST Gate supports TCP recovery / replay for missing messages.

Important implementation requirements:

```text
- detect sequence gaps;
- request missing data where possible;
- know recovery limits from configuration;
- fail the dataset/session quality if gaps cannot be recovered;
- log all recovery attempts and results.
```

## Minimum collector state machine

Initial implementation should be designed as:

```text
LOAD_CONFIG
LOAD_TEMPLATES
CONNECT_INSTRUMENT_INCREMENTAL
CONNECT_INSTRUMENT_REPLAY
BUILD_INSTRUMENT_TABLE
CONNECT_INCREMENTAL_A_B
BUFFER_INCREMENTALS
CONNECT_SNAPSHOT
APPLY_SNAPSHOT
APPLY_BUFFERED_INCREMENTALS
STEADY_STATE
GAP_DETECTED
UDP_RECOVERY
TCP_RECOVERY
RESET_ON_EMPTY_BOOK
STOP
```

## Minimum normalized events to support

```text
InstrumentDefinition
TradingSessionStatus
OrderAdd
OrderDelete
OrderChangePartialFill
OrderDeleteFullFill
Trade
EmptyBookSession
EmptyBookFull
SnapshotStart
SnapshotEntry
SnapshotEnd
GapDetected
RecoveryRequest
RecoveryApplied
RecoveryFailed
```

## Safety and repository policy

Do not commit:

```text
- downloaded PDFs;
- downloaded ZIPs;
- templates.xml unless explicitly approved;
- configuration.xml unless explicitly approved;
- raw packet captures;
- raw production data;
- credentials;
- local connection parameters.
```

Allowed in Git:

```text
- this summary;
- derived enum tables;
- placeholder config examples;
- code that works in dry-run/test mode;
- tests using synthetic messages.
```

## Open questions for next reading step

```text
1. What exact channel names/groups exist in templatesT0/configuration.xml?
2. Is ORDERS-LOG available in test T0 configuration?
3. Which multicast groups and ports are used for ORDERS-LOG T0?
4. Which template ids in templatesT0/templates.xml correspond to current PDF 1.30.2?
5. What are the exact TCP Recovery limits in configuration.xml?
6. Do test FAST feeds require VPN, login, or only network reachability?
7. Is production Full_orders_log schema identical to test ORDERS-LOG schema?
```

## Next source to read

```text
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/configuration.xml
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/templates.xml
```
