# Roadmap

Дата обновления: 2026-07-14
Статус: gated engineering roadmap
Текущий gate: RT-3 pre-merge evidence synchronization

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
Issue #21: open, CHANGES_REQUIRED
PR #23: open, not merged
Branch: mimo/issue-21-rt3-fast-decoder
Last verified implementation/evidence head: 3fde6847d652ebd5277ca03a496dc701392eb75e
CI #155: success, all 7 jobs passed
Corrected specification PR #27: merged
Main merge commit: e2e616673758b1cb888f5e3b4b7844343327c579
```

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
byteVector
decimal component operators
historical FAST profile compatibility
```

### RT-3 remaining sequence

```text
1. This state-sync commit: synchronize final acceptance evidence.
2. Update evidence in Issue #21 and PR #23.
3. Final architecture review.
4. Explicit owner merge authorization.
5. Manual owner merge.
6. Post-merge main CI green.
7. Issue #21 DONE.
```

RT-3 is not declared DONE and not declared merged until all of the above are complete. Each MiMo run is one small owner-authorized task in the existing branch and PR.

### Next engineering gate after RT-3 closure

```text
CI optimization:
  job routing by changed files
  full matrix on main and manual gate
  vcpkg/CMake caching as a separate step
```

Not started. Not part of RT-3 or RT-4.

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
