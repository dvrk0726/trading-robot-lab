# AI Context

Дата обновления: 2026-07-15
Репозиторий: `dvrk0726/trading-robot-lab`
Текущий gate: RT-4 research/specification — Issue #38, Draft PR #39

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
Secrets, personal data and raw market data не хранятся в Git.
```

## Performance-first development policy

```text
C++20 owns every latency-critical realtime/hot-path component:
  MOEX packet/framing, sequencing and recovery, FAST decoding,
  normalized realtime market-data events, future L3/L2 books,
  realtime state, RiskEngine, OrderManager and future runtime
  execution components.
Python is not used in the hot path; it is used for research,
  analysis, reports, UI, orchestration and offline tooling.
Correctness, deterministic behavior and fail-closed validation
  are mandatory and cannot be sacrificed for speed.
Performance claims are accepted only from measured Release benchmarks;
  relevant metrics include latency distribution and tail latency,
  throughput, allocations, memory behavior and execution-time
  predictability.
Do not add speculative abstractions, genericity, compatibility layers
  or unnecessary allocations to the critical path without measured
  necessity.
Development velocity is a priority; prefer small functional gates
  that advance the MOEX system over premature infrastructure
  complexity.
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
QSH retirement: DONE
Performance-first documentation: DONE — Issue #36 / PR #37
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
  required checks active, deletion and force-push blocked
```

Accepted implementation: specialized MOEX SPECTRA T0/T1 decoder, not a general-purpose FAST 1.1 engine.

## Authoritative RT-3 target

Historical accepted implementation profiles:

```text
T0 templatesT0/templates.xml
SHA-256 DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E
Role: test system corresponding to the production trading-system version

T1 templatesT1/templates.xml
SHA-256 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Role: next trading-system release
```

These are the RT-3 accepted compile/test profiles. Current official endpoint contents are rechecked separately by RT-4 and do not rewrite historical acceptance evidence.

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

## QSH retirement — DONE

```text
Issue: #33 - QSH retirement record
PR: #34, merged
Main merge SHA: 7c05cfb979cd0144be508e41a6f3a6229bfab1cb
Post-merge CI: #175 / run 29361711016 — success, exactly 6 jobs

Historical implementation evidence:
  Stage 2A head: 0a39e7cd5ace38adce28d32f6eb1a325a9e1d1c2
  Stage 2A CI: #172 / run 29359345488 — success, exactly 6 jobs

The QSH/QScalp/OrdLog product support, old QSH L3/L2 book,
Trading Lab QSH integration, tombstone job, run_qsh and QSH routing
are retired and absent. They are not part of the future architecture.

*.qsh remains mentioned only as a raw-market-data safety ban; not
product support.
```

## RT-4 research/specification — CURRENT

```text
Issue: #38 open
Draft PR: #39 open
Branch: docs/issue-38-rt4-spec
Base main SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
Scope: documentation only
MiMo: not authorized
Implementation: not started and not authorized
Merge: not authorized
```

Authoritative RT-4 documents in PR #39:

```text
docs/rt4_spectra_framing_sequencing_recovery_spec.md
docs/rt4_moex_fast_source_update_2026-07-15.md
```

Architecture gates:

```text
Gate A: framing, A/B sequencing, bounded reordering, gaps, fail-closed
Gate B: .mxraw + RT-3 integration and preamble AutoVerify
Gate C: Snapshot + buffered Incremental recovery
Gate D: Release performance and production-evidence acceptance
```

Current official-source update:

```text
MOEX SPECTRA FAST: v1.30.2, 2026-04-10
T0 configuration SHA-256:
AE80702BC3E179CAF5DA025E94FDAC6AC7A6A4FF1353E7FB5D0396DE987C4118
Current T0 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Current T1 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
External 4-byte preamble byte order: unresolved in official text
```

Official `fast_sensor` 1.30.0.1337 accepted the T0 configuration. A safe bounded gap/statistics/order-check received zero packets while MOEX access/routing confirmation remained pending. No credentials, connection addresses or raw market-data captures are stored.

Gate A requires explicit LittleEndian or BigEndian configuration and no default guess. Gate B may compare both interpretations with decoded tag 34, fail closed on ambiguity, and lock one byte order per logical feed after verification.

## Sequence

```text
RT-4 documentation/specification PR #39
-> Architecture Review
-> separate Owner merge authorization
-> post-merge main CI verification
-> separate Owner authorization for Gate A implementation
```

CI-2 caching is POSTPONED, not started and not authorized. Reconsider only when measured CI duration or cost materially slows development.

RT-4 implementation remains not started and not authorized. Future normalized events and the new L3/L2 book are designed from official MOEX SPECTRA data; no automatic reuse of old QSH code.

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

For `CHANGES_REQUIRED`:

```text
use the existing branch and PR;
read the current specification and review instruction;
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
Current task: finish documentation-only Issue #38 / Draft PR #39.
Review exact five-file diff and docs-only CI.
Do not start RT-4 implementation.
Do not launch MiMo.
Do not merge without separate explicit Owner authorization.
```
