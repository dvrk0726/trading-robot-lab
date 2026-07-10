# Roadmap

Дата обновления: 2026-07-10  
Статус: gated engineering roadmap

## Главный порядок

```text
repository workflow and protection decision
-> local FAST metadata inspection
-> raw capture/replay contracts
-> specialized FAST decoding
-> realtime book/data quality
-> research/backtest/paper
-> VPTS/certification
-> owner-approved production only later
```

Нельзя перескакивать gate из-за готовности отдельного компонента.

## Completed foundation

### Architecture and repository

```text
Trading Lab / Trading Runtime / Shared Contracts architecture;
ADR-0001 and ADR-0002;
security baseline;
shared schemas/test vectors;
Strategy Package standard;
local Trading Lab demo foundation.
```

### Historical C++ data contour

```text
ADR-0003;
QSH/OrdLog ingest;
L3/order-book reconstruction and data-quality diagnostics;
M10X complete;
20/20 regression tests;
control commit 54cd53df4b92473e49dd5dff96b2024590b82e42.
```

Remaining 907 crossed snapshots remain gated with `strategy_ready=false`.

### MOEX realtime research

```text
FAST 1.29.1 studied;
FAST_9.0 templates and T0 configuration studied;
FIX SPECTRA and VPTS requirements studied;
ADR-0004;
MOEX_REALTIME_ARCHITECTURE documented;
QuickFAST rejected as production hot-path foundation;
specialized C++ decoder direction accepted;
test-access questionnaire sent.
```

## Gate WF-1 — MiMo/GitHub workflow

Status:

```text
IMPLEMENTED IN PR / OWNER REVIEW GATE
Issue #1
Pull Request #15
RT-1 blocked
Merge not performed
```

Implemented result:

```text
permanent MiMo instruction;
READY_FOR_MIMO one-line command;
one task at a time;
dedicated branch and Pull Request;
no direct implementation in main;
no auto-merge and no MiMo merge;
canonical statuses;
Python/C++/hygiene GitHub Actions;
existing 20 QSH/M10X regression gate;
secret/raw-data/large-file checks;
Owner Review Package;
label synchronization;
current AI_CONTEXT/PROJECT_STATE/ROADMAP;
main protection option guide.
```

Verified full CI run #8:

```text
Repository hygiene: PASS
Python tests and contracts: PASS
C++ QSH M10X regression: PASS
CTest inventory: exactly 20
All 20 tests: PASS
```

Any later PR head must repeat these checks before merge.

WF-1 remaining acceptance:

```text
current PR #15 head passes all required checks;
Architecture/Review accepts final diff;
Owner accepts process;
Owner chooses main protection option;
PR #15 merged manually;
labels synchronized;
auto-merge disabled;
selected protection mode recorded;
Issue #1 DONE.
```

Main protection options:

```text
A. Enable server-side ruleset/branch protection if available on the GitHub plan.
B. Owner decides on upgrade or explicitly accepts temporary procedural protection.
```

No purchase or plan change is automatic. Procedural protection still requires branch, PR, CI, review, manual merge and no direct main work by MiMo.

Until all WF-1 acceptance items are complete, Issue #14 remains `DRAFT`.

## RT-1 — FAST configuration/templates inspector

Status:

```text
DRAFT / specification complete / implementation not started
Issue #14
Blocked by WF-1
```

Scope:

```text
local C++20/CMake CLI;
parse configuration.xml and templates.xml;
list ORDERS-LOG/FUT-INFO metadata;
validate template IDs and ordered fields;
calculate SHA-256;
produce human-readable and deterministic JSON;
synthetic/sanitized fixtures;
new tests plus existing 20 QSH/M10X regressions.
```

Non-goals:

```text
network;
UDP/TCP recovery;
FAST binary decoder;
FIX/TWIME;
credentials;
order sending;
QuickFAST production dependency;
official private XML in Git;
QSH semantic change;
strategy_ready weakening.
```

RT-1 completion:

```text
MiMo branch and PR;
all CI passed;
Architecture/Review accepted;
Owner review if a visible report/UI is included;
manual merge;
PROJECT_STATE/ROADMAP updated;
Issue #14 DONE.
```

## RT-2 — Raw capture/replay contract

Blocked by RT-1 acceptance.

Planned scope:

```text
capture metadata contract;
packet timestamp/sequence/source identity;
local file rotation and provenance;
deterministic replay input format;
no production connection credentials in Git.
```

## RT-3 — Specialized C++ FAST decoder foundation

Blocked by RT-1 and RT-2.

Planned scope:

```text
FAST primitives/operators;
template-driven state;
mandatory-field checks;
sequence/reset handling;
differential tests against reference tools;
no full order-entry stack.
```

## RT-4 — SPECTRA feed processors

Blocked by decoder correctness.

```text
FUT-INFO;
ORDERS-LOG Snapshot;
ORDERS-LOG Incremental;
Snapshot + buffered Incremental bootstrap;
sequence gap/recovery state;
normalized market events.
```

## RT-5 — Realtime Data Quality and book state

Blocked by RT-4.

```text
sequence freshness;
book invariants;
crossed/locked diagnostics;
recovery status;
strategy_ready gating;
replay/live parity evidence.
```

No strategy-ready claim without measured evidence.

## Research and paper stages

Only after trustworthy data contour:

```text
research hypotheses;
deterministic backtests;
fees/slippage/latency sensitivity;
out-of-sample checks;
paper trading;
operational and risk evidence.
```

Historical results do not authorize live.

## Certification and production gate

Before any production order entry:

```text
VPTS/certification requirements satisfied;
approved access and network architecture;
RiskEngine and kill switch reviewed;
monitoring/audit/recovery complete;
Owner explicitly approves stage, cost and access;
production remains disabled by default.
```

## Immediate sequence

```text
1. Confirm final CI on current PR #15 head.
2. Complete Architecture/Review and Owner review.
3. Owner selects main protection option without automatic spending.
4. Merge PR #15 manually only after acceptance.
5. Synchronize labels and apply/record selected protection mode.
6. Mark Issue #1 DONE.
7. Move Issue #14 to READY_FOR_MIMO.
8. Owner runs the universal MiMo command once.
9. MiMo implements RT-1 in a separate branch and stops at PR.
10. Review RT-1 before any RT-2 work.
```
