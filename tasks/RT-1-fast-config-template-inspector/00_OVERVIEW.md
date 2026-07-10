# RT-1 — FAST Configuration and Template Inspector

Date: 2026-07-10
Status: ready for owner review; implementation not started
Executor: MiMo Code
Reviewer: Architecture/Review Agent
Owner gate: required before implementation begins

## Objective

Create a local C++ tool that reads MOEX SPECTRA `configuration.xml` and `templates.xml`, validates their structure and produces a deterministic inspection report.

This stage prepares the foundation for the future specialized FAST decoder. It does not connect to MOEX and does not decode live FAST packets.

## Existing verified context

```text
FAST documentation: version 1.29.1
FAST template family: FAST_9.0
Checked template SHA-256:
dbd50f1e0becc2b2ebd9dac8e4c6609ba1538566811b610cde9b6dd3e7f66a8e

Relevant templates:
29 OrdersLogMessage
30 BookMessage
31 DefaultIncrementalRefreshMessage
32 DefaultSnapshotMessage
40 SecurityDefinition
45 SecurityGroupStatus
46 TradingSessionStatus
```

The T0 configuration contains FUT-INFO, ORDERS-LOG Incremental A/B, Snapshot A/B and TCP Historical Replay.

## Required reading

Before implementation MiMo must read:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/moex/MOEX_REALTIME_ARCHITECTURE.md
docs/moex/moex_source_version_check.md
docs/moex/fast_spectra_t0_templates_notes.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Deliverables

```text
1. C++20/23 inspector executable integrated into the existing CMake structure.
2. XML parsing for MOEX configuration and FAST templates.
3. Human-readable summary output.
4. Deterministic JSON inspection report.
5. Normalized C++ metadata contracts for templates, fields, feed groups and endpoints.
6. Unit tests using synthetic/sanitized fixtures.
7. Optional local integration test against owner-provided official XML files.
8. MiMo implementation report with build/test commands and commit SHA.
```

## Non-goals

```text
no UDP/TCP network connection;
no multicast join;
no FAST binary wire decoder;
no QuickFAST production dependency;
no raw packet capture;
no L3/L2 book building;
no ClickHouse/PostgreSQL/Parquet integration;
no FIX/TWIME;
no order sending;
no secrets or MOEX credentials;
no committing owner-provided official XML unless explicitly approved.
```

## Architectural rule

The inspector may use XML only at startup/offline. Future realtime decoding must use generated/direct C++ decoders and must not interpret XML in the hot path.

## Workflow

```text
Owner approves RT-1 specification
-> MiMo implements locally
-> MiMo builds and runs tests
-> MiMo commits and pushes
-> Reviewer inspects diff and tests
-> Owner accepts or requests corrections
```
