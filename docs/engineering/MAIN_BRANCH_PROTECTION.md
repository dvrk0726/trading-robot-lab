# Main Branch Protection

Дата: 2026-07-10  
Статус: one-time repository owner setup

## Почему это отдельный шаг

Документы, scripts и CI запрещают прямую разработку в `main`, но окончательное server-side принуждение выполняет GitHub branch ruleset.

Текущий GitHub connector проекта не предоставляет действие создания ruleset. Поэтому владелец один раз включает его в интерфейсе GitHub после merge workflow Pull Request.

## Открыть настройки

```text
Repository dvrk0726/trading-robot-lab
-> Settings
-> Rules
-> Rulesets
-> New ruleset
-> New branch ruleset
```

## Основные параметры

```text
Ruleset name: Protect main
Enforcement status: Active
Target branches: Include default branch
```

Не добавлять bypass actors. Если GitHub требует оставить admin bypass, выбрать вариант только через Pull Request, а не direct push.

## Включить правила

```text
Restrict deletions
Block force pushes
Require a pull request before merging
Require status checks to pass
Require conversation resolution before merging
```

Для одного владельца на текущем этапе:

```text
Required approvals: 0
Dismiss stale approvals: optional
Require review from Code Owners: off
```

Причина: технический review выполняется Architecture/Review Agent через diff и CI, но он не является отдельным GitHub-пользователем, который может оставить формальное approval. PR и checks при этом остаются обязательными.

## Required status checks

После первого успешного запуска CI добавить точные checks:

```text
Repository hygiene
Python tests and contracts
C++ QSH M10X regression (20 tests)
```

Включить:

```text
Require branches to be up to date before merging
```

Если GitHub ещё не показывает check в списке, сначала открыть workflow PR и дождаться хотя бы одного run.

## Auto-merge

Проверить:

```text
Settings
-> General
-> Pull Requests
-> Allow auto-merge: disabled
```

MiMo также не имеет права вызывать auto-merge или merge через API.

## Не включать сейчас без отдельного решения

```text
Require signed commits
Merge queue
Required deployments
Code Owner approval
Push restrictions to a single actor
```

Эти функции могут быть добавлены позже, но не должны случайно заблокировать единственного владельца на текущем этапе.

## Проверка

После сохранения ruleset:

1. Открыть новую test branch.
2. Убедиться, что direct push в `main` отклоняется или помечается нарушением ruleset.
3. Убедиться, что PR нельзя merge до прохождения трёх checks.
4. Убедиться, что unresolved conversation блокирует merge.
5. Убедиться, что force-push и deletion `main` запрещены.

## Acceptance для Issue #1

Issue #1 считается полностью завершённым только после:

```text
workflow PR merged;
labels synchronized;
CI checks успешно появились;
Protect main ruleset включён;
auto-merge выключен;
Issue #14 переведён из DRAFT в READY_FOR_MIMO отдельным решением.
```

До этого RT-1 не запускается.
