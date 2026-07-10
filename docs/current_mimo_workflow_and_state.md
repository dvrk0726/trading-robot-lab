# Current MiMo Workflow and Project State — Superseded

Дата обновления: 2026-07-10  
Статус: superseded; не использовать как рабочую инструкцию

Этот файл сохраняется только как указатель для старых ссылок.

Старая схема с task-файлами `M1.md`, ручным `mimo_save.ps1` и прямой работой в `main` отменена.

Актуальные источники:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/mimo_developer_workflow.md
docs/handoffs/owner_mimo_practical_workflow.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
docs/engineering/OWNER_REVIEW_PACKAGE.md
```

Актуальная схема:

```text
Issue READY_FOR_MIMO
-> MiMo feature branch
-> implementation and tests
-> Pull Request
-> CI and Architecture/Review
-> Owner review when required
-> reviewed merge
-> state update
```

Универсальная команда владельца:

```text
Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись.
```

MiMo не работает напрямую в `main`, не выполняет merge и не начинает следующую задачу до review предыдущей.

Текущее состояние проекта хранится только в `PROJECT_STATE.md`; этот файл больше не дублирует milestone history.
