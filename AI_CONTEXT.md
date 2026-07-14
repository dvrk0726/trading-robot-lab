# AI Context

Дата обновления: 2026-07-14
Репозиторий: `dvrk0726/trading-robot-lab`
Текущий gate: RT-1 DONE; RT-2 DONE; RT-3 DONE; CI-1 DONE; QSH retirement Issue #33 / draft PR #34, Stage 1 in progress (not merged)

## Источник истины

Перед существенной работой проверить фактический GitHub:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
current Issue and labels
current Pull Request, branch and full head SHA
changed files and diff
latest CI
```

При конфликте документов, чата, MiMo report и GitHub доверять фактическим Issue, PR, commits, code, tests and CI.

## Неизменяемая архитектура

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry заблокирован до VPTS/certification и решения Owner.
C++ используется для low-level data/realtime/runtime contour.
Python используется для research, reports and UI.
Secrets, personal data and raw market data не хранятся в Git.
```

Authoritative ADRs:

```text
decisions/ADR-0001-hybrid-python-cpp-architecture.md
decisions/ADR-0002-two-system-lab-runtime-architecture.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Подтверждённые завершённые этапы

```text
RT-1: DONE — local MOEX configuration/templates inspector
RT-2: DONE — .mxraw v1 raw segment and deterministic replay
RT-3: DONE — specialized MOEX SPECTRA T0/T1 decoder
CI-1: DONE — required-check-preserving routing, docs-only smoke PR #32,
  main SHA 0699a533a1ed44a9d47e05b049aca4061bebaac0, post-merge CI #165 success
```

RT-2 payload bytes остаются opaque. `capture_index` не является FAST `MsgSeqNum` или exchange sequence.

## RT-3 — DONE

```text
Issue #21: closed, completed
Implementation PR #23: merged
Final reviewed PR head: a1443d3f909151d327b83042e43c1cc4c04cc732
Merge commit on main: 377618c360c165d88dde4cfe0cee87f8747cba03
Pre-merge CI #156: success, all 7 jobs passed
Post-merge main CI #157: success, all 7 jobs passed
Owner-local Windows Release acceptance on 3fde6847d652ebd5277ca03a496dc701392eb75e:
  configure/build success, inventory 15/15, PASS
Repository: Public
Owner server-side protection active: main branch, PR required,
  unresolved conversations block merge, branch must be up to date,
  7 CI checks required, deletion and force-push blocked
```

Accepted implementation: specialized MOEX SPECTRA T0/T1 decoder, not a general-purpose FAST 1.1 engine.

## Authoritative RT-3 target

Build one template-driven C++20 decoder for the two accepted official MOEX SPECTRA profiles:

```text
T0 templatesT0/templates.xml
SHA-256 DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E
Role: test system corresponding to the production trading-system version

T1 templatesT1/templates.xml
SHA-256 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Role: next trading-system release
```

`FAST_9.0/templates.xml` is byte-identical to accepted T0 and is not a third profile. `FAST_8.6` and `backup/` are not RT-3 targets.

Source priority:

```text
1. MOEX SPECTRA FAST documentation and official templates
2. FIX FAST 1.1 only for base wire rules MOEX does not restate
3. third-party implementations only as cross-check
```

## RT-3 required scope

```text
one bounded FAST message body
template ID and previous-template-ID reuse
presence maps
ordinary and nullable uInt32/uInt64/int32/int64
ASCII and charset="unicode" strings
exact decimal exponent/mantissa
field without operator
constant
mandatory and optional sequences
single sequence length instruction
limits, deterministic issues, reset and transactional rollback
Windows/MSVC and Linux/GCC Release tests
T0 and T1 official XML compilation outside Git
```

## RT-3 excluded scope

The following must be rejected fail-closed and removed from positive production/test claims:

```text
default
copy
increment
delta
tail
generic field dictionaries and scopes
user-defined dictionaries
typeRef
templateRef
groupRef
reference resolution and cycle detection
generic group instructions outside T0/T1
byteVector
decimal component operators
historical FAST profile compatibility
```

Previous-template-ID reuse is retained and is not the XML `<copy>` operator.

## QSH retirement — in progress (Stage 1, not merged)

```text
Issue: #33
Draft PR: #34, branch mimo/issue-33-qsh-retirement-stage1
Stage 1 is in progress. Not merged. Not complete.

The QSH/QScalp/OrdLog implementation, old QSH L3/L2 book,
Trading Lab QSH integration and archive QSH documents are being
retired. They are not part of the future architecture.

The exact check name C++ QSH M10X regression (20 tests) remains
only as a temporary tombstone because the main ruleset still
requires seven checks. Stage 2 may remove the tombstone job/routing
only after the Owner updates the main ruleset from seven to six.

.qsh bans remain only as raw market-data safety barriers, not
product support.
```

## Sequence

```text
finish QSH retirement (Stage 1)
-> synchronize, accept, merge and post-merge verify
-> CI-2 caching
-> separately specified and authorized RT-4
```

RT-4 remains not started and not authorized. Future normalized events
and order book (RT-5/RT-6) must be designed from official MOEX SPECTRA
data; no automatic reuse of the old QSH book.

## Workflow

Authoritative process documents:

```text
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/mimo_developer_workflow.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

MiMo is launched only with an exact owner-authorized prompt supplied by Architecture/Review:

```text
mimo --model xiaomi/mimo-v2.5-pro --prompt "<exact task>"
```

There is no executable universal self-selection command for the current workflow.

For `CHANGES_REQUIRED`:

```text
use the existing branch and PR;
read the merged specification and current review instruction;
change only allowed files;
run exact tests;
create one scoped commit;
push to the existing branch;
wait for CI;
stop.
```

MiMo never writes to `main`, merges, enables auto-merge, force-pushes, deletes branches or begins the next task.

## Immediate next gate

```text
RT-1, RT-2, RT-3 DONE and merged. CI-1 DONE.
QSH retirement Issue #33 / draft PR #34, Stage 1 in progress and not merged.
Current gate: finish QSH retirement, synchronize/accept/merge, post-merge verify.
CI-2 caching follows after QSH retirement is merged.
RT-4 requires a separate specification and explicit Owner authorization. Not started.
```
