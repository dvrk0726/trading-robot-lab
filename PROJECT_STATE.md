# PROJECT_STATE

Дата последнего обновления: 2026-07-08
Репозиторий: `dvrk0726/trading-robot-lab`
Статус: архитектурно зафиксирована двухсистемная модель `Trading Lab + Trading Runtime + Shared Contracts`

## Назначение файла

Этот файл фиксирует текущее состояние проекта: что уже сделано, какие решения приняты и какой следующий шаг.

Любой ИИ-агент или разработчик должен читать этот файл после `AI_CONTEXT.md`.

## Главная цель проекта

Построить не одного монолитного торгового робота, а дисциплинированную платформу для исследования и дальнейшего безопасного исполнения торговых стратегий на фьючерсах MOEX.

Целевая модель:

```text
Trading Lab      — исследование, тестирование, replay, отчеты, анализ.
Trading Runtime  — легкое исполнение утвержденных Strategy Packages.
Shared Contracts — единые форматы данных, сигналов, заявок, risk decisions и отчетов.
```

На текущем этапе задача не в live trading и не в подключении брокера.

Текущая задача:

```text
исследование -> статистическая проверка -> backtest -> replay -> paper -> owner gate -> live later
```

## Принятое архитектурное решение

Принято решение использовать две отдельные программы и общий слой контрактов:

```text
decisions/ADR-0002-two-system-lab-runtime-architecture.md
```

Суть решения:

```text
Trading Lab не умеет отправлять реальные заявки.
Trading Runtime не запускает неутвержденные стратегии.
Стратегии передаются из Lab в Runtime только через Strategy Package.
Каждый OrderIntent проходит RiskEngine.
Live disabled by default and owner-gated.
```

## Почему принято это решение

После анализа материалов по `robot_uralpro`, Avellaneda-Stoikov, market-making алгоритмам, практическим тестам SIU5 и статистике стало ясно:

```text
исследовательский код нельзя смешивать с live execution;
боевой runtime должен быть легким и строгим;
но Lab и Runtime должны иметь общие контракты, чтобы тестовая логика не расходилась с реальной.
```

Главный риск, который предотвращает архитектура:

```text
в backtest стратегия работает по одной логике,
а в live runtime — по другой.
```

Для этого вводятся:

```text
Shared Contracts
Strategy Package
Signal Parity Test
Runtime Package Validation
Risk Engine Gate
```

## Что уже сделано

### 1. Создан приватный GitHub-репозиторий

```text
trading-robot-lab
```

Владелец:

```text
dvrk0726
```

Назначение: хранить код, документацию, архитектурные решения, research notes, идеи стратегий, оценки и контекст для ИИ-агентов.

### 2. Подключен доступ ChatGPT/GitHub

Доступ к репозиторию проверен.

Доступны:

```text
чтение файлов
создание файлов
обновление файлов
создание GitHub Issues
работа с репозиторием через GitHub connector
```

### 3. Созданы базовые документы

```text
README.md
AI_CONTEXT.md
PROJECT_STATE.md
SECURITY.md
ROADMAP.md
.env.example
.gitignore
```

`AI_CONTEXT.md` является главным стартовым файлом для нового ИИ-агента.

### 4. Создана архитектурная документация

```text
docs/01_hybrid_architecture.md
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
docs/system_architecture_and_user_interface_requirements.md
docs/trading_robot_vision_and_research_plan.md
```

Приняты базовые архитектурные принципы:

```text
Python = research, analysis, prototype, backtest, reporting.
C++    = future low-latency core, risk, replay/paper/live execution gateway.
Trading Lab and Trading Runtime are separate applications.
Shared Contracts prevent logic drift.
```

Главное правило:

```text
strategy -> risk -> execution -> broker/exchange
```

Стратегия не отправляет заявки напрямую.

### 5. Создан регламент взаимодействия ИИ-агентов

```text
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
.github/ISSUE_TEMPLATE/ai_agent_task.md
```

Открыто координационное Issue:

```text
#1 [ARCH] Establish AI agent communication workflow
```

Принята схема:

```text
Issue -> work -> result -> review -> handoff -> state update
```

Основные роли:

```text
Strategy Master Agent        торговая логика, гипотезы, риск, ТЗ на бэктест
Python Research Agent        данные, прототипы, бэктест, отчеты
C++ Core Agent               будущий быстрый контур, типы, risk/paper/replay
Architecture Agent           docs, ADR, roadmap, PROJECT_STATE
Owner / Human Gate           финальные решения, особенно перед live
```

### 6. Создана база знаний по стратегиям

```text
strategy_knowledge_base/
  ideas/
  strategies/
  strategy_families/
  evaluations/
  research_notes/
  strategy_master_agent/
```

### 7. Обработаны и структурированы ключевые материалы

Research notes:

```text
strategy_knowledge_base/research_notes/NOTE-20260708-002-market-maker-algorithms-parts-1-4.md
strategy_knowledge_base/research_notes/NOTE-20260708-003-market-maker-algorithms-parts-5-8.md
strategy_knowledge_base/research_notes/NOTE-20260708-004-bulashev-statistics-for-traders.md
strategy_knowledge_base/research_notes/NOTE-20260708-005-avellaneda-stoikov-limit-order-book.md
strategy_knowledge_base/research_notes/NOTE-20260708-006-r0man-market-maker-test-siu5.md
strategy_knowledge_base/research_notes/NOTE-20260708-007-robot-uralpro-hft-context.md
```

Ideas:

```text
strategy_knowledge_base/ideas/IDEA-20260708-002-inventory-imbalance-market-making.md
strategy_knowledge_base/ideas/IDEA-20260708-003-regime-aware-market-making-price-function.md
strategy_knowledge_base/ideas/IDEA-20260708-004-ri-synthetic-index-lead-lag.md
```

Evaluation:

```text
strategy_knowledge_base/evaluations/EVAL-20260708-001-siu5-market-maker-r0man.md
```

## Главные выводы из материалов

### 1. Старый `robot_uralpro` — не готовый боевой робот

Он ценен как:

```text
источник архитектурных идей
источник торговой гипотезы RI / synthetic index
пример старого HFT event loop
материал для изучения order management
```

Но не имеет статуса готового live решения.

Причины:

```text
устаревшая инфраструктура
изменившийся рынок
неизвестная актуальная прибыльность
отсутствие современного risk engine
отсутствие replay-бэктеста
риск случайной отправки реальных заявок
```

### 2. Первая стратегия-кандидат

```text
RI / Synthetic Index Lead-Lag Statistical Arbitrage
```

Современная формулировка:

```text
Do not assume who leads. Measure who leads.
```

Нужно проверить:

```text
кто лидер: RI или synthetic index
на каком лаге
стабильно ли это внутри дня
стабильно ли это по режимам рынка
выживает ли edge после комиссий и slippage
```

### 3. Вторая стратегия-кандидат

```text
Regime-aware Inventory Market Making
```

Требует:

```text
order book data
depth imbalance
spread state
fill probability
inventory control
volatility regime filter
cancel latency
queue approximation
event-driven replay
```

Поэтому это более позднее направление.

### 4. Статистика обязательна

Правило проекта:

```text
No strategy is accepted without statistical validation.
```

Минимум:

```text
sample size
expected trade return
confidence interval
PnL distribution
MAE/MFE
max drawdown
loss streaks
VaR / Expected Shortfall
out-of-sample
parameter stability
commission/slippage sensitivity
```

## Принятые принципы

### Principle 1. Research first

До live — только research, backtest, replay, paper.

### Principle 2. Strategy is not execution

Стратегия возвращает `OrderIntent`, но не отправляет заявку.

### Principle 3. Risk Engine is mandatory

Каждый `OrderIntent` проходит RiskEngine.

### Principle 4. Live disabled by default

Live mode must require explicit owner approval.

### Principle 5. Lab cannot trade

Trading Lab cannot send real broker/exchange orders.

### Principle 6. Runtime only runs approved packages

Trading Runtime can only run validated and approved Strategy Packages.

### Principle 7. Signal parity required

Перед запуском в Runtime стратегия должна пройти проверку совпадения сигналов на test vectors.

### Principle 8. Secrets are not stored in GitHub

No `.env`, broker keys, passwords, tokens, private keys.

## Текущее целевое устройство проекта

```text
apps/
  lab/
    backend/
    frontend/
    research/
    reports/

  runtime/
    core/
    risk/
    order_manager/
    strategy_loader/
    telemetry/
    execution/

shared/
  contracts/
  schemas/
  strategy_sdk/
  test_vectors/

strategy_packages/
  examples/

docs/
decisions/
strategy_knowledge_base/
```

## Ближайшие задачи

### Task 1. Создать skeleton проекта

```text
apps/lab/
apps/runtime/
shared/contracts/
shared/schemas/
shared/strategy_sdk/
shared/test_vectors/
strategy_packages/examples/
```

### Task 2. Описать первые Shared Contracts

```text
MarketEvent
FeatureSnapshot
StrategySignal
OrderIntent
RiskDecision
PositionSnapshot
TradeEvent
RuntimeLog
```

### Task 3. Описать Strategy Package Standard

```text
manifest.yaml
params.yaml
risk_limits.yaml
instruments.yaml
validation_report.json
approval.json
package.hash
```

### Task 4. Создать dummy no-trade strategy package

Цель: проверить, что Runtime может загрузить пакет, проверить hash/approval/schema and reject invalid package.

### Task 5. Формализовать STRAT по RI / Synthetic Index Lead-Lag

Создать:

```text
strategy_knowledge_base/strategies/STRAT-20260708-001-ri-synthetic-index-lead-lag.md
```

### Task 6. Создать задачу Python Research Agent

Issue:

```text
[PYTHON] Build RI synthetic index lead-lag research prototype
```

Цель:

```text
load data
build synthetic index
align timestamps
calculate cross-correlation by lag
classify leader/lagger
produce research report
no trading
```

### Task 7. Создать Runtime skeleton без брокера

Runtime должен:

```text
load Strategy Package
validate package
read replay/paper MarketEvent stream
run TradeAgent
produce OrderIntent
run RiskEngine
write AuditLog
not send real orders
```

## Открытые вопросы

### 1. Источник исторических данных

Нужно выбрать источник данных по фьючерсам MOEX, акциям для synthetic index, весам индекса, FX factor and session calendar.

### 2. Формат хранения данных

Candidates:

```text
Parquet
PostgreSQL
ClickHouse
CSV only for small samples
```

### 3. Язык первого Runtime skeleton

Возможные варианты:

```text
Python for earliest skeleton
C# if closer to old robot context
C++ later for low-latency core
```

Решение: не делать premature low-latency optimization before strategy edge is validated.

### 4. UI stack

Trading Lab needs web UI, but exact stack not selected yet.

## Explicit Non-Goals Now

```text
no live trading
no broker connection
no real API keys
no colocation setup
no ultra-low-latency optimization
no direct port of old robot into production
no strategy profitability claims without validation
```

## Current Next Step

Start Phase 1 from `ROADMAP.md`:

```text
Shared Contracts and Repository Skeleton
```

In parallel, Strategy Master Agent should formalize:

```text
STRAT-20260708-001-ri-synthetic-index-lead-lag.md
```

Then create GitHub Issues for:

```text
[ARCH] Create shared contracts and project skeleton
[PYTHON] Build RI synthetic index lead-lag research prototype
[RUNTIME] Build no-broker runtime skeleton
```
