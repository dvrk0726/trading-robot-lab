# SECURITY

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: обязательные правила безопасности

## Назначение

Проект связан с market data, будущими биржевыми доступами и потенциальным исполнением заявок. GitHub содержит только проверяемый исходный код, документацию, безопасные synthetic fixtures и конфигурационные примеры.

## Неизменяемые safety gates

```text
Trading Lab не отправляет реальные заявки.
Стратегия не вызывает exchange/broker gateway напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry запрещён до VPTS/certification, security/risk review и решения Owner.
Клонирование репозитория не должно давать возможность отправить реальную заявку.
```

## Секреты и персональные данные

Запрещено коммитить:

```text
.env
.env.local
.env.production
.env.live
secrets/
private/
credentials/
*.key
*.pem
*.pfx
*.p12
*.ppk
id_rsa*
id_ed25519*
```

Также запрещены:

```text
API keys, access/refresh tokens, webhook secrets;
MOEX/broker logins and passwords;
private certificates and signing keys;
VPS/SSH credentials;
реальные cookies/session files;
реальные account IDs и реквизиты счетов;
статические IP, адреса, порты и identifiers приватного подключения;
персональные данные;
сканы документов;
необезличенные финансовые документы.
```

Если значение похоже на настоящий secret, оно считается секретом, даже если репозиторий приватный.

Разрешён только безопасный пример:

```text
.env.example
```

Пример содержит пустые или явно фейковые значения и безопасные defaults:

```env
TRADING_MODE=backtest
LIVE_TRADING_ENABLED=false
BROKER_API_KEY=put_key_here
BROKER_ACCOUNT_ID=put_account_here
```

## Market data и raw artifacts

В обычный Git запрещено добавлять:

```text
*.qsh
*.pcap
*.pcapng
raw FAST packets/captures;
реальные тиковые данные и order logs;
полные broker/exchange reports;
Parquet market archives;
ClickHouse/PostgreSQL/DuckDB/SQLite database files;
large generated CSV/JSON reports;
data/raw/
data/private/
data/live/
data/broker/
data/reports/
```

Разрешены:

```text
маленькие synthetic fixtures;
обезличенные sample-файлы;
JSON schemas;
expected test vectors;
описания форматов;
инструкции получения данных вне Git.
```

Retired и third-party raw форматы (включая *.qsh) не являются product inputs и запрещены в Git.

## Binaries и scripts

По умолчанию запрещены:

```text
*.exe
*.dll
*.msi
*.so
*.dylib
*.jar
*.bin
build directories
unknown archives
```

Произвольные shell/batch scripts также запрещены по умолчанию:

```text
*.bat
*.cmd
*.ps1
```

Разрешённые исключения:

```text
tools/*.ps1
owner_review_packages/**/START_DEMO.cmd
owner_review_packages/**/STOP_DEMO.cmd
```

Исключение для Owner Review Package действует только при соблюдении `docs/engineering/OWNER_REVIEW_PACKAGE.md`.

Разрешённый demo script должен быть plaintext, коротким, reviewable и локальным. Он не должен:

```text
содержать credentials;
скачивать/запускать неизвестные binaries;
открывать firewall;
требовать administrator privileges;
использовать ExecutionPolicy Bypass;
подключаться к broker/MOEX;
запускать order-entry/live path;
массово завершать пользовательские процессы.
```

`STOP_DEMO.cmd` завершает только собственный demo-процесс по PID/marker, а не все `python.exe` или аналогичные процессы.

## Старый HFT-архив

Старый `robot_uralpro` не загружается целиком.

Правильный процесс:

```text
анализировать локально/изолированно;
выделить безопасный source/documentation;
удалить binaries, configs, logs, credentials и account identifiers;
описать provenance;
коммитить только очищенный материал после review.
```

Старый robot — источник идей и engineering context, а не готовый production runtime.

## Live configuration

Безопасные defaults:

```env
TRADING_MODE=backtest
LIVE_TRADING_ENABLED=false
```

Любой будущий live-capable path обязан требовать минимум:

```text
явный mode=live;
отдельный enable flag;
manual Owner gate;
RiskEngine;
kill switch;
position/order/loss/rate limits;
market-data freshness and abnormal-price checks;
audit logging;
VPTS/certification gate.
```

Наличие конфигурационных flags само по себе не разрешает live.

## Git и Pull Request safety

Implementation code выполняется только через feature branch и Pull Request.

MiMo запрещено:

```text
редактировать/коммитить/push код напрямую в main;
force-push;
merge или auto-merge;
обходить CI;
скрывать failed tests;
начинать следующую задачу до review предыдущей.
```

Перед commit:

```powershell
git status --short
git diff --check
git diff --stat
git diff
python tools/check_repository_hygiene.py
```

Перед merge:

```text
required CI passed;
Architecture/Review Agent reviewed diff;
secrets/raw data/binaries absent;
all review comments resolved;
Owner approval obtained where required.
```

## Automated hygiene

`tools/check_repository_hygiene.py` и GitHub Actions должны проверять минимум:

```text
forbidden paths/extensions;
private key/token signatures;
raw data and database files;
unauthorized .cmd/.ps1;
oversized tracked files;
build/generated artifacts.
```

Автоматическая проверка не заменяет ручной review.

## Incident response

Если secret попал в commit/GitHub:

1. Немедленно считать его скомпрометированным.
2. Отозвать/заменить у provider/broker/server.
3. Удалить из текущих файлов.
4. Проверить полную Git history.
5. При необходимости очистить history.
6. Проверить logs, artifacts и forks/caches.

Обычный последующий commit с удалением строки не делает опубликованный secret безопасным.

Если попали персональные данные или private connection parameters, остановить работу и провести отдельный incident review.

## Owner-gated действия

Только Owner принимает решения о:

```text
реальных доступах;
MOEX/broker contracts and costs;
private network parameters;
paper/live transition;
production hardware;
VPTS/certification;
production order entry.
```

ИИ-агент не должен просить сохранить эти данные в Git или Issue.

## Safe default

При сомнении:

```text
не подключать сеть;
не отправлять заявки;
не добавлять secret/private data;
не запускать неизвестный binary/script;
не ослаблять RiskEngine или strategy_ready;
остановиться и запросить review.
```
