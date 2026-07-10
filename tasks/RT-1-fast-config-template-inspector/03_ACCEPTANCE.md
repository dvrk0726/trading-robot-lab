# RT-1 Acceptance Criteria

RT-1 is accepted only after implementation review. A successful build alone is insufficient.

## Functional acceptance

All must be true:

```text
[ ] CLI reads configuration.xml and templates.xml from operator-supplied paths.
[ ] CLI performs no network access.
[ ] File SHA-256 and size are reported.
[ ] Templates are listed with stable IDs, names and ordered fields.
[ ] Feed groups/endpoints are listed with transport and role.
[ ] Required template IDs 29, 30, 31, 32, 40, 45 and 46 are checked.
[ ] FUT-INFO and ORDERS-LOG Incremental A/B, Snapshot A/B and Historical Replay are checked.
[ ] Strict mode fails on missing required components.
[ ] Non-strict mode preserves warnings without hiding them.
[ ] Human-readable output is understandable.
[ ] JSON report is deterministic and documented.
```

## Correctness acceptance

```text
[ ] Field order from templates.xml is preserved.
[ ] Optional/mandatory presence is preserved.
[ ] Sequence boundaries and length fields are represented correctly.
[ ] Unknown types/operators are surfaced explicitly.
[ ] Duplicate template IDs are rejected.
[ ] Invalid endpoint ports are rejected.
[ ] No QSH flags or QSH parser enums are reused for FAST metadata.
[ ] No raw XML is embedded in JSON output.
```

## Architecture acceptance

```text
[ ] QuickFAST is not added as production dependency.
[ ] Inspector parsing is isolated from future realtime hot path.
[ ] No universal FIX message tree is introduced.
[ ] No UDP/TCP receiver is added.
[ ] No order-entry code is added.
[ ] No database dependency is added.
[ ] New contracts are small value types with clear ownership.
[ ] CMake integration follows existing repository conventions.
```

## Testing acceptance

```text
[ ] All new unit tests pass.
[ ] Existing QSH/M10X tests continue to pass.
[ ] Error cases do not crash the process.
[ ] Deterministic JSON test passes.
[ ] SHA-256 test passes.
[ ] Windows/MSVC build passes.
[ ] Linux build result is reported when environment is available.
[ ] Optional official-file local integration result is reported.
```

## Repository acceptance

```text
[ ] No official XML, credentials, binaries or build directories are committed.
[ ] No unrelated files are changed.
[ ] Commit is logically scoped to RT-1.
[ ] Module README is included.
[ ] MiMo report is included under agent_workspaces/mimo/reports/.
[ ] Report contains commands, results, limitations and commit SHA.
```

## Required implementation report path

```text
agent_workspaces/mimo/reports/2026-07-10-rt1-fast-config-template-inspector.md
```

## Reviewer checklist

The Architecture/Review Agent must inspect:

```text
changed-file list;
C++ API boundaries;
XML dependency and license;
error handling;
JSON schema stability;
test coverage;
absence of network/order code;
absence of secrets/raw official files;
compatibility with future generated FAST decoder.
```

## Stop conditions

Reject or pause RT-1 if any of the following occurs:

```text
QuickFAST becomes a mandatory hot-path dependency;
implementation starts decoding live packets;
network connection is added;
real credentials are requested or committed;
large unrelated refactor is mixed into the task;
existing QSH tests regress;
unknown XML elements are silently discarded;
report output is nondeterministic without justification.
```

## Gate to RT-2

RT-2 may begin only after:

```text
RT-1 accepted by reviewer;
owner approves the next stage;
normalized metadata contracts are stable;
known template/config hashes are recorded;
open RT-1 defects are closed or explicitly deferred.
```
