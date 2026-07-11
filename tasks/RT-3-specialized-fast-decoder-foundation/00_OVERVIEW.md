# RT-3 — Specialized C++ FAST Decoder Foundation

Date: 2026-07-11  
Status: specification draft for owner review  
Issue: #21  
Executor after approval: MiMo Code  
Reviewer: Architecture/Review Agent

## Objective

Create the first correctness-focused binary FAST decoder layer for the MOEX realtime contour:

```text
RT-1 compiled template metadata
+ one bounded FAST message byte span
+ explicit decoder session state
-> deterministic typed decoded message
-> future RT-4 SPECTRA feed processors and recovery
```

RT-3 is offline. It does not connect to MOEX and does not interpret a UDP datagram as a feed packet. The public decode boundary is exactly one FAST message payload.

## Why this stage exists

RT-2 preserves raw bytes and deterministic replay. RT-3 must decode those bytes without mixing codec correctness with packet framing, A/B sequencing, recovery, normalized market events or book state.

The decoder must therefore be:

```text
specialized for the currently accepted MOEX SPECTRA FAST template profile;
template-driven rather than hard-coded by message name;
transactional so failed messages do not corrupt dictionary state;
bounded against malformed lengths, sequences and presence maps;
deterministic across Windows/MSVC and Linux/GCC;
independent of network, databases, Python and strategy code.
```

## Required reading

Before implementation MiMo must read:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/mimo_developer_workflow.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
docs/moex/MOEX_REALTIME_ARCHITECTURE.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
tasks/RT-1-fast-config-template-inspector/00_OVERVIEW.md
tasks/RT-2-raw-capture-replay-contract/00_OVERVIEW.md
tasks/RT-3-specialized-fast-decoder-foundation/01_REQUIREMENTS.md
tasks/RT-3-specialized-fast-decoder-foundation/02_TEST_PLAN.md
tasks/RT-3-specialized-fast-decoder-foundation/03_ACCEPTANCE.md
```

Normative public protocol references:

```text
https://fixtrading.org/standards/fast/
https://ftp.moex.com/pub/FAST/Spectra/test/
```

Official or owner-provided XML and raw market data remain outside Git. Only small synthetic templates and independently derived wire vectors may be committed.

## Deliverables

```text
1. C++20/CMake decoder library integrated with the existing cpp/moex_fast project.
2. Immutable decoder-specific compiled template tree that preserves operators and nesting.
3. Bounded stop-bit, presence-map, integer, string, decimal and sequence primitives.
4. Correct none/constant/default/copy/increment/delta/tail semantics for supported types.
5. Explicit DecoderSession with template-ID state, dictionaries and reset API.
6. Transactional state commit: any failed decode leaves session state unchanged.
7. Typed DecodedMessage model preserving null, exact integers and decimal exponent/mantissa.
8. Exact-one-message CLI for synthetic/public vectors with deterministic text and JSON output.
9. Offline adapter contract accepting RawPacketRecord.payload as a byte span without feed framing claims.
10. Release-active Windows/Linux tests, reference-derived vectors and implementation report.
```

## Non-goals

```text
no socket creation or multicast join;
no real capture;
no private IP addresses, ports, credentials or owner artifacts;
no SPECTRA UDP packet header parsing;
no multiple-message datagram framing;
no exchange packet sequence extraction;
no A/B merge or deduplication;
no gap detection, historical replay request or Snapshot/Incremental bootstrap;
no normalized market events;
no order-log semantics or order-book reconstruction;
no FIX/TWIME session or order sending;
no Strategy, RiskEngine, backtest, paper or production enablement.
```

## Architectural boundaries

- One `DecoderSession` belongs to one logical ordered source stream and is not thread-safe.
- The compiled template set is immutable and may be shared between sessions.
- `decode_one` consumes one FAST message prefix and returns `bytes_consumed`; `decode_exact` additionally rejects trailing bytes.
- RT-3 never guesses packet framing. A real UDP payload may require RT-4 framing before it reaches `decode_one`.
- Decoder reset is explicit. RT-3 does not infer resets from `MsgSeqNum`, packet gaps, endpoint changes or trading-session events.
- Decimal values remain exact `(exponent, mantissa)` pairs. Conversion to binary floating point is outside the decoder.
- Expected malformed input uses explicit status/error values. No expected input error escapes the public API as an uncaught exception.

## Workflow gate

```text
Architecture branch + specification PR
-> owner reviews specification
-> specification PR merged
-> post-merge main CI green
-> Issue #21 moves to READY_FOR_MIMO
-> MiMo creates a separate implementation branch and PR
-> Architecture/Review validates code and tests
-> owner performs local acceptance and approves merge
```

MiMo must not implement RT-3 from this specification branch and must not begin RT-4.