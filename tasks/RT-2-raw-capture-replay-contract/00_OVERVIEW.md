# RT-2 — Raw Segment Format and Synthetic Capture/Replay

Date: 2026-07-10  
Status: specification draft for owner review  
Issue: #18  
Executor after approval: MiMo Code  
Reviewer: Architecture/Review Agent

## Objective

Create the first offline raw-market-data source-of-truth layer for the MOEX realtime contour:

```text
synthetic packet producer
-> versioned immutable raw segments
-> validator/reader
-> deterministic replay callback
-> future RT-3 FAST decoder
```

RT-2 does not connect to MOEX. It proves the storage and replay contract using synthetic packet records only.

## Why this stage exists

The realtime architecture requires raw bytes to survive decoder defects and downstream failures. Decoder fixes must be able to replay the original capture rather than relying on derived databases or normalized events.

The raw path must therefore be:

```text
independent of Python, dashboards, databases and strategies;
versioned and portable across Windows/Linux;
explicit about timestamps, source identity and local capture order;
immutable after finalization;
self-validating against truncation and corruption;
deterministic under synthetic write/replay tests.
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
tasks/RT-2-raw-capture-replay-contract/01_REQUIREMENTS.md
tasks/RT-2-raw-capture-replay-contract/02_TEST_PLAN.md
tasks/RT-2-raw-capture-replay-contract/03_ACCEPTANCE.md
```

## Deliverables

```text
1. C++20/CMake raw segment module, preferably under cpp/moex_raw/.
2. Stable v1 binary segment contract documented in the module README.
3. Writer API accepting in-memory synthetic packet records.
4. Safe .partial -> finalized segment lifecycle and deterministic rotation.
5. Reader and validator with bounded parsing.
6. Deterministic replay callback preserving record order and payload bytes.
7. CLI coverage for synthetic generation, inspection and replay digest.
8. Release-active Windows and Linux tests.
9. MiMo implementation report with exact commands, results, commit and PR.
```

## Non-goals

```text
no socket creation;
no multicast join;
no real UDP/TCP capture;
no MOEX login or connection parameters;
no FAST binary decode;
no exchange sequence extraction from payload;
no A/B deduplication;
no Snapshot/Incremental bootstrap;
no book building;
no Parquet/ClickHouse/PostgreSQL integration;
no pcap/pcapng dependency;
no compression or object-storage upload;
no FIX/TWIME;
no order sending;
no production enablement.
```

## Architectural boundaries

- The required sequence is a local `capture_index`; it is not an exchange message sequence.
- Payload bytes are opaque in RT-2.
- Source metadata is logical and privacy-safe: feed group, endpoint role, A/B designation, transport, channel/source identifiers and endpoint fingerprint. Raw addresses and ports are not required in reports or fixtures.
- Timestamps have explicit nanosecond units and clock-domain fields. Native time structs are never serialized.
- Binary encoding is fixed-width little-endian and manually serialized. Persisting raw C++ structs is forbidden.
- Segment rotation is deterministic by configured record/byte limits. Wall-clock rotation is deferred.
- A finalized segment is never reopened for append by the writer.

## Workflow gate

```text
Architecture branch + specification PR
-> owner reviews specification
-> specification PR merged
-> Issue #18 moves to READY_FOR_MIMO
-> MiMo creates a separate implementation branch and PR
-> reviewer validates code/tests
-> owner approves merge
```

MiMo must not implement RT-2 from this specification branch.