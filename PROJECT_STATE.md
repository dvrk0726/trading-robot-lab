# Project State

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: workflow Pull Request prepared and verified; owner acceptance pending before RT-1

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

## Current active work — Issue #1 / PR #15

```text
Issue #1: [ARCH] Establish MiMo branch, Pull Request and CI workflow
Branch: chore/issue-1-mimo-pr-workflow
Pull Request: #15
Merge: not performed
Owner acceptance: pending
RT-1: blocked
```

Implemented in PR #15:

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

Verified CI evidence from full run #8:

```text
Repository hygiene: PASS
Python tests and contracts: PASS
C++ QSH M10X regression: PASS
CTest inventory: exactly 20
All 20 tests: PASS
```

Every later PR head must also pass these checks before merge.

## Remaining Issue #1 acceptance

```text
current PR #15 head passes all checks;
Architecture/Review accepts final diff;
Owner accepts workflow or requests scoped changes;
Owner chooses branch-protection option;
PR #15 merged manually;
labels synchronized after merge;
auto-merge confirmed disabled;
selected main protection mode recorded;
Issue #1 moved to DONE.
```

For private repositories GitHub plan may limit server-side rulesets/branch protection. No plan purchase or upgrade is automatic. Owner chooses:

```text
A. Enable server-side ruleset/branch protection if available.
B. Decide on upgrade or explicitly accept temporary procedural protection.
```

Procedural protection always requires branch, Pull Request, successful CI, Architecture/Review, manual owner merge, no auto-merge and no direct main work by MiMo.

See `docs/engineering/MAIN_BRANCH_PROTECTION.md`.

## RT-1 status

```text
Issue #14: [MIMO][C++] RT-1 FAST configuration/templates inspector
Status: DRAFT
Implementation: not started
Blocked by: Issue #1 / PR #15 acceptance and post-merge process setup
```

RT-1 task package is complete:

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
1. Confirm final CI on the current PR #15 head.
2. Complete Architecture/Review of the final diff.
3. Owner reviews the prepared process.
4. Owner selects the main protection option without automatic spending.
5. Merge PR #15 manually only after acceptance.
6. Synchronize labels and apply/record the selected protection mode.
7. Move Issue #1 to DONE.
8. Only then move Issue #14 to READY_FOR_MIMO.
9. Run the universal MiMo command once for RT-1.
```

## Explicit non-goals now

```text
no RT-1 implementation before the process gate;
no RT-2;
no network market-data connection;
no FIX/TWIME;
no FAST binary decoder;
no broker/exchange order sending;
no live or production enablement;
no committed credentials/private connection data;
no QSH semantic rewrite;
no strategy_ready weakening.
```
