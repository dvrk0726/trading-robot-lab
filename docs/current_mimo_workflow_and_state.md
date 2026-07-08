# Current MiMo Workflow and Project State

Дата: 2026-07-08
Статус: active handoff note

## Назначение

Этот документ нужен другому ИИ-агенту или разработчику, чтобы быстро понять:

```text
что уже сделано;
как сейчас работает процесс с MiMo;
какие команды использует владелец;
что нужно делать дальше;
что запрещено.
```

## Текущий исполнитель кода

Основной локальный coding agent на компьютере владельца:

```text
Xiaomi MiMo Code
```

Локальная папка проекта:

```text
C:\ProjectsHFT\trading-robot-lab
```

Рабочая модель:

```text
xiaomi/mimo-v2.5-pro
```

## Главная архитектура проекта

Проект строится как две отдельные системы плюс общий слой контрактов:

```text
Trading Lab      — исследования, backtest, replay, отчеты, анализ.
Trading Runtime  — легкое исполнение утвержденных Strategy Packages.
Shared Contracts — общие форматы данных, сигналов, заявок, risk decisions и отчетов.
```

Ключевое решение зафиксировано в:

```text
decisions/ADR-0002-two-system-lab-runtime-architecture.md
```

## Правила безопасности

MiMo и любой другой implementation agent не имеет права:

```text
добавлять broker connection;
добавлять live trading;
добавлять реальные API keys;
создавать .env с секретами;
писать код отправки реальных заявок;
обходить RiskEngine;
ставить live_approved=true;
коммитить .mimocode/;
коммитить secrets/, private/, credentials/;
```

## Как мы работаем с MiMo

Используется короткий task-file workflow.

Ассистент/Strategy Master создает в GitHub короткий файл задачи:

```text
M1.md
M2.md
M3.md
M4.md
...
```

Пользователь на компьютере делает:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M4.md"
.\tools\mimo_save.ps1 "Short commit message"
```

После этого пользователь пишет в чат:

```text
готово
```

Ассистент сам проверяет изменения и отчет через GitHub connector.

## Почему так сделано

Пользователь не хочет вручную копировать длинные промпты, читать длинные отчеты и каждый раз показывать много скриншотов.

Цель процесса:

```text
минимум действий пользователя;
MiMo работает по короткому файлу-задаче;
MiMo пишет подробный отчет в репозиторий;
пользователь делает один save/push;
ассистент проверяет GitHub сам.
```

## Скрипт сохранения работы MiMo

Создан helper script:

```text
tools/mimo_save.ps1
```

Он делает:

```text
git status
git add -A
git commit -m <message>
git push
```

И должен остановиться при опасных файлах:

```text
.env
.env.*
secrets/
private/
credentials/
.mimocode/
*.key
*.pem
*.pfx
*.p12
```

## Уже выполненные задачи

### Initial MiMo context read

MiMo прочитал базовый контекст и создал отчет:

```text
agent_workspaces/mimo/reports/2026-07-08-initial-context-read.md
```

Также добавлено:

```text
MIMO_START.md
.gitignore updated with .mimocode/
```

### M1 / Issue #2 — Project skeleton

Выполнено и запушено.

Создан каркас проекта:

```text
apps/lab/backend/
apps/lab/frontend/
apps/lab/research/
apps/lab/reports/
apps/runtime/core/
apps/runtime/risk/
apps/runtime/order_manager/
apps/runtime/strategy_loader/
apps/runtime/telemetry/
apps/runtime/execution/
shared/contracts/
shared/schemas/
shared/strategy_sdk/
shared/test_vectors/
strategy_packages/examples/
```

Отчет:

```text
agent_workspaces/mimo/reports/2026-07-08-issue-002-project-skeleton.md
```

### M2 / Issue #6 — First shared contracts

Выполнено и запушено.

Созданы первые JSON schemas:

```text
shared/schemas/market_event.schema.json
shared/schemas/feature_snapshot.schema.json
shared/schemas/strategy_signal.schema.json
shared/schemas/order_intent.schema.json
shared/schemas/risk_decision.schema.json
```

Создан/обновлен flow:

```text
shared/contracts/contract_flow.md
shared/contracts/README.md
shared/schemas/README.md
```

Отчет:

```text
agent_workspaces/mimo/reports/2026-07-08-issue-006-shared-contracts.md
```

### M3 / Issue #7 — Test vectors and validation

Выполнено и запушено.

Создано:

```text
shared/test_vectors/basic_flow/01_market_event.json
shared/test_vectors/basic_flow/02_feature_snapshot.json
shared/test_vectors/basic_flow/03_strategy_signal.json
shared/test_vectors/basic_flow/04_order_intent.json
shared/test_vectors/basic_flow/05_risk_decision.json
shared/test_vectors/basic_flow/README.md
shared/schemas/validate_examples.py
```

MiMo запустил:

```text
python shared/schemas/validate_examples.py
```

Результат: все 5 examples valid.

Отчет:

```text
agent_workspaces/mimo/reports/2026-07-08-issue-007-test-vectors.md
```

## Текущая задача

### M4 / Issue #8 — Strategy Package standard

Создано, но еще не выполнено пользователем на локальном MiMo.

Файлы уже созданы в GitHub:

```text
M4.md
```

Issue:

```text
#8 [ARCH] Define Strategy Package standard and dummy package
```

Что должен сделать MiMo:

```text
strategy_packages/README.md
strategy_packages/STRATEGY_PACKAGE_STANDARD.md
strategy_packages/examples/dummy_no_trade_v001/manifest.yaml
strategy_packages/examples/dummy_no_trade_v001/params.yaml
strategy_packages/examples/dummy_no_trade_v001/risk_limits.yaml
strategy_packages/examples/dummy_no_trade_v001/instruments.yaml
strategy_packages/examples/dummy_no_trade_v001/approval.json
strategy_packages/examples/dummy_no_trade_v001/validation_report.json
strategy_packages/examples/dummy_no_trade_v001/package.hash.example
strategy_packages/examples/dummy_no_trade_v001/README.md
```

Критичное правило:

```text
approval.json must keep live_approved=false
```

Команды для пользователя:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M4.md"
.\tools\mimo_save.ps1 "Add strategy package standard"
```

Потом пользователь пишет:

```text
готово
```

## Как проверять работу MiMo

После слова `готово` ассистент проверяет:

```text
1. Отчет в agent_workspaces/mimo/reports/.
2. Список созданных/измененных файлов.
3. Нет broker/live/secrets.
4. live_approved не стал true.
5. Работа соответствует Issue.
6. Следующие шаги и открытые вопросы в отчете.
```

## Ближайшие следующие задачи после M4

Вероятные следующие этапы:

```text
M5 — Strategy Package validator / package hash checker.
M6 — dummy package load check.
M7 — Runtime skeleton without broker.
M8 — formal STRAT for RI / Synthetic Index Lead-Lag.
```

Но следующую задачу создавать только после review результата M4.
