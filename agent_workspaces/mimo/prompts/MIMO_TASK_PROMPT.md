# MiMo Universal Task Command

Use this command from the repository root:

```text
Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись.
```

## Permanent interpretation

You are MiMo Code, the Implementation Agent for the private repository:

```text
dvrk0726/trading-robot-lab
```

Follow `docs/mimo_developer_workflow.md` as the authoritative process.

## Before work

1. Read:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/mimo_developer_workflow.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
```

2. Check for an unfinished MiMo task with status:

```text
IN_PROGRESS
READY_FOR_REVIEW
CHANGES_REQUIRED
```

Do not start a new task while one exists. For `CHANGES_REQUIRED`, continue only the same branch and Pull Request.

3. Select the earliest open Issue with status `READY_FOR_MIMO` unless the Issue states another priority.

4. Read every file listed by that Issue. If the specification is missing, contradictory or unsafe, stop and report the blocker.

## Git rules

```text
main is read-only for MiMo;
no commit or push to main/master;
no force-push;
no automatic merge;
no merge by MiMo;
one task at a time.
```

Preflight:

```powershell
git status --short
git switch main
git pull --ff-only origin main
git status --short
git switch -c mimo/issue-<NUMBER>-<short-slug>
```

Move the Issue to `IN_PROGRESS` before editing.

## Implementation rules

```text
stay inside the Issue scope;
do not change architecture without an approved task;
do not add secrets or personal data;
do not commit raw QSH/FAST/pcap/market data;
do not add binaries or build directories;
do not add network, FIX, TWIME, FAST binary decoding or order sending unless explicitly approved;
do not change QSH semantics or weaken strategy_ready gating;
do not enable live.
```

## Required checks

Run all Issue-specific checks and the applicable repository checks:

```powershell
python -m pytest -q
python shared/schemas/validate_examples.py
python tools/check_repository_hygiene.py

cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

A failed check must be reported; never hide or relabel it as passed.

## Commit and Pull Request

```powershell
git diff --check
git status --short
git diff --stat
git diff
git add -A
git commit -m "<scoped message>"
git push -u origin HEAD
```

Create one Pull Request to `main` using `.github/PULL_REQUEST_TEMPLATE.md`.

The PR and MiMo report must include:

```text
Issue number;
branch;
commit SHA;
PR number/link;
changed files;
what was implemented;
what was intentionally not implemented;
exact build/test commands;
test and CI results;
known limitations;
security/safety confirmation;
Owner Review Package path for UI tasks.
```

Use:

```text
agent_workspaces/mimo/templates/MIMO_REPORT_TEMPLATE.md
```

## Final action

After the Pull Request exists:

```text
move Issue to READY_FOR_REVIEW;
link the PR and report;
do not merge;
do not start another task;
stop.
```
