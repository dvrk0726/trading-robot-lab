# RT-3 Requirements — MOEX SPECTRA T0/T1 Profile

Date: 2026-07-13  
Status: corrected specification for owner review

This document supersedes the broad generic-FAST requirements previously merged in PR #22.

## 1. Build and placement

Implement in C++20 and CMake under `cpp/moex_fast/`.

Required build gates:

```text
Windows / MSVC Release with /W4 /WX
Linux / GCC Release with -Wall -Wextra -Wpedantic -Werror
all tests active under NDEBUG/Release
no QuickFAST production dependency
no Python runtime dependency in the decoder
no network, database or pcap dependency
```

Existing RT-1, RT-2 and QSH/M10X behavior must not be weakened.

## 2. Exact public boundary

RT-3 decodes exactly one bounded FAST message body. It does not parse the MOEX 4-byte preamble or UDP framing.

Required operations are equivalent to:

```text
compile_templates(xml bytes/path, compile limits)
  -> immutable CompiledTemplateSet or compile issues

DecoderSession(compiled templates, decode limits)
session.decode_one(byte span)
session.decode_exact(byte span)
session.reset()
```

`decode_one` may consume one valid prefix and return `bytes_consumed`. `decode_exact` succeeds only when the complete span is exactly one FAST message.

## 3. Frozen source contract

Primary source directory:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/
```

Accepted template files:

```text
T0: templatesT0/templates.xml
SHA-256: DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E
Role: version corresponding to the production trading system

T1: templatesT1/templates.xml
SHA-256: 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Role: next-release trading-system version
```

`FAST_9.0/templates.xml` has the same accepted hash as T0 and does not add a third profile. `FAST_8.6` and `backup/` are excluded.

Current MOEX protocol documents:

```text
spectra_fastgate_ru.pdf  v1.30.2, 2026-04-10
spectra_fastgate_en.pdf  v1.30.2, 2026-04-10
```

FIX FAST 1.1 is used only for base wire semantics of constructs present in the accepted MOEX XML: stop-bit entities, nullable forms, presence maps, strings, decimals, template ID and sequence length. It is not a requirement to implement all FAST 1.1 template features.

## 4. Exact accepted XML profile

The compiler must support the namespace used by both files:

```text
http://www.fixprotocol.org/ns/fast/td/1.1
```

The accepted instruction profile is the exact namespace-safe inventory of the hash-bound T0/T1 files. It includes:

```text
templates
template
uInt32
uInt64
int32
int64
string (default ASCII and charset="unicode")
decimal
sequence
length
constant
```

Supported field behavior:

```text
no operator element
constant operator only
mandatory or optional presence
```

The compiler must preserve:

```text
template id and name
field name and optional FIX tag
source order
primitive type
mandatory/optional presence
string charset
decimal structure
sequence structure and its single length instruction
constant value
```

The current T0/T1 sequence profile contains 16 sequences, each with exactly one length instruction; 11 are mandatory and 5 are optional; no sequence length operator is present.

### Fail-closed exclusions

Compilation must fail with a stable unsupported-feature issue for any of:

```text
default
copy
increment
delta
tail
dictionary attribute or dictionary state
user-defined dictionary
exponent/mantissa component operators
typeRef
templateRef
groupRef
generic group instruction
byteVector instruction not present in the accepted inventory
unknown field/operator/attribute that affects wire decoding
```

Unsupported XML must never be ignored or approximated. A new official MOEX construct requires a new source audit and owner-approved specification revision.

## 5. Template compilation safety

Compilation must reject at least:

```text
malformed XML or wrong namespace
duplicate, missing, zero, non-numeric or out-of-range template IDs
missing required names
invalid FIX tag values
unknown presence or charset values
multiple operator children
invalid constant literal or constant/type mismatch
missing or duplicate sequence length
misplaced length or constant elements
excessive templates, fields or nesting
any excluded construct from section 4
```

Any compile issue makes the result unusable. The compiled set is immutable and retains template-file SHA-256, byte size and compiler/schema version.

Official XML files remain outside Git. Synthetic fixtures may be committed.

## 6. Wire primitives

Implement bounded decoding for the primitive types required by T0/T1:

```text
presence maps
uInt32/uInt64 stop-bit integers
int32/int64 stop-bit integers
nullable forms for optional integer fields
ASCII strings and nullable ASCII
Unicode strings as UTF-8 byte vectors with nullable length when optional
exact decimal exponent:int32 + mantissa:int64
sequence lengths as uInt32, nullable for optional sequences
```

Required rules:

- never read beyond the supplied span;
- detect overflow before shift/add/sign extension;
- reject overlong/non-canonical integer and presence-map encodings;
- nullable integer offset-by-one applies only to non-negative values;
- NULL and empty strings remain distinct;
- optional decimal NULL is a NULL exponent and consumes no mantissa;
- Unicode output must be valid UTF-8;
- length/count limits are checked before allocation or iteration.

## 7. Presence-map matrix for the accepted profile

Presence-map bits are consumed only as follows:

| Instruction | Mandatory | Optional |
|---|---:|---:|
| template identifier | first message-segment bit | first message-segment bit |
| field without operator | no field bit; ordinary wire representation | no field bit; nullable wire representation |
| `constant` | no field bit | one field bit |
| sequence length without operator | no field bit; ordinary uInt32 | no field bit; nullable uInt32 |

For optional `constant`, bit 1 means the constant value is present and bit 0 means the field is absent.

A sequence entry has its own presence map only when an accepted instruction inside that entry allocates a bit. The compiler must derive this from the matrix; it must not add entry presence maps unconditionally.

Untransmitted trailing presence bits are implicit zero only after a correctly terminated map. Unterminated and overlong maps are errors.

## 8. Template-ID session state

The first message presence-map bit controls template-ID transmission:

```text
bit set   -> decode and validate transmitted template ID
bit clear -> reuse previous successful template ID
```

Omission without a previous ID and unknown IDs are errors. A template-ID change commits only after the whole message succeeds. `reset()` clears it.

This internal template-ID reuse is required by the stream format and is not support for the XML field operator `<copy>`.

## 9. No runtime field dictionaries

The accepted T0/T1 profile has no dictionary-dependent field operators and no `dictionary` attributes. Therefore:

```text
DecoderSession stores no generic field dictionary
CompiledTemplateSet contains no generic dictionary manifest
no dictionary key/scoping API is part of RT-3
```

Existing generic operator/dictionary code in PR #23 must be removed rather than retained as a claimed or dormant production capability, unless a small helper is demonstrably reused by the accepted profile and has no generic behavior.

## 10. Decimal semantics

A supported decimal has no whole-field or component operator. It is decoded as an exact scaled number:

```text
DecodedDecimal { exponent:int32, mantissa:int64 }
```

Mandatory decimal: ordinary signed exponent followed by ordinary signed mantissa.  
Optional decimal: nullable signed exponent; if NULL, the field is absent and no mantissa bytes are consumed; otherwise an ordinary signed mantissa follows.

No floating-point conversion is allowed inside the decoder.

## 11. Sequence semantics

For each accepted sequence:

1. Decode its single length instruction immediately before entries.
2. Mandatory sequence length is ordinary uInt32.
3. Optional sequence length is nullable uInt32; NULL differs from present length zero.
4. Validate entry count before allocation or iteration.
5. Decode entries in source order.
6. Read an entry presence map only when required by the compiled accepted-profile matrix.
7. A failure in any entry fails and rolls back the entire message.

## 12. Ownership, state and rollback

```text
CompiledTemplateSet: immutable and shareable
DecoderSession: owns previous-template-ID state for one ordered logical stream; not thread-safe
DecodedMessage: owns all values independently of input/session lifetime
```

All state changes are transactional:

```text
successful decode -> commit previous template ID once
failed decode -> previous template ID unchanged
decode_exact trailing bytes -> no commit
allocation/report failure -> no commit
```

No generic dictionary journal is required because generic field dictionaries are outside the accepted profile.

## 13. Limits and errors

Configurable defaults must not exceed:

```text
message bytes: 1 MiB
presence-map bytes: 64
templates: 4096
fields per template: 65535
nesting depth: 32
sequence entries: 100000
total decoded nodes: 1000000
string bytes: 1 MiB and within message limit
```

Expected malformed input returns explicit status/issues, not uncaught exceptions. Issues include stable code, byte offset, template ID when known, deterministic field path and human-readable message.

## 14. Diagnostic CLI and RT-2 boundary

A minimal offline `moex-fast-decode` diagnostic CLI may remain because it is used for owner-local verification. It must accept one message body from hex or file and must not claim UDP/preamble parsing.

A synthetic integration test may pass one `RawPacketRecord.payload` byte span into `DecoderSession`. The decoder library itself must not depend on `.mxraw` filesystem parsing.

## 15. Explicit non-goals

```text
MOEX 4-byte preamble and UDP framing
network capture or multicast
multiple-message datagram splitting
A/B sequencing/deduplication
gap detection or recovery
Snapshot/Incremental bootstrap
normalized events and books
FIX/TWIME/order sending
strategy, paper or production enablement
historical template compatibility
full FAST 1.1 engine
```
