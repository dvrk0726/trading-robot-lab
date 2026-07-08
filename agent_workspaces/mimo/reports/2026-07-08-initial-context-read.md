# Initial Context Read Report

Дата: 2026-07-08
Агент: MiMo (Implementation Agent)
Задача: Чтение обязательного контекста проекта

## Файлы прочитаны

- AI_CONTEXT.md
- PROJECT_STATE.md
- ROADMAP.md
- SECURITY.md
- docs/mimo_developer_workflow.md

---

## 1. Цель проекта

Создать дисциплинированную платформу для исследования и безопасного исполнения торговых стратегий на фьючерсах MOEX. Две отдельные системы:

- **Trading Lab** — исследование, тестирование, replay, отчеты, анализ.
- **Trading Runtime** — легкое исполнение утвержденных Strategy Packages.
- **Shared Contracts** — единые форматы данных, сигналов, заявок, risk decisions и отчетов.

Порядок: исследование → статистическая проверка → backtest → replay → paper → owner gate → live later.

---

## 2. Архитектура

Принятое решение: двухсистемная модель (ADR-0002).

```
Trading Lab не умеет отправлять реальные заявки.
Trading Runtime не запускает неутвержденные стратегии.
Стратегии передаются из Lab в Runtime только через Strategy Package.
Каждый OrderIntent проходит RiskEngine.
Live disabled by default and owner-gated.
```

Стек:
- Python = research, analysis, prototype, backtest, reporting.
- C++ = future low-latency core, risk, replay/paper/live execution gateway.

Главное правило: `strategy -> risk -> execution -> broker/exchange`. Стратегия не отправляет заявки напрямую.

---

## 3. Trading Lab

Назначение: исследование, тестирование, replay, отчеты, анализ.

Целевая структура:
```
apps/lab/
  backend/
  frontend/
  research/
  reports/
```

Lab не имеет права отправлять реальные заявки на биржу/брокера.

---

## 4. Trading Runtime

Назначение: легкое исполнение утвержденных Strategy Packages.

Целевая структура:
```
apps/runtime/
  core/
  risk/
  order_manager/
  strategy_loader/
  telemetry/
  execution/
```

Runtime может только:
- загружать Strategy Package
- валидировать пакет
- читать replay/paper MarketEvent stream
- запускать TradeAgent
- производить OrderIntent
- запускать RiskEngine
- писать AuditLog

Runtime не может отправлять реальные заявки (на текущем этапе).

---

## 5. Shared Contracts

Единый слой контрактов между Lab и Runtime:

```
shared/
  contracts/
  schemas/
  strategy_sdk/
  test_vectors/
```

Первые schemas:
- MarketEvent
- FeatureSnapshot
- StrategySignal
- OrderIntent
- RiskDecision
- PositionSnapshot
- TradeEvent
- RuntimeLog

Strategy Package Standard:
- manifest.yaml
- params.yaml
- risk_limits.yaml
- instruments.yaml
- validation_report.json
- approval.json
- package.hash

---

## 6. Запреты для MiMo

MiMo запрещено:
- добавлять реальные API keys
- добавлять реальные broker credentials
- создавать live order sending code
- подключать брокера
- изменять SECURITY.md без review
- отключать RiskEngine
- обходить StrategyPackage validation
- делать live_approved=true
- удалять safety checks
- скрывать ошибки тестов

Если задача требует что-то из списка выше — остановиться и создать Owner Decision Request.

---

## 7. Как работать по задачам

Рабочий цикл MiMo:
```
GitHub Issue
  -> MiMo reads context
  -> MiMo makes scoped changes
  -> MiMo runs checks
  -> MiMo writes report
  -> Human/Strategy/Architecture review
  -> merge/accept or request changes
```

Перед правкой:
1. Прочитать Issue.
2. Прочитать обязательные файлы контекста.
3. Кратко сформулировать план.
4. Проверить, какие файлы будут меняться.
5. Убедиться, что задача не требует live/broker/secrets.
6. Если задача опасная или неясная — остановиться и задать вопрос.

---

## 8. Куда писать отчеты

Путь: `agent_workspaces/mimo/reports/YYYY-MM-DD-issue-XXX-short-title.md`

Пример: `agent_workspaces/mimo/reports/2026-07-08-issue-002-project-skeleton.md`

В отчете должно быть:
- Issue, Task title, Agent, Date/time
- Context read
- Summary
- Files changed/created/not changed
- Commands run, Test results
- What was completed / not completed
- Risks, Open questions, Next steps
- Handoff / review needed

---

## 9. Ближайшие GitHub Issues

Из PROJECT_STATE.md:

1. **[ARCH] Create shared contracts and project skeleton**
   - Создать структуру apps/lab/, apps/runtime/, shared/, strategy_packages/
   - Описать первые Shared Contracts
   - Описать Strategy Package Standard
   - Создать dummy no-trade strategy package

2. **[PYTHON] Build RI synthetic index lead-lag research prototype**
   - Загрузить данные
   - Построить synthetic index
   - Выровнять timestamps
   - Рассчитать cross-correlation by lag
   - Классифицировать leader/lagger
   - Создать research report
   - Без торговли

3. **[RUNTIME] Build no-broker runtime skeleton**
   - Runtime config loader
   - Strategy Package loader/validator
   - Simple TradeAgent interface
   - RiskEngine minimal rules
   - OrderManager stub
   - Telemetry/logging
   - Replay/paper event input
   - Без отправки реальных заявок

---

## Статус проекта

- Phase 0 (Knowledge Base and Architecture Foundation): mostly done
- Phase 1 (Shared Contracts and Repository Skeleton): next
- Phase 2-7: upcoming/later/future

Текущий приоритет: Phase 1 — создать shared contracts и repository skeleton.
