## Issue and status

```text
Issue: #XXX
Issue status: READY_FOR_REVIEW
Branch: mimo/issue-XXX-short-slug
Base: main
Commit SHA: <sha>
MiMo report: <path>
```

Closes/relates to: #XXX

## Objective

```text
Exact bounded result from the Issue.
```

## What changed

```text
- user-visible or engineering change
```

## What intentionally did not change

```text
- explicit non-goal
- protected existing behavior
```

## Changed files

```text
M path/to/file
A path/to/file
```

## Local build and test evidence

Exact commands:

```text
command
```

Results:

```text
PASS / FAIL / NOT RUN
passed/failed count and relevant output
```

For C++ tasks:

```text
Compiler/toolchain:
New tests:
Task-specific C++ test inventory and evidence: <list tests or state none>
```

For Python tasks:

```text
pytest:
contract/example validation:
```

## GitHub Actions

- [ ] Repository hygiene passed
- [ ] Python tests and contracts passed
- [ ] All required CI checks passed (see active issue/PR for matrix)
- [ ] Task-specific C++ tests passed (if applicable)

Do not merge while a required check is pending or failed.

## Repository hygiene

- [ ] `python tools/check_repository_hygiene.py` passed
- [ ] no secrets, credentials, personal data or private connection parameters
- [ ] no official private XML or raw market data
- [ ] no forbidden raw market-data files (*.qsh, *.pcap, *.pcapng, raw FAST)
- [ ] no databases, binaries or build directories
- [ ] no unexpected large/generated files

## Safety and architecture

- [ ] implementation was done on a dedicated branch, not `main`
- [ ] no force-push, auto-merge or merge was used by MiMo
- [ ] Trading Lab still cannot send real orders
- [ ] every OrderIntent still passes RiskEngine
- [ ] live remains disabled by default
- [ ] no broker/MOEX order connection was added
- [ ] no unapproved network/FIX/TWIME/FAST binary decoder was added
- [ ] no source-specific legacy semantics leaked into normalized event or order-book contracts
- [ ] no unrelated architecture expansion or refactor

## Owner Review Package

- [ ] Not applicable
- [ ] Included at `owner_review_packages/issue-XXX/`

For UI/user-facing tasks include:

```text
OWNER_REVIEW.md:
START_DEMO.cmd:
STOP_DEMO.cmd:
Interface address:
Review scenario:
Screenshots:
Known limitations:
```

## Known limitations and open questions

```text
- limitation or question
```

## Review requested

- [ ] Architecture review
- [ ] Code review
- [ ] Risk review
- [ ] Security review
- [ ] Owner review

## Stop confirmation

- [ ] MiMo stopped after creating/updating this Pull Request
- [ ] MiMo did not start the next task
- [ ] MiMo did not merge this Pull Request
