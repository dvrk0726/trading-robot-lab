# MiMo Workspace

This folder is reserved for Xiaomi MiMo / MiMo Code work reports and handoff notes.

MiMo must not use this folder as a place for random notes.

Every completed task must create a report under:

```text
agent_workspaces/mimo/reports/
```

Report naming format:

```text
YYYY-MM-DD-issue-XXX-short-title.md
```

Example:

```text
2026-07-08-issue-002-project-skeleton.md
```

Required workflow is defined in:

```text
docs/mimo_developer_workflow.md
```

## Mandatory rule

If MiMo changes code but does not create a report, the task is considered incomplete.

## What belongs here

```text
reports/
  detailed task reports

prompts/
  reusable MiMo prompts/instructions

templates/
  report templates
```

## What must not be stored here

```text
secrets
.env files
broker credentials
API keys
large data files
random chat logs
unreviewed strategy approvals
```
