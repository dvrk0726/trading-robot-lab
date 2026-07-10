# AI Context

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Текущий gate: review and owner acceptance of workflow PR before RT-1

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

## Подтверждённое состояние historical contour

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
FAST 1.29.1;
FAST_9.0 templates.xml;
T0 configuration.xml;
FIX SPECTRA;
VPTS requirements;
MOEX realtime architecture.
```

QuickFAST не является production hot-path основой. Планируется специализированный C++ SPECTRA FAST decoder, но он не входит в RT-1.

MOEX test-access questionnaire отправлена. MOEX запросила дополнительные сведения для создания логинов.

Запрещено сохранять в Git:

```text
personal data;
static IP;
logins/passwords;
private connection addresses/ports/identifiers;
official owner-provided private artifacts.
```

## Текущий process gate

```text
Issue #1: [ARCH] Establish MiMo branch, Pull Request and CI workflow
Pull Request #15: chore: establish MiMo branch, Pull Request and CI workflow
Branch: chore/issue-1-mimo-pr-workflow
Merge: not performed
RT-1: blocked
```

Workflow package включает:

```text
permanent MiMo instruction;
universal READY_FOR_MIMO command;
branch-only implementation;
canonical task statuses;
one-task-at-a-time rule;
Pull Request template;
Python/C++/hygiene GitHub Actions;
20-test QSH/M10X regression gate;
secret/raw-data/large-file hygiene checker;
Owner Review Package;
label synchronization;
no auto-merge/no MiMo merge;
main branch protection decision guide.
```

Полный CI run #8 для PR #15 подтвердил:

```text
Repository hygiene: PASS
Python tests and contracts: PASS
C++ QSH M10X regression: PASS
Expected CTest inventory: exactly 20
All 20 regression tests: PASS
```

Любой новый head commit PR должен повторно пройти те же checks перед merge.

## Server-side защита main

Для private repository availability branch protection/rulesets зависит от GitHub plan. Owner не покупает и не меняет plan автоматически.

После принятия PR #15 Owner выбирает один вариант:

```text
A. Если функция доступна — включить ruleset/branch protection и required checks.
B. Если функция недоступна — отдельно решить вопрос upgrade либо явно принять временное procedural limitation для private solo repository.
```

Даже при временном limitation обязательны feature branch, Pull Request, успешный CI, Architecture/Review, manual owner merge, no auto-merge и запрет direct main work для MiMo.

Инструкция:

```text
docs/engineering/MAIN_BRANCH_PROTECTION.md
```

## RT-1

```text
Issue #14: [MIMO][C++] RT-1 FAST configuration/templates inspector
Status: DRAFT
Blocked by: Issue #1 / PR #15 acceptance
Implementation: not started
MiMo branch/commit/PR: none
```

Task package:

```text
tasks/RT-1-fast-config-template-inspector/00_OVERVIEW.md
tasks/RT-1-fast-config-template-inspector/01_REQUIREMENTS.md
tasks/RT-1-fast-config-template-inspector/02_TEST_PLAN.md
tasks/RT-1-fast-config-template-inspector/03_ACCEPTANCE.md
```

RT-1 scope:

```text
local C++20/CMake CLI inspector;
parse configuration.xml and templates.xml;
inspect ORDERS-LOG/FUT-INFO structure;
validate required template IDs and ordered field metadata;
calculate input SHA-256;
produce human-readable and deterministic JSON output;
use synthetic/sanitized fixtures;
run new tests and existing 20 QSH/M10X regressions.
```

RT-1 non-goals:

```text
no network;
no UDP/TCP recovery;
no FAST binary decoder;
no FIX/TWIME;
no order sending;
no credentials;
no QuickFAST production dependency;
no committed official private XML;
no QSH semantic changes;
no strategy_ready weakening.
```

RT-1 cannot move to `READY_FOR_MIMO` until:

```text
current head of PR #15 passes all checks;
Architecture/Review accepts the diff;
Owner accepts the workflow and branch-protection option;
PR #15 is merged manually;
canonical labels are synchronized;
auto-merge remains disabled;
Issue #1 is completed;
Architecture/Review explicitly releases Issue #14.
```

## AI workflow

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

MiMo выполняет одну задачу, создаёт feature branch и PR, запускает проверки, готовит отчёт, не выполняет merge и останавливается.

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
1. Confirm all checks on the current PR #15 head.
2. Complete Architecture/Review of the final diff.
3. Owner reviews and accepts or requests exact changes.
4. Owner selects the branch-protection option without automatic spending.
5. Merge PR #15 manually only after acceptance.
6. Synchronize labels and apply the selected main protection mode.
7. Mark Issue #1 DONE.
8. Only then move Issue #14 to READY_FOR_MIMO.
9. Run MiMo once for RT-1; MiMo stops at a new Pull Request.
```
