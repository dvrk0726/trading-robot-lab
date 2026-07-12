# RT-3 Test Plan — MOEX SPECTRA T0/T1 Profile

Date: 2026-07-13  
Status: corrected specification for owner review

This plan tests only the hash-bound MOEX T0/T1 profile defined in `01_REQUIREMENTS.md`. Tests for generic FAST field operators, dictionaries and references are removed.

## 1. Test principles

All committed tests are self-contained, deterministic and active in Release builds.

```text
Windows / MSVC Release
Linux / GCC Release
no network access in CI
no private or official XML committed to Git
no raw market-data captures
no QuickFAST dependency
```

Synthetic templates must use only the accepted MOEX profile or intentionally contain one excluded construct to verify fail-closed compilation.

Expected values must come from literal independently derived vectors, not from comparing two production functions that share logic.

## 2. CI inventory

The implementation correction must publish the exact RT-3 test-executable count and run the same inventory on Windows and Linux. Silently deleting a test to make CI green is prohibited.

Every correction also runs:

```text
RT-1 inspector tests
RT-2 raw/replay tests
QSH/M10X tests
Python/contracts checks
repository hygiene
```

## 3. Official-source inventory verification

Owner-local acceptance downloads and hashes:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/templates.xml
expected SHA-256 DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/templates.xml
expected SHA-256 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

A namespace-aware inventory check must prove:

```text
T0: 19 templates, 393 fields, 70 constant, 323 no-operator
T1: 19 templates, 396 fields, 70 constant, 326 no-operator
copy/delta/increment/default/tail: zero
dictionary attributes: zero
namespace: http://www.fixprotocol.org/ns/fast/td/1.1
```

`FAST_9.0/templates.xml` may be checked as evidence that it is byte-identical to T0, but it is not a separate acceptance target.

Before final owner acceptance, download the current production `templates.xml` and hash it. If it is neither accepted T0 nor T1, stop the gate and perform a new source audit; do not expand the implementation automatically.

## 4. Compiler tests

### 4.1 Accepted synthetic profile

Compile small synthetic templates covering:

```text
root namespace
multiple template IDs
uInt32/uInt64/int32/int64
ASCII and charset="unicode" strings
decimal without operators
mandatory and optional fields
mandatory and optional sequence
exactly one sequence length
mandatory and optional constant
nested sequence entry fields in source order
```

Assert the immutable compiled representation exactly:

```text
template IDs/names
field names/FIX tags/order
wire type and presence
string charset
constant literal
sequence length and child order
presence-map-bit decisions
provenance SHA-256/size/compiler version
```

### 4.2 Rejected constructs

Each case must fail compilation with no usable compiled set and a stable issue code:

```text
default
copy
increment
delta
tail
dictionary attribute
user-defined dictionary
typeRef
templateRef
groupRef
generic group instruction
byteVector outside the accepted inventory
decimal exponent/mantissa component operators
unknown wire-affecting element or attribute
wrong namespace
unknown charset or presence
multiple operators
invalid constant value/type
missing or duplicate sequence length
duplicate/invalid template ID
excessive templates/fields/nesting
```

The compiler must never silently ignore one of these constructs.

## 5. Stop-bit integer tests

Use literal vectors for ordinary and nullable forms of the integer widths used by T0/T1.

Cover at least:

```text
0, 1, -1
127/128 and signed stop-bit boundaries
INT32_MIN/MAX and INT64_MIN/MAX
UINT32_MAX and UINT64_MAX
nullable NULL versus zero
nullable negative values without offset-by-one
true nullable maximum values requiring widened intermediate representation
truncation at every byte position
unterminated entity
shift/add/sign-extension overflow
overlong encodings that fit in any smaller byte count
```

Representative required literals include:

```text
ordinary uInt32 0              -> 80
nullable uInt32 NULL           -> 80
nullable uInt32 0              -> 81
nullable int32 -1              -> FF
ordinary int32 64              -> 00 C0
ordinary INT32_MIN             -> 78 00 00 00 80
ordinary INT64_MIN             -> 7F 00 00 00 00 00 00 00 00 80
nullable UINT32_MAX            -> 10 00 00 00 80
```

## 6. Presence-map tests

Test:

```text
one-byte and multi-byte maps
bit order across byte boundaries
implicit zero suffix after a valid stop byte
unterminated map
overlong all-zero map rejection: 00 80
configured and hard byte limits
exact cursor position
```

Instruction-matrix message tests must prove:

```text
mandatory no-operator field: no field bit, ordinary wire value
optional no-operator field: no field bit, nullable wire value
mandatory constant: no field bit and no wire bytes
optional constant: exactly one bit
```

Sequence-entry presence maps must be read only when an accepted instruction inside the entry allocates a bit. Add a regression test proving that optional no-operator entry fields do not cause an entry presence map.

## 7. String tests

### ASCII

```text
mandatory empty and non-empty
optional NULL distinct from empty
zero preamble handling
unterminated string
overlong encoding
size limit
```

Required literals:

```text
mandatory empty -> 80
optional NULL   -> 80
optional empty  -> 00 80
"AB"            -> 41 C2
```

### Unicode

```text
mandatory and optional length preamble
NULL distinct from empty
valid UTF-8 of 1–4 bytes
invalid continuation
overlong UTF-8
surrogate encoding
code point above U+10FFFF
length overflow/truncation/limit
```

## 8. Decimal tests

Only no-operator decimals are supported.

Cover:

```text
mandatory positive/zero/negative exponent and mantissa
optional NULL exponent terminates the field
no mantissa consumption after NULL exponent
optional non-null exponent plus ordinary mantissa
int32/int64 boundary and overflow paths
exact exponent/mantissa output with no floating point
```

No test may require default/copy/increment/delta/tail or decimal component operators.

## 9. Template-ID state tests

Within one `DecoderSession` test:

```text
first explicit template ID
second message reuses previous ID
later explicit ID change
unknown ID
first-message omission
truncated/non-canonical ID
failure after provisional ID change
reset clears previous ID
decode_exact trailing-byte rollback
```

After every failed decode, a known-good continuation must prove the previous template ID is unchanged.

## 10. Sequence tests

Cover the accepted profile:

```text
mandatory sequence length uInt32
optional sequence NULL length
optional present empty sequence length zero
one and multiple entries
source-order fields
entry pmap absent when no accepted instruction needs a bit
entry pmap present for an optional constant synthetic case
nested decoded paths
entry-count limit before allocation/iteration
total-node limit
truncated length/body
failure in entry N rolls back the whole message
```

No sequence length operator is supported or tested.

## 11. Transaction and ownership tests

Because generic field dictionaries are removed, transactional state consists of previous template ID and any temporary decode/output state.

Prove:

```text
failed decode leaves session fingerprint unchanged
trailing bytes under decode_exact do not commit
failed sequence after earlier entries does not commit
reset returns a deterministic empty state
two sessions sharing one CompiledTemplateSet remain independent
DecodedMessage remains valid after input/session changes
```

## 12. Limits and malformed-input tests

Use small configured limits to exercise:

```text
message bytes
presence-map bytes
templates
fields
nesting
sequence entries
total nodes
string bytes
```

Malformed input must not cause out-of-bounds access, unchecked recursion, unbounded loops, allocation before validation or uncaught exceptions.

## 13. Diagnostic CLI tests

Test the retained minimal `moex-fast-decode` CLI only for:

```text
valid exact message from hex
valid message from file
quoted paths
malformed hex
unknown template
truncated message
trailing bytes under --exact
deterministic text/JSON
no network or preamble/framing claim
```

No generic-operator value-source output is required. Output sources are limited to wire and constant.

## 14. RT-2 integration test

A repository-safe synthetic test passes one complete FAST message body in `RawPacketRecord.payload` to one session and proves ordered decoding and failure rollback. It must not claim UDP framing, preamble handling, A/B sequencing or recovery.

## 15. Removal verification

The implementation correction must demonstrate that PR #23 no longer exposes or claims support for:

```text
default/copy/increment/delta/tail
generic field dictionaries/scopes/keys
references or cycle resolution
generic operator synthetic tests
```

Rejection tests replace positive tests for those constructs. Dead generic production branches and data structures must be removed, not merely left untested.
