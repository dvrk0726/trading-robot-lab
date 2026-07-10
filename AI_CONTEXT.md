# AI Context

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Текущий gate: RT-2 specification review — Issue #18 / PR #19

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

## RT-1 — complete

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

## RT-2 — specification gate

```text
Issue #18: [MIMO][C++] RT-2 raw segment format and synthetic capture/replay
Status: DRAFT
Specification branch: docs/issue-18-rt2-raw-capture-replay-spec
Specification PR: #19
Implementation: not started
```

Task package:

```text
tasks/RT-2-raw-capture-replay-contract/00_OVERVIEW.md
tasks/RT-2-raw-capture-replay-contract/01_REQUIREMENTS.md
tasks/RT-2-raw-capture-replay-contract/02_TEST_PLAN.md
tasks/RT-2-raw-capture-replay-contract/03_ACCEPTANCE.md
```

RT-2 planned scope:

```text
versioned immutable .mxraw binary segments;
explicit little-endian manual serialization;
logical source/timestamp/local capture-order contracts;
.partial -> finalized lifecycle;
deterministic record/byte rotation;
bounded reader/validator;
deterministic synthetic replay digest;
no network and no FAST decode.
```

RT-2 implementation must not begin until PR #19 is reviewed, owner-approved and merged, then Issue #18 moves to READY_FOR_MIMO.

## Future FIX architecture

Issue #17 preserves future SPECTRA FIX 4.4 session/order-control/Drop Copy requirements. It remains architecture-only and is not part of RT-2.

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
1. Review RT-2 specification PR #19.
2. Correct specification only if architecture/safety gaps are found.
3. Owner approves and merges PR #19.
4. Move Issue #18 from DRAFT to READY_FOR_MIMO.
5. Run the universal MiMo command once.
6. MiMo implements in a new mimo/issue-18-* branch and stops at a separate PR.
7. Do not start RT-3, network capture, FAST decoding or FIX work during RT-2.
```