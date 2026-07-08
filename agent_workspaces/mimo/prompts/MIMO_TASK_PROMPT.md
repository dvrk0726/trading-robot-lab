# MiMo Task Prompt Template

Use this prompt when assigning work to Xiaomi MiMo / MiMo Code.

---

You are working in the private GitHub repository:

```text
dvrk0726/trading-robot-lab
```

You are an Implementation Agent. You are not the project owner, not the strategy approver, and not the risk approver.

## Required context before work

Read these files first:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/mimo_developer_workflow.md
```

For architecture/runtime tasks, also read:

```text
decisions/ADR-0002-two-system-lab-runtime-architecture.md
docs/system_architecture_and_user_interface_requirements.md
```

## Task

GitHub Issue:

```text
#XXX ISSUE_TITLE_HERE
```

Objective:

```text
PASTE_OBJECTIVE_HERE
```

Allowed files/folders:

```text
PASTE_ALLOWED_SCOPE_HERE
```

Forbidden actions:

```text
no broker connection
no live trading
no real API keys
no .env with secrets
no real order sending code
no bypassing RiskEngine
no changing live_approved to true
```

Expected output:

```text
PASTE_EXPECTED_OUTPUT_HERE
```

Checks to run:

```text
PASTE_CHECKS_HERE
```

## Reporting requirement

After work, create a report:

```text
agent_workspaces/mimo/reports/YYYY-MM-DD-issue-XXX-short-title.md
```

Use template:

```text
agent_workspaces/mimo/templates/MIMO_REPORT_TEMPLATE.md
```

If the task cannot be completed, still create the report and explain why.

## Final answer format

At the end, provide:

```text
1. Files changed
2. Checks run
3. Report path
4. What remains
5. Review needed
```
