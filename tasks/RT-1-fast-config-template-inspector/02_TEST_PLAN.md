# RT-1 Test Plan

## Test policy

All automated tests must run without network access and without MOEX credentials.

Use synthetic/sanitized XML fixtures committed to the repository. Owner-provided official XML may be used only for an optional local integration test and must not be committed unless separately approved.

## 1. Build checks

Required:

```text
clean CMake configure;
clean build with MSVC on Windows;
unit-test executable builds;
CLI executable builds;
no new compiler warnings in changed code;
no generated binaries committed.
```

Linux/GCC or Clang build should be run when the local environment or CI supports it. Lack of a Linux environment must be stated in the MiMo report.

## 2. CLI tests

Test:

```text
--help;
no arguments;
missing configuration path;
missing templates path;
invalid output path;
valid inputs without --json-out;
valid inputs with --json-out;
strict mode;
non-strict mode.
```

Verify exit codes and concise actionable error text.

## 3. Template parser tests

Synthetic fixtures must cover:

```text
valid minimal template;
multiple templates;
mandatory and optional fields;
constant operator;
sequence and sequence length;
nested sequence if parser supports it;
ASCII string;
Unicode charset attribute;
integer types;
decimal type;
field with FIX id;
field without FIX id.
```

Failure/edge cases:

```text
malformed XML;
missing template id;
non-numeric template id;
duplicate template id;
empty template name;
unknown wire type;
unsupported FAST operator;
broken sequence length definition;
missing required template 29 or 30;
extra unknown template.
```

Unknown but structurally readable elements must produce an explicit warning or error; they must not disappear silently.

## 4. Configuration parser tests

Synthetic fixtures must cover:

```text
valid FUT-INFO group;
valid ORDERS-LOG Incremental A/B;
valid ORDERS-LOG Snapshot A/B;
valid TCP Historical Replay;
UDP and TCP endpoints;
host/IP and port fields;
feed identifiers.
```

Failure/edge cases:

```text
ORDERS-LOG missing;
Incremental B missing;
Snapshot A or B missing;
Historical Replay missing;
FUT-INFO missing;
invalid port: zero, negative, above 65535, non-numeric;
missing multicast group;
duplicate endpoint;
unknown protocol;
malformed XML.
```

## 5. Provenance and hash tests

Verify:

```text
SHA-256 is stable for identical bytes;
one-byte change produces a different hash;
file size is correct;
path is recorded without copying XML content;
report does not contain credentials or raw XML.
```

Optional local integration check:

```text
FAST_9.0 templates.xml should produce the known SHA-256:
dbd50f1e0becc2b2ebd9dac8e4c6609ba1538566811b610cde9b6dd3e7f66a8e
```

## 6. Deterministic report tests

For the same inputs, two runs must produce logically identical JSON.

Variable values such as absolute path or build timestamp must either be excluded from deterministic comparison or explicitly normalized.

Golden/snapshot tests should verify:

```text
schema version;
template ordering;
field ordering;
feed ordering;
warning/error ordering;
overall status.
```

## 7. Resource-safety tests

At minimum test:

```text
large but reasonable synthetic template count;
large sequence field count;
truncated file;
empty file;
read-only output directory or output write failure;
no uncontrolled memory growth;
no crash on invalid input.
```

The inspector is not a hot-path component, but a normal FAST_9.0 template/config pair should complete comfortably within one second on the development machine. Record the measured time; do not treat an unmeasured hard performance claim as fact.

## 8. Regression protection

Existing QSH/M10X tests must continue to pass. RT-1 must not alter QSH flag semantics or weaken `strategy_ready` gating.

## 9. Required MiMo test report

MiMo must report:

```text
exact build commands;
exact test commands;
number of tests passed/failed;
Windows compiler/version;
Linux compiler/version if tested;
integration test status with official XML;
known limitations;
commit SHA.
```
