# Project State

Дата обновления: 2026-07-13  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-1 DONE; RT-2 DONE; RT-3 CHANGES_REQUIRED in PR #23

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
Implementation PR #23: open
Branch: mimo/issue-21-rt3-fast-decoder
Head: 062649476500f6060d22b9740cafa1b0250f3ba5
Corrected specification PR #27: merged
Main merge commit: e2e616673758b1cb888f5e3b4b7844343327c579
Post-merge CI #126: success
```

PR #23 was implemented against a superseded near-general FAST scope. It must be corrected in the existing branch and PR to the merged MOEX T0/T1 profile.

Issue #21 body, PR #23 title/body and canonical PR scope comment have been synchronized to the corrected specification. Historical conflicting review instructions are non-executable.

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
decimal component operators
historical profile compatibility
```

Unsupported XML must fail compilation explicitly.

## Current blockers

```text
state/workflow synchronization PR #28 is not merged yet;
PR #23 branch has not yet been synchronized with main after PR #27;
PR #23 code and tests still contain generic FAST scope that must be removed;
no implementation correction is owner-authorized yet.
```

## Next verified sequence

```text
PR #28 owner review and merge
-> post-merge main CI
-> merge current main into PR #23 branch without force push
-> re-audit actual diff
-> freeze one small correction task
-> owner authorization
-> MiMo correction in existing PR #23
-> CI and architecture review
-> owner local acceptance
-> owner-authorized merge
-> post-merge main CI
-> Issue #21 DONE
```

RT-4 remains blocked until the complete RT-3 sequence finishes.
