# QSH retirement — Stage 1

Issue: #33

Status: `SCOPE_FROZEN`

## Purpose

Remove the legacy QSH/QScalp/OrdLog implementation and active integrations. The future project uses the official MOEX SPECTRA FAST contour only.

## Stage 1 rule

Delete the QSH product code, old QSH L3/L2 reconstruction, tests, diagnostics, UI/database integration and active architecture claims.

Temporarily keep the exact required-check name:

```text
C++ QSH M10X regression (20 tests)
```

Replace its build with a fast tombstone absence check. This check is removed only in Stage 2 after the Owner updates the main ruleset.

## Do not preserve

Do not move QSH parser, enums, order-book code or tests into MOEX modules. A new book will be designed later from normalized MOEX events.

## Keep only as safety barriers

```text
.gitignore: *.qsh
tools/check_repository_hygiene.py: forbidden .qsh suffix
SECURITY.md: raw market-data prohibition including *.qsh
```

## Non-goals

No RT-4, new order book, CI-2 caching, history rewrite, force-push, merge or auto-merge.

The complete executable specification and acceptance criteria are in Issue #33.
