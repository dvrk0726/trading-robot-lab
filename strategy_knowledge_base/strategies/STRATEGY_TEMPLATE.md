# STRAT-YYYYMMDD-001-short-name

## Metadata

```text
ID: STRAT-YYYYMMDD-001
Status: draft
Created: YYYY-MM-DD
Author/Agent: strategy_master
Source idea: IDEA-YYYYMMDD-001
Strategy family: TBD
Target market: TBD
Target instruments: TBD
Time horizon: TBD
Expected holding time: TBD
```

## 1. Краткое описание стратегии

Описать стратегию так, чтобы другой ИИ/разработчик понял ее суть за 1 минуту.

## 2. Рыночная гипотеза

Какая неэффективность используется?

## 3. Инструменты

```text
Primary instrument: TBD
Hedge/reference instruments: TBD
Synthetic basket: TBD
Currency factor: TBD
```

## 4. Входные данные

Какие данные нужны:

- trades;
- quotes;
- order book;
- index values;
- basket components;
- bid/ask;
- fees;
- trading schedule;
- tick size;
- lot size.

## 5. Формулы и признаки

Описать все формулы.

Пример:

```text
synthetic_index = ...
spread = futures_price - coefficient * synthetic_index
z_score = (spread - mean(spread_window)) / std(spread_window)
```

## 6. Условия входа

### Long

```text
TBD
```

### Short

```text
TBD
```

## 7. Условия выхода

```text
TBD
```

## 8. Управление позицией

- Максимальная позиция.
- Добавление к позиции: да/нет.
- Усреднение: да/нет.
- Переворот позиции: да/нет.
- Закрытие перед клирингом/концом сессии: да/нет.

## 9. Risk management

Обязательные лимиты:

```text
max_position_abs: TBD
max_order_qty: TBD
max_daily_loss: TBD
max_orders_per_minute: TBD
max_slippage: TBD
max_data_staleness_ms: TBD
kill_switch_condition: TBD
```

## 10. Execution assumptions

Как предполагается исполнять заявки:

- market order;
- limit order;
- passive order;
- aggressive order;
- cancel/replace;
- queue position assumption;
- partial fills;
- timeout.

## 11. Комиссии и проскальзывание

Указать модель:

```text
commission_model: TBD
slippage_model: TBD
latency_model: TBD
```

## 12. Рыночные режимы, где стратегия может работать

- Режим 1.
- Режим 2.
- Режим 3.

## 13. Рыночные режимы, где стратегия может ломаться

- Режим 1.
- Режим 2.
- Режим 3.

## 14. План бэктеста

Минимальный план:

1. Подготовить данные.
2. Проверить качество данных.
3. Посчитать сигнал.
4. Прогнать простой бэктест.
5. Добавить комиссии.
6. Добавить проскальзывание.
7. Добавить задержки.
8. Проверить разные периоды.
9. Проверить разные параметры.
10. Сделать решение: отклонить / доработать / перевести в replay.

## 15. Метрики оценки

- Net PnL.
- Gross PnL.
- Sharpe/Sortino, если применимо.
- Max drawdown.
- Win rate.
- Average trade.
- Number of trades.
- Turnover.
- Fees share of gross PnL.
- Slippage sensitivity.
- Latency sensitivity.
- Worst day.
- Best day.

## 16. Красные флаги

Что должно сразу остановить разработку:

- PnL исчезает после комиссий.
- PnL исчезает при небольшой задержке.
- Слишком мало сделок.
- Вся прибыль в 1-2 днях.
- Стратегия требует нереалистичного исполнения.
- Сильная зависимость от одного параметра.
- Не выдерживает другой период данных.

## 17. Решение

```text
draft / ready_for_backtest / backtested / needs_rework / paper_candidate / rejected
```

## 18. Связанные файлы

- Source idea: `strategy_knowledge_base/ideas/...`
- Evaluation: `strategy_knowledge_base/evaluations/...`
- Code prototype: `src/...` or `cpp/...`
