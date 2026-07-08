---
name: AI Agent Task
about: Task, question, handoff, or review request for an AI agent or developer
title: "[ROLE] Short task title"
labels: "no-live"
assignees: ""
---

## Type

Choose one:

- [ ] Task Request
- [ ] Clarification Request
- [ ] Result / Handoff
- [ ] Review Request
- [ ] Owner Decision Request
- [ ] Blocker

## From

Agent / developer who creates the issue:

```text
Strategy Master Agent / Python Research Agent / C++ Core Agent / Architecture Agent / Owner
```

## To

Target agent / developer:

```text
Strategy Master Agent / Python Research Agent / C++ Core Agent / Architecture Agent / Owner
```

## Context read

Before working, confirm that required context was read:

- [ ] `AI_CONTEXT.md`
- [ ] `PROJECT_STATE.md`
- [ ] `SECURITY.md`
- [ ] `README.md`
- [ ] `ROADMAP.md`
- [ ] `docs/ai_team_workflow.md`
- [ ] `docs/ai_agent_communication_protocol.md`

If strategy-related:

- [ ] `strategy_knowledge_base/README.md`
- [ ] `strategy_knowledge_base/strategy_master_agent/STRATEGY_MASTER_PROMPT.md`

If architecture-related:

- [ ] `docs/01_hybrid_architecture.md`
- [ ] `decisions/ADR-0001-hybrid-python-cpp-architecture.md`

## Objective

Describe the exact task or question.

```text
TBD
```

## Inputs

List files, data, formulas, assumptions, or previous decisions used as input.

```text
TBD
```

## Expected output

What should be created or changed?

```text
TBD
```

## Constraints

Mandatory constraints:

- no broker connection unless explicitly approved by Owner;
- no live trading;
- no real API keys;
- no `.env` with secrets;
- no real order sending code;
- do not bypass risk engine;
- do not claim strategy profitability without tests.

Additional constraints for this task:

```text
TBD
```

## Done criteria

How to verify that the task is complete?

```text
TBD
```

## Tests / checks required

```text
TBD
```

## Risks / open questions

```text
TBD
```

## Handoff after done

Who should review or continue after this task?

```text
TBD
```

## Labels to add

Recommended labels:

- `strategy`
- `research`
- `backtest`
- `python`
- `cpp`
- `risk`
- `architecture`
- `docs`
- `security`
- `blocked`
- `needs-clarification`
- `needs-owner-decision`
- `no-live`
