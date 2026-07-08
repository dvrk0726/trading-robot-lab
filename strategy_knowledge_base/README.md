# Strategy Knowledge Base

Дата создания: 2026-07-08
Статус: рабочая база знаний для ИИ-стратега

## Назначение

Эта папка предназначена для второго ИИ-агента, который будет отвечать за стратегическую часть проекта: идеи HFT/low-latency стратегий, классификацию стратегий, формализацию гипотез, превращение сырых идей в проверяемые стратегии и подготовку требований к бэктесту.

Важно: эта папка не предназначена для live-торговли и не должна содержать брокерские ключи, реальные доступы, `.env`, исполняемые файлы или код отправки реальных заявок.

## Главная логика работы

Стратегическая мысль должна проходить путь:

```text
raw idea -> hypothesis -> formal strategy -> backtest plan -> evaluation -> decision
```

Идея не считается стратегией, пока она не формализована.

Стратегия не считается пригодной к разработке, пока не описаны:

- рыночная гипотеза;
- инструменты;
- входные данные;
- сигнал;
- условия входа;
- условия выхода;
- риск;
- комиссии;
- проскальзывание;
- задержки;
- market regime, где стратегия может работать;
- market regime, где стратегия может сломаться;
- минимальный план бэктеста.

## Структура

```text
strategy_knowledge_base/
  README.md
  ideas/
    README.md
    IDEA_TEMPLATE.md
  strategies/
    README.md
    STRATEGY_TEMPLATE.md
  strategy_families/
    README.md
  evaluations/
    README.md
    EVALUATION_TEMPLATE.md
  research_notes/
    README.md
  strategy_master_agent/
    STRATEGY_MASTER_PROMPT.md
```

## Разделы

### `ideas/`

Сюда записываются сырые идеи и гипотезы. Идея может быть неполной, спорной или непроверенной.

Пример статуса идеи:

```text
raw
needs_clarification
ready_for_strategy_spec
rejected
merged_into_strategy
```

### `strategies/`

Сюда попадают только формализованные стратегии. Стратегия должна быть описана так, чтобы другой ИИ или разработчик мог написать бэктест без повторного объяснения.

Пример статуса стратегии:

```text
draft
ready_for_backtest
backtested
needs_rework
paper_candidate
rejected
```

### `strategy_families/`

Классификация типов стратегий: market making, statistical arbitrage, latency arbitrage, index arbitrage, order book imbalance, mean reversion, momentum, event-driven и другие.

### `evaluations/`

Результаты проверки стратегий: бэктест, replay, стресс-тест, анализ рисков, решение — развивать или закрыть.

### `research_notes/`

Конспекты материалов, которые пользователь передаст ИИ-стратегу: статьи, книги, заметки, видео, фрагменты старого HFT-робота, наблюдения по рынку.

### `strategy_master_agent/`

Инструкция для ИИ-стратега: что читать, как записывать идеи, как формализовать стратегии, что запрещено делать.

## Правила именования

Идеи:

```text
ideas/IDEA-YYYYMMDD-001-short-name.md
```

Стратегии:

```text
strategies/STRAT-YYYYMMDD-001-short-name.md
```

Оценки:

```text
evaluations/EVAL-YYYYMMDD-001-strategy-name.md
```

Заметки:

```text
research_notes/NOTE-YYYYMMDD-001-source-name.md
```

## Запреты

ИИ-стратег не должен:

- писать live execution code;
- подключать брокера;
- использовать реальные API-ключи;
- добавлять `.env`;
- добавлять `.exe`/`.dll`;
- обещать прибыльность без бэктеста;
- считать стратегию рабочей без комиссий, проскальзывания и задержек;
- смешивать сырые идеи и формализованные стратегии.

## Связанные документы

Перед работой ИИ-стратег должен прочитать:

```text
AI_CONTEXT.md
PROJECT_STATE.md
SECURITY.md
docs/01_hybrid_architecture.md
decisions/ADR-0001-hybrid-python-cpp-architecture.md
```
