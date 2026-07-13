# RT-3 — MOEX SPECTRA T0/T1 FAST Decoder Foundation

Date: 2026-07-13  
Status: corrected specification for owner review  
Issue: #21  
Implementation PR: #23 remains open and must not be changed by this documentation PR

This revision supersedes the broader RT-3 scope merged in PR #22. The earlier text incorrectly expanded RT-3 into a near-general FAST 1.1 engine. RT-3 is limited to the constructs actually present in the accepted MOEX SPECTRA T0 and T1 template sets.

## Objective

Create one offline, template-driven C++20 decoder that accepts exactly one bounded FAST message body and decodes it using either the accepted MOEX T0 or T1 `templates.xml` file.

```text
accepted T0 or T1 templates.xml
+ one bounded FAST message body
+ one DecoderSession
-> deterministic typed DecodedMessage
```

The decoder is not split into T0-specific and T1-specific implementations. Differences in template IDs and fields are supplied by the XML template set.

## Authoritative sources and version roles

Source priority is fixed:

1. MOEX SPECTRA FAST documentation and template files under `https://ftp.moex.com/pub/FAST/Spectra/test/`.
2. FIX FAST 1.1 only for base wire semantics that MOEX uses but does not restate fully.
3. Third-party implementations only as non-normative cross-checks.

Accepted template targets:

| Target | MOEX role | Path | SHA-256 |
|---|---|---|---|
| T0 | test system corresponding to the production trading-system version | `templatesT0/templates.xml` | `DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E` |
| T1 | test system for the next trading-system release | `templatesT1/templates.xml` | `84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F` |

`FAST_9.0/templates.xml` is byte-identical to the accepted T0 file and is not a third independent profile. `FAST_8.6` and `backup/` are historical material and are not RT-3 compatibility targets.

The current MOEX protocol documents are `spectra_fastgate_ru.pdf` and `spectra_fastgate_en.pdf`, version 1.30.2 dated 2026-04-10.

Before final owner acceptance, the current production template file must be downloaded and hashed again. If its hash is neither accepted T0 nor accepted T1, RT-3 stops for a new source audit; the decoder must not guess or silently broaden its profile.

## Frozen MOEX XML profile

The hash-bound inventory contains:

```text
T0: 19 templates, 393 fields, 70 constant elements, 323 fields without an operator
T1: 19 templates, 396 fields, 70 constant elements, 326 fields without an operator
```

Both sets contain zero occurrences of:

```text
copy
delta
increment
default
tail
dictionary attributes
```

The supported field behavior is therefore limited to:

- a field with no operator element;
- the `constant` operator;
- the primitive and structural XML instructions actually present in the hash-bound T0/T1 files;
- mandatory and optional presence;
- template identifiers, decimals, sequences and sequence lengths as used by those files.

`none` may remain an internal enum name for “no operator element”, but it is not an additional FAST operator.

## Explicitly excluded from RT-3

The following are not implemented, preserved as dormant capabilities, or moved to a future roadmap item:

```text
default/copy/increment/delta/tail field operators
runtime field dictionaries and dictionary scopes
user-defined dictionaries
typeRef/templateRef/groupRef
reference resolution and cycle detection
generic group instructions not present in T0/T1
historical FAST_8.6 or backup profile compatibility
```

If any excluded construct appears in an input template, compilation fails closed with a stable unsupported-feature issue. A future official MOEX template that introduces a new construct requires a new owner-approved source audit and specification change.

## Required decoder properties

- template-driven, with no message-name or T0/T1 hard-coded decode branches;
- bounded cursor reads and checked arithmetic;
- correct stop-bit, nullable, presence-map, decimal and sequence rules for the accepted profile;
- immutable compiled template set;
- one `DecoderSession` per ordered logical source stream;
- previous-template-ID state and explicit reset;
- transactional decode: any failure leaves session state unchanged;
- deterministic owned output and stable issues on Windows/MSVC and Linux/GCC.

## Non-goals

```text
no UDP/datagram framing or 4-byte MOEX preamble parsing
no sockets or multicast
no A/B sequencing or deduplication
no gap detection or recovery
no Snapshot/Incremental bootstrap
no normalized market events or books
no FIX/TWIME order entry
no strategy, paper or production enablement
```

RT-4 remains blocked until the corrected RT-3 specification is merged, PR #23 is corrected to this scope, owner acceptance is complete, owner authorizes merge, and post-merge CI on `main` is green.
