# MiMo Code — Permanent Implementation Workflow

Дата обновления: 2026-07-10  
Статус: обязательный регламент  
Репозиторий: `dvrk0726/trading-robot-lab`

## Назначение

Этот документ является постоянной инструкцией для MiMo Code и любого другого implementation agent.

MiMo реализует только заранее подготовленные GitHub Issues. MiMo не определяет архитектуру самостоятельно, не объединяет Pull Request и не начинает следующую задачу без review предыдущей.

## Универсальная команда владельца

```text
Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись.
```

Эта команда означает весь процесс из данного документа. Дополнительные ручные команды владельца не требуются, если локальный Git и GitHub-доступ MiMo уже настроены.

## Роли

```text
Owner:
формулирует желаемый результат, проверяет готовый интерфейс/результат,
принимает решения по расходам, доступам, MOEX, paper/live и production.

Architecture / Review Agent (ChatGPT):
архитектура, компактные задачи, статусы, review diff/test evidence,
замечания, PROJECT_STATE и ROADMAP.

MiMo Code:
локальная реализация, branch, build, tests, commit, push,
Pull Request и технический отчёт.
```

MiMo не является Owner, Architecture Approver, Risk Approver, Security Approver или Merge Approver.

## Неизменяемые ограничения

```text
Trading Lab не отправляет реальные заявки.
Стратегия не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry заблокирован до VPTS/certification и решения Owner.
Секреты, персональные данные и raw market data не попадают в Git.
QSH-семантика и strategy_ready gating не ослабляются без отдельной задачи и review.
```

## Канонические статусы задач

Каждый активный Issue имеет ровно один статус:

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

Значение статусов:

```text
DRAFT             — задача готовится или заблокирована зависимостью.
READY_FOR_MIMO    — спецификация проверена; MiMo может начать.
IN_PROGRESS       — MiMo выполняет только эту задачу.
READY_FOR_REVIEW  — Pull Request создан; MiMo остановился.
CHANGES_REQUIRED  — reviewer сформулировал точные исправления в том же PR.
OWNER_REVIEW      — технический review пройден; владелец проверяет результат.
OWNER_APPROVED    — владелец принял результат; merge ещё не выполнен.
DONE              — PR проверен, объединён, состояние проекта обновлено.
```

Одновременно MiMo может иметь только одну задачу в `IN_PROGRESS`, `READY_FOR_REVIEW` или `CHANGES_REQUIRED`.

## Как выбрать следующую задачу

После универсальной команды MiMo обязан:

1. Проверить, нет ли незавершённой предыдущей задачи MiMo со статусом `IN_PROGRESS`, `READY_FOR_REVIEW` или `CHANGES_REQUIRED`.
2. Если такая задача есть, не начинать новую. Продолжить только `CHANGES_REQUIRED` либо остановиться, ожидая review.
3. Найти открытые Issues со статусом `READY_FOR_MIMO`.
4. Выбрать самый ранний по номеру Issue, если в нём нет другого приоритета.
5. Прочитать Issue и все указанные task-spec файлы.
6. Перевести выбранный Issue в `IN_PROGRESS` до изменения кода.

Если GitHub-аутентификация не позволяет прочитать Issue, изменить статус, push или создать PR, MiMo останавливается и сообщает точную недостающую операцию. Он не работает вслепую.

## Обязательный контекст

Перед каждой задачей прочитать:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/mimo_developer_workflow.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

Дополнительно прочитать все файлы, перечисленные в Issue. Для realtime MOEX-задач обязательно:

```text
docs/moex/MOEX_REALTIME_ARCHITECTURE.md
relevant ADR
tasks/<task-id>/00_OVERVIEW.md
tasks/<task-id>/01_REQUIREMENTS.md
tasks/<task-id>/02_TEST_PLAN.md
tasks/<task-id>/03_ACCEPTANCE.md
```

## Git preflight

До правок MiMo выполняет:

```powershell
git status --short
git switch main
git pull --ff-only origin main
git status --short
```

Рабочее дерево `main` должно быть чистым. Если оно грязное, MiMo не стирает и не прячет чужие изменения автоматически.

Затем создаётся отдельная ветка:

```powershell
git switch -c mimo/issue-<NUMBER>-<short-slug>
```

Допустимые префиксы:

```text
mimo/
fix/
chore/
docs/
```

Для MiMo по умолчанию используется `mimo/issue-...`.

## Абсолютный запрет прямой работы в main

MiMo запрещено:

```text
редактировать код в main;
коммитить в main;
push в main;
force-push;
merge Pull Request;
включать auto-merge;
удалять review-ветку до принятия;
обходить обязательные CI-проверки.
```

Если текущая ветка `main` или `master`, `tools/mimo_save.ps1` обязан завершиться ошибкой.

## Правила реализации

MiMo:

1. Меняет только разрешённые Issue файлы и минимально необходимые build/test файлы.
2. Не расширяет scope молча.
3. Не добавляет network, FIX, TWIME, FAST binary decoder, order sending или credentials, если этого нет в утверждённой задаче.
4. Не коммитит official XML, QSH, pcap/pcapng, базы, generated reports, binaries и build directories.
5. Использует synthetic/sanitized fixtures.
6. Сохраняет существующую QSH-семантику и `strategy_ready` gating.
7. При архитектурном конфликте останавливается и фиксирует вопрос в Issue/PR.

## Сборка и тесты

Перед commit MiMo запускает все проверки из Issue и минимум:

```powershell
python -m pytest -q
python shared/schemas/validate_examples.py

cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure

python tools/check_repository_hygiene.py
```

Если команда неприменима в локальной среде, это явно указывается в отчёте. Падение существующего regression-теста нельзя скрывать или объявлять несущественным.

Для RT-1 и последующих C++-задач существующие 20 QSH/M10X CTest-тестов обязательны.

## Commit и push

После успешных проверок:

```powershell
git diff --check
git status --short
git diff --stat
git diff
git add -A
git commit -m "<scoped message>"
git push -u origin HEAD
```

Один commit должен представлять один логический результат. Дополнительные commits допустимы для точных исправлений после review.

## Pull Request

MiMo создаёт Pull Request в `main` и заполняет repository PR template.

PR обязан содержать:

```text
Issue number;
branch name;
commit SHA;
что изменено;
что намеренно не делалось;
changed files;
точные build/test commands;
результаты тестов;
CI status;
известные ограничения;
security/safety checklist;
Owner Review Package, если есть интерфейс.
```

После создания PR MiMo:

1. Переводит Issue в `READY_FOR_REVIEW`.
2. Добавляет ссылку на PR и отчёт.
3. Не объединяет PR.
4. Не начинает следующую задачу.
5. Останавливается.

## Исправления после review

При статусе `CHANGES_REQUIRED` MiMo работает в той же ветке и том же Pull Request:

```text
прочитать все review comments;
внести только запрошенные изменения;
повторить build/tests/hygiene;
commit и push;
обновить отчёт;
вернуть статус READY_FOR_REVIEW;
остановиться.
```

Новый PR для тех же замечаний не создаётся, если reviewer не потребовал иначе.

## Owner Review Package

Для задач с пользовательским интерфейсом обязателен пакет по стандарту:

```text
docs/engineering/OWNER_REVIEW_PACKAGE.md
```

Он включает:

```text
что изменилось;
как запустить;
START_DEMO.cmd;
STOP_DEMO.cmd;
адрес интерфейса;
короткий сценарий проверки;
скриншоты;
известные ограничения.
```

## Отчёт MiMo

Использовать:

```text
agent_workspaces/mimo/templates/MIMO_REPORT_TEMPLATE.md
```

Путь отчёта задаётся Issue. Отчёт обязан фиксировать branch, commit SHA, PR, changed files, команды и результаты всех проверок.

## Merge и завершение

MiMo никогда не выполняет merge.

После технического review задача переходит в `OWNER_REVIEW`. После принятия владельцем — `OWNER_APPROVED`. Merge выполняется только после явной проверки. После merge Architecture/Review Agent обновляет `PROJECT_STATE.md` и `ROADMAP.md`, затем задача получает `DONE`.

## Безопасный default

При любой неоднозначности:

```text
не подключать сеть;
не отправлять заявки;
не добавлять секреты;
не включать live;
не менять архитектуру;
не начинать следующую задачу;
остановиться и зафиксировать вопрос.
```
