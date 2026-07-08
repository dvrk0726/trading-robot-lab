# MiMo Developer Workflow

Дата создания: 2026-07-08
Статус: рабочий регламент для использования MiMo как ИИ-разработчика

## Назначение

Этот документ фиксирует, как использовать Xiaomi MiMo / MiMo Code как разработчика проекта `trading-robot-lab`.

MiMo не должен работать как свободный чат-бот, который сам решает, что менять.

MiMo должен работать как исполнитель инженерных задач по GitHub Issues, с обязательным отчетом после каждой работы.

## Что известно о MiMo из официальных источников

По официальному сайту MiMo:

```text
Xiaomi MiMo Code — AI coding assistant for developers, focused on understanding, building and collaboration, with infinite context.
Xiaomi MiMo Claw — agentic platform for planning, tool calls, multi-step reasoning and autonomous execution.
Xiaomi MiMo API — developer platform with OpenAI-compatible and Anthropic-compatible APIs.
MiMo-V2.5-Pro — 1T total parameters, 42B active parameters, 1M context.
MiMo-V2.5-Pro-UltraSpeed — advertised as up to 1000 tokens/s peak speed for coding agents.
```

Important caution:

```text
Do not assume MiMo is always correct.
Do not allow MiMo to bypass project process.
Do not allow MiMo to introduce broker/live/order-sending code unless explicitly approved by Owner.
```

## Роль MiMo в проекте

MiMo выполняет роль:

```text
Implementation Agent / Developer Agent
```

Его задача:

```text
читать GitHub Issue
читать обязательный контекст
делать ограниченное изменение
запускать проверки
писать отчет
передавать результат на review
```

MiMo НЕ является:

```text
Owner
Strategy Master Agent
Risk approver
Live trading approver
Security approver
```

## Обязательные файлы, которые MiMo должен читать перед работой

Перед любой задачей:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/mimo_developer_workflow.md
```

Для архитектурных задач:

```text
docs/01_hybrid_architecture.md
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
docs/system_architecture_and_user_interface_requirements.md
```

Для strategy/research задач:

```text
strategy_knowledge_base/README.md
strategy_knowledge_base/strategy_master_agent/STRATEGY_MASTER_PROMPT.md
strategy_knowledge_base/ideas/
strategy_knowledge_base/research_notes/
```

## Как ставить задачу MiMo

Плохая постановка:

```text
Сделай систему.
Напиши runtime.
Разберись и доделай.
```

Хорошая постановка:

```text
Задача: создать shared contract MarketEvent.
Контекст: ADR-0002, ROADMAP Phase 1.
Файлы: shared/contracts/market_event.schema.json, shared/contracts/README.md.
Что нельзя делать: broker/live/order sending.
Проверка: JSON schema валидна, README объясняет поля.
Отчет: записать в agent_workspaces/mimo/reports/YYYY-MM-DD-issue-N.md.
```

## Формат задачи для MiMo

Каждая задача должна иметь:

```text
Issue number
Objective
Context files
Files allowed to change
Files forbidden to change
Expected output
Constraints
Tests/checks
Report path
Handoff target
```

## Что MiMo должен делать перед изменением кода

Перед правкой MiMo должен:

```text
1. Прочитать Issue.
2. Прочитать обязательные файлы контекста.
3. Кратко сформулировать план.
4. Проверить, какие файлы он будет менять.
5. Убедиться, что задача не требует live/broker/secrets.
6. Если задача опасная или неясная — остановиться и задать вопрос.
```

## Что MiMo должен делать после изменения кода

После выполнения MiMo обязан:

```text
1. Запустить доступные проверки/тесты.
2. Записать полный отчет в agent_workspaces/mimo/reports/.
3. В отчете указать, что сделано, что не сделано, какие файлы изменены.
4. Указать команды проверки и результат.
5. Указать риски и открытые вопросы.
6. Оставить handoff: кто должен review.
```

## Обязательный отчет MiMo

Каждая выполненная задача должна создавать отчет:

```text
agent_workspaces/mimo/reports/YYYY-MM-DD-issue-XXX-short-title.md
```

Пример:

```text
agent_workspaces/mimo/reports/2026-07-08-issue-002-project-skeleton.md
```

Если MiMo не смог выполнить задачу, он все равно должен создать отчет с причиной.

## Что должно быть в отчете

```text
Issue
Task title
Agent
Date/time
Context read
Summary
Files changed
Files created
Files not changed
Commands run
Test results
What was completed
What was not completed
Risks
Open questions
Next steps
Handoff / review needed
```

## Правила безопасности для MiMo

MiMo запрещено:

```text
добавлять реальные API keys
добавлять реальные broker credentials
создавать live order sending code
подключать брокера
изменять SECURITY.md без review
отключать RiskEngine
обходить StrategyPackage validation
делать live_approved=true
удалять safety checks
скрывать ошибки тестов
```

Если задача требует что-то из списка выше, MiMo должен остановиться и создать Owner Decision Request.

## Как проверять код MiMo

После работы MiMo reviewer должен проверить:

```text
1. Есть ли отчет в agent_workspaces/mimo/reports/.
2. Соответствует ли изменение GitHub Issue.
3. Не изменены ли лишние файлы.
4. Не добавлены ли live/broker/secrets.
5. Есть ли тесты или хотя бы проверочные команды.
6. Запускаются ли проверки.
7. Понятно ли, что сделано и что не сделано.
8. Нужно ли Strategy/Risk/Architecture review.
```

## Review Checklist

```text
[ ] Report exists
[ ] Issue objective satisfied
[ ] Scope respected
[ ] No live trading code
[ ] No broker connection
[ ] No secrets
[ ] No risk bypass
[ ] Tests/checks documented
[ ] Open questions documented
[ ] Handoff clear
```

## Как я буду контролировать MiMo

Strategy Master Agent / reviewer будет читать:

```text
GitHub Issue
changed files
agent_workspaces/mimo/reports/latest report
test outputs
risk/open questions
```

Если отчет отсутствует, работа считается неполной.

Если MiMo меняет код без объяснения, работа требует ручного review.

Если MiMo добавляет live/broker/secrets — работа rejected and must be reverted.

## Рабочий цикл MiMo

```text
GitHub Issue
  -> MiMo reads context
  -> MiMo makes scoped changes
  -> MiMo runs checks
  -> MiMo writes report
  -> Human/Strategy/Architecture review
  -> merge/accept or request changes
```

## Bottom line

MiMo можно использовать как сильного исполнителя кода, но только в рамках:

```text
small scoped tasks
mandatory context
mandatory report
no live/broker/secrets
review before acceptance
```
