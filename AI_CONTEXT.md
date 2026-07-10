# AI_CONTEXT

Дата последнего обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: private research and engineering repository

## Читать в таком порядке

Любой новый ИИ-агент или разработчик обязан сначала прочитать:

```text
1. AI_CONTEXT.md
2. PROJECT_STATE.md
3. ROADMAP.md
4. docs/moex/MOEX_REALTIME_ARCHITECTURE.md
5. docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
6. relevant ADR and task specification
```

## Главная цель

Построить дисциплинированную платформу для исследования и дальнейшего безопасного исполнения торговых стратегий на срочном рынке MOEX.

Целевая модель:

```text
Trading Lab      — research, replay, backtests, reports, visualization.
Trading Runtime  — lightweight execution of approved Strategy Packages.
Shared Contracts — normalized events, signals, OrderIntent, RiskDecision and reports.
```

## Неизменяемые правила

```text
Trading Lab не отправляет реальные заявки.
Стратегия не вызывает биржевой шлюз напрямую.
Strategy -> OrderIntent -> RiskEngine -> OrderManager -> Execution.
Runtime запускает только утверждённые Strategy Packages.
Live выключен по умолчанию и требует решения владельца.
Production order entry заблокирован до VPTS/certification gate.
Секреты, raw market data и реальные доступы не хранятся в Git.
```

## Роли

```text
Owner / Human Gate:
финальные решения, расходы, доступы, paper/live и production gates.

Architecture / Review Agent (ChatGPT):
официальные источники, архитектура, task specification, code/diff review, acceptance.

Implementation Agent (MiMo Code):
локальная реализация, сборка, тесты, commit, push и технический отчёт.
```

MiMo не должен самостоятельно менять основную архитектуру, включать live или переписывать работающий модуль без утверждённого задания.

## Что уже работает

Исторический C++ QSH/OrdLog слой создан и доведён до инженерно полезного состояния:

```text
C++20 qsh_ingest;
QSH/OrdLog parsing;
transaction grouping;
L3 reconstruction;
L3 -> L2 export;
Data Quality;
strategy_ready gating;
исследование NonSystem, crossed-state и Quote semantics;
тесты M10X: 20/20.
```

Контрольный commit M10X:

```text
54cd53df4b92473e49dd5dff96b2024590b82e42
```

Подтверждённые исторические QSH flags:

```text
0x94  = Add + Buy + Quote
0x414 = Add + Buy + TxEnd
```

Исторический QSH 2021 используется только как engineering sample. Он не доказывает современную прибыльность.

## Текущий главный приоритет

```text
MOEX Realtime RT-1
```

Следующий ограниченный этап:

```text
local configuration.xml/templates.xml inspector;
normalized C++ FAST contracts;
template IDs, fields, hashes and ORDERS-LOG channels validation;
no network connection;
no order sending;
no QuickFAST in production hot path.
```

Realtime roadmap:

```text
docs/moex/MOEX_REALTIME_ARCHITECTURE.md
```

## MOEX FAST/FIX knowledge state

Изучены и зафиксированы:

```text
FAST specification 1.29.1;
FAST_9.0 templates.xml;
T0 configuration.xml;
ORDERS-LOG Incremental A/B;
Snapshot A/B;
TCP Historical Replay;
FIX SPECTRA;
VPTS certification procedure;
production FAST / Full_orders_log direction.
```

Важные template IDs:

```text
OrdersLogMessage                  29
BookMessage                       30
DefaultIncrementalRefreshMessage  31
DefaultSnapshotMessage            32
SecurityDefinition                40
SecurityGroupStatus               45
TradingSessionStatus              46
```

FAST MDFlags и исторические QSH flags — разные контракты. Их нельзя объединять в один enum.

## Решение по FAST decoder

QuickFAST не является фундаментом production hot path.

Целевое решение:

```text
собственный специализированный C++ SPECTRA FAST decoder;
wire primitives отдельно;
generated direct decoders from templates.xml;
no XML interpretation in hot path;
no universal FIX object tree;
minimal allocations;
differential testing against reference tools;
template hash verification at startup.
```

QuickFAST/fast_sensor/reference codec допускаются только как диагностический эталон и инструмент сравнения.

## Архитектура хранения

```text
Raw source of truth: immutable binary segments or pcapng on NVMe.
Archive: compressed Parquet.
Hot analytics: ClickHouse.
Control metadata: PostgreSQL.
Local research: DuckDB + Python.
Future archive: S3-compatible/MinIO.
```

Базы данных не должны синхронно блокировать UDP capture.

## Старый HFT robot

Старый `robot_uralpro` ценен как:

```text
источник идей;
пример исторической инфраструктуры;
источник гипотезы RI / synthetic index;
материал для анализа order management.
```

Он не является готовым production robot и не переносится напрямую.

## Стратегии-кандидаты

```text
1. RI / Synthetic Index Lead-Lag Statistical Arbitrage.
2. Regime-aware Inventory Market Making.
```

Правило:

```text
Do not assume who leads. Measure who leads.
No strategy is accepted without statistical validation.
```

## Безопасность

Запрещено коммитить:

```text
.env;
API keys, passwords and tokens;
private keys;
MOEX/broker credentials;
real account identifiers;
raw QSH/FAST captures;
large databases and market archives;
EXE/DLL/build directories.
```

## Работа с GitHub

Крупный код пишет MiMo локально и отправляет обычным `git push`.

Прямая запись через ИИ-коннектор используется для небольших документов и точечных изменений. Большие задачи разделяются на компактный Issue и несколько task-spec файлов.

Полные правила:

```text
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

## Обязательный результат задачи MiMo

```text
commit SHA;
список изменённых файлов;
что реализовано;
команды сборки и тестов;
результаты тестов;
известные ограничения;
что намеренно не делалось;
handoff для следующего агента.
```

## Live warning

Проект связан с финансовым риском. Любой путь к реальным заявкам требует research, backtest, deterministic replay, paper trading, risk review, security review, VPTS/certification и явного решения владельца.
