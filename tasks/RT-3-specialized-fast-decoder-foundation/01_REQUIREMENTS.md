# RT-3 Requirements

## 1. Build, placement and compatibility

Implement in C++20 and CMake inside the existing `cpp/moex_fast/` project. The decoder may add a new static library and CLI, but the existing RT-1 inspector library, CLI and six tests must remain supported.

Required platforms and build rules:

```text
Windows / MSVC Release with /W4 /WX;
Linux / GCC Release with -Wall -Wextra -Wpedantic -Werror;
no QuickFAST production dependency;
no Python runtime dependency in the decoder;
no network, database or pcap dependency;
all decoder tests active under NDEBUG/Release.
```

The implementation must not weaken RT-1, RT-2 or QSH/M10X tests. Official or owner-provided XML and raw market data remain outside Git.

## 2. Exact public boundary

RT-3 decodes exactly one bounded FAST message payload. It does not parse SPECTRA UDP packet headers and does not discover message boundaries inside a datagram.

Required public operations are equivalent to:

```text
compile_templates(templates.xml bytes/path, compile limits)
  -> immutable CompiledTemplateSet or compile issues

DecoderSession(compiled templates, decode limits)

session.decode_one(byte span)
  -> status, bytes_consumed, decoded message, issues

session.decode_exact(byte span)
  -> same, but trailing bytes are an error

session.reset()
```

`decode_one` may successfully consume a prefix and report `bytes_consumed`. `decode_exact` succeeds only when the full span is one message. A `RawPacketRecord.payload` can be passed directly as a byte span, but the caller is responsible for asserting that one record contains one FAST message.

## 3. Ownership and concurrency

```text
CompiledTemplateSet: immutable after successful compilation, shareable between sessions and threads.
DecoderSession: owns template-ID and dictionary state for one logical ordered source stream; not thread-safe.
DecodedMessage: owns its decoded values and remains valid independently of the input buffer and session.
```

One session must never be shared across independent `(session_id, source_id, channel_id)` streams. RT-3 does not merge A/B sources and does not infer stream identity.

## 4. Error model

Expected malformed input must not escape the public API as an uncaught exception. Use explicit result/status values.

Required decode status classes:

```text
ok;
need_more_data;
invalid_encoding;
non_canonical_encoding;
integer_overflow;
invalid_presence_map;
unknown_template;
missing_previous_template;
missing_dictionary_value;
invalid_operator_state;
invalid_sequence_length;
limit_exceeded;
trailing_bytes;
unsupported_template_feature;
internal_error translated at API/CLI boundary.
```

Each issue must include:

```text
stable machine-readable code;
message byte offset;
template id when known;
field path when known;
human-readable message.
```

Field paths must be deterministic, for example:

```text
OrdersLogMessage.MsgSeqNum
OrdersLogMessage.MDEntries[3].MDEntryPx.mantissa
```

## 5. Transactional state

All decode state changes are transactional.

```text
successful decode -> template-ID state and dictionaries commit once;
failed decode -> template-ID state and dictionaries are byte-for-byte unchanged;
decode_exact with trailing bytes -> no state commit;
limit failure after earlier fields -> no state commit;
output allocation/report failure translated before state commit.
```

A journal/overlay or equivalent mechanism is preferred over copying every dictionary value, but correctness is mandatory. Tests must prove rollback after failures occurring early, in the middle of a sequence and after provisional dictionary updates.

## 6. Hard safety limits

Provide configurable limits with hard implementation ceilings. Defaults may be lower but must not exceed:

```text
message bytes:              1 MiB;
presence-map bytes:         64;
template count:             4096;
fields/nodes per template:  65535;
template nesting depth:     32;
sequence entries:           100000 per sequence;
total decoded nodes:        1000000 per message;
string/byte-vector bytes:   1 MiB per value and within message limit;
dictionary entries:         bounded by compiled template set;
```

All additions, multiplications, cursor movement, sequence counts and allocation sizes require checked arithmetic before use. Recursion depth and total node count must be checked before descending or allocating.

## 7. Template source and compilation

The flattened RT-1 `FastFieldDescriptor` is not sufficient for decoding because it loses operator and hierarchy semantics. RT-3 must compile an immutable decoder-specific template tree from XML while preserving RT-1 inspection behavior.

The compiler must preserve or resolve:

```text
template id and name;
field name and optional FIX tag;
mandatory/optional presence;
primitive type;
nested group and sequence structure;
sequence length field;
operator kind and exact initial value;
dictionary attribute and canonical dictionary key;
typeRef/templateRef/groupRef information when used by the accepted profile;
decimal exponent and mantissa subinstructions;
static constant values;
source order.
```

No XML element, operator or attribute that affects wire decoding may be silently ignored. Unsupported constructs are compile errors, not warnings followed by guessed decoding.

Compile-time validation must reject at least:

```text
duplicate template ids;
missing/invalid template ids;
duplicate ambiguous dictionary keys;
unresolved or cyclic references;
missing sequence length instruction;
invalid primitive/operator combinations;
invalid initial values or values outside target range;
unknown presence values;
unsupported dictionary scopes;
excessive nesting, templates or fields;
empty required names when needed for deterministic keys;
operator state whose type is inconsistent across references.
```

The compiled set must retain templates-file SHA-256 and a deterministic compiler/schema version for provenance.

## 8. Wire cursor and stop-bit primitives

Implement bounded primitives for:

```text
presence maps;
unsigned 32-bit and 64-bit stop-bit integers;
signed 32-bit and 64-bit stop-bit integers;
nullable integer forms;
ASCII strings;
Unicode strings;
byte vectors;
decimal exponent/mantissa;
sequence lengths.
```

Rules:

```text
never read past the supplied span;
unterminated stop-bit or presence-map values return need_more_data/invalid encoding;
integer overflow is detected before shift/add/sign extension;
overlong or non-canonical integer encodings are rejected;
nullable null and non-null mappings follow FIX FAST normative rules exactly;
null and empty string/byte-vector remain distinct;
all length prefixes are checked before allocation and cursor movement;
invalid UTF-8 in a Unicode field is an explicit error unless the accepted template profile declares opaque bytes;
ASCII fields reject bytes outside the accepted FAST ASCII domain;
```

Presence-map bits are consumed only according to the compiled FAST instruction matrix. Exhausting the transmitted map yields implicit zero bits only where the FAST standard permits; malformed unterminated maps are rejected.

## 9. Template-ID state

The decoder session must implement FAST template-ID state:

```text
when the template-id presence bit is set, decode and validate the transmitted template id;
when absent, reuse the session's previous template id;
absence with no previous template id is an error;
unknown ids are errors;
template-id changes commit only after the whole message succeeds;
reset clears the previous template id.
```

Tests must cover first-message omission, reuse, change, unknown id, truncated id and rollback after a later field fails.

## 10. Value model

The decoded tree must preserve semantic distinctions required downstream.

Required scalar value alternatives:

```text
null;
uint64;
int64;
ASCII string;
Unicode string;
byte vector;
exact decimal { exponent:int32, mantissa:int64 }.
```

Required composite alternatives:

```text
group with ordered child fields;
sequence with ordered entries, each entry an ordered group/tree.
```

Every decoded semantic field must retain:

```text
field name;
optional FIX tag;
field path/source order;
value or explicit null;
value source: wire, constant, default, previous/copy, increment, delta or tail.
```

Optional null, present empty sequence and absent/null sequence must remain distinguishable. Decimal values must never be converted to `double` inside the decoder.

## 11. Operator semantics

Support the FAST operators required by the accepted SPECTRA template profile:

```text
none;
constant;
default;
copy;
increment;
delta;
tail.
```

The operator compiler must use an explicit normative table for:

```text
whether a presence-map bit is consumed;
wire nullability;
initial value behavior;
dictionary lookup/update behavior;
mandatory versus optional behavior;
undefined previous value behavior.
```

The implementation must follow FIX FAST 1.1 semantics rather than approximate rules.

Required safety and correctness behavior:

```text
constant values consume no wire bytes except any normative optional-presence indication;
default/copy/increment use exact presence and initial/previous rules;
increment and integer delta use checked arithmetic;
delta supports the accepted integer, decimal and string/byte-vector forms with checked prefix/suffix lengths;
tail supports accepted string/byte-vector forms with checked retained-prefix lengths;
null dictionary updates and undefined states follow the normative table;
operator application never changes state before the enclosing message commits.
```

Any operator/type combination not implemented and not needed by the accepted profile must fail template compilation explicitly.

## 12. Dictionary identity and scopes

Dictionary state must use canonical keys, not only field names.

A key must include enough information to avoid collisions, including:

```text
dictionary scope;
resolved template/type identity where applicable;
operator key attribute or deterministic field identity;
value type, including decimal exponent/mantissa component;
```

Supported scopes must be explicitly enumerated from the accepted profile and FAST standard. Unknown scope names are compile errors. Dictionary entries are predeclared from the compiled template set so malformed input cannot cause unbounded key creation.

## 13. Decimal semantics

A decimal is exact and consists of exponent and mantissa instructions. The compiler must preserve operators/initial values for both components.

Rules:

```text
null decimal is represented explicitly;
if the normative encoding makes a null exponent terminate the decimal, mantissa bytes must not be consumed;
exponent range fits int32 and mantissa fits int64;
component dictionary updates are transactional;
no floating-point rounding occurs;
JSON renders exponent and mantissa as integers, not scientific binary floating point.
```

## 14. Groups and sequences

Groups and sequences are decoded recursively from the compiled tree.

```text
optional group presence follows its compiled presence-map rule;
sequence length is decoded through its own instruction/operator;
null optional sequence differs from a present sequence of length zero;
entry count is validated before allocation/looping;
each sequence entry receives its own presence map where required by FAST;
field and entry order are preserved;
limits are checked before every entry and nested node.
```

A malformed entry rolls back the entire message, including state changes from prior entries.

## 15. Reset behavior

Provide an explicit `DecoderSession::reset()` equivalent that clears:

```text
previous template id;
all operator dictionaries;
all decoder-session transient state.
```

Reset does not alter the immutable compiled template set. RT-3 does not infer resets from `MsgSeqNum`, packet gaps, endpoint changes, session events or clock time. Any future protocol-driven reset belongs to RT-4 unless explicitly added by a later approved specification.

## 16. Deterministic reports and CLI

Add a decoder CLI equivalent to:

```text
moex-fast-decode --templates <templates.xml> --hex <one-message-hex>
moex-fast-decode --templates <templates.xml> --input <one-message.bin>
                 [--json-out <report.json>] [--exact]
```

The CLI must not accept a multicast endpoint or claim to parse a SPECTRA datagram. Exactly one of `--hex` or `--input` is required.

Report requirements:

```text
versioned schema;
decoder/compiler version;
templates basename, size and SHA-256;
input byte length and SHA-256;
template id/name;
bytes_consumed;
deterministic ordered decoded tree;
exact null/value/value-source representation;
decimal exponent/mantissa integers;
byte vectors as lowercase hex;
stable issue codes, offsets and field paths;
overall status;
no credentials, private endpoints or raw full local paths by default.
```

Text and JSON output must be deterministic across Windows and Linux. JSON escaping and numeric types require strict parser tests.

## 17. RT-2 integration boundary

The decoder library must expose a span-based API that accepts `RawPacketRecord.payload` without copying solely for API adaptation.

A test-only integration must demonstrate:

```text
synthetic RawPacketRecord payload containing exactly one independently encoded FAST message;
RT-2 replay callback passes the payload to one DecoderSession for that stream;
decoded results preserve record order;
a malformed record produces a decode error and leaves decoder state unchanged;
no packet framing, sequence, A/B or recovery claim is made.
```

The production decoder library should not depend on filesystem replay or `.mxraw` parsing; composition belongs to the caller/integration layer.

## 18. Security and repository hygiene

Forbidden in Git:

```text
real MOEX raw packet payloads;
official or owner-provided private template/configuration files;
private IP addresses or ports;
credentials, tokens or personal data;
generated large binary archives;
QuickFAST binaries or copied proprietary/reference artifacts.
```

Allowed fixtures are small synthetic templates, manually documented public FAST examples and independently derived binary vectors with provenance comments.

## 19. Documentation and evidence

Implementation must update:

```text
cpp/moex_fast/README.md;
AI_CONTEXT.md;
PROJECT_STATE.md;
ROADMAP.md;
agent_workspaces/mimo/reports/<date>-rt3-fast-decoder.md.
```

The report must list exact implementation SHA, final head SHA, CI run identifiers, test inventory, commands, results, supported operator/type matrix and explicit unsupported features. No capability may be claimed without an active test or owner-local evidence.