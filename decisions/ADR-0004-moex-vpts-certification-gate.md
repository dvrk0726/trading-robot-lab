# ADR-0004 — MOEX VPTS Certification Gate Before Production Trading

Date: 2026-07-09
Status: accepted

## Context

The project is moving from historical QSH/OrdLog research toward MOEX SPECTRA connectivity.

A reviewed MOEX document defines the certification procedure for external software/hardware systems (VPTS) that connect to Moscow Exchange systems through exchange protocols.

The document makes clear that production trading connectivity is not just a technical connection problem. A VPTS must satisfy operational, logging, recovery and scenario-testing requirements before it can be used in the production trading contour.

## Decision

Production trading is a separate future stage and is blocked until the project passes a dedicated MOEX VPTS certification gate.

This gate is mandatory for any path that can send real exchange orders.

## Mandatory rule

```text
No production order sending until VPTS certification requirements are implemented, tested and explicitly approved by the owner.
```

## Required stages before production trading

### Stage 1 — Market data only

Allowed:

```text
- historical QSH/OrdLog research;
- test FAST market data experiments;
- production market data collection if licensed;
- normalized data storage;
- replay, backtest and paper trading.
```

Not allowed:

```text
- real order sending;
- broker/exchange production order entry;
- live strategy execution.
```

### Stage 2 — SPECTRA test access

Before any production trading work, the system must work on the MOEX SPECTRA test contour.

Minimum requirements:

```text
- FIX/FAST session setup;
- Logon / HeartBeat;
- correct disconnect and reconnect;
- market data capture where available;
- test-only order-entry experiments;
- no real credentials in Git;
- no production order routing.
```

### Stage 3 — VPTS readiness features

Before certification or production order entry, the Runtime must implement:

```text
- full interaction logging;
- order request logging;
- exchange response logging;
- execution report logging;
- reconnect handling;
- restart during trading day;
- exchange-side restart handling;
- reserve / duplicate server switch handling where applicable;
- configurable command rate control;
- request timeout handling;
- safe handling of unexpected exchange responses;
- manual kill switch;
- owner-controlled enablement of any order-sending mode.
```

### Stage 4 — FIX SPECTRA scenario coverage

The system must support the relevant SPECTRA FIX certification scenarios before production order sending is considered:

```text
- session establishment;
- session termination and reconnect;
- futures limit order placement;
- long-lived sell order placement;
- cancel by OrigClOrdID;
- option order placement if options will be traded;
- order replace / modification;
- order status request;
- mass cancel;
- partial fill scenario;
- full fill scenario;
- DropCopy ExecutionReport verification if DropCopy is used.
```

Multileg / OTC / LP / RFS scenarios are out of scope unless the owner explicitly decides to trade those instruments or services.

### Stage 5 — Full test trading day

For SPECTRA production readiness, the application must be able to run through a full trading day on the test polygon, including:

```text
- main session;
- intermediate clearing;
- clearing;
- evening trading session;
- all command types declared in the MOEX questionnaire.
```

### Stage 6 — Owner gate and MOEX certification

Production order entry can only be discussed after:

```text
- research/backtest/replay/paper validation;
- RiskEngine approval;
- security review;
- VPTS readiness checks;
- SPECTRA test contour run;
- owner approval;
- MOEX certification/compliance procedure where required.
```

## Consequences

- Trading Lab remains unable to send real orders.
- Strategy code still cannot talk directly to FIX/TWIME/order-entry protocols.
- Strategy output must remain Signal / OrderIntent only.
- Every OrderIntent must pass RiskEngine.
- Runtime order sending remains disabled by default.
- Production credentials must never be committed.
- Raw market data and logs must not be committed unless sanitized and explicitly approved.
- Market data collection can progress earlier than production trading, but it must remain separate from order sending.

## Current project interpretation

The project may proceed toward:

```text
MOEX SPECTRA test access -> FIX/FAST adapter -> realtime data collector -> paper trading
```

The project may not proceed directly toward:

```text
production FIX/TWIME order sending -> live trading
```

Production trading is a later certification-gated stage, not a current implementation task.
