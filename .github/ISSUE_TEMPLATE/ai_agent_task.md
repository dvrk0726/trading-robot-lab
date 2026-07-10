---
name: AI Agent Task
about: Compact implementation, research, review, or owner-decision task
title: "[ROLE] Short task title"
labels: "no-live"
assignees: ""
---

## Type

Choose one:

- [ ] Implementation Task
- [ ] Research Task
- [ ] Review Request
- [ ] Clarification Request
- [ ] Result / Handoff
- [ ] Owner Decision Request
- [ ] Blocker

## From / To

```text
From: Owner / Architecture-Review Agent / MiMo Code / Research Agent
To:   Owner / Architecture-Review Agent / MiMo Code / Research Agent
```

## Required context read

- [ ] `AI_CONTEXT.md`
- [ ] `PROJECT_STATE.md`
- [ ] `ROADMAP.md`
- [ ] `SECURITY.md`
- [ ] `docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md`
- [ ] relevant architecture document and ADR
- [ ] task specification files listed below

For MOEX realtime work:

- [ ] `docs/moex/MOEX_REALTIME_ARCHITECTURE.md`
- [ ] `docs/moex/MOEX_SOURCE_INDEX.md`

## Objective

One exact, bounded objective:

```text
TBD
```

## Task specification

For a small task, keep the specification in this Issue.

For a large task, keep this Issue compact and link files:

```text
tasks/<task-id>/00_OVERVIEW.md
tasks/<task-id>/01_REQUIREMENTS.md
tasks/<task-id>/02_TEST_PLAN.md
tasks/<task-id>/03_ACCEPTANCE.md
```

Spec paths:

```text
TBD
```

## Existing behavior that must remain working

```text
TBD
```

## Inputs

```text
TBD
```

## Expected changes

Files/modules allowed to change:

```text
TBD
```

Expected output:

```text
TBD
```

## Explicit non-goals

```text
TBD
```

## Mandatory constraints

- no live trading;
- no production order sending;
- no real credentials or `.env` in Git;
- no bypass of RiskEngine;
- no silent architecture expansion;
- no raw market data or binaries in Git;
- no claims of profitability without evidence;
- do not modify unrelated files;
- do not commit generated/build directories.

Additional constraints:

```text
TBD
```

## Done criteria

```text
TBD
```

## Tests and checks required

Include exact commands where possible:

```text
TBD
```

## Performance / correctness evidence

```text
TBD or not applicable
```

## Risks and open questions

```text
TBD
```

## Required handoff

Agent must report:

```text
commit SHA;
changed files;
what was implemented;
build/test commands;
test results;
known limitations;
what was intentionally not done;
recommended next step;
whether PROJECT_STATE/ROADMAP need update.
```

## Review gate

- [ ] MiMo implementation completed
- [ ] build/tests passed
- [ ] Architecture-Review Agent reviewed diff and evidence
- [ ] Owner decision obtained where required

## Labels

Recommended:

- `mimo`
- `cpp`
- `python`
- `research`
- `architecture`
- `docs`
- `risk`
- `security`
- `blocked`
- `needs-clarification`
- `needs-owner-decision`
- `no-live`
