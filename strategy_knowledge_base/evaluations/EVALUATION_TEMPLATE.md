# EVAL-YYYYMMDD-001-strategy-name

## Metadata

```text
ID: EVAL-YYYYMMDD-001
Strategy: STRAT-YYYYMMDD-001
Date: YYYY-MM-DD
Evaluator: strategy_master / research_agent / developer
Status: draft
Decision: TBD
```

## 1. Что проверяли

Какая стратегия, какая версия, какие параметры.

## 2. Данные

```text
Instruments: TBD
Period: TBD
Data source: TBD
Data type: trades / quotes / order_book / synthetic
Data quality notes: TBD
```

## 3. Исполнение в тесте

```text
Execution model: simple / slippage / order_book_replay
Commission model: TBD
Slippage model: TBD
Latency model: TBD
Partial fills: yes/no
Queue model: yes/no
```

## 4. Основные результаты

```text
Gross PnL: TBD
Net PnL: TBD
Max drawdown: TBD
Trades: TBD
Win rate: TBD
Average trade: TBD
Fees share of gross PnL: TBD
Worst day: TBD
Best day: TBD
```

## 5. Чувствительность

Проверить:

- комиссии;
- проскальзывание;
- задержка данных;
- задержка отправки заявки;
- параметры стратегии;
- разные рыночные режимы.

## 6. Что выглядит хорошо

- Пункт 1.
- Пункт 2.
- Пункт 3.

## 7. Что выглядит плохо

- Пункт 1.
- Пункт 2.
- Пункт 3.

## 8. Риски

- Риск 1.
- Риск 2.
- Риск 3.

## 9. Вывод

```text
continue_research / ready_for_replay / ready_for_paper / needs_rework / reject
```

## 10. Следующие действия

1. TBD
2. TBD
3. TBD

## Связанные файлы

- Strategy: `strategy_knowledge_base/strategies/...`
- Code: `src/...` or `cpp/...`
- Data notes: `strategy_knowledge_base/research_notes/...`
