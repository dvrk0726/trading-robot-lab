---
name: AI Agent Task
about: Compact task with branch, tests, Pull Request and review gates
title: "[ROLE] Short task title"
labels: "DRAFT,no-live"
assignees: ""
---

## Status

Exactly one:

```text
DRAFT
```

Allowed lifecycle:

```text
DRAFT -> READY_FOR_MIMO -> IN_PROGRESS -> READY_FOR_REVIEW
-> CHANGES_REQUIRED -> READY_FOR_REVIEW
-> OWNER_REVIEW -> OWNER_APPROVED -> DONE
```

## Type

- [ ] Implementation Task
- [ ] Research Task
- [ ] Review Request
- [ ] Clarification Request
- [ ] Owner Decision Request
- [ ] Blocker

## From / To

```text
From: Owner / Architecture-Review Agent / MiMo Code / Research Agent
To:   Owner / Architecture-Review Agent / MiMo Code / Research Agent
```

## Objective

One exact, bounded result:

```text
TBD
```

## Required context

- [ ] `AI_CONTEXT.md`
- [ ] `PROJECT_STATE.md`
- [ ] `ROADMAP.md`
- [ ] `SECURITY.md`
- [ ] `docs/mimo_developer_workflow.md`
- [ ] `docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md`
- [ ] relevant architecture document and ADR
- [ ] task specification files listed below

For MOEX realtime work:

- [ ] `docs/moex/MOEX_REALTIME_ARCHITECTURE.md`

## Task specification

Small task: keep the specification here.

Larger task: link the split package:

```text
tasks/<task-id>/00_OVERVIEW.md
tasks/<task-id>/01_REQUIREMENTS.md
tasks/<task-id>/02_TEST_PLAN.md
tasks/<task-id>/03_ACCEPTANCE.md
```

Paths:

```text
TBD
```

## Dependency / readiness gate

```text
Blocking Issues:
Owner decision required:
Why this task is or is not READY_FOR_MIMO:
```

The Issue must not receive `READY_FOR_MIMO` until scope, tests, non-goals and dependencies are complete.

## Existing behavior that must remain working

```text
TBD
```

## Allowed changes

```text
TBD
```

## Expected output

```text
TBD
```

## Explicit non-goals

```text
TBD
```

## Mandatory constraints

- work only on a dedicated branch;
- no commit or push to `main`/`master`;
- Pull Request required;
- no auto-merge or merge by MiMo;
- MiMo stops after one Pull Request;
- no next task before review of the previous one;
- no live trading;
- no production order sending;
- no real credentials, personal data or `.env` in Git;
- no bypass of RiskEngine;
- no raw market data, official private artifacts or binaries in Git;
- no unrelated refactor or architecture expansion;
- no weakening of existing safety/data-quality gates.

Additional constraints:

```text
TBD
```

## Branch and Pull Request

Expected branch:

```text
mimo/issue-<NUMBER>-<short-slug>
```

PR base:

```text
main
```

## Done criteria

```text
TBD
```

## Tests and checks required

Include exact commands and expected counts:

```text
TBD
```

For C++ work, state whether the existing 20 QSH/M10X CTest tests are mandatory. For Python work, state the required pytest/validation commands.

## Owner Review Package

- [ ] Not applicable
- [ ] Required under `owner_review_packages/issue-<NUMBER>/`

For UI tasks follow `docs/engineering/OWNER_REVIEW_PACKAGE.md`.

## Required MiMo report

```text
agent_workspaces/mimo/reports/YYYY-MM-DD-issue-XXX-short-title.md
```

Report must contain:

```text
branch;
commit SHA;
Pull Request;
changed files;
implemented and intentionally omitted scope;
exact build/test commands;
local results and CI status;
hygiene/safety evidence;
known limitations;
Owner Review Package path when applicable.
```

## Review gate

- [ ] MiMo implementation completed in a feature branch
- [ ] Pull Request created
- [ ] required GitHub Actions passed
- [ ] Architecture/Review Agent reviewed diff and evidence
- [ ] all change requests resolved
- [ ] Owner review completed where required
- [ ] no automatic merge
- [ ] project state updated after merge
