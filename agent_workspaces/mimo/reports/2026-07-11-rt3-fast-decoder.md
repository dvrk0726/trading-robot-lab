# RT-3 FAST Decoder — Round 2 Implementation Report

Date: 2026-07-11
Issue: #21
PR: #23 (existing, not new)
Branch: mimo/issue-21-rt3-fast-decoder

## Summary

Addressed all 7 blocking items from Architecture Review Round 2 (CHANGES_REQUIRED).

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

- **Dictionary collision detection**: Dictionary keys registered during field parsing. Duplicate keys with different field paths rejected with `duplicate_dictionary_key` error.
- **Reference rejection**: typeRef/templateRef/groupRef rejected with `unsupported_reference` error (accepted MOEX SPECTRA profile uses inline definitions only).
- **Operator/type validation**: Unsupported operator/type combinations rejected at compile time.
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
- **Exact RT-3 inventory**: 8 tests verified by name (test_decoder_primitives, test_decoder_compiler, test_decoder_session, test_decoder_operators, test_decoder_sequences, test_decoder_rollback, test_decoder_cli, test_decoder_limits).
- **Total count**: 14 tests (6 RT-1 + 8 RT-3).
- **Separate verification**: Windows/MSVC and Linux/GCC each verify both inventories independently.

## Test Inventory (14 total)

### RT-1 Inspector Tests (6)
1. test_template_parser
2. test_config_parser
3. test_provenance
4. test_deterministic_report
5. test_resource_safety
6. test_cli

### RT-3 Decoder Tests (8)
1. test_decoder_primitives — stop-bit uInt32/uInt64/i32/i64, nullable offset encoding, presence maps, ASCII stop-bit strings, Unicode, byte vectors, decimals, JSON escaping
2. test_decoder_compiler — valid compilation, duplicate IDs, missing IDs, invalid XML, empty templates, sequences, decimals, dictionary collision, reference rejection, operator/type validation, SHA-256 provenance
3. test_decoder_session — single message, template-ID reuse, first-message no-ID, unknown ID, trailing bytes, bytes_consumed, reset, optional null
4. test_decoder_operators — default, copy, increment, constant
5. test_decoder_sequences — simple 2-entry, empty
6. test_decoder_rollback — copy state, template-ID, fingerprint determinism
7. test_decoder_cli — valid hex, invalid hex, missing templates
8. test_decoder_limits — max_message_bytes, hard ceiling clamping, pmap limit, string limit, nullable non-canonical, signed max-width (INT32_MAX/MIN), cursor restore, session independence

## Supported Operator/Type Matrix

| Operator | uInt32/uInt64 | int32/int64 | ASCII/Unicode | byteVector | decimal |
|----------|:---:|:---:|:---:|:---:|:---:|
| none | yes | yes | yes | yes | yes |
| constant | yes | yes | yes | - | - |
| default | yes | yes | yes | yes | yes |
| copy | yes | yes | yes | yes | yes |
| increment | yes | yes | - | - | - |
| delta | yes | yes | yes | yes | yes |
| tail | - | - | yes | yes | - |

## Unsupported Constructs

- typeRef, templateRef, groupRef (compile error)
- Unknown dictionary scopes (compile error)
- Unsupported operator/type combinations (compile error)
- Cyclic references (compile error)

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

No SPECTRA UDP packet framing, socket/multicast, exchange sequence/gap policy, A/B merge/deduplication, Snapshot/Incremental recovery, normalized market events, order-book reconstruction, FIX/TWIME session, or order sending.

## RT-4 Block

RT-4 (SPECTRA feed processors) must not begin until this PR is merged and Issue #21 is moved to DONE.
