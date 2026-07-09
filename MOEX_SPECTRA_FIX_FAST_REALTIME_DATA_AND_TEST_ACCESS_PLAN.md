# MOEX_SPECTRA_FIX_FAST_REALTIME_DATA_AND_TEST_ACCESS_PLAN

Owner decision date: 2026-07-09
Primary agent: MiMo
Status: ready for planning / implementation

## Mission

Move the project from offline historical QSH/OrdLog reconstruction toward:

```text
1. MOEX SPECTRA test access;
2. FIX/FAST protocol study and local adapter architecture;
3. realtime market data collection;
4. paper/simulated execution;
5. only later: production trading contour with explicit owner approval.
```

This task is NOT allowed to send real trading orders.

This task is NOT allowed to store real credentials in Git.

This task is a planning + implementation foundation task for MOEX connectivity.

## Known official MOEX pages to use

```text
Test SPECTRA access:
https://www.moex.com/s438

General test access:
https://www.moex.com/s328

FAST Gate:
https://www.moex.com/s441

Full_orders_log:
https://www.moex.com/a588

SIMBA:
https://www.moex.com/s3320
```

## Current product decision

For now use:

```text
SPECTRA test contour + FIX/FAST documentation
```

Protocol split:

```text
FAST = market data feed / order book / full orders log style data
FIX  = transaction/order-entry protocol for test environment and later controlled trading integration
```

Do not start with SIMBA or colocation.

SIMBA is faster and more HFT-oriented, but it is for colocation and should be treated as later-stage infrastructure, not MVP.

## Key facts to record from MOEX docs

MiMo must verify and document these from official MOEX pages:

### Test SPECTRA access

- Test access is available to both individuals and legal entities.
- There are T0 and T1 test stands.
- T0 corresponds to production trading system version.
- T1 corresponds to the next release version.
- Connection is possible via dedicated channels from colocation zone and via internet.
- To get login/password for the test contour, an application form must be filled.
- For production connection of developed trading systems, MOEX requires a procedure/check for compliance with exchange requirements.
- When contacting support, protocol must be specified: Plaza-2, FIX/FAST, TWIME, SIMBA.
- FIX/FAST documentation and distributions are linked from the SPECTRA test access page.

### FAST Gate

- FAST Gate is multicast realtime market data.
- It provides data from MOEX markets, including SPECTRA futures market.
- It includes instruments, quotes, trades, indexes, and active anonymous orders.
- For SPECTRA FAST production connection, a dedicated channel is required.
- Minimum bandwidth for SPECTRA FAST Gate production is at least 4 Mbit/s.
- For production access, a market data agreement is required.

### Full_orders_log

- Full_orders_log is an additional option for FAST Gate / FAST Gate SPECTRA.
- It provides technical ability to receive information about all anonymous transactions in the SPECTRA trading system during a trading session.
- It allows reconstructing an order book of unlimited depth.
- It provides deleted and moved orders, unlike exchange aggregates 5/20/50.
- It requires a market data agreement matching the intended data use.
- The listed subscription fee is 21,000 RUB/month in addition to the relevant FAST Gate tariffs.

## Important distinction

Test SPECTRA access is NOT the same as real current production market data.

Use test access for:

```text
- protocol learning;
- connection plumbing;
- decoding messages;
- session management;
- FIX order-entry sandbox;
- replay/collector architecture;
- safety logic;
- paper execution flow.
```

Use production FAST + Full_orders_log for:

```text
- real current market data collection;
- building a fresh research dataset;
- comparing live Full_orders_log semantics with historical QSH/OrdLog;
- future strategy research.
```

Use production FIX/TWIME only much later for:

```text
- real trading orders;
- after certification/checks;
- after paper/live gates;
- after risk kill-switches;
- after owner approval.
```

## Required work

### 1. Create MOEX connectivity documentation

Create:

```text
docs/moex/moex_spectra_connectivity_plan.md
```

Must include:

```text
- official links;
- what to apply for first;
- what to ask MOEX support;
- difference between test SPECTRA and production FAST Full_orders_log;
- why FIX/FAST is the first path;
- why SIMBA is deferred;
- expected credentials and network artifacts;
- local config/secrets policy;
- no-live-trading policy.
```

### 2. Create application/request checklist

Create:

```text
docs/moex/moex_test_access_request_checklist.md
```

Include a ready-to-send Russian request text:

```text
Здравствуйте.

Просим предоставить тестовый доступ к торговой системе Срочного рынка SPECTRA для разработки и тестирования программного обеспечения.

Интересующие протоколы:
1. FIX/FAST.
2. FAST market data.
3. FIX transaction/order-entry test access.

Цель:
- изучение протоколов;
- разработка market data collector;
- тестирование реконструкции стакана;
- тестирование order-entry flow только на тестовом контуре;
- без подключения к промышленному торговому контуру на данном этапе.

Просьба уточнить:
- какую анкету заполнить для физического лица / юридического лица;
- можно ли подключиться к T0 через интернет;
- какие логины/пароли/адреса/порты будут выданы;
- где актуальные FIX/FAST templates/configs для SPECTRA T0;
- есть ли Full_orders_log на тестовом контуре;
- можно ли получить тестовые параметры для FAST multicast через интернет;
- какие требования к клиентскому ПО;
- какие шаги нужны позже для промышленного контура;
- какие проверки/сертификация требуются для ВПТС.
```

### 3. Create architecture doc for local collector

Create:

```text
docs/moex/moex_realtime_collector_architecture.md
```

Architecture must include:

```text
cpp/moex_fast_capture/       raw multicast packet capture / FAST decoder skeleton
cpp/moex_fix_test_client/    FIX session/order-entry test client skeleton
cpp/moex_shared/             protocol-independent normalized messages
python/tools/                diagnostics, reports, conversion, QA
storage/raw/moex/            ignored path for raw captures
storage/normalized/moex/     ignored path for normalized parquet/csv/jsonl
config/examples/             safe example configs only
```

Do not commit raw captures.

Do not commit credentials.

### 4. Add safe config templates

Create example config only:

```text
config/examples/moex_spectra_test.example.yaml
```

Must contain placeholders only:

```yaml
environment: test
market: spectra
protocols:
  fast:
    enabled: false
    config_xml_path: "CHANGE_ME"
    templates_path: "CHANGE_ME"
    multicast_groups:
      incremental_a: "CHANGE_ME"
      incremental_b: "CHANGE_ME"
      snapshot_a: "CHANGE_ME"
      snapshot_b: "CHANGE_ME"
      instrument_replay: "CHANGE_ME"
      message_replay: "CHANGE_ME"
  fix:
    enabled: false
    host: "CHANGE_ME"
    port: 0
    sender_comp_id: "CHANGE_ME"
    target_comp_id: "CHANGE_ME"
    username: "CHANGE_ME"
    password_env: "MOEX_FIX_PASSWORD"
safety:
  allow_live_orders: false
  allow_test_orders: false
  max_order_qty: 1
  require_owner_confirmation: true
```

### 5. Add repository ignore rules

Ensure `.gitignore` ignores:

```text
.env
.env.*
*.pcap
*.pcapng
*.qsh
*.qsh.gz
storage/raw/
storage/normalized/
data/raw/moex/
data/reports/moex/generated/
config/local/
secrets/
```

Do not remove existing useful ignore rules.

### 6. Create initial collector skeleton, no live network yet

Create minimal compilable skeletons but do not connect to MOEX yet:

```text
cpp/moex_fast_capture/
cpp/moex_fix_test_client/
cpp/moex_shared/
```

Minimum behavior:

```text
moex_fast_capture --help
moex_fast_capture --config <yaml> --dry-run
moex_fix_test_client --help
moex_fix_test_client --config <yaml> --dry-run
```

Dry-run must:

```text
- load config;
- validate placeholders are not used when enabled=true;
- print intended mode;
- refuse live order sending;
- refuse any order send unless allow_test_orders=true AND environment=test.
```

### 7. Define normalized message contracts

Create docs and/or C++ headers for:

```text
NormalizedInstrument
NormalizedOrderEvent
NormalizedTradeEvent
NormalizedBookSnapshot
NormalizedFixOrderIntent
NormalizedExecutionReport
```

Important: trading strategy must never talk directly to FIX.

Flow must be:

```text
Strategy -> Signal -> OrderIntent -> RiskGate -> ExecutionAdapter -> FIX/TWIME later
```

### 8. Add safety gates

Add docs and minimal code constants:

```text
LIVE_TRADING_DEFAULT = false
ALLOW_TEST_ORDER_ENTRY_DEFAULT = false
ALLOW_PRODUCTION_ORDER_ENTRY_DEFAULT = false
```

Any command that could send orders must be disabled by default.

For now, no order-sending implementation is required.

### 9. Add build/test integration

If the repo CMake structure allows it, add CMake targets:

```text
moex_fast_capture
moex_fix_test_client
```

Add tests:

```text
config_placeholder_rejected_when_enabled
live_orders_disabled_by_default
test_orders_disabled_by_default
production_order_entry_refuses_to_start
strategy_cannot_bypass_risk_gate_contract
```

If CMake integration is too large, create docs and skeleton files first and explicitly say what remains.

### 10. Update project state

Update:

```text
PROJECT_STATE.md
ROADMAP.md
AI_CONTEXT.md
```

Add:

```text
New direction: MOEX SPECTRA FIX/FAST test access and realtime data collection.
Current status: planning / skeleton only.
No live trading.
No broker connection.
No real credentials.
No real orders.
```

## Required final report

Update or create:

```text
agent_workspaces/mimo/reports/YYYY-MM-DD-moex-spectra-fix-fast-plan.md
```

Include:

```text
- build/test result;
- docs created;
- skeletons created;
- safety gates added;
- exact next manual steps for owner;
- exact information needed from MOEX after application;
- next recommended task.
```

## Manual steps for owner after this task

The report must tell owner to:

```text
1. Open https://www.moex.com/s438
2. Fill the SPECTRA test access application form.
3. Specify protocols: FIX/FAST.
4. Specify T0 first.
5. Ask whether FAST Full_orders_log is available on test contour.
6. Ask whether internet connection is enough for test FAST.
7. Wait for credentials / host / port / multicast config / templates.
8. Do not paste credentials into ChatGPT or commit them to Git.
9. Put credentials only into local config/env on the machine.
```

## Done criteria

```text
1. MOEX connectivity docs created;
2. test access request checklist created;
3. realtime collector architecture documented;
4. safe example config created;
5. gitignore protects raw data and secrets;
6. initial dry-run CLI skeletons created where feasible;
7. normalized contracts documented or stubbed;
8. safety gates documented and/or tested;
9. project state/roadmap updated;
10. no real credentials committed;
11. no raw data committed;
12. no live order sending implemented;
13. final report written.
```

## Safety

```text
No broker connection.
No production FIX/TWIME order sending.
No live trading.
No real order sending.
No real MOEX credentials in repo.
No .env commit.
No raw market data commit.
No profitability claims.
No bypass around RiskGate.
Production trading requires separate owner approval and explicit separate task.
```

## Expected next task

After MOEX test access credentials/configs are available:

```text
MOEX_SPECTRA_FAST_TEST_FEED_CONNECT_AND_CAPTURE_DRY_RUN
```

After FAST test feed works:

```text
MOEX_SPECTRA_FULL_ORDERS_LOG_PRODUCTION_DATA_COLLECTION_PLAN
```

After production market data collection works:

```text
MOEX_PAPER_TRADING_ENGINE_ON_REALTIME_DATA
```

Only after long paper validation:

```text
MOEX_PRODUCTION_TRADING_GATE_REVIEW_AND_CERTIFICATION_PLAN
```
