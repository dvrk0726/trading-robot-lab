# FIX SPECTRA Notes

Date: 2026-07-09
Status: first-pass summary
Source: https://ftp.moex.com/pub/FIX/Spectra/test/docs/spectra_fixgate_ru.pdf
Source version: FIX protocol specification for MOEX derivatives market, version 1.23.0, document dated 2025-11-20

## Purpose

This document extracts project-relevant implementation notes from the MOEX SPECTRA FIX Gate Russian specification.

It is not a full copy of the manual.

Use the official PDF as the source of truth when implementing protocol details.

## Scope for this project

FIX SPECTRA is order-entry / order-management infrastructure.

Project use:

```text
FIX test contour -> controlled test orders -> ExecutionReport handling -> certification readiness later
```

Not allowed now:

```text
production order sending
live trading
strategy directly talking to FIX
credentials in Git
```

FAST remains the market-data path. FIX must remain behind Runtime + RiskGate + explicit owner gate.

## High-level protocol model

FIX version:

```text
BeginString = FIX.4.4
```

Every message uses:

```text
Standard Header
Standard Trailer
```

Important header fields:

```text
8   BeginString
9   BodyLength
35  MsgType
49  SenderCompID
56  TargetCompID
34  MsgSeqNum
52  SendingTime
122 OrigSendingTime optional for resend
97  PossResend optional
43  PossDupFlag optional
```

Important trailer field:

```text
10 CheckSum
```

## Session-level messages

Supported session-level messages:

```text
A  Logon
5  Logout
0  Heartbeat
1  Test Request
3  Reject
2  Resend Request
4  Sequence Reset
```

### Logon

`Logon` must be the first message in every connection.

Important fields:

```text
98  EncryptMethod = 0
108 HeartBtInt
141 ResetSeqNumFlag optional
```

Project rule:

```text
FIX client must implement sequence numbers, Heartbeat, Test Request response, Resend Request and Sequence Reset before any order-entry test.
```

### Sequence counters

Both sides have In and Out counters.

Important behavior:

```text
- if incoming MsgSeqNum is greater than expected, Resend Request is required;
- if incoming MsgSeqNum is lower than expected, session reset/recovery logic is required;
- FIX Gate may adjust server-side counter if client Logon has lower MsgSeqNum;
- client must avoid reprocessing old/non-actual orders on resend.
```

Implementation consequence:

```text
Order manager must be idempotent around resent ExecutionReport messages.
```

## Trading messages

Supported trading messages:

```text
D  New Order Single
F  Order Cancel Request
q  Order Mass Cancel Request
G  Order Cancel/Replace Request
H  Order Status Request
8  Execution Report
9  Order Cancel Reject
r  Order Mass Cancel Report
```

## New Order Single

Message type:

```text
MsgType = D
```

Purpose:

```text
Place a new limit order.
```

Important fields:

```text
60    TransactTime       required, UTC
11    ClOrdID            required, String20, unique per SenderCompID
40    OrdType            required, only Limit = 2
55    Symbol             required, instrument symbol
461   CFICode            required for options and multileg instruments
167   SecurityType       required for multileg instruments, MLEG
1     Account            required for client account
59    TimeInForce        optional, default 0 Day/quote order
54    Side               required, 1 Buy / 2 Sell
38    OrderQty           required
44    Price              required for limit order
432   ExpireDate         required for TimeInForce=6 GTD
1300  MarketSegmentID    optional, F futures / O options
20008 Flags              optional
20035 NccRequest         optional
376   ComplianceID       optional, order creation marking
```

For the project:

```text
ComplianceID should be set consciously for algorithm-generated test orders.
Use a dedicated field in OrderIntent/ExecutionAdapter mapping; do not hardcode silently.
```

## ComplianceID

`ComplianceID` marks how the order was created.

Relevant values:

```text
blank / empty = not specified
M = manual input
S = conditional order / stop-loss
R = robot/algorithm
A = auto-following algorithm
D = position close due to failed MarginCall
```

Important warning:

```text
The field is passed to regulatory reports and cannot be corrected post factum.
```

Project rule:

```text
Any future algorithmic order must have explicit ComplianceID policy.
```

## Order Cancel Request

Message type:

```text
MsgType = F
```

Purpose:

```text
Cancel an active order.
```

Important fields:

```text
11 ClOrdID        required, id of cancel request
37 OrderID        conditional, exchange order id; required if OrigClOrdID absent
41 OrigClOrdID    conditional, original client order id; required if OrderID absent
55 Symbol         required
461 CFICode       required for options and calendar spreads
54 Side           required
60 TransactTime   required, UTC
38 OrderQty       required
```

Project rule:

```text
The system must be able to cancel by OrigClOrdID and by OrderID where available.
```

## Order Mass Cancel Request

Message type:

```text
MsgType = q
```

Purpose:

```text
Mass cancel active orders matching criteria.
```

Important fields:

```text
11   ClOrdID                 required
530  MassCancelRequestType   required
1300 MarketSegmentID         conditional, F futures / O options
54   Side                    optional, 1 buy / 2 sell / Y all, default Y
1    Account                 conditional for firm-level logins
20008 Flags                  optional: 0x10 system, 0x20 non-system, 0x40 all; default 0x40
55   Symbol                  conditional when MassCancelRequestType=1
60   TransactTime            required
```

Supported uses:

```text
MassCancelRequestType=1  cancel all orders by instrument
MassCancelRequestType=8/9 cancel all orders by market segment
```

Project rule:

```text
Runtime must expose an emergency mass-cancel path before any production trading is considered.
It must be owner-gated and risk-gated.
```

## Order Cancel/Replace Request

Message type:

```text
MsgType = G
```

Purpose:

```text
Modify price/quantity of an active order.
```

Important fields:

```text
11 ClOrdID        required, id of replace request
37 OrderID        conditional, required if OrigClOrdID absent
41 OrigClOrdID    conditional, required if OrderID absent
38 OrderQty       required, new quantity
44 Price          required for limit order
55 Symbol         required
461 CFICode       required for options and multileg
54 Side           required
60 TransactTime   required, UTC
20008 Flags       optional
20035 NccRequest  optional
376 ComplianceID  optional
```

Project rule:

```text
Cancel/replace must be treated as a new command with its own ClOrdID and state lifecycle.
```

## Order Status Request

Message type:

```text
MsgType = H
```

Purpose:

```text
Request current status of a specific order.
```

Important fields:

```text
11 ClOrdID        conditional, required if OrderID absent
37 OrderID        conditional, required if ClOrdID absent
55 Symbol         required
54 Side           required
790 OrdStatusReqID optional, echoed back in ExecutionReport
```

Expected response:

```text
ExecutionReport with ExecType=I Order Status
or ExecutionReport with OrdStatus=8 Rejected and OrdRejReason=5 UnknownOrder
```

## Execution Report

Message type:

```text
MsgType = 8
```

Purpose:

```text
Report order state changes, rejects, fills and status responses.
```

Generated for:

```text
- successful order placement;
- order placement reject;
- successful cancel;
- order status request;
- order status reject;
- replace;
- GTD/multiday re-placement;
- execution/fill.
```

Important fields:

```text
11    ClOrdID
41    OrigClOrdID
150   ExecType
39    OrdStatus
17    ExecID
37    OrderID
198   SecondaryOrderID
336   TradingSessionID
1     Account
60    TransactTime
55    Symbol
54    Side
44    Price
32    LastQty
38    OrderQty
151   LeavesQty
14    CumQty
6     AvgPx
103   OrdRejReason
58    Text
790   OrdStatusReqID
31    LastPx
136   NoMiscFees group
432   ExpireDate
20008 Flags
20018 Revision
378   ExecRestatementReason
20035 NccRequest
20051 Flags2
376   ComplianceID
```

Important detail:

```text
Empty ClOrdID means unsolicited ExecutionReport.
```

Project rule:

```text
OrderManager must accept unsolicited ExecutionReports and reconcile state by OrderID/Symbol/Side/account where possible.
```

## Key status/exec values

### OrdStatus tag 39

```text
0 New
1 Partially filled
2 Filled
4 Canceled
5 Replaced
6 Pending Cancel
8 Rejected
A Pending New
C Expired
E Pending Replace
```

### ExecType tag 150

```text
0 New
3 Done for day
4 Canceled
5 Replaced
6 Pending Cancel
8 Rejected
C Expired
E Pending Replace
F Trade
I Order Status
```

## Basic order lifecycle scenarios

### Placement accepted

```text
Client -> NewOrderSingle
FIX Gate -> ExecutionReport ExecType=0, OrdStatus=0
```

### Full fill after placement

```text
Client -> NewOrderSingle
FIX Gate -> ExecutionReport New
FIX Gate -> ExecutionReport ExecType=F, OrdStatus=2 Filled
```

### Partial fill and cancel remainder

```text
Client -> NewOrderSingle
FIX Gate -> ExecutionReport New
FIX Gate -> one or more ExecutionReport ExecType=F, OrdStatus=1 PartiallyFilled
Client/FIX/system -> cancel remainder
FIX Gate -> ExecutionReport ExecType=4, OrdStatus=4 Canceled
```

### Cancel flow

```text
Client -> OrderCancelRequest
FIX Gate -> ExecutionReport ExecType=6, OrdStatus=6 PendingCancel
FIX Gate -> ExecutionReport ExecType=4, OrdStatus=4 Canceled
```

If order was already filled:

```text
FIX Gate -> OrderCancelReject, CxlRejReason=0 Too late to cancel, OrdStatus=2 Filled
```

### Mass cancel flow

```text
Client -> OrderMassCancelRequest
FIX Gate -> ExecutionReport Canceled for each canceled order
FIX Gate -> OrderMassCancelReport
```

## Flood control / anomalous activity control

FIX Gate limits client application activity per FIX session.

Document states available login limits like:

```text
30, 60, 90, ... up to 300 trading operations per second
```

Trading operations include:

```text
New Order Single
Order Cancel Request
Order Cancel/Replace Request
Mass Cancel Request
```

Non-trading non-session messages are limited to:

```text
500/sec
```

On limit breach:

```text
Reject MsgType=3
SessionRejectReason=7100 Flood control
Text format includes penalty_remain, queue_size, message
```

Project rule:

```text
FIX ExecutionAdapter must implement configurable rate limiting below MOEX login limits.
No strategy package may bypass this limiter.
```

## Error handling

Validation/system error:

```text
Reject MsgType=3
SessionRejectReason=7101 System error
Text format: code=%d;message=%s
```

Application unavailable:

```text
BusinessMessageReject MsgType=j
BusinessRejectReason=4 Application not available
```

Bad SenderCompID / TargetCompID:

```text
Logout MsgType=5
Text=FIX protocol violation
session closed
```

Project rule:

```text
Do not treat all rejects the same. Separate protocol reject, business reject, cancel reject and order reject.
```

## Cancel On Disconnect

MOEX supports Cancel On Disconnect.

If enabled for the login, the system can automatically cancel active regular, non-GTD, non-address orders when the client disconnects or stops sending messages.

Important behavior:

```text
- enabled/disabled via Client Center request;
- controlled by p2login;
- client sets HeartBtInt in Logon;
- if from 2*HeartBtInt to 3*HeartBtInt no message is received, or TCP connection is lost, active orders are automatically canceled;
- canceled orders generate ExecutionReport ExecType=4, OrdStatus=4, ExecRestatementReason=100.
```

Project rule:

```text
For any future production order-entry discussion, Cancel On Disconnect policy must be explicit.
For test contour, we must ask MOEX whether COD is available/enabled for the test login.
```

## Drop Copy

Drop Copy is a separate FIX session/login used to receive order state and trades.

Important points:

```text
- separate FIX login is issued;
- trading operations through Drop Copy login are forbidden;
- two modes: trades only, or order states + trades;
- session level is the same as normal FIX session;
- supports Heartbeat and Resend Request;
- reports are ExecutionReport messages;
- Drop Copy covers FIX Gate, Plaza-2 Gate and TWIME-originated events.
```

Project interpretation:

```text
Drop Copy should be considered later as an independent reconciliation/audit feed.
It is not required for first FAST collector.
It is relevant for future certified trading runtime.
```

## Implementation boundaries

Allowed next:

```text
- document-only design;
- test FIX client skeleton in dry-run mode;
- synthetic FIX message parser/serializer tests;
- local state machine tests without network;
- test-contour only after owner approval and MOEX credentials.
```

Not allowed:

```text
- production order-entry;
- real credentials in Git;
- real order sending;
- strategy direct FIX access;
- disabling RiskGate;
- bypassing rate limiter.
```

## Minimum future FIX test client capabilities

```text
1. Build valid FIX.4.4 messages with checksum/body length.
2. Maintain incoming/outgoing sequence numbers.
3. Send Logon and process Logon response.
4. Send periodic Heartbeat.
5. Respond to Test Request.
6. Send Resend Request when sequence gap is detected.
7. Process Sequence Reset / Gap Fill.
8. Parse ExecutionReport, Reject, BusinessMessageReject, OrderCancelReject, OrderMassCancelReport.
9. Support dry-run generation of NewOrderSingle, Cancel, Replace, Status, MassCancel without network.
10. Enforce allow_test_orders=false by default.
11. Hard refuse production mode until separate owner gate.
12. Apply configurable command rate limiter.
```

## Minimum normalized execution contracts

```text
FixSessionEvent
FixReject
FixBusinessReject
OrderAccepted
OrderRejected
OrderPendingCancel
OrderCanceled
OrderCancelRejected
OrderPendingReplace
OrderReplaced
OrderPartiallyFilled
OrderFilled
OrderExpired
OrderStatusSnapshot
MassCancelAccepted
MassCancelRejected
CancelOnDisconnectCancel
DropCopyExecutionReport
```

## Open questions for MOEX / future test access

```text
1. What SenderCompID / TargetCompID values will be issued for test SPECTRA FIX?
2. What host/port are used for test FIX Gate?
3. Is TLS/VPN required or plain TCP inside test access network?
4. What HeartBtInt is recommended on test contour?
5. What rate limit will be assigned to the test login?
6. Is Cancel On Disconnect available/enabled for test login?
7. Will Drop Copy login be available for test?
8. Which account code should be used for futures test orders?
9. Which futures/options symbols are available and liquid on test polygon?
10. Should ComplianceID=R be used for all algorithm-generated test orders?
```

## Recommended next step

Do not start live trading work.

Recommended next documentation step:

```text
Read MOEX SPECTRA test access page and prepare a precise request/checklist for test FIX/FAST access.
```

Recommended next implementation step only after owner approval:

```text
Create dry-run-only FIX message builder/parser skeleton with no network and no order sending.
```
