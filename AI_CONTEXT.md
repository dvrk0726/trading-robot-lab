# AI Context

Дата обновления: 2026-07-15  
Репозиторий: `dvrk0726/trading-robot-lab`  
Текущий gate: RT-4 Gate A1 post-merge state sync — Issue #44, Draft PR #45

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
Secrets, personal data, private connection data and raw market data не хранятся в Git.
```

## Performance-first policy

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
Performance claims are accepted only from measured Release benchmarks.
Do not add speculative abstractions, compatibility layers or
  unnecessary allocations to the critical path without measured need.
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
CI-1: DONE — required-check-preserving routing
QSH retirement: DONE
Performance-first documentation: DONE — Issue #36 / PR #37
RT-4 specification: DONE — Issue #38 / PR #39
RT-4 post-merge state sync: DONE — Issue #40 / PR #41
RT-4 Gate A1 UDP framing: DONE — Issue #42 / PR #43
```

RT-2 payload bytes остаются opaque. `capture_index` не является FAST `MsgSeqNum` или exchange sequence.

## RT-3 verified checkpoint

```text
Issue #21: closed completed
Implementation PR #23: merged
Final reviewed PR head: a1443d3f909151d327b83042e43c1cc4c04cc732
Merge commit on main: 377618c360c165d88dde4cfe0cee87f8747cba03
Pre-merge CI #156: success
Post-merge CI #157: success
Owner-local Windows Release acceptance: inventory 15/15, PASS
```

Accepted implementation: specialized MOEX SPECTRA T0/T1 decoder, not a general-purpose FAST 1.1 engine.

Historical accepted RT-3 compile/test profiles:

```text
T0 templates SHA-256:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Current official endpoint contents are tracked separately by RT-4 and do not rewrite historical RT-3 acceptance evidence.

## QSH retirement — DONE

```text
Issue #33
PR #34 merged
Main merge SHA: 7c05cfb979cd0144be508e41a6f3a6229bfab1cb
Post-merge CI #175: success
```

QSH/QScalp/OrdLog product support, old QSH L3/L2 book, Trading Lab QSH integration, tombstone job, `run_qsh` and QSH routing are retired and absent. `*.qsh` remains mentioned only as a raw-market-data safety ban.

## RT-4 specification — DONE

```text
Issue #38: closed completed
PR #39: merged
Reviewed PR head: afd128a49584fce1131323ac7b19e5b5d7b1997a
Main merge SHA: 136293ede211619b7d9198d85ed3afb0f2577514
Post-merge main CI #189: success
```

Authoritative RT-4 documents:

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

Current official-source evidence:

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

Gate A requires explicit LittleEndian or BigEndian configuration and no default guess. Gate B may compare both interpretations with decoded tag 34, fail closed on ambiguity, and lock one byte order per logical feed after verification.

## RT-4 state-sync verified checkpoint

```text
Issue #40: closed completed
PR #41: merged
Reviewed PR head: 6789fb3621d70465114a32d2b146562e7f6809e8
Main merge SHA: acb74763e7dd395f210ac738c425c7d544a6cb51
Post-merge main CI #194: success
RT-4 implementation before Issue #42: not started
```

## RT-4 Gate A1 verified checkpoint

```text
Issue #42: closed completed
PR #43: merged
Reviewed PR head: fc8c42bcd34ed65851267e9fefbc379d7206d2ca
Main merge SHA: ebfb3096b8a62704e5bf57a77d7971fd36acef2a
Pre-merge CI #199: success
Post-merge main CI #200: success
MOEX FAST inventory: 16 = RT-1 6 + RT-3 9 + RT-4 A1 1
```

Accepted A1 implementation: explicit-endian 4-byte external MsgSeqNum framing, one borrowed FAST body beginning at byte four, bounded deterministic validation and empty output on failure. Production framing code performs no payload copy or heap allocation. A2 sequencing and duplicate suppression are not included.

## MOEX connectivity checkpoint

```text
MOEX support confirmed access activation for the registered external static IPv4.
Official Windows instruction: PPTP; data encryption optional.
Current home external IPv4 matches the registered address.
Windows VPN result: remote-access error 807.
TCP 1723 to the supplied VPN endpoint: unreachable from registered connection.
Windows Firewall default outbound policy: Allow.
Explicit enabled outbound block targeting the endpoint: not found.
Registered antivirus: Windows Defender only.
MOEX support follow-up: pending.
```

Do not store the VPN endpoint, external/private IP addresses, credentials, VPN profiles, screenshots containing connection details, or raw/decoded market-data packets.

## Current gate — RT-4 A1 post-merge state sync

```text
Issue #44: open
Draft PR #45: open
Branch: docs/issue-44-rt4-a1-post-merge-state-sync
Base main SHA: ebfb3096b8a62704e5bf57a77d7971fd36acef2a
Scope: exactly AI_CONTEXT.md, PROJECT_STATE.md and ROADMAP.md
Code, tests, CMake and workflow changes: prohibited
MiMo: prohibited
Merge: not authorized
A2: not started and not authorized
```

## Sequence

```text
complete Issue #44 three-file state synchronization
-> docs-only CI
-> Architecture Review
-> separate Owner merge authorization
-> post-merge main CI verification
-> close Issue #44 completed
```

A2 does not begin automatically. It requires a separate current-state review, bounded plan, new Issue/branch/PR and explicit Owner authorization.

CI-2 caching is POSTPONED, not started and not authorized.

## Workflow

Authoritative process documents:

```text
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/mimo_developer_workflow.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

MiMo is launched only with an exact Owner-authorized prompt supplied by Architecture/Review:

```text
mimo --model xiaomi/mimo-v2.5-pro --prompt "<exact task>"
```

MiMo never writes to `main`, merges, enables auto-merge, force-pushes, deletes branches or begins the next task.

## Immediate next gate

```text
Review the exact three-file documentation diff in Draft PR #45.
Verify docs-only CI.
Do not modify code or launch MiMo.
Do not merge without separate explicit Owner authorization.
Do not begin A2.
```
