# Roadmap

Дата обновления: 2026-07-13  
Статус: gated engineering roadmap  
Текущий gate: RT-3 implementation correction in existing PR #23

## Главный порядок

```text
repository workflow and protection
-> local MOEX FAST metadata inspection
-> raw segment contract and deterministic replay
-> specialized MOEX T0/T1 FAST decoding
-> SPECTRA framing, sequencing and recovery
-> realtime data quality and books
-> research/backtest/paper
-> VPTS/certification
-> owner-approved production only later
```

Следующий этап не начинается до закрытия текущего gate.

## Завершено

### Workflow foundation — DONE

```text
branch-only implementation
Pull Request review
CI baseline
no MiMo merge or auto-merge
procedural main protection
scope-freeze protocol
```

### RT-1 — DONE

```text
local configuration.xml/templates.xml inspector
normalized metadata and provenance
Windows/Linux Release tests
```

### RT-2 — DONE

```text
.mxraw v1 raw segment contract
synthetic capture/inspect/replay
bounded validation
CRC32C and SHA-256 provenance
deterministic replay
Windows/Linux Release tests
```

## RT-3 — current gate

### Objective

One template-driven C++20 decoder for the accepted official MOEX SPECTRA T0 and T1 template files.

### Verified checkpoint

```text
Issue #21: CHANGES_REQUIRED
PR #23: open
Branch: mimo/issue-21-rt3-fast-decoder
Head: 062649476500f6060d22b9740cafa1b0250f3ba5
Corrected specification PR #27: merged
Main merge commit: e2e616673758b1cb888f5e3b4b7844343327c579
Post-merge CI #126: success
```

### Accepted profile

```text
T0 SHA-256 DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E
T1 SHA-256 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Required scope:

```text
field without operator
constant
template ID reuse
presence maps
ordinary and nullable integers
ASCII and Unicode strings
exact decimal
sequences and single length instruction
limits, reset and transactional rollback
T0/T1 official XML compilation
Windows/Linux Release tests
```

Excluded scope:

```text
default/copy/increment/delta/tail
generic field dictionaries
user-defined dictionaries
references and cycle resolution
generic groups outside T0/T1
decimal component operators
historical FAST profile compatibility
```

### RT-3 remaining sequence

```text
1. Synchronize canonical state/workflow documents.
2. Merge current main into PR #23 branch without force push.
3. Re-audit the resulting implementation diff.
4. Remove generic FAST code and tests not required by T0/T1.
5. Correct production-critical pmap, nullable and sequence behavior.
6. Prove T0/T1 compilation and exact test inventory on Windows/Linux.
7. Architecture review.
8. Owner local acceptance.
9. Owner-authorized merge.
10. Post-merge main CI and Issue #21 DONE.
```

Each MiMo run is one small owner-authorized correction in the existing branch and PR.

## RT-4 — BLOCKED

Planned only after RT-3 is DONE:

```text
MOEX 4-byte preamble and message boundary
SPECTRA packet/framing contract
A/B sequencing and deduplication
gap detection and recovery
Snapshot + buffered Incremental bootstrap
```

No RT-4 implementation, branch or MiMo prompt is allowed before RT-3 acceptance, merge and green post-merge CI.

## Later stages

```text
RT-5 realtime data quality and normalized events
RT-6 ORDERS-LOG L3/L2 and storage
RT-7 T0 pilot and measured capacity
RT-8 research/backtest/paper on certified data
RT-9 FIX/TWIME test and VPTS readiness
RT-10 production certification and explicit owner gate
```

Names and scope of later stages remain provisional until the preceding gate supplies evidence.
