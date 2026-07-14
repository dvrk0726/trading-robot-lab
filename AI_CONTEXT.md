# AI Context

Дата обновления: 2026-07-14
Репозиторий: `dvrk0726/trading-robot-lab`
Текущий gate: RT-3 pre-merge evidence synchronization

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
decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md
decisions/ADR-0004-moex-vpts-certification-gate.md
```

## Подтверждённые завершённые этапы

```text
RT-1: DONE — local MOEX configuration/templates inspector
RT-2: DONE — .mxraw v1 raw segment and deterministic replay
M10X/QSH regression: 20/20
```

RT-2 payload bytes остаются opaque. `capture_index` не является FAST `MsgSeqNum` или exchange sequence.

## Текущий RT-3 checkpoint

```text
Issue: #21, open, label CHANGES_REQUIRED
Implementation PR: #23, open, not merged
Implementation branch: mimo/issue-21-rt3-fast-decoder
Last verified implementation/evidence head: 3fde6847d652ebd5277ca03a496dc701392eb75e
CI #155: success, all 7 jobs passed
Corrected specification PR: #27, merged
Specification merge commit on main: e2e616673758b1cb888f5e3b4b7844343327c579
RT-4: BLOCKED

Acceptance evidence on 3fde6847d652ebd5277ca03a496dc701392eb75e:
  FAST Release Windows/MSVC: 6 RT-1 + 9 RT-3 = 15 tests, all passed
  FAST Release Linux/GCC: 6 RT-1 + 9 RT-3 = 15 tests, all passed
  RAW Windows: 18 tests passed
  RAW Linux: 18 tests passed
  QSH M10X regression: 20 tests passed
  Python tests/contracts: passed
  Repository hygiene: passed
  Owner-local Windows Release acceptance on 3fde6847d652ebd5277ca03a496dc701392eb75e:
    configure/build success, inventory 15/15, PASS
  Repository: Public
  Owner server-side protection active: main branch, PR required,
    unresolved conversations block merge, branch must be up to date,
    7 CI checks required, deletion and force-push blocked
```

PR #23 remains the only implementation PR for RT-3. No new branch or PR was created.

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
1. This state-sync commit synchronizes final acceptance evidence.
2. Update evidence in Issue #21 and PR #23.
3. Final architecture review.
4. Explicit owner merge authorization.
5. Manual owner merge.
6. Post-merge main CI green.
7. Issue #21 may then move to DONE.
```

RT-3 is not declared DONE and not declared merged until all of the above are complete. RT-4 remains BLOCKED until the full RT-3 sequence finishes.

After RT-3 closure, the next engineering gate is CI optimization: job routing by changed files, full matrix on main/manual gate, vcpkg/CMake caching as a separate step. CI optimization is not started now.
