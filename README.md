# trading-robot-lab

Private research and engineering repository for a two-system trading architecture:

```text
Trading Lab      — research, data quality, replay, backtests, reports and UI.
Trading Runtime  — future execution of approved Strategy Packages.
Shared Contracts — versioned events, signals, OrderIntent and RiskDecision schemas.
```

## Current state

The historical C++ QSH/OrdLog engineering contour is complete at the current level. M10X has 20/20 regression tests. The next planned implementation task is RT-1, a local offline inspector for MOEX FAST configuration/templates.

RT-1 must not start until the MiMo branch/Pull Request/CI workflow is accepted and merged.

Read:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
```

## Architecture

```text
Trading Lab cannot send real orders.
Strategy cannot call execution directly.
Every OrderIntent must pass RiskEngine.
Live is disabled by default.
Production order entry is certification- and owner-gated.
C++ is used for low-level data/realtime/runtime work.
Python is used for research, reports and UI.
```

Architecture decisions are stored under `decisions/`.

## Implementation workflow

Authoritative instructions:

```text
docs/mimo_developer_workflow.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

Universal MiMo command:

```text
Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись.
```

Implementation is branch-only. MiMo does not commit/push code directly to `main`, does not merge and does not start the next task before review.

## Baseline checks

```powershell
python -m pytest -q
python shared/schemas/validate_examples.py
python tools/check_repository_hygiene.py

cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

GitHub Actions runs repository hygiene, Python/contract checks and the complete 20-test QSH/M10X C++ regression suite.

## Security

Never commit credentials, personal data, private connection parameters, official private XML, raw QSH/FAST/pcap data, databases, binaries or build directories. See `SECURITY.md`.

For user-facing work, use `docs/engineering/OWNER_REVIEW_PACKAGE.md`.
