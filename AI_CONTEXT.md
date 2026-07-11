# AI Context

Дата обновления: 2026-07-11  
Репозиторий: `dvrk0726/trading-robot-lab`  
Текущий gate: RT-3 specification PR #22 — owner review

## Назначение проекта

Создать исследовательскую и будущую production-ready торговую систему с жёстким разделением:

```text
Trading Lab      — data quality, replay, research, backtest, reports and UI.
Trading Runtime  — future execution of approved Strategy Packages.
Shared Contracts — common events, signals, OrderIntent, RiskDecision and reports.
```

## Неизменяемая архитектура

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry заблокирован до VPTS/certification и решения Owner.
C++ используется для low-level data/realtime/runtime contour.
Python используется для research, reports and UI.
Secrets, personal data and raw market data не хранятся в Git.
```

Authoritative ADRs:

```text
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Подтверждённое historical состояние

```text
M10X complete.
20/20 CTest regression tests.
Control commit: 54cd53df4b92473e49dd5dff96b2024590b82e42
Remaining crossed snapshots: 907
strategy_ready for affected data: false
```

Подтверждённые historical flags:

```text
0x94  = Add + Buy + Quote
0x414 = Add + Buy + TxEnd
```

Historical QSH 2021 — engineering sample для parser/replay/book mechanics, не доказательство современной прибыльности.

## MOEX realtime foundation

Изучены и зафиксированы:

```text
FAST 1.29.x / FAST_9.0;
актуальная совместимость profiles spectra-1.29 / spectra-1.30;
T0 configuration.xml structure;
FIX SPECTRA and Drop Copy requirements;
VPTS/certification gate;
Snapshot + Incremental architecture;
QuickFAST diagnostic-only decision;
specialized C++ decoder direction.
```

MOEX test-access questionnaire отправлена; private access details remain outside Git.

Запрещено сохранять в Git:

```text
personal data;
static IP and private addresses/ports;
logins/passwords;
official owner-provided private artifacts;
raw packet captures and generated market-data archives.
```

## Workflow foundation

```text
Issue #1: DONE
PR #15: merged (82077f6e54e439f27027301ac02813c018d380fc)
Main protection: Option B procedural protection active
MiMo: one READY_FOR_MIMO task, branch + PR, no merge
```

Authoritative process:

```text
docs/mimo_developer_workflow.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
docs/engineering/OWNER_REVIEW_PACKAGE.md
docs/engineering/MAIN_BRANCH_PROTECTION.md
```

Universal MiMo command:

```text
Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись.
```

The universal command is not authorized for Issue #21 while it remains DRAFT.

## RT-1 — DONE

```text
Issue #14: DONE
PR #16: merged
Source head: c9fcbb18cd4c78e2f774e18d76b86ceddc80d305
Merge commit: ab74f560c1bcf9d09ae7bdfb8552c745928fd022
Post-merge CI #32 on main: passed
Owner local Release build: passed
Owner CTest: 6/6
Owner strict real-file integration: valid, zero issues
```

Delivered:

```text
offline C++20/CMake configuration/templates inspector;
official-style MOEX hierarchy parsing;
FAST metadata and <length> handling;
spectra-1.29 / spectra-1.30 profile detection;
deterministic JSON/text reports;
Windows/Linux Release-active tests.
```

RT-1 does not connect to MOEX or decode wire packets.

## RT-2 — DONE

```text
Issue #18: DONE
Specification PR #19: merged
Implementation PR #20: merged
Implementation source head: 4afbc9f4ed25974ac62fad13eeb7f6a20daec4e1
Merge commit: 060371112d921c1c1f4055cfbdb99049bdf8a2af
Current main control head: 5f1f9c1beaee080fe44eaccda7c7370d9324546d
Post-merge/current-main CI #74: passed
Owner Release build and CTest 18/18: passed
Owner inspect/replay: valid, 4 segments, 10 records, 0 issues
```

Delivered:

```text
versioned immutable `.mxraw` v1 segments;
checked little-endian serialization;
CRC32C and SHA-256 provenance;
`.partial` -> finalized lifecycle;
deterministic rotation;
bounded validation and stream-set grouping;
per-stream reports;
MXREPLAY1 deterministic replay;
CLI synth/inspect/replay;
Windows/Linux 18-test gates.
```

RT-2 payload bytes remain opaque. `capture_index` is not FAST `MsgSeqNum` or exchange sequence.

## RT-3 — specification owner review

```text
Issue #21: DRAFT — OWNER_SPEC_REVIEW
Architecture branch: docs/issue-21-rt3-fast-decoder-spec
Specification PR: #22
Specification head before evidence update: 8dd3cfb7e53c85e9bcfbdcdafc2a735dd4dd0708
Specification CI #75 (run 29148020256): green 7/7
Task package: tasks/RT-3-specialized-fast-decoder-foundation/
MiMo implementation: NOT AUTHORIZED
RT-4: BLOCKED
```

Normative RT-3 boundary:

```text
input = exactly one bounded FAST message byte span;
compiled templates = immutable decoder-specific tree;
session = one ordered logical source stream;
output = deterministic owned typed DecodedMessage;
decode state = transactional template-id + dictionaries;
reset = explicit only;
```

Required decoder foundation:

```text
presence maps and stop-bit primitives;
nullable uInt32/uInt64/int32/int64;
ASCII, Unicode, byte vector;
exact decimal exponent/mantissa;
template-id state and reuse;
none/constant/default/copy/increment/delta/tail;
canonical dictionary keys/scopes;
groups and sequences with hard limits;
transactional rollback on every failure;
decode_one and decode_exact;
stable issues with offsets and field paths;
deterministic text/JSON CLI;
span compatibility with RawPacketRecord.payload;
hard-coded vectors + independent test-only reference encoder;
Windows/Linux Release tests.
```

Explicit RT-3 non-goals:

```text
no SPECTRA UDP packet framing or message-boundary guessing;
no socket or multicast;
no exchange sequence/gap policy;
no A/B merge/deduplication;
no Snapshot/Incremental recovery;
no normalized market events;
no order-log/book semantics;
no FIX/TWIME or order sending;
no strategy, paper or production enablement.
```

Authoritative RT-3 specification files:

```text
tasks/RT-3-specialized-fast-decoder-foundation/00_OVERVIEW.md
tasks/RT-3-specialized-fast-decoder-foundation/01_REQUIREMENTS.md
tasks/RT-3-specialized-fast-decoder-foundation/02_TEST_PLAN.md
tasks/RT-3-specialized-fast-decoder-foundation/03_ACCEPTANCE.md
```

The flattened RT-1 field descriptor is not sufficient for decoding. RT-3 must compile a separate hierarchy preserving operators, dictionaries, references, decimals, groups and sequences while keeping RT-1 behavior compatible.

## Future FIX architecture

Issue #17 preserves future SPECTRA FIX 4.4 session/order-control/Drop Copy requirements. It remains architecture-only and is not part of RT-3.

## Canonical statuses

```text
DRAFT
READY_FOR_MIMO
IN_PROGRESS
READY_FOR_REVIEW
CHANGES_REQUIRED
OWNER_REVIEW
OWNER_APPROVED
DONE
```

## Immediate actions

```text
1. Owner reviews RT-3 specification PR #22.
2. Do not run MiMo implementation while Issue #21 is DRAFT.
3. After explicit approval, merge PR #22 manually.
4. Confirm green post-merge main CI.
5. Move Issue #21 to READY_FOR_MIMO only after that gate.
6. Do not start RT-4 until RT-3 is DONE.
```