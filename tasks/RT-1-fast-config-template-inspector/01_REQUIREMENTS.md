# RT-1 Requirements

## 1. Build and placement

Use the existing repository build conventions. Prefer a new module under `cpp/moex_fast/` unless inspection of the current CMake structure shows a cleaner compatible location.

Requirements:

```text
C++20 minimum;
CMake build;
Windows/MSVC support required;
Linux/GCC or Clang compatibility preserved;
no dependency on QuickFAST;
XML dependency must be lightweight, license-compatible and documented.
```

## 2. Command-line interface

The executable name may be finalized after repository inspection. It must support an equivalent interface:

```text
moex-fast-inspect \
  --configuration <path/configuration.xml> \
  --templates <path/templates.xml> \
  [--json-out <path/report.json>] \
  [--strict]
```

Required behavior:

```text
--help prints usage;
missing file produces non-zero exit code;
malformed XML produces non-zero exit code;
validation failure is visible in console and JSON;
output is deterministic for identical inputs;
no network access occurs.
```

## 3. File provenance

For each input record:

```text
path supplied by operator;
file name;
file size;
SHA-256;
parse status;
validation status;
```

Do not copy the XML content into the report.

## 4. Template inspection

Parse FAST template definitions into normalized metadata.

Minimum metadata:

```text
template id;
template name;
field order;
field name;
FIX field id when present;
wire type;
presence: mandatory/optional;
operator: constant or none;
constant value when present;
sequence nesting;
sequence length field;
charset when present.
```

Validate:

```text
template IDs are unique;
template names are non-empty;
field order is preserved;
known wire types are recognized;
sequence structure is valid;
optional/mandatory information is preserved;
unsupported operators are reported;
unknown fields/types are not silently discarded.
```

Required template checks:

```text
29 OrdersLogMessage
30 BookMessage
31 DefaultIncrementalRefreshMessage
32 DefaultSnapshotMessage
40 SecurityDefinition
45 SecurityGroupStatus
46 TradingSessionStatus
```

Missing required templates must produce a validation error in strict mode and a warning in non-strict mode.

## 5. Configuration inspection

Parse feed groups and connections into normalized metadata.

Minimum metadata:

```text
market/feed group name;
market ID when present;
feed type or role;
connection protocol;
A/B source designation;
IP address or host;
multicast group;
port;
feed identifier;
Snapshot/Incremental/Instrument Replay/Historical Replay role;
TCP or UDP transport.
```

Required logical checks:

```text
ORDERS-LOG exists;
Incremental A and B exist;
Snapshot A and B exist;
Historical Replay exists;
FUT-INFO exists;
ports are in valid numeric range;
required endpoint attributes are present;
duplicate endpoint definitions are reported.
```

The inspector must not attempt to connect to any endpoint.

## 6. Normalized C++ metadata contracts

Create small value types equivalent to:

```text
InspectionIssue
InputFileInfo
FastFieldDescriptor
FastTemplateDescriptor
FeedEndpoint
FeedGroup
InspectionReport
```

Rules:

```text
no exchange-order sending concepts;
no QSH flag reuse;
FAST metadata types remain separate from QSH parser types;
no universal runtime FIX message tree;
no dynamic polymorphism unless justified;
no business logic mixed into XML parser.
```

## 7. Report format

JSON report must contain:

```text
report schema version;
inspector build/version;
input provenance;
template summary;
feed summary;
required-template results;
required-feed results;
warnings;
errors;
overall status: valid / warning / invalid.
```

The JSON structure must be documented and stable enough for future automated checks.

## 8. Error handling

No uncaught exception may reach the user for expected input errors.

At minimum distinguish:

```text
file not found;
file read error;
malformed XML;
unsupported XML structure;
duplicate template ID;
missing required template;
invalid endpoint/port;
missing required feed component;
output write failure.
```

## 9. Security and repository hygiene

```text
No credentials in code, tests or reports.
No real MOEX passwords/login IDs.
No owner-provided official XML committed without approval.
Synthetic fixtures only in Git.
Generated build files remain ignored.
No binaries committed.
```

## 10. Documentation

Add a short module README covering:

```text
purpose;
build command;
usage examples;
report meaning;
strict vs non-strict behavior;
known limitations;
statement that the tool performs no network access.
```
