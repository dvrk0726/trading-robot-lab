# Project State

Дата обновления: 2026-07-14
Репозиторий: `dvrk0726/trading-robot-lab`
Статус: RT-1 DONE; RT-2 DONE; RT-3 acceptance evidence synchronized, awaiting final review and merge

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

## RT-3 — current gate

```text
Issue #21: open, CHANGES_REQUIRED
Implementation PR #23: open, not merged
Branch: mimo/issue-21-rt3-fast-decoder
Last verified implementation/evidence head: 3fde6847d652ebd5277ca03a496dc701392eb75e
CI #155: success, all 7 jobs passed
Corrected specification PR #27: merged
Main merge commit: e2e616673758b1cb888f5e3b4b7844343327c579
```

PR #23 was corrected to the merged MOEX T0/T1 profile in its existing branch. Issue #21 body, PR #23 title/body and canonical PR scope comment are synchronized to the corrected specification.

Acceptance evidence on 3fde6847d652ebd5277ca03a496dc701392eb75e:

```text
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

## Current blockers

```text
RT-3 is not declared DONE and not declared merged.
After this state-sync commit, the following remain:
  update evidence in Issue #21 and PR #23;
  final architecture review;
  explicit owner merge authorization;
  manual owner merge;
  post-merge main CI green.
RT-4 remains BLOCKED until the full RT-3 sequence finishes.
```

## Next verified sequence

```text
1. This state-sync commit: synchronize final acceptance evidence.
2. Update evidence in Issue #21 and PR #23.
3. Final architecture review.
4. Explicit owner merge authorization.
5. Manual owner merge.
6. Post-merge main CI green.
7. Issue #21 DONE.
```

RT-3 is not declared DONE and not declared merged until all of the above are complete. RT-4 remains BLOCKED until the full RT-3 sequence finishes.

After RT-3 closure, the next engineering gate is CI optimization: job routing by changed files, full matrix on main/manual gate, vcpkg/CMake caching as a separate step. CI optimization is not started now.
