# Project State

Дата обновления: 2026-07-14
Репозиторий: `dvrk0726/trading-robot-lab`
Статус: RT-1 DONE; RT-2 DONE; RT-3 DONE; CI-1 DONE; QSH retirement Stage 1 + Stage 2A implementation complete (not merged)

## Архитектурные границы

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry требует VPTS/certification и решения Owner.
Secrets, private connection data and raw market data не хранятся в Git.
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
  7 CI checks required, deletion and force-push blocked
```

Accepted implementation: specialized MOEX SPECTRA T0/T1 decoder, not a general-purpose FAST 1.1 engine.

## CI-1 — DONE

```text
Required-check-preserving routing
Docs-only smoke PR #32
Main SHA: 0699a533a1ed44a9d47e05b049aca4061bebaac0
Post-merge CI #165: success
```

## QSH retirement — implementation complete in PR #34 (not merged)

```text
Issue: #33
Draft PR: #34, branch mimo/issue-33-qsh-retirement-stage1
Stage 2A implementation head: 0a39e7cd5ace38adce28d32f6eb1a325a9e1d1c2
Stage 2A CI #172 / run 29359345488: success, exactly 6 jobs

Stage 1 (product/docs removal) and Stage 2A (CI/routing removal)
are implemented. The gate is not merged and not finally complete
until Owner merge and successful post-merge main CI.

The QSH/QScalp/OrdLog product support, old QSH L3/L2 book,
Trading Lab QSH integration and archive QSH documents are retired.
They are not part of the future architecture.

The QSH tombstone job, run_qsh, QSH routing and QSH routing tests
are removed.

The active Protect main ruleset (ID 18924726) now requires exactly
six checks:
  - Repository hygiene
  - Python tests and contracts
  - C++ MOEX FAST Windows/MSVC (RT-1: 6, RT-3: 9)
  - C++ MOEX FAST Linux/GCC (RT-1: 6, RT-3: 9)
  - C++ MOEX RAW Windows/MSVC (18 tests)
  - C++ MOEX RAW Linux/GCC (18 tests)

.qsh remains mentioned only as a raw-market-data safety ban.
```

## Authoritative RT-3 profile

```text
T0 SHA-256:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Required:

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

Excluded and to be removed from positive implementation claims:

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

## Sequence

```text
final Architecture Review and Owner merge authorization
-> merge PR #34
-> post-merge main CI with six jobs
-> close Issue #33
-> CI-2 caching
-> separately specified and authorized RT-4
```

RT-4 remains not started and not authorized. Future normalized events
and order book (RT-5/RT-6) must be designed from official MOEX SPECTRA
data; no automatic reuse of the old QSH book.
