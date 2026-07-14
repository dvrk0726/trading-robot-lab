# RT-3 Specialized MOEX SPECTRA T0/T1 Decoder — Round 2 Implementation Report

Date: 2026-07-11
Issue: #21
PR: #23 (existing, not new)
Branch: mimo/issue-21-rt3-fast-decoder

## Summary

Addressed all 7 blocking items from Architecture Review Round 2 (CHANGES_REQUIRED). This is a specialized decoder for the two accepted MOEX SPECTRA T0/T1 template profiles — not a general-purpose FAST 1.1 engine.

## Changes Made

### 1. Wire Primitives (FIX FAST 1.1 Normative)

**wire_cursor.hpp/cpp** — Complete rewrite of wire encoding primitives:

- **Nullable unsigned integers**: 0x00 = NULL, otherwise value = decoded - 1 (offset encoding per FAST 1.1 §6.3.2). Decoded 0 (wire 0x80) rejected as non-canonical for nullable unsigned.
- **Nullable signed integers**: 0x00 = NULL, otherwise value = decoded - 1 (signed). Decoded INT32_MIN/INT64_MIN rejected (would underflow).
- **ASCII strings**: Stop-bit encoded (NOT zero-terminated). Each byte: bit 7 = stop (1 = last), bits 6-0 = character (0x01..0x7F). Empty string = 0x80. Cursor restored on failure.
- **Presence maps**: MUST find stop bit even when requested bits already filled. Unterminated maps rejected. Cursor restored on failure.
- **Signed canonical encoding**: Minimum byte count enforced. Value that fits in N-1 bytes rejected when encoded in N bytes.
- **Signed max-width validation**: For int32 (5 bytes/35 bits) and int64 (10 bytes/70 bits), bits above type width must be sign extension. Uses wider accumulator (uint64_t for int32, pre-shift check for int64).
- **Configured limits**: max_bytes parameter added to all string/vector read functions.
- **Cursor state**: All failure paths restore cursor to start position.

**Shared JSON escaping**: `json_escape_string()` function added to wire_cursor for use by both text and JSON report generators.

### 2. Template Compiler (fast_compiler.cpp)

- **Excluded operator rejection**: `default`, `copy`, `increment`, `delta`, `tail` rejected with `unsupported_operator` at compile time (fail-closed). No dictionary collision detection needed since excluded operators are rejected before dictionary logic.
- **Reference rejection**: typeRef/templateRef/groupRef rejected with `unsupported_reference` error (accepted MOEX SPECTRA profile uses inline definitions only).
- **Decimal component operator rejection**: Any operator in `<exponent>` or `<mantissa>` rejected with `unsupported_decimal_component_operator`.
- **Operator validation**: Only `none` (field without operator) and `constant` are accepted.
- **SHA-256 provenance**: Templates file SHA-256 and size retained in compiled set.

### 3. Decoder Session (fast_decoder.cpp)

- **max_message_bytes enforcement**: Checked before any decoding begins. Returns `LimitExceeded` with `message_size_limit` issue.
- **Hard ceiling clamping**: Constructor clamps configured limits to hard ceilings (1 MiB message, 64-byte pmap, 100K sequence entries, 1M nodes, 1 MiB strings).
- **Issue details**: All error paths set code, offset, template_id (when known), and field_path (when known).

### 4. Reports (decoder_report.cpp)

- **Shared JSON escaping**: Uses `json_escape_string()` for all string values.
- **Field metadata**: JSON output includes `fieldPath`, `isPresent`, `isNull`, `source`, `type` for every decoded field.
- **Lowercase hex**: Byte vectors rendered as lowercase hex (not uppercase).
- **Issue escaping**: Issue code, fieldPath, and message use shared JSON escaping.

### 5. CLI (decode_main.cpp)

- No changes needed (already correct).

### 6. CI (.github/workflows/ci.yml)

- **Exact RT-1 inventory**: 6 tests verified by name (test_template_parser, test_config_parser, test_provenance, test_deterministic_report, test_resource_safety, test_cli).
- **Exact RT-3 inventory**: 9 tests verified by name (test_decoder_primitives, test_decoder_compiler, test_decoder_session, test_decoder_operators, test_decoder_sequences, test_decoder_rollback, test_decoder_cli, test_decoder_limits, test_decoder_reference_oracle).
- **Total count**: 15 tests (6 RT-1 + 9 RT-3).
- **Separate verification**: Windows/MSVC and Linux/GCC each verify both inventories independently.

## Test Inventory (15 total)

### RT-1 Inspector Tests (6)
1. test_template_parser
2. test_config_parser
3. test_provenance
4. test_deterministic_report
5. test_resource_safety
6. test_cli

### RT-3 Decoder Tests (9)
1. test_decoder_primitives — stop-bit uInt32/uInt64/i32/i64, nullable offset encoding, presence maps (stop-bit termination, overlong rejection), ASCII stop-bit strings (mandatory/nullable), Unicode strings (mandatory/nullable), decimals (mandatory/nullable exponent+mantissa). No byteVector support.
2. test_decoder_compiler — valid compilation, duplicate IDs, missing IDs, invalid XML, empty templates, sequences, decimals, excluded-operator rejection (default/copy/increment/delta/tail → unsupported_operator), reference rejection (typeRef/templateRef/groupRef → unsupported_reference), decimal component operator rejection (→ unsupported_decimal_component_operator), structural validation, presence-map matrix, compile limits, SHA-256 provenance. No generic dictionary collision detection (excluded operators rejected before dictionary logic).
3. test_decoder_session — single message, template-ID reuse, first-message no-ID, unknown ID, trailing bytes, bytes_consumed, reset, optional null
4. test_decoder_operators — accepted operators: field without operator (none), constant. Compile-time rejection of excluded operators: default, copy, increment (→ unsupported_operator). Decimal component operator rejection: constant/copy/delta in exponent/mantissa (→ unsupported_decimal_component_operator). Optional decimal null/non-null decode.
5. test_decoder_sequences — simple 2-entry, empty
6. test_decoder_rollback — transactional rollback: failed decode preserves previous-template-ID state only (no dictionary state); fingerprint determinism across sessions
7. test_decoder_cli — valid hex, invalid hex, missing templates
8. test_decoder_limits — max_message_bytes, hard ceiling clamping, pmap limit, string limit, nullable non-canonical, signed max-width (INT32_MAX/MIN), cursor restore, session independence
9. test_decoder_reference_oracle — independent FIX FAST 1.1 reference encoder: verifies oracle-encoded byte vectors for presence maps, stop-bit integers, nullable integers, ASCII/Unicode strings, decimals, boundary cases. Does not link against the decoder.

## Accepted Operators

Only two operators are accepted by the specialized MOEX SPECTRA T0/T1 decoder:

| Operator | Effect |
|----------|--------|
| none (field without operator) | Value read from wire; optional fields use nullable wire encoding |
| constant | Value supplied by template; no wire bytes consumed; optional constants use presence-map bit |

All other FAST 1.1 operators are excluded and rejected at compile time with `unsupported_operator`:
`default`, `copy`, `increment`, `delta`, `tail`.

Decimal component operators (`<exponent>`, `<mantissa>`) must not contain operators — any operator on a decimal component is rejected with `unsupported_decimal_component_operator`.

## Excluded Scope (fail-closed)

The following constructs are recognized in XML only for fail-closed rejection at compile time:

- **Excluded operators**: `default`, `copy`, `increment`, `delta`, `tail` → `unsupported_operator`
- **Excluded references**: `typeRef`, `templateRef`, `groupRef` → `unsupported_reference`
- **byteVector**: Not a supported wire type (compile-time absence proven via `static_assert`)
- **Generic dictionaries**: No dictionary scope, collision detection, or reference resolution
- **Generic groups**: Not a supported wire type outside T0/T1 sequences
- **Decimal component operators**: Any operator in `<exponent>` or `<mantissa>` → `unsupported_decimal_component_operator`
- **Cyclic references**: Not applicable (all references rejected)

Previous-template-ID reuse is retained and is distinct from the XML `<copy>` operator.

## Commands

```powershell
# Windows/MSVC
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release
ctest --test-dir build/moex_fast -C Release --output-on-failure

# Linux/GCC
cmake -S cpp/moex_fast -B build/moex_fast -DCMAKE_BUILD_TYPE=Release
cmake --build build/moex_fast --config Release
ctest --test-dir build/moex_fast -C Release --output-on-failure
```

## Non-goals

Not a general-purpose FAST 1.1 engine — only MOEX SPECTRA T0/T1 template profiles. No byteVector decoding, no generic field dictionaries/scopes/collision detection, no stateful operators (default/copy/increment/delta/tail) or dictionary state, no typeRef/templateRef/groupRef resolution, no generic group instructions outside T0/T1 sequences, no decimal component operators, no SPECTRA UDP packet framing, socket/multicast, exchange sequence/gap policy, A/B merge/deduplication, Snapshot/Incremental recovery, normalized market events, order-book reconstruction, FIX/TWIME session, or order sending.

## Implementation Report

Last verified code head before documentation commit: `e89a2cf9d3f38f523ed3301d71f38f9cded5a7b9`

CI #153 success — all 7 jobs passed (Windows/MSVC and Linux/GCC, Release configurations, 15/15 tests).

## RT-4 Block

RT-4 (SPECTRA feed processors) must not begin until this PR is merged and Issue #21 is moved to DONE.
