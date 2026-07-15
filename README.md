# trading-robot-lab

Research and engineering repository for a two-system trading architecture:

```text
Trading Lab      — research, data quality, replay, backtests, reports and UI.
Trading Runtime  — future execution of approved Strategy Packages.
Shared Contracts — versioned events, signals, OrderIntent and RiskDecision schemas.
```

## Current state

```text
RT-1 local MOEX configuration/templates inspector: DONE
RT-2 .mxraw v1 raw capture/replay contract: DONE
RT-3 specialized MOEX SPECTRA T0/T1 FAST decoder: DONE
CI-1 baseline CI and documentation: DONE
QSH retirement: DONE (PR #34 merged, main SHA 7c05cfb979cd0144be508e41a6f3a6229bfab1cb,
  post-merge CI #175 / run 29361711016 success, 6 jobs)
  Active Protect main ruleset (ID 18924726): 6 required checks.
CI-2 caching: POSTPONED, not started, not authorized
RT-4 research/specification: next separate gate
RT-4 implementation: not started, not authorized
```

Read before work:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/mimo_developer_workflow.md
```

## Architecture

```text
Trading Lab cannot send real orders.
Strategy cannot call execution directly.
Every OrderIntent must pass RiskEngine.
Live is disabled by default.
Production order entry is certification- and owner-gated.
Secrets, private connection data and raw market data are absent from Git.
```

### Language boundary

```text
C++20 — latency-critical realtime/hot path:
  MOEX packet/framing, sequencing and recovery, FAST decoding,
  normalized realtime market-data events, future L3/L2 books,
  realtime state, RiskEngine, OrderManager and future runtime
  execution components.

Python — research, analysis, reports, UI, orchestration and
  offline tooling. Not used in the hot path.
```

Performance claims require measured Release benchmarks: latency
distribution and tail latency, throughput, allocations, memory
behavior and execution-time predictability.

Architecture decisions are stored under `decisions/`.

## Implementation workflow

All implementation and process changes use a dedicated branch and Pull Request. MiMo never commits or pushes to `main`, merges, enables auto-merge, force-pushes or starts the next task before review.

MiMo is launched only with an exact owner-authorized command supplied by Architecture/Review:

```text
mimo --model xiaomi/mimo-v2.5-pro --prompt "<exact task>"
```

For a new `READY_FOR_MIMO` task, the prompt may authorize a new branch and PR. For `CHANGES_REQUIRED`, MiMo must use the existing branch and PR and perform only the exact requested correction.

## Baseline checks

```powershell
python -m pytest -q
python shared/schemas/validate_examples.py
python tools/check_repository_hygiene.py
```

C++ checks are task- and module-specific; see the active task or PR for the exact build/test commands.

Task-specific checks are added on top of the baseline. Green CI does not replace architecture review or owner acceptance.

## Security

Never commit credentials, personal data, private connection parameters, official owner-provided XML, raw QSH/FAST/pcap data, databases, binaries or build directories. See `SECURITY.md`.
