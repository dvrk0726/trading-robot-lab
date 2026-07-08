# PROJECT_STATE

Дата последнего обновления: 2026-07-08  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: active research / historical market microstructure data layer

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

Текущая цепочка развития:

```text
data quality -> microstructure replay -> statistical validation -> backtest -> paper -> owner gate -> live later
```

## Принятые архитектурные решения

### ADR-0002 — Two-system Lab / Runtime architecture

```text
decisions/ADR-0002-two-system-lab-runtime-architecture.md
```

Суть:

```text
Trading Lab не умеет отправлять реальные заявки.
Trading Runtime не запускает неутвержденные стратегии.
Стратегии передаются из Lab в Runtime только через Strategy Package.
Каждый OrderIntent проходит RiskEngine.
Live disabled by default and owner-gated.
```

### ADR-0003 — C++ QSH / OrdLog Data Engine

```text
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
```

Суть:

```text
C++20 = низкоуровневый слой данных и будущая база replay/runtime core.
Python = research, Trading Lab dashboard, анализ, графики, отчеты.
```

Первый C++-модуль:

```text
cpp/qsh_ingest
```

Он должен читать исторические QSH/OrdLog/Quotes/Deals/AuxInfo, делать Data Quality, восстанавливать L3/L2 стакан и отдавать нормализованные данные Python-лаборатории.

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
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
docs/system_architecture_and_user_interface_requirements.md
docs/trading_robot_vision_and_research_plan.md
```

### 5. Создан регламент взаимодействия ИИ-агентов

```text
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
.github/ISSUE_TEMPLATE/ai_agent_task.md
```

Принята схема:

```text
Issue -> work -> result -> review -> handoff -> state update
```

Основные роли:

```text
Strategy Master Agent        торговая логика, гипотезы, риск, ТЗ на бэктест
Python Research Agent        данные, прототипы, бэктест, отчеты
C++ Core Agent               быстрый контур, QSH/OrdLog, replay, future runtime core
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

### 8. Создана базовая техническая инфраструктура

Сделано или начато:

```text
project skeleton
Python package skeleton
core trading models
safe config loader
minimal risk engine
synthetic spread strategy prototype
shared JSON schemas
schema validation examples
Strategy Package standard
dummy no-trade package
Trading Lab demo database
Trading Lab demo dashboard
Trading Lab demo charts
chart controls and marker fixes
QSH data source notes
qsh2txt/txt2qsh notes
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
кто лидер: RI or synthetic index
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

## QSH / OrdLog решение

### Доступные и желательные данные

Доступен инженерный пример:

```text
RTS-3.21.2021-01-05.OrdLog.qsh
```

Важно:

```text
2021 OrdLog = engineering sample, not current trading evidence.
```

Использовать его можно для:

```text
parser development
L3 book reconstruction
replay mechanics
queue/fill model prototype
Data Quality checks
historical order book visualization
```

Нельзя использовать его как:

```text
proof of current profitability
final live-trading validation
final estimate of modern execution quality
```

### Разница потоков

```text
OrdLog.qsh:
  L3 / full order-log level data.
  Best for queue/fill/order book reconstruction.

Quotes.qsh:
  L2 quote/book stream.
  Good for spread/mid/depth, but not full queue reconstruction.

Deals.qsh:
  trade prints.

AuxInfo.qsh:
  auxiliary market/session info.
```

Движок должен поддержать оба режима:

```text
Mode A: L3 mode from OrdLog.qsh
Mode B: L2 mode from Quotes.qsh + Deals.qsh + AuxInfo.qsh
```

Но первый приоритет — `OrdLog.qsh`.

## Принятые принципы

### Principle 1. Research first

До live — только research, Data Quality, backtest, replay, paper.

### Principle 2. Data Quality first

Любая стратегия опирается только на проверенные данные.

### Principle 3. Strategy is not execution

Стратегия возвращает `OrderIntent`, но не отправляет заявку.

### Principle 4. Risk Engine is mandatory

Каждый `OrderIntent` проходит RiskEngine.

### Principle 5. Live disabled by default

Live mode must require explicit owner approval.

### Principle 6. Lab cannot trade

Trading Lab cannot send real broker/exchange orders.

### Principle 7. Runtime only runs approved packages

Trading Runtime can only run validated and approved Strategy Packages.

### Principle 8. Signal parity required

Перед запуском в Runtime стратегия должна пройти проверку совпадения сигналов на test vectors.

### Principle 9. Secrets are not stored in GitHub

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

cpp/
  qsh_ingest/

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

## Current priority

Главная текущая задача:

```text
M9_CPP_QSH_ORDLOG_DATA_LAYER.md
```

GitHub Issue:

```text
#13 [MIMO][C++] Build QSH / OrdLog data layer and historical order book replay foundation
```

Цель:

```text
Raw QSH / OrdLog / Quotes / Deals / AuxInfo
  -> C++20 parser / normalizer
  -> L3 order book reconstruction
  -> L2 snapshots / event stream
  -> Data Quality report
  -> Python Trading Lab visualization and research
```

## What MiMo must do now

MiMo must implement M9 incrementally:

```text
1. Create cpp/qsh_ingest C++20 CMake skeleton.
2. Implement qsh-ingest CLI help.
3. Implement QSH signature/header reader.
4. Implement stream type detection.
5. Implement and test LEB128 / signed LEB128 / growing int readers.
6. Decode first 100 OrdLog records.
7. Scan whole OrdLog file with Data Quality counters.
8. Group records by TxEnd.
9. Reconstruct L3 order book on a limited safe segment.
10. Export L2 snapshots.
11. Generate metadata.json and data_quality.json.
12. Show generated quality metadata in Trading Lab.
13. Create MiMo report.
```

Required MiMo report:

```text
agent_workspaces/mimo/reports/2026-07-08-m9-cpp-qsh-ordlog-data-layer.md
```

## User-facing target

Trading Lab should eventually show:

```text
Data Sources
Data Quality
Historical Order Book
Price / Mid / Spread chart
Trades
Event stream
Strategy Replay
Simulated orders/fills
PnL diagnostics
```

For the current task, minimum visible result:

```text
Data Quality section can display C++ generated metadata and quality reports.
```

## Explicit Non-Goals Now

```text
no live trading
no broker connection
no real API keys
no colocation setup
no real order sending
no direct port of old robot into production
no profitability claims from old historical data
no raw real QSH committed to GitHub
no EXE/DLL/binary tools committed
```

## Next step after M9

After M9 is complete:

```text
M10 — normalized microstructure research / first historical replay reports
M11 — RI / Synthetic Index Lead-Lag research using current/valid weights and data
M12 — event-driven replay bridge with latency/fill assumptions
```
