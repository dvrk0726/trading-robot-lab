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
DONE
Issue #1: DONE
Pull Request #15: merged (82077f6e54e439f27027301ac02813c018d380fc)
```

Merged result:

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

## RT-1 — FAST configuration/templates inspector

Status:

```text
READY_FOR_REVIEW (Round 5 corrections applied)
Issue #14
Branch: feat/rt-1-fast-config-inspector
PR: #16
Head: 1d8b12a703ba4860262210ff430cb7ff10c5d2f6
CI run #20: all 5 jobs green (pending push)
Review cycle: Round 5 CHANGES_REQUIRED → Round 5 corrections applied
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
1. Round 5 corrections applied for RT-1 on feat/rt-1-fast-config-inspector.
2. Commit 1d8b12a703ba4860262210ff430cb7ff10c5d2f6.
3. CI run #20: all 5 jobs green (Windows 6/6, Linux 6/6, QSH 20/20, Python 3/3, hygiene PASS).
4. Issue #14 moved to READY_FOR_REVIEW.
5. Stop — do not merge, do not start RT-2.
```
