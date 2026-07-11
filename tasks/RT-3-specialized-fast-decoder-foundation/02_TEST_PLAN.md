# RT-3 Test Plan

## 1. Test principles

All decoder tests are self-contained, deterministic and active in Release builds. Synthetic templates and binary vectors must be small, public-safe and committed with comments explaining how expected bytes/values were derived.

The suite must not depend on network access, private MOEX files, raw packet captures, QuickFAST binaries, wall-clock timing or test execution order.

Required platforms:

```text
Windows / MSVC Release;
Linux / GCC Release.
```

Every test executable must fail through an active `CHECK`/assertion mechanism under `NDEBUG`.

## 2. Inventory and CI gate

Add a dedicated RT-3 decoder test inventory to GitHub Actions on Windows and Linux. CI must verify the exact expected count before running CTest, as RT-2 does.

The final inventory count is fixed by the implementation PR and must be documented. Silently removing a test executable to make CI green is prohibited.

Every RT-3 PR must also run:

```text
RT-1 inspector: 6/6 Windows and Linux;
RT-2 raw/replay: 18/18 Windows and Linux;
QSH/M10X: 20/20;
Python/contracts;
repository hygiene.
```

## 3. Independent evidence strategy

Avoid testing decoder output only against helper functions that use the same decoder logic.

Use at least three independent evidence classes:

```text
A. hard-coded primitive and message byte vectors with literal expected values;
B. a test-only reference encoder/oracle implemented separately from production decode code;
C. state-transition and corruption tests that assert exact rollback and error offsets.
```

The reference encoder may support only the synthetic test templates, but must not call production encoding/decoding primitives. Use a fixed seed for generated/differential cases.

Optional local comparison against QuickFAST or another public reference implementation may be documented, but it is not a production or CI dependency and cannot replace self-contained tests.

## 4. Template compiler tests

### 4.1 Valid compilation

Compile synthetic templates containing:

```text
all supported primitive types;
mandatory and optional fields;
none/constant/default/copy/increment/delta/tail;
explicit initial values;
all supported dictionary scopes;
decimal exponent and mantissa operators;
nested groups;
sequences with entry presence maps;
static references used by the accepted profile.
```

Verify the immutable compiled tree exactly:

```text
template ids and names;
field order and paths;
FIX tags;
operator kind;
initial values;
presence-map consumption flags;
dictionary canonical keys;
sequence length instruction;
resolved references;
template SHA-256 provenance.
```

### 4.2 Invalid compilation

Each case must return a stable compile issue and no usable compiled set:

```text
duplicate/non-numeric/out-of-range template id;
duplicate ambiguous dictionary key;
unknown primitive or operator;
operator/type mismatch;
invalid initial integer/decimal/string value;
unknown presence value;
unknown dictionary scope;
missing sequence length;
unresolved reference;
reference cycle;
excessive nesting;
excessive templates/fields;
inconsistent dictionary value type;
silently ignored wire-affecting XML element attempt.
```

## 5. Presence-map tests

Hard-coded vectors must cover:

```text
one-byte maps;
multi-byte maps;
all-zero payload bits;
all-one payload bits;
bit ordering across byte boundaries;
implicit zero bits after the transmitted terminating byte where valid;
unterminated map;
map exceeding configured and hard limits;
truncated map;
exact cursor offset after decode.
```

Test nested sequence-entry presence maps independently from the message-level map.

## 6. Unsigned integer tests

For uInt32 and uInt64, including nullable forms:

```text
0, 1, 127, 128, boundary powers of 2;
UINT32_MAX and UINT64_MAX where representable by the normative encoding;
null versus zero;
minimum and maximum encoded byte counts;
truncated values at every byte position;
unterminated values;
overflow before shift/add;
overlong/non-canonical encodings;
cursor unchanged or exact offset on failure according to the public result contract.
```

Literal expected bytes are mandatory for representative boundaries.

## 7. Signed integer tests

For int32 and int64, including nullable forms:

```text
0, 1, -1;
positive and negative stop-bit boundaries;
INT32_MIN/MAX;
INT64_MIN/MAX;
null versus zero and -1;
sign extension boundaries;
truncation/unterminated input;
overflow and non-canonical encodings.
```

## 8. String and byte-vector tests

### 8.1 ASCII

```text
null;
empty;
one byte;
termination at each position;
multi-byte value;
invalid ASCII domain byte;
unterminated value;
configured size limit;
message-boundary truncation.
```

### 8.2 Unicode

```text
null;
empty;
valid one-, two-, three- and four-byte UTF-8 sequences;
invalid continuation;
overlong UTF-8;
surrogate encoding;
code point above U+10FFFF;
length prefix overflow/truncation;
size limit.
```

### 8.3 Byte vector

```text
null distinct from empty;
zero bytes in payload;
all 0x00/0xFF patterns;
length prefix boundaries;
truncation after a positive partial body;
size limit and checked arithmetic.
```

## 9. Decimal tests

Hard-coded vectors and stateful templates must cover:

```text
positive/zero/negative exponent;
positive/zero/negative mantissa;
null decimal where exponent null terminates the value;
no mantissa consumption after null exponent;
component none/default/copy/increment/delta cases required by profile;
exponent int32 overflow;
mantissa int64 overflow;
transaction rollback after exponent state changes but mantissa fails;
JSON exact exponent/mantissa integers without floating-point conversion.
```

## 10. Template-ID state tests

Use a sequence of messages in one session:

```text
first message with explicit id;
second message omitting id and reusing previous;
third message changing id;
unknown id;
first message omitting id;
truncated id;
non-canonical id;
message with valid id followed by later field failure;
decode_exact trailing-byte failure.
```

After every failed case, decode a known-good message and prove the previous template-id state is exactly what it was before the failure.

## 11. Operator matrix tests

For every supported operator/type combination, test both the wire-present and wire-absent/null paths defined by the normative table.

### 11.1 none

```text
mandatory direct value;
optional null and non-null;
no unintended dictionary update.
```

### 11.2 constant

```text
mandatory constant emits value without wire bytes;
optional constant present/null behavior;
invalid wire bytes remain unconsumed and are detected by decode_exact.
```

### 11.3 default

```text
absent uses initial/default;
present overrides;
present null;
mandatory/optional differences;
no dictionary collision.
```

### 11.4 copy

```text
initial value path;
wire update;
absent previous-value reuse;
null update and subsequent reuse behavior;
undefined previous value behavior;
cross-message state;
rollback after provisional update.
```

### 11.5 increment

```text
initial path;
wire update;
absent previous+1;
undefined previous behavior;
UINT/INT overflow;
rollback after provisional increment.
```

### 11.6 delta

```text
integer positive/negative/zero delta;
checked overflow;
decimal component delta;
ASCII/Unicode/byte-vector accepted prefix/suffix cases;
invalid subtraction length;
result length limit;
undefined base behavior;
rollback.
```

### 11.7 tail

```text
wire tail replacement;
absent previous reuse;
initial base;
empty tail;
retained-prefix boundary;
invalid retained length;
result size limit;
null behavior;
rollback.
```

## 12. Dictionary-scope tests

Create templates whose fields have identical names but different scopes/keys/types.

Verify:

```text
global entries share only where normative;
template/type-scoped entries do not leak;
explicit key attributes are honored;
decimal exponent and mantissa do not collide;
different primitive types cannot alias;
reset clears every scope;
two DecoderSession objects using one CompiledTemplateSet remain independent.
```

## 13. Group and sequence tests

```text
mandatory group;
optional present and absent group;
empty sequence;
null optional sequence distinct from empty;
one and many entries;
entry-level presence maps;
nested group inside sequence;
nested sequence to accepted depth;
configured sequence-entry limit;
total-node limit;
length integer overflow;
length greater than remaining bytes;
failure in entry N after successful entries 0..N-1 rolls back the entire message.
```

Output order and deterministic field paths must be asserted exactly.

## 14. Transaction rollback tests

Snapshot session state through a deterministic debug/test fingerprint or equivalent test-only introspection.

For each failure location below, compare before/after fingerprints and then decode a known-good continuation:

```text
presence map;
template id;
first scalar;
after copy update;
after increment update;
after delta update;
decimal mantissa;
optional group;
sequence length;
sequence middle entry;
last field;
trailing byte under decode_exact;
limit exceeded after decoded nodes were provisionally created;
output/report failure injection if production path can fail.
```

No failed case may change dictionary values or previous template id.

## 15. Limits and resource safety

Use small configured limits to trigger each path without allocating excessive memory:

```text
message size;
presence-map bytes;
templates;
fields;
nesting;
sequence entries;
total decoded nodes;
string/byte-vector bytes.
```

Add checked-arithmetic boundary tests near `SIZE_MAX`, `UINT32_MAX` and `UINT64_MAX` using mocked lengths/cursors rather than huge real allocations.

Malformed input must not cause:

```text
out-of-bounds read;
unchecked recursion;
unbounded loop;
allocation before validating length/count;
quadratic dictionary key creation;
uncaught exception;
session mutation.
```

## 16. Golden message vectors

Commit at least these independent hard-coded vectors:

```text
single template with constants and mandatory integers;
stateful two-message copy/increment sequence with second template id omitted;
message with optional nulls, empty values and decimal;
message with sequence entries and entry presence maps;
string delta/tail message if those forms are supported by accepted profile.
```

Each vector file or test section must document:

```text
template XML fragment;
hex bytes;
expected bytes_consumed;
expected decoded tree;
expected session transition;
provenance: manually derived from FIX FAST rules or independent reference encoder.
```

Tests compare literal expected output, not one production function against another.

## 17. Deterministic differential tests

Use the separate test-only encoder/oracle with a fixed seed to produce many valid messages for the supported operator matrix.

For each generated message:

```text
decode_exact succeeds;
all values equal the encoder's semantic source values;
bytes_consumed equals input size;
state transition matches the oracle;
Windows/Linux deterministic JSON matches a checked golden subset.
```

Mutate generated messages deterministically by truncation, stop-bit removal, length increase and bit flips. Assert bounded explicit failure and unchanged session state.

## 18. CLI end-to-end tests

Run the built `moex-fast-decode` executable through a portable quoted process helper.

Required scenarios:

```text
--hex valid exact message -> text valid;
--input valid file -> JSON valid;
paths containing spaces;
malformed hex;
missing/duplicate arguments;
truncated message;
unknown template;
trailing bytes under --exact;
invalid JSON output path;
deterministic repeated JSON byte equality;
no full local path leak in default report.
```

Parse JSON with a strict test-only parser and verify types, arrays, nested fields, null representation, value source, decimal components, issue offsets and full input consumption.

## 19. RT-2 replay integration test

Link a test target with both RT-2 and RT-3 libraries or use an equivalent repository-safe integration target.

Test flow:

```text
independent test encoder creates three valid stateful FAST messages;
write them as three synthetic RawPacketRecord payloads in one `.mxraw` stream;
validate and replay the stream through the RT-2 callback;
pass each record payload to one DecoderSession using decode_exact;
assert record order, decoded values and final state;
insert a malformed middle record in a separate fixture;
assert explicit decode error, no decoder-state mutation from the bad record and no framing/recovery claim.
```

The integration test must not infer `MsgSeqNum`, packet sequence, A/B state or recovery policy.

## 20. Reset tests

```text
reset after several stateful messages;
previous template id cleared;
all dictionaries cleared;
compiled templates unchanged;
first post-reset omitted template id fails;
explicit-id message succeeds from initial state;
reset of one session does not affect another.
```

## 21. Report determinism and provenance

Verify text and JSON include:

```text
schema/compiler/decoder versions;
templates basename/size/SHA-256;
input length/SHA-256;
template id/name;
bytes_consumed;
ordered field tree;
null versus empty;
value-source enum;
exact decimal;
lowercase byte-vector hex;
stable errors with offset/path;
overall status.
```

Run identical vectors repeatedly and compare output bytes. Avoid timestamps, pointer values, unordered-map iteration and platform path separators in canonical output.

## 22. Owner-local acceptance package

The implementation PR must document exact PowerShell commands for:

```text
clean Release configure/build;
RT-3 CTest;
golden CLI decode;
stateful two-message decode;
truncated/corrupt rejection;
RT-2 synthetic replay -> RT-3 decode integration;
git head/status verification.
```

No official MOEX raw packet is required to accept RT-3 foundation. Before RT-4, the owner may additionally compile current official local templates against the accepted profile without committing them.