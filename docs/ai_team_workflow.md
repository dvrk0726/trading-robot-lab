# AI Team Workflow

Дата обновления: 2026-07-10  
Статус: обязательный регламент

## Рабочая схема

```text
Owner intent
-> Architecture/Review Agent prepares Issue and specs
-> READY_FOR_MIMO
-> MiMo creates branch, implements and tests
-> Pull Request + CI
-> Architecture/Review
-> Owner review when required
-> reviewed merge
-> PROJECT_STATE/ROADMAP update
```

## Обязательный контекст

Перед существенной работой читать:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

MiMo дополнительно читает `docs/mimo_developer_workflow.md`, Issue, все task specs и relevant ADR.

## Роли

### Owner

Формулирует желаемый результат, проверяет готовый результат и принимает решения по расходам, доступам, MOEX, hardware, paper/live и production.

### Architecture / Review Agent

Отвечает за архитектуру, компактные задачи, зависимости, review diff/CI/test evidence, замечания, `PROJECT_STATE.md` и `ROADMAP.md`.

### MiMo Code

Implementation Agent. Выполняет одну готовую задачу в отдельной ветке, запускает проверки, создаёт commit, push, Pull Request и отчёт. Не меняет архитектуру самостоятельно, не выполняет merge и не начинает следующую задачу до review.

### Strategy / Research Agent

Формализует гипотезы, требования к данным, replay/backtest планы и evidence. Не выдаёт historical result за доказательство текущей прибыльности.

## Неизменяемые границы

```text
Trading Lab не отправляет реальные заявки.
Strategy создаёт Signal/OrderIntent и не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production заблокирован certification/owner gate.
C++ — low-level data/realtime/runtime contour.
Python — research, reports and UI.
Secrets и raw market data не хранятся в Git.
```

## Канонические статусы

Каждый implementation Issue имеет ровно один статус:

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

```text
DRAFT            — scope/dependency/test/decision ещё не готовы.
READY_FOR_MIMO   — задача полностью определена и может быть начата.
IN_PROGRESS      — MiMo работает только над этой задачей.
READY_FOR_REVIEW — PR создан; MiMo остановился.
CHANGES_REQUIRED — точные исправления в том же PR.
OWNER_REVIEW     — технический review пройден; проверяет владелец.
OWNER_APPROVED   — владелец принял; merge ещё отдельное действие.
DONE             — PR объединён и состояние проекта обновлено.
```

Одновременно у MiMo может быть только одна задача в `IN_PROGRESS`, `READY_FOR_REVIEW` или `CHANGES_REQUIRED`.

## READY_FOR_MIMO gate

Статус разрешён только если зафиксированы:

```text
bounded objective;
required context;
dependencies;
allowed files/scope;
explicit non-goals;
existing behavior to preserve;
acceptance criteria;
exact tests/checks;
report path;
Owner Review Package requirement.
```

## Issue и task specs

Одна задача = один Issue. Большая спецификация хранится отдельно:

```text
tasks/<task-id>/00_OVERVIEW.md
tasks/<task-id>/01_REQUIREMENTS.md
tasks/<task-id>/02_TEST_PLAN.md
tasks/<task-id>/03_ACCEPTANCE.md
```

Issue остаётся компактным и ссылается на эти файлы.

## Branch / Pull Request

```text
main -> dedicated branch -> commit -> push branch -> Pull Request -> CI -> review
```

Для MiMo:

```text
mimo/issue-<NUMBER>-<short-slug>
```

Запрещены direct code push в `main`, force-push, auto-merge, merge MiMo, unrelated changes и следующая задача до review предыдущей.

## CI baseline

```text
repository hygiene;
Python tests and contract validation;
C++ QSH/M10X build and all 20 regression tests.
```

Task-specific checks добавляются поверх baseline. Build без acceptance review недостаточен.

## Review routing

```text
Architecture review — module boundaries, contracts, layout, Python/C++ boundary.
Strategy review     — signals, formulas, entry/exit, backtest assumptions.
Risk review         — RiskEngine, limits, kill switch and protections.
Security review     — credentials, network, scripts, CI permissions, private artifacts.
Owner review        — user-facing result and access/cost/hardware/stage decisions.
```

Для UI используется `docs/engineering/OWNER_REVIEW_PACKAGE.md`.

## Каналы коммуникации

```text
Issue        — task, dependency, blocker, owner decision.
Pull Request — implementation, review and corrections.
MiMo report  — commands, results, limitations and handoff.
ADR/docs     — durable decisions and rules.
PROJECT_STATE.md — current verified state.
ROADMAP.md       — ordered gates and future work.
```

Общий chat-файл в репозитории не используется.

## Handoff

Implementation agent сообщает:

```text
Issue;
branch;
commit SHA;
Pull Request;
changed files;
implemented and intentionally omitted scope;
commands and results;
CI status;
known limitations;
requested review;
Owner Review Package path when applicable.
```

## Stop conditions

Остановиться и зафиксировать blocker при запросе credentials/private parameters, обходе safety gate, неутверждённом расширении scope, raw data в Git, архитектурном конфликте или незавершённом предыдущем MiMo PR.
