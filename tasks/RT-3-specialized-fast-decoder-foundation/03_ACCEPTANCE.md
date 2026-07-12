# RT-3 Acceptance Criteria — MOEX SPECTRA T0/T1 Profile

Date: 2026-07-13  
Status: corrected specification for owner review

RT-3 is accepted only when every criterion below is satisfied. Green CI alone is not sufficient.

## 1. Scope

The delivered component is one specialized, template-driven C++20 decoder for the accepted MOEX SPECTRA T0 and T1 template sets.

It must not be described as a full FAST 1.1 engine.

```text
input: one bounded FAST message body
profile: accepted T0 or T1 templates.xml
output: deterministic owned typed message
session state: previous template ID only
```

No UDP framing, MOEX 4-byte preamble, networking, sequencing/recovery, books, order entry or strategy code is added.

## 2. Authoritative source gate

Owner-local evidence must verify:

```text
T0 SHA-256:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Namespace-aware inventory must report:

```text
T0: 19 templates, 393 fields, 70 constant, 323 no-operator
T1: 19 templates, 396 fields, 70 constant, 326 no-operator
copy/delta/increment/default/tail: 0
dictionary attributes: 0
namespace: http://www.fixprotocol.org/ns/fast/td/1.1
```

The current production template file is downloaded and hashed immediately before final owner acceptance. If its hash is neither accepted T0 nor accepted T1, acceptance is blocked for a new source audit. No automatic scope expansion is allowed.

## 3. Template compiler

Both accepted T0 and T1 files compile successfully outside Git.

The immutable compiled tree preserves exactly what affects the accepted wire profile:

```text
template ID/name
field name/FIX tag/source order
integer/string/decimal type
mandatory/optional presence
ASCII/Unicode charset
constant value
sequence and its single length instruction
```

Every compile issue produces no usable compiled set.

The compiler explicitly rejects:

```text
default/copy/increment/delta/tail
dictionary attributes and dictionary names
typeRef/templateRef/groupRef
generic group instructions
byteVector outside the accepted inventory
decimal component operators
unknown wire-affecting XML
invalid IDs, names, literals, charset or presence
missing/duplicate sequence length
excessive templates/fields/nesting
```

No unsupported construct is silently ignored.

## 4. Generic FAST scope removal

The implementation PR no longer contains or claims positive support for:

```text
default
copy
increment
delta
tail
generic field dictionary state
dictionary scopes or canonical dictionary keys
user-defined dictionaries
reference resolution or cycle detection
generic operator test matrices
```

Dead generic branches, data structures and positive tests are removed. Rejection tests remain to enforce fail-closed behavior.

Internal previous-template-ID reuse remains and is not treated as the XML `<copy>` operator.

## 5. Primitive correctness

Literal independent tests pass for all accepted primitive behavior:

```text
presence maps
ordinary and nullable uInt32/uInt64
ordinary and nullable int32/int64
ASCII and nullable ASCII
Unicode and nullable Unicode
exact decimal exponent/mantissa
sequence length uInt32 and nullable optional length
```

Required correctness includes:

```text
no out-of-bounds reads
correct signed sign extension
nullable offset-by-one only for non-negative integers
NULL distinct from zero and empty
canonical/overlong rejection
valid UTF-8 enforcement
no mantissa after NULL decimal exponent
checked length/count arithmetic before allocation
```

## 6. Presence-map correctness

Active tests prove:

```text
message template-ID bit is first
mandatory no-operator field uses no field bit
optional no-operator field uses no field bit and nullable wire form
mandatory constant uses no field bit and no wire bytes
optional constant uses exactly one bit
entry pmap exists only when an accepted entry instruction allocates a bit
unterminated and overlong maps fail
```

The compiler must not assign pmap bits solely because a field or sequence is optional.

## 7. Template-ID state and reset

```text
explicit ID works
previous successful ID reuse works
ID change works
unknown/missing/truncated/non-canonical ID fails
failed message does not change previous ID
decode_exact trailing bytes do not commit
reset clears previous ID
```

## 8. Decimal correctness

Supported decimals have no operator and no component operators.

```text
mandatory decimal = ordinary exponent + ordinary mantissa
optional decimal = nullable exponent; NULL omits mantissa
output remains exact {exponent:int32, mantissa:int64}
no decoder-internal floating-point conversion
```

## 9. Sequence correctness

All accepted T0/T1 sequence structures compile and synthetic wire tests prove:

```text
single length instruction
mandatory ordinary uInt32 length
optional nullable uInt32 length
NULL sequence distinct from present empty sequence
entry order and field order preserved
entry pmap conditional on the accepted matrix
entry and node limits checked before iteration/allocation
failure in any entry fails the whole message
```

No sequence length operator is supported.

## 10. Transaction, ownership and errors

```text
CompiledTemplateSet is immutable and safely shareable
DecoderSession belongs to one ordered logical source stream and is not thread-safe
DecodedMessage owns values independently of input/session lifetime
any failed decode leaves previous-template-ID state unchanged
expected malformed input returns explicit status/issues, not uncaught exceptions
issues contain stable code, offset, template ID when known and deterministic field path
```

No generic field dictionary or dictionary rollback is part of the accepted design.

## 11. Limits

Active tests cover configured and hard paths for:

```text
message bytes
presence-map bytes
templates
fields
nesting
sequence entries
total decoded nodes
string bytes
```

No allocation or loop starts before the corresponding length/count is validated.

## 12. Diagnostic and RT-2 boundaries

The existing `moex-fast-decode` remains a minimal offline diagnostic for one FAST message body. It makes no network, UDP, preamble, framing or unsupported-operator claim.

A synthetic RT-2 integration test may pass one complete message body in `RawPacketRecord.payload` to a session. The production decoder library does not depend on `.mxraw` filesystem parsing.

## 13. CI and regression gate

The final correction head has green evidence for:

```text
RT-3 Windows/MSVC Release exact inventory
RT-3 Linux/GCC Release same inventory
RT-1 inspector regression
RT-2 raw/replay regression
QSH/M10X regression
Python/contracts
repository hygiene
```

Warnings-as-errors remain enabled. Test assertions remain active under `NDEBUG`.

## 14. Documentation accuracy

Before owner review, implementation documentation and PR #23 description must state only the accepted MOEX T0/T1 capability. They must not claim complete operator-table, dictionary or reference support.

Exact implementation head SHA, CI run, changed files and test inventory must be recorded. Stale counts or unsupported capability claims are blocking.

## 15. Owner-local acceptance

The owner runs the exact reviewed implementation head and verifies:

```text
clean Release configure/build
all RT-3 tests
T0 official XML hash + successful compilation
T1 official XML hash + successful compilation
current production XML hash decision
one exact synthetic decode with constants, optional nullable fields and decimal
one sequence decode including NULL versus empty
previous-template-ID reuse
truncated/corrupt input rejection
known-good continuation proving rollback
clean git status and exact HEAD
```

## 16. Merge and completion

```text
corrected specification PR reviewed
-> owner explicitly authorizes specification merge
-> post-merge main CI verified
-> PR #23 corrected to the merged scope in its existing branch
-> architecture review
-> owner local acceptance
-> owner explicitly authorizes implementation merge
-> post-merge main CI verified
-> Issue #21 may move to DONE
```

RT-4 remains blocked until all of the above is complete. Neither MiMo nor automation may merge.
