# Project State

Дата обновления: 2026-07-15
Репозиторий: `dvrk0726/trading-robot-lab`
Статус: RT-4 research/specification — Issue #38, Draft PR #39

## Архитектурные границы

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry требует VPTS/certification и решения Owner.
Secrets, private connection data and raw market data не хранятся в Git.
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

## Завершённые этапы

### RT-1 — DONE

```text
Issue #14
Implementation PR #16
Merge commit ab74f560c1bcf9d09ae7bdfb8552c745928fd022
Offline MOEX configuration/templates inspector
Windows/Linux Release tests
```

### RT-2 — DONE

```text
Issue #18
Specification PR #19
Implementation PR #20
Merge commit 060371112d921c1c1f4055cfbdb99049bdf8a2af
.mxraw v1 raw segments and deterministic replay
Owner local Release acceptance passed
```

RT-2 does not decode FAST and does not infer exchange sequence from `capture_index`.

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

## CI-1 — DONE

```text
Required-check-preserving routing
Docs-only smoke PR #32
Main SHA: 0699a533a1ed44a9d47e05b049aca4061bebaac0
Post-merge CI #165: success
```

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

*.qsh remains mentioned only as a raw-market-data safety ban.
```

## Performance-first documentation — DONE

```text
Issue #36: closed completed
PR #37: merged
Main merge SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
CI-2 caching: POSTPONED
RT-4 research/specification selected as next gate
```

## Authoritative RT-3 profile

Historical accepted implementation profiles:

```text
T0 SHA-256:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

These hashes remain the accepted RT-3 compile/test evidence. Current official endpoint contents are tracked separately by RT-4.

Required RT-3 scope:

```text
one bounded FAST message body
template ID and previous-template-ID reuse
presence maps
ordinary/nullable integer primitives
ASCII and Unicode strings
exact decimal
field without operator
constant
sequences and single length instruction
limits, reset, deterministic errors and transactional rollback
T0/T1 official XML compilation
Windows/Linux Release tests
```

Excluded and fail-closed:

```text
default/copy/increment/delta/tail
generic field dictionaries/scopes/keys
user-defined dictionaries
typeRef/templateRef/groupRef
reference resolution/cycles
generic groups outside accepted inventory
byteVector
decimal component operators
historical profile compatibility
```

Unsupported XML must fail compilation explicitly.

## RT-4 research/specification — CURRENT

```text
Issue: #38 open
Draft PR: #39 open
Branch: docs/issue-38-rt4-spec
Base main SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
Scope: exactly five documentation files
RT-4 implementation: not started and not authorized
MiMo: not authorized
Merge: not authorized
```

Authoritative files under review:

```text
docs/rt4_spectra_framing_sequencing_recovery_spec.md
docs/rt4_moex_fast_source_update_2026-07-15.md
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
```

Approved architecture structure:

```text
Gate A: UDP framing, A/B sequencing, bounded reordering,
        gap detection, explicit monotonic timeout and fail-closed
Gate B: RT-2 .mxraw + RT-3 integration, tag-34 verification,
        one-time preamble endian AutoVerify
Gate C: Snapshot + buffered Incremental recovery
Gate D: Windows/Linux Release performance and production evidence
```

Current official-source evidence:

```text
MOEX SPECTRA FAST document: v1.30.2, 2026-04-10
T0 configuration SHA-256:
AE80702BC3E179CAF5DA025E94FDAC6AC7A6A4FF1353E7FB5D0396DE987C4118
Current T0 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Current T1 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
External UDP preamble byte order: unresolved in official text
```

The 2026-07-11 RT-3 source audit remains unchanged as historical evidence. The 2026-07-15 endpoint update is recorded in a separate RT-4 document.

Official `fast_sensor` 1.30.0.1337 accepted current T0 configuration. A safe gap/statistics/order-check test received zero packets while MOEX access/routing confirmation remained pending. No credentials, connection addresses or raw captures are stored.

Gate A uses explicit LittleEndian or BigEndian with no default. Gate B may compare both values with decoded tag 34, fail closed on ambiguity and lock one byte order after verification.

## Sequence

```text
Issue #38 / Draft PR #39 documentation review
-> docs-only CI
-> Architecture Review
-> separate Owner merge authorization
-> post-merge main CI verification
-> separate Owner authorization for RT-4 Gate A implementation
```

CI-2 caching is POSTPONED, not started and not authorized. Reconsider only when measured CI duration or cost materially slows development.

Future normalized events and new L3/L2 book are designed from official MOEX SPECTRA data; no automatic reuse of the old QSH code.
