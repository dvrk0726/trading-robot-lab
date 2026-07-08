# AI Agent Communication Protocol

Дата создания: 2026-07-08
Репозиторий: `dvrk0726/trading-robot-lab`
Статус: практический протокол общения между ИИ-агентами

## Назначение

Этот документ отвечает на практический вопрос: как одному ИИ-агенту общаться с другим ИИ-разработчиком внутри проекта.

Короткий ответ:

```text
Документация = правила и контекст.
GitHub Issues = задачи и вопросы.
Pull Request comments = обсуждение конкретных изменений.
PROJECT_STATE.md = текущее состояние проекта.
decisions/ = крупные архитектурные решения.
```

## Почему не один общий chat-файл

Не нужно делать один файл вида:

```text
AI_CHAT.md
AGENT_MESSAGES.md
```

Причины:

- несколько агентов могут одновременно его редактировать;
- появятся конфликты;
- история быстро станет грязной;
- будет сложно понять, какая задача закрыта, а какая нет;
- рабочие вопросы смешаются с важными решениями.

Вместо этого используется более чистая схема:

```text
вопрос или задача -> GitHub Issue
изменение кода или документа -> Pull Request / commit
крупное решение -> ADR
итог состояния -> PROJECT_STATE.md
```

## Главные каналы общения

### 1. GitHub Issue

Используется для:

- постановки задачи другому агенту;
- вопроса к другому агенту;
- фиксации блокера;
- передачи результата;
- запроса owner decision.

Одна задача должна жить в одном Issue.

### 2. Pull Request comments

Используется для:

- обсуждения конкретного изменения;
- review кода;
- review торговой логики;
- замечаний по risk engine;
- проверки, что задача соответствует ТЗ.

### 3. Markdown-документы

Используются для долговременной памяти:

- стратегия;
- архитектура;
- ADR;
- research note;
- итоговая оценка;
- правила безопасности.

## Типы сообщений

### Task Request

Когда один агент ставит задачу другому.

```markdown
## Type
Task Request

## From
Strategy Master Agent

## To
Python Research / Backtest Agent

## Objective
Сделать простой расчет spread между фьючерсом RI и synthetic index.

## Context
Ссылка на STRAT/IDEA/NOTE.

## Required input
- futures price series
- synthetic index series
- timestamp
- commission assumptions

## Expected output
- Python function
- test
- short markdown report

## Constraints
- no broker connection
- no live trading
- no real secrets
- backtest only

## Done criteria
- function works on sample data
- test passes
- report explains assumptions

## Handoff after done
Return to Strategy Master Agent for review.
```

### Clarification Request

Когда агенту не хватает информации.

```markdown
## Type
Clarification Request

## From
Python Research / Backtest Agent

## To
Strategy Master Agent

## Question
Какой lookback window использовать для normal spread estimation?

## Why it matters
Без этого нельзя корректно посчитать z-score и условия входа.

## Options
1. fixed window 60 minutes
2. fixed window 1 trading session
3. adaptive volatility window

## Recommendation
Начать с 60 minutes как baseline и потом сравнить варианты.
```

### Result / Handoff

Когда агент закончил работу и передает результат дальше.

```markdown
## Type
Result / Handoff

## From
Python Research / Backtest Agent

## To
Strategy Master Agent

## Result
Сделан baseline spread calculation на sample data.

## Files changed
- src/trading_robot_lab/...
- tests/...
- research/...

## Checks
- unit tests passed
- sample report generated

## Findings
- spread is unstable during session open
- commission sensitivity is high
- entry threshold needs review

## Open questions
- exclude first 15 minutes of trading session?
- use bid/ask instead of last price?

## Next action requested
Strategy Master Agent should update STRAT file and define next backtest assumptions.
```

### Owner Decision Request

Когда задача требует решения владельца.

```markdown
## Type
Owner Decision Request

## Reason
Task may affect live trading / broker connection / sensitive data / architecture direction.

## Decision needed
Что именно должен решить владелец.

## Safe default
Do nothing / keep backtest mode / do not connect broker.

## Recommended option
Рекомендация агента с объяснением.
```

## Когда обязательно создавать Issue

Issue обязательно создавать, если:

- задача занимает больше одного маленького изменения;
- задача передается другому агенту;
- есть риск неправильного понимания;
- нужно owner decision;
- найден блокер;
- меняется архитектура;
- меняется стратегия;
- затрагивается risk engine;
- затрагивается будущий execution gateway.

Issue не обязательно создавать, если:

- исправлена опечатка;
- добавлена маленькая очевидная правка;
- обновлен один документ без изменения смысла.

## Именование Issues

Формат:

```text
[ROLE] Короткое название задачи
```

Примеры:

```text
[STRATEGY] Formalize RI synthetic index spread idea
[PYTHON] Build baseline spread calculation prototype
[CPP] Add core market event types
[RISK] Define max position and kill switch rules
[ARCH] Record decision about Python/C++ boundary
[OWNER] Decide whether to allow paper trading stage
```

## Рекомендуемые Labels

```text
strategy
research
backtest
python
cpp
risk
architecture
docs
security
blocked
needs-clarification
needs-owner-decision
no-live
```

## Правила handoff

Передача задачи считается нормальной только если есть:

```text
1. ссылка на исходный файл или Issue;
2. понятная цель;
3. входные данные;
4. ожидаемый результат;
5. ограничения;
6. критерии готовности;
7. кому вернуть результат.
```

Плохой handoff:

```text
Посмотри стратегию и сделай бэктест.
```

Хороший handoff:

```text
Сделай baseline backtest для STRAT-20260708-001-ri-synthetic-index-spread.md.
Используй только исторические данные, без broker API.
Вход: futures_price, synthetic_index, timestamp.
Выход: trades.csv, pnl summary, max drawdown, commission sensitivity.
Критерий готовности: отчет показывает PnL до/после комиссий и список слабых мест.
Вернуть Strategy Master Agent для review.
```

## Правила review

### Strategy review

Нужен, если PR меняет:

- сигнал;
- формулу;
- условия входа;
- условия выхода;
- параметры риска;
- расчет PnL;
- логику backtest assumptions.

### Risk review

Нужен, если PR меняет:

- max position;
- max loss;
- order rate limit;
- kill switch;
- stale data protection;
- duplicate order protection;
- abnormal price protection.

### Architecture review

Нужен, если PR меняет:

- границы модулей;
- публичные интерфейсы;
- формат данных;
- структуру папок;
- Python/C++ boundary;
- storage approach.

### Owner review

Нужен, если PR:

- приближает live trading;
- добавляет broker API;
- требует реальные ключи;
- может отправить реальные заявки;
- меняет режимы безопасности;
- обходит risk engine.

## Безопасный default

Если агент сомневается, безопасное действие:

```text
не подключать брокера
не отправлять заявки
не добавлять ключи
не включать live
создать Issue с вопросом
пометить needs-owner-decision
```

## Как фиксировать итог работы за день / этап

Если был сделан существенный этап, нужно обновить:

```text
PROJECT_STATE.md
```

Добавить:

- что сделано;
- какие файлы созданы;
- какие решения приняты;
- какие задачи следующие;
- какие блокеры есть.

Если принято архитектурное решение, нужно создать ADR:

```text
decisions/ADR-XXXX-short-name.md
```

## Минимальный процесс разработки

```text
1. Owner или агент создает Issue.
2. Агент читает обязательный контекст.
3. Агент делает маленькое изменение.
4. Агент пишет результат в Issue или PR.
5. Нужный агент делает review.
6. PROJECT_STATE.md обновляется при крупном изменении.
7. Следующая задача создается отдельным Issue.
```

## Главный вывод

Общение ИИ-агентов должно идти не как свободный чат, а как инженерный процесс:

```text
Issue -> work -> result -> review -> handoff -> state update
```

Так другой ИИ-разработчик сможет зайти в репозиторий, прочитать контекст и продолжить работу без долгого объяснения.
