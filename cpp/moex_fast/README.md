# moex_fast — MOEX SPECTRA T0/T1 Configuration Inspector and Specialized Decoder

Local C++20 CLI tools for inspecting MOEX SPECTRA `configuration.xml`/`templates.xml` and decoding single FAST binary messages against the accepted T0/T1 template profiles.

## Components

### RT-1: FAST Configuration and Template Inspector (`moex-fast-inspect`)

Parse and validate FAST template definitions and feed configuration from MOEX SPECTRA XML files. Produces a human-readable summary and deterministic JSON inspection report.

### RT-3: Specialized MOEX SPECTRA T0/T1 Decoder (`moex-fast-decode`)

Offline binary FAST message decoder specialized for the two accepted MOEX SPECTRA template profiles (T0 and T1). Accepts exactly one bounded FAST message payload (hex or binary file) and produces deterministic text and JSON decode reports. This is not a general-purpose FAST 1.1 engine — it supports only the operator/type combinations present in the official MOEX SPECTRA T0/T1 templates.

## Build

```bash
cmake -S cpp/moex_fast -B build/moex_fast -DCMAKE_BUILD_TYPE=Release
cmake --build build/moex_fast --config Release
```

On Windows with MSVC:

```powershell
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release
```

## Tests

```bash
ctest --test-dir build/moex_fast -C Release --output-on-failure
```

Current inventory: 15 tests (6 RT-1 + 9 RT-3 decoder).

### RT-1 Tests (6)

- `test_template_parser` — XML template parsing
- `test_config_parser` — Feed configuration parsing
- `test_provenance` — SHA-256 provenance tracking
- `test_deterministic_report` — Deterministic JSON report output
- `test_resource_safety` — Resource limit enforcement
- `test_cli` — CLI integration tests

### RT-3 Tests (9)

- `test_decoder_primitives` — Wire primitives: stop-bit uInt32/uInt64/i32/i64, nullable offset encoding, presence maps (stop-bit termination, overlong rejection), ASCII stop-bit strings (mandatory/nullable), Unicode strings (mandatory/nullable), decimals (mandatory/nullable exponent+mantissa). No byteVector support.
- `test_decoder_compiler` — Template compiler: valid compilation, duplicate IDs, missing IDs, invalid XML, empty templates, sequences, decimals, excluded-operator rejection (default/copy/increment/delta/tail → unsupported_operator), reference rejection (typeRef/templateRef/groupRef → unsupported_reference), decimal component operator rejection (→ unsupported_decimal_component_operator), structural validation, presence-map matrix, compile limits, SHA-256 provenance. No generic dictionary collision detection (excluded operators are rejected before dictionary logic).
- `test_decoder_session` — Session: single message, template-ID reuse, first-message no-ID, unknown ID, trailing bytes, bytes_consumed, reset, optional null
- `test_decoder_operators` — Accepted operators: field without operator (none), constant. Compile-time rejection of excluded operators: default, copy, increment (→ unsupported_operator). Decimal component operator rejection: constant/copy/delta in exponent/mantissa (→ unsupported_decimal_component_operator). Optional decimal null/non-null decode.
- `test_decoder_sequences` — Sequences: simple 2-entry, empty
- `test_decoder_rollback` — Transactional rollback: failed decode preserves previous-template-ID state only (no dictionary state); fingerprint determinism across sessions
- `test_decoder_cli` — CLI: valid hex, invalid hex, missing templates
- `test_decoder_limits` — Limits: max_message_bytes, hard ceiling clamping, pmap limit, string limit, nullable non-canonical, signed max-width (INT32_MAX/MIN), cursor restore, session independence
- `test_decoder_reference_oracle` — Independent FIX FAST 1.1 reference encoder: verifies oracle-encoded byte vectors for presence maps, stop-bit integers (uInt32/uInt64/i32/i64), nullable integers, ASCII/Unicode strings, decimals, boundary cases. Does not link against the decoder.

## Usage: RT-1 Inspector

```bash
moex-fast-inspect \
  --configuration <path/configuration.xml> \
  --templates <path/templates.xml> \
  [--json-out <path/report.json>] \
  [--strict]
```

## Usage: RT-3 Decoder

```bash
moex-fast-decode --templates <templates.xml> --hex <one-message-hex>
moex-fast-decode --templates <templates.xml> --input <one-message.bin>
                 [--json-out <report.json>] [--exact]
```

- `--templates` — Path to templates.xml (required)
- `--hex` — One FAST message as hex string (mutually exclusive with --input)
- `--input` — Path to one FAST message binary file (mutually exclusive with --hex)
- `--json-out` — Path for deterministic JSON report output
- `--exact` — Reject trailing bytes after one message

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

## Non-goals

- Not a general-purpose FAST 1.1 engine — only MOEX SPECTRA T0/T1 template profiles
- No byteVector decoding
- No generic field dictionaries, scopes, or collision detection
- No stateful operators (default/copy/increment/delta/tail) or dictionary state
- No typeRef, templateRef, or groupRef resolution
- No generic group instructions outside T0/T1 sequences
- No decimal component operators
- No SPECTRA UDP packet framing or message-boundary guessing
- No socket or multicast connections
- No exchange sequence/gap policy
- No A/B merge or deduplication
- No Snapshot/Incremental recovery
- No normalized market events or order-book reconstruction
- No FIX/TWIME session or order sending

## Security

This tool performs no network access. No credentials, secrets, or private connection data are used or stored. All test fixtures are synthetic.
