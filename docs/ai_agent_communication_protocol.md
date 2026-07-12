# AI Agent Communication Protocol

Дата обновления: 2026-07-12  
Статус: практический протокол

## Главный принцип

```text
Issue = задача, статус, dependency или owner decision.
Pull Request = конкретное изменение и review.
MiMo report = команды, evidence, ограничения и handoff.
ADR/docs = долговременные решения.
PROJECT_STATE.md = текущее проверенное состояние.
```

Один общий `AI_CHAT.md` не используется.

## Task Request

Issue должен содержать:

```text
Status: DRAFT или READY_FOR_MIMO
From / To
Objective
Required context
Dependencies
Allowed scope
Existing behavior to preserve
Explicit non-goals
Acceptance criteria
Tests/checks
Report path
Owner Review Package requirement
Handoff
```

Пример статуса до полной готовности:

```text
DRAFT
```

MiMo не начинает `DRAFT`.

Перед выдачей MiMo-команды задача должна пройти scope-freeze protocol (см. раздел ниже). Scope должен быть зафиксирован и утверждён владельцем до перехода в `READY_FOR_MIMO`.

## Ready handoff to MiMo

Architecture/Review Agent переводит Issue в `READY_FOR_MIMO` только после проверки полноты спецификации, закрытия зависимостей и завершения scope-freeze protocol. Scope-freeze checklist должен быть полностью закрыт; anti-overengineering check должен подтвердить минимальность решения.

Универсальная команда:

```text
Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись.
```

MiMo обязан выбрать одну задачу, перевести её в `IN_PROGRESS`, создать отдельную ветку и не начинать следующую задачу.

## Result / Handoff

После реализации MiMo создаёт Pull Request и report. Handoff содержит:

```text
Issue number;
branch;
commit SHA;
Pull Request number/link;
changed files;
what was implemented;
what was intentionally not implemented;
exact commands;
local test results;
GitHub Actions status;
known limitations;
security/hygiene result;
Owner Review Package path if applicable.
```

После этого Issue получает `READY_FOR_REVIEW`, и MiMo останавливается.

## Review comments

Review ведётся внутри Pull Request.

Reviewer обязан формулировать замечание как проверяемое действие:

```text
Problem:
Evidence:
Required change:
Files/scope:
Verification command or acceptance check:
```

Не использовать расплывчатые комментарии вида «переделай лучше».

При замечаниях Issue получает `CHANGES_REQUIRED`. MiMo исправляет тот же branch/PR, повторяет проверки, обновляет report, возвращает `READY_FOR_REVIEW` и снова останавливается.

## Owner Decision Request

Используется, если требуется решение по:

```text
расходам;
доступам;
MOEX/broker interaction;
hardware;
private network parameters;
paper/live/production stage;
security or risk exception;
существенному изменению архитектуры.
```

Формат:

```text
Reason:
Decision needed:
Options:
Recommendation:
Safe default:
Impact on current task:
```

Safe default — не выполнять рискованное действие до решения.

## Blocker

Blocker фиксируется в Issue, когда отсутствует dependency, specification, required access, compatible environment или согласованное решение.

```text
Blocker:
Observed evidence:
Why work cannot safely continue:
Minimal action needed:
Work that remains safe and complete:
```

Blocker не маскируется как успешный результат.

## Status transitions

```text
DRAFT
-> READY_FOR_MIMO
-> IN_PROGRESS
-> READY_FOR_REVIEW
-> CHANGES_REQUIRED -> READY_FOR_REVIEW (repeat as needed)
-> OWNER_REVIEW
-> OWNER_APPROVED
-> DONE
```

Каждый Issue имеет ровно один status label. Дополнительные labels описывают область: `mimo`, `cpp`, `python`, `architecture`, `docs`, `security`, `risk`, `research`, `blocked`, `needs-owner-decision`, `no-live`.

## Scope-freeze protocol

Перед выдачей MiMo-команды задача должна пройти mandatory scope-freeze protocol. Этот протокол дополняет, но не заменяет существующие GitHub Issue status labels (`DRAFT`, `READY_FOR_MIMO`, `IN_PROGRESS` и остальные).

### Preparation/execution states

```text
RESEARCH
SCOPE_FROZEN
OWNER_AUTHORIZED
MIMO_RUNNING
ARCHITECTURE_REVIEW
ACCEPTED
CHANGES_REQUIRED
```

- `RESEARCH` — активное исследование: defect/objective формулируется, evidence собирается, scope определяется.
- `SCOPE_FROZEN` — все checklist-пункты закрыты, scope зафиксирован и не изменяется.
- `OWNER_AUTHORIZED` — владелец утвердил frozen scope.
- `MIMO_RUNNING` — MiMo выполняет утверждённую задачу.
- `ARCHITECTURE_REVIEW` — PR создан, Architecture/Review Agent проверяет результат.
- `ACCEPTED` / `CHANGES_REQUIRED` — итог проверки.

**No MiMo command may be issued during `RESEARCH`.** Задача остаётся в `RESEARCH` до тех пор, пока все пункты scope-freeze checklist не закрыты.

### Scope-freeze checklist

Перед запросом owner authorization Architecture/Review Agent обязан подтвердить каждый пункт:

```text
[ ] defect/objective подтверждён кодом, тестами или authoritative sources
[ ] proposed change является минимальным для текущего approved gate
[ ] нет unresolved фактов, способных материально изменить реализацию
[ ] перечислены exact allowed files
[ ] перечислены exact acceptance checks/tests
[ ] перечислены explicit non-goals
[ ] задача является одной небольшой логической работой
[ ] задача не поглощает следующий roadmap item
[ ] reviewer выполнил финальную попытку уменьшить scope
```

**Любой незакрытый пункт удерживает задачу в `RESEARCH`.**

### Anti-overengineering check

Перед scope freeze необходимо ответить на три вопроса:

```text
1. Требуется ли это утверждёнными acceptance criteria?
2. Подтверждено ли authoritative profile/code evidence?
3. Можно ли исправить подтверждённый defect меньшим изменением?
```

Если существует меньшее compliant решение, необходимо выбрать его.

### One active canonical MiMo task

Текущая коррекция может иметь только одну активную authorization. Каноническая постановка должна содержать:

```text
base/full head SHA
current gate
allowed files
required changes
acceptance tests
explicit non-goals
owner authorization
```

Superseded authorization должна быть явно помечена как non-executable. Нельзя создавать цепочки неоднозначных активных prompts. Предпочтительна одна каноническая запись задачи вместо серии замещающих инструкций.

### Scope lock after launch

После запуска MiMo scope заблокирован. Нельзя:

```text
добавлять требования;
переименовывать задачу;
переразбивать задачу;
просить MiMo импровизировать.
```

Если новая информация делает specification неверной:

```text
остановить выполнение или позволить bounded run завершиться без следующего этапа;
проверить актуальное GitHub state;
вернуть задачу в RESEARCH;
заморозить новый scope;
получить новое owner authorization.
```

### Stable roadmap naming

Working phases могут декомпозировать approved gate, но должны быть представлены как стабильная иерархия. Нельзя представлять sub-phases как replacement roadmaps или вводить бесконечную вложенную нумерацию.

### Owner is not the final specification safety barrier

Owner approval остаётся обязательным, но Architecture/Review Agent отвечает за полноту scope, минимальность и evidence. Процесс не должен зависеть от того, что владелец заметит technical scope drift.

## Pull Request rules

```text
base = main;
head = dedicated feature branch;
Issue linked;
PR template completed;
CI evidence included;
no auto-merge;
no merge by MiMo;
no next task before review.
```

Top-level PR conversation используется для итоговых замечаний. Inline comments — для конкретной строки или API boundary.

## Review routing

```text
Architecture/Review Agent — scope, contracts, dependencies, architecture, tests.
Strategy Agent            — signals, formulas and research assumptions.
Risk reviewer             — RiskEngine, limits and protections.
Security reviewer         — credentials, private artifacts, scripts and permissions.
Owner                     — visible result and owner-gated decisions.
```

## Owner review

После технического принятия Issue получает `OWNER_REVIEW`.

Для UI владелец получает пакет по `docs/engineering/OWNER_REVIEW_PACKAGE.md`: launch/stop scripts, адрес, короткий сценарий, screenshots и ограничения.

Owner feedback превращается в точные PR comments или отдельный Issue. После принятия используется `OWNER_APPROVED`; это ещё не merge.

## Completion

`DONE` разрешён только когда:

```text
PR merged after review;
required checks passed;
review comments resolved;
Owner approval obtained where required;
PROJECT_STATE.md and ROADMAP.md updated if state changed;
follow-up defects recorded explicitly.
```

## Безопасная коммуникация

Никогда не помещать в Issues, PR comments, reports или docs реальные credentials, персональные данные, private connection parameters или raw market data.
