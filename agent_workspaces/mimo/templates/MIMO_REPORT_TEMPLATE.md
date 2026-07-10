# MiMo Task Report Template

> Copy to the report path specified by the Issue. One report belongs to one Issue and one Pull Request.

## Identity

```text
Issue: #XXX Title
Status on start: READY_FOR_MIMO
Agent: MiMo Code
Date/time: YYYY-MM-DD HH:MM timezone
Branch: mimo/issue-XXX-short-slug
Base: main
Commit SHA: <sha>
Pull Request: #XXX <link>
```

## Context Read

- [ ] `AI_CONTEXT.md`
- [ ] `PROJECT_STATE.md`
- [ ] `ROADMAP.md`
- [ ] `SECURITY.md`
- [ ] `docs/mimo_developer_workflow.md`
- [ ] `docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md`
- [ ] all Issue-specific architecture/ADR/task-spec files

Task-specific files:

```text
path/to/file
```

## Task Summary

```text
Exact bounded objective from the Issue.
```

## Scope Confirmation

```text
Allowed files/modules:

Explicit non-goals:

Existing behavior that had to remain unchanged:
```

## Files Changed

```text
M path/to/file
A path/to/file
D path/to/file
```

## What Was Implemented

```text
- item
```

## What Was Intentionally Not Implemented

```text
- item and reason
```

## Commands Run

```powershell
# exact commands in execution order
```

## Local Test Results

```text
Command:
Result: PASS / FAIL / NOT RUN
Passed/failed count:
Relevant output:
```

For C++ tasks include compiler, CMake generator/configuration and CTest count. For RT-1 and later C++ work explicitly report the existing QSH/M10X regression result.

## GitHub Actions

```text
Workflow run/link:
Python job: PASS / FAIL / PENDING
C++ QSH/M10X job: PASS / FAIL / PENDING
Repository hygiene job: PASS / FAIL / PENDING
Other jobs:
```

MiMo may create the PR while CI is pending, but must not claim acceptance. Review waits for CI.

## Repository Hygiene Evidence

- [ ] `python tools/check_repository_hygiene.py` passed
- [ ] no `.env`, keys, credentials or personal data
- [ ] no official XML or owner connection parameters
- [ ] no QSH/FAST/pcap/raw market data
- [ ] no databases, binaries or build directories
- [ ] no oversized tracked files
- [ ] no unexpected generated reports

## Safety and Architecture Check

- [ ] no direct work in `main`/`master`
- [ ] no force-push
- [ ] no auto-merge or merge
- [ ] no broker/exchange order connection added
- [ ] no live trading enabled
- [ ] every OrderIntent still requires RiskEngine
- [ ] Trading Lab still cannot send real orders
- [ ] QSH semantics unchanged unless explicitly in scope
- [ ] `strategy_ready` gating not weakened
- [ ] no unrelated architecture expansion

## Owner Review Package

For a user-facing task:

```text
Package path: owner_review_packages/issue-XXX/
START_DEMO.cmd: present / not applicable
STOP_DEMO.cmd: present / not applicable
Interface address:
Review scenario:
Screenshots:
Known limitations:
```

For a non-UI task write `Not applicable` and explain the owner-visible result.

## Known Limitations

```text
- limitation
```

## Risks / Open Questions

```text
- risk or question
```

## Review Requests

- [ ] Architecture review
- [ ] Code review
- [ ] Risk review
- [ ] Security review
- [ ] Owner review

## Final Handoff

```text
Issue moved to: READY_FOR_REVIEW
PR created: yes
Next task started: no
Merge performed: no
Required reviewer action:
```

## Final Status

```text
READY_FOR_REVIEW / BLOCKED
```
