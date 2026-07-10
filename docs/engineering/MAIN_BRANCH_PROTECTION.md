# Main Branch Protection

Дата: 2026-07-10  
Статус: one-time repository owner decision/setup

## Что уже защищает main без платной функции

Repository process уже запрещает direct implementation в `main`:

```text
MiMo instruction requires a feature branch and Pull Request;
tools/mimo_save.ps1 refuses main/master;
CI runs on every Pull Request;
MiMo cannot merge or enable auto-merge;
one task must be reviewed before the next starts.
```

Это обязательные правила независимо от GitHub plan.

## Ограничение GitHub plan

Для private repository server-side branch protection/rulesets могут требовать GitHub Pro, Team или Enterprise.

Нельзя автоматически покупать или менять plan. Owner сначала проверяет доступность настройки и отдельно решает вопрос расходов.

Текущий GitHub connector проекта не предоставляет действие создания ruleset/branch protection rule, поэтому server-side настройка выполняется один раз через GitHub UI, если она доступна на текущем plan.

## Вариант A — Rulesets доступны

Открыть:

```text
Repository dvrk0726/trading-robot-lab
-> Settings
-> Rules
-> Rulesets
-> New ruleset
-> New branch ruleset
```

Параметры:

```text
Ruleset name: Protect main
Enforcement status: Active
Target branches: Include default branch
```

Включить:

```text
Restrict deletions
Block force pushes
Require a pull request before merging
Require status checks to pass
Require conversation resolution before merging
```

Для одного владельца:

```text
Required approvals: 0
Require review from Code Owners: off
Bypass actors: none, если GitHub позволяет
```

Если нужен admin bypass, выбрать режим только через Pull Request, а не разрешение direct push.

## Вариант B — доступна только Branch protection rule

Открыть:

```text
Repository
-> Settings
-> Branches
-> Add branch protection rule
Branch name pattern: main
```

Включить эквивалентные правила:

```text
Require a pull request before merging
Require status checks before merging
Require conversation resolution before merging
Do not allow bypassing the above settings, если доступно
Allow force pushes: off
Allow deletions: off
```

## Required status checks

После первого успешного CI run добавить точные уникальные checks:

```text
Repository hygiene
Python tests and contracts
C++ QSH M10X regression (20 tests)
```

Также включить `Require branches to be up to date before merging`, если эта опция доступна.

## Если защита недоступна на текущем plan

Не покупать plan автоматически.

Зафиксировать Owner Decision:

```text
Option 1: upgrade GitHub plan and enable server-side protection;
Option 2: temporarily accept procedural protection for the private solo repository.
```

При Option 2 остаются обязательными:

```text
feature branch;
Pull Request;
all CI checks pass;
Architecture/Review Agent review;
manual owner merge only;
no direct main work by MiMo;
no auto-merge;
no next task before review.
```

Это не считается полной server-side защитой. Ограничение явно записывается в Issue #1 и `PROJECT_STATE.md`.

## Auto-merge

Проверить:

```text
Settings
-> General
-> Pull Requests
-> Allow auto-merge: disabled
```

## Не включать без отдельного решения

```text
Require signed commits
Merge queue
Required deployments
Code Owner approval
Push restrictions to a single actor
```

## Проверка при доступной защите

1. Создать test branch.
2. Убедиться, что direct push в `main` блокируется.
3. Убедиться, что merge невозможен до трёх successful checks.
4. Убедиться, что unresolved conversation блокирует merge.
5. Убедиться, что force-push и deletion `main` запрещены.

## Acceptance для Issue #1

Обязательные items:

```text
workflow PR merged;
labels synchronized;
CI checks successful;
auto-merge disabled;
Owner explicitly selected branch-protection option;
Issue #14 released separately only after process acceptance.
```

Для полной защиты предпочтителен активный ruleset/branch protection rule. Если функция недоступна без платного plan, Owner должен явно принять временное procedural limitation до RT-1.
