# Project State

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-1 Round 7 corrections applied, READY_FOR_REVIEW

## Архитектура

```text
Trading Lab      — research, data quality, replay, backtest, reports and UI.
Trading Runtime  — future execution of approved Strategy Packages.
Shared Contracts — normalized events, signals, OrderIntent, RiskDecision and reports.
```

Неизменяемые решения:

```text
Trading Lab cannot send real orders.
Strategy cannot call execution directly.
Every OrderIntent passes RiskEngine.
Live is disabled by default.
Production order entry requires VPTS/certification and Owner approval.
C++ is used for low-level data/realtime/runtime work.
Python is used for research, reports and UI.
```

## Completed foundation

```text
private repository and security rules;
Trading Lab / Trading Runtime / Shared Contracts skeleton;
shared schemas and test vectors;
Strategy Package standard and no-trade example;
local Trading Lab dashboard/demo work;
C++ QSH/OrdLog ingest and historical book reconstruction foundation;
M10X engineering milestone;
MOEX official-source research and realtime architecture;
ADR-0001 through ADR-0004.
```

## Historical QSH status

```text
M10X: complete
Regression tests: 20/20
Control commit: 54cd53df4b92473e49dd5dff96b2024590b82e42
Remaining crossed snapshots: 907
strategy_ready for affected data: false
```

Confirmed historical flags:

```text
0x94  = Add + Buy + Quote
0x414 = Add + Buy + TxEnd
```

No QSH semantic change is part of the current work.

## MOEX realtime status

Studied and documented:

```text
FAST 1.29.1;
FAST_9.0 templates;
T0 configuration structure;
FIX SPECTRA;
VPTS/certification requirements;
Snapshot + Incremental bootstrap architecture;
QuickFAST diagnostic-only decision;
specialized C++ SPECTRA decoder direction.
```

MOEX access process:

```text
questionnaire sent;
MOEX requested additional information for login creation;
private data and connection parameters remain outside Git.
```

## Completed workflow gate — Issue #1 / PR #15

```text
Issue #1: DONE
Pull Request #15: merged (82077f6e54e439f27027301ac02813c018d380fc)
```

Merged workflow package:

```text
permanent MiMo workflow;
universal READY_FOR_MIMO command;
branch-only implementation;
canonical eight task statuses;
one-task-at-a-time rule;
Pull Request template;
GitHub Actions for hygiene, Python/contracts and 20 C++ regressions;
repository hygiene checker;
main/master guard in mimo_save.ps1;
Owner Review Package standard;
reviewed START_DEMO.cmd / STOP_DEMO.cmd exception;
label synchronization workflow;
current AI workflow/protocol documents;
legacy direct-main documents marked superseded;
main protection option guide.
```

Main protection: Option B active.

## RT-1 status

```text
Issue #14: [MIMO][C++] RT-1 FAST configuration/templates inspector
Status: READY_FOR_REVIEW (Round 6 corrections applied)
Branch: feat/rt-1-fast-config-inspector
PR: #16
Head: f4ecf9a (code: f4ecf9a1478bbdbc7a8b7931caa82f7bff9b8e90)
CI run: (pending)
Review cycle: Round 6 CHANGES_REQUIRED → Round 6 corrections applied
```

RT-1 task package:

```text
tasks/RT-1-fast-config-template-inspector/00_OVERVIEW.md
tasks/RT-1-fast-config-template-inspector/01_REQUIREMENTS.md
tasks/RT-1-fast-config-template-inspector/02_TEST_PLAN.md
tasks/RT-1-fast-config-template-inspector/03_ACCEPTANCE.md
```

RT-1 must remain offline and local. It does not include network, UDP/TCP recovery, FAST binary decoding, FIX/TWIME, credentials or order sending.

## Task lifecycle

```text
DRAFT
-> READY_FOR_MIMO
-> IN_PROGRESS
-> READY_FOR_REVIEW
-> CHANGES_REQUIRED -> READY_FOR_REVIEW
-> OWNER_REVIEW
-> OWNER_APPROVED
-> DONE
```

No second MiMo task starts while the previous task is in progress, review or changes-required state.

## Immediate next actions

```text
1. Round 6 corrections applied for RT-1 on feat/rt-1-fast-config-inspector.
2. Profile logic hardened: always auto-detect, separate detected/requested/selected/evidence.
3. spectra-1.29 rejects conflicting 1.30 evidence (ID 48, wrong-name ID 47/48).
4. CLI validates --profile values; unsupported values rejected.
5. 6 new Round 6 tests added; all 99 tests pass on Windows.
6. Local verification: 6/6 RT-1 executables, QSH 20/20, Python 3/3, hygiene PASS.
7. Push to PR #16, Issue #14 moved to READY_FOR_REVIEW.
8. Stop — do not merge, do not start RT-2.
```

## Explicit non-goals now

```text
no RT-2 until RT-1 accepted;
no network market-data connection;
no FIX/TWIME;
no FAST binary decoder;
no broker/exchange order sending;
no live or production enablement;
no committed credentials/private connection data;
no QSH semantic rewrite;
no strategy_ready weakening.
```
