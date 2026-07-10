# AI Context

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Текущий gate: RT-2 Round 7 corrections complete — Issue #18 / PR #20 READY_FOR_REVIEW

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

## RT-2 — Round 7 corrections complete

```text
Issue #18: CHANGES_REQUIRED -> READY_FOR_REVIEW
Implementation PR: #20
Branch: mimo/issue-18-rt2-raw-capture-replay
Implementation commit: `7f72647`
CI #62 (run 29122599257): ALL GREEN (7/7 jobs)
```

Round 7 corrections delivered:

```text
stage-accurate fault injection: real short-header-write test (preconfigured before open);
stage-aware reader fault injection for record-header, record-tail, content-hash, file-hash reads;
seek failure at file-hash and record-scan stages;
reader record-header/tail I/O failures return IoError (not Corrupt);
real CLI end-to-end tests: single-file, one-stream, two-stream inspect JSON via moex-raw executable;
strict partial/corrupt exit codes; synth overflow with empty output dir;
invalid JSON output path; deterministic repeated JSON output; strict JSON parser on CLI output;
UTC aggregation: ordered first/last non-zero UTC-valid timestamp (not min/max);
fixture with numerically decreasing UTC values;
checked cross-segment arithmetic: checked_add_u64 for segment_index + 1 and last_capture_index + 1;
SEGMENT_INDEX_OVERFLOW and CAPTURE_INDEX_OVERFLOW issue codes;
should_rotate() uses checked_add_u64; UINT64_MAX boundary tests;
18/18 Release tests (Windows + Linux).
```

Test results:

```text
RT-2:         18/18 tests passed (Windows + Linux Release)
RT-1:          6/6  tests passed (no regression)
QSH/M10X:     20/20 tests passed (no regression)
Python:         3/3  passed
Hygiene:        PASS
```

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
1. Owner reviews RT-2 Round 7 corrections in PR #20.
2. If accepted, merge PR #20.
3. Move Issue #18 to DONE.
4. Do not start RT-3 until RT-2 is DONE.
```