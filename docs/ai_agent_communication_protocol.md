# AI Agent Communication Protocol

Дата обновления: 2026-07-13  
Статус: обязательный практический протокол

## Источник истины

```text
Issue = objective, status, dependency and owner decision.
Pull Request = concrete change, diff, review and CI evidence.
Task specs = authoritative technical contract for the current gate.
MiMo report = implementation evidence, not proof by itself.
PROJECT_STATE.md = current verified checkpoint.
GitHub code, commits, diff and CI override stale text.
```

Один общий `AI_CHAT.md` не используется.

## Task lifecycle

Canonical Issue statuses:

```text
DRAFT
READY_FOR_MIMO
IN_PROGRESS
READY_FOR_REVIEW
CHANGES_REQUIRED
OWNER_REVIEW
OWNER_APPROVED
DONE
```

Preparation/execution states:

```text
RESEARCH
SCOPE_FROZEN
OWNER_AUTHORIZED
MIMO_RUNNING
ARCHITECTURE_REVIEW
ACCEPTED or CHANGES_REQUIRED
```

No MiMo command may be issued during `RESEARCH` or `SCOPE_FROZEN`.

## Scope-freeze requirements

Before requesting owner authorization, Architecture/Review must prove:

```text
[ ] objective/defect is confirmed by code, tests or authoritative sources
[ ] proposed change is minimal for the current approved gate
[ ] no unresolved fact can materially change implementation
[ ] exact allowed files are listed
[ ] exact tests/checks are listed
[ ] explicit non-goals are listed
[ ] task is one small logical work item
[ ] task does not absorb the next roadmap item
[ ] final scope-reduction attempt was performed
```

Anti-overengineering check:

```text
1. Is the change required by the accepted criteria?
2. Is it supported by authoritative profile or code evidence?
3. Is there a smaller compliant solution?
```

If a smaller compliant solution exists, use it.

## Exact owner-authorized MiMo handoff

MiMo is launched only with:

```text
mimo --model xiaomi/mimo-v2.5-pro --prompt "<exact task>"
```

There is no executable universal command that lets MiMo choose work by itself.

The canonical prompt must contain:

```text
repository
Issue and current gate
base and full current head SHA
branch rule
Pull Request rule
required reading
exact allowed files
exact required changes
existing behavior to preserve
exact acceptance tests/checks
explicit non-goals
commit/push/CI/stop requirements
owner authorization
```

Only one canonical executable prompt may exist for the current task. Superseded prompts or comments must be marked `NON-EXECUTABLE`.

## New task versus correction

### READY_FOR_MIMO

A new branch and PR are allowed only when the exact owner-authorized prompt explicitly says so.

### CHANGES_REQUIRED

The prompt must require:

```text
existing branch only
existing PR only
no new branch
no new PR
one small correction
one scoped commit
push, CI, report and stop
```

MiMo may not infer permission from an old `READY_FOR_MIMO` Issue body when the actual label or PR state is `CHANGES_REQUIRED`.

## Issue content

An active implementation Issue should contain:

```text
current status
objective
current authoritative specification
current branch and PR when they exist
dependencies
allowed scope
existing behavior to preserve
explicit non-goals
acceptance criteria
exact checks
owner decision or authorization state
```

The Issue remains compact and links to large task specs.

## Pull Request content

A PR must contain:

```text
Issue reference
branch and full head SHA
current scope
changed files
implemented and omitted behavior
exact commands/results
CI run and jobs
known limitations
security/hygiene result
owner gate
```

PR description and comments must not claim capabilities that code/tests do not prove.

## Review comments

Every blocking review comment uses:

```text
Problem:
Evidence:
Required change:
Files/scope:
Verification command or acceptance check:
```

Comments must be concrete and testable. Do not use vague instructions such as “make it better”.

When research invalidates earlier instructions, publish one canonical superseding comment and explicitly mark older conflicting instructions non-executable.

## MiMo result / handoff

MiMo reports:

```text
Issue
branch
full starting SHA
full final SHA
Pull Request
changed files
what was implemented
what was intentionally not implemented
exact commands and results
CI workflow and jobs
known limitations
security/hygiene result
report path
```

After push and CI, MiMo stops. Architecture/Review independently verifies the report against GitHub.

## Blocker

A blocker is recorded when a dependency, source, environment, access or decision is missing:

```text
Blocker:
Observed evidence:
Why work cannot safely continue:
Minimal action needed:
Work that remains safe and complete:
```

A blocker is never presented as successful completion.

## Owner decision request

Use for:

```text
architecture changes
merge
force push or history rewrite
branch deletion
irreversible actions
cost/access/hardware decisions
MOEX/broker interaction
paper/live/production stage
security or risk exceptions
transition to the next roadmap gate
```

Format:

```text
Reason:
Decision needed:
Options:
Recommendation:
Safe default:
Impact on current task:
```

Safe default is to stop the risky action.

## Merge and completion

```text
Architecture review passed
-> OWNER_REVIEW
-> owner accepts result
-> OWNER_APPROVED
-> owner separately authorizes merge
-> merge
-> post-merge main CI verified
-> state documents updated
-> DONE
```

Neither MiMo nor automation may merge, enable auto-merge or release the next roadmap task.
