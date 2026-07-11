# moex_fast — FAST Configuration Inspector and Binary Decoder

Local C++20 CLI tools for inspecting MOEX SPECTRA `configuration.xml`/`templates.xml` and decoding single FAST binary messages.

## Components

### RT-1: FAST Configuration and Template Inspector (`moex-fast-inspect`)

Parse and validate FAST template definitions and feed configuration from MOEX SPECTRA XML files. Produces a human-readable summary and deterministic JSON inspection report.

### RT-3: Specialized FAST Decoder (`moex-fast-decode`)

Offline binary FAST message decoder. Accepts exactly one bounded FAST message payload (hex or binary file) and produces deterministic text and JSON decode reports.

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

Current inventory: 14 tests (6 RT-1 + 8 RT-3 decoder).

### RT-1 Tests (6)

- `test_template_parser` — XML template parsing
- `test_config_parser` — Feed configuration parsing
- `test_provenance` — SHA-256 provenance tracking
- `test_deterministic_report` — Deterministic JSON report output
- `test_resource_safety` — Resource limit enforcement
- `test_cli` — CLI integration tests

### RT-3 Tests (8)

- `test_decoder_primitives` — Wire primitives: stop-bit uInt32/uInt64/i32/i64, nullable offset encoding, presence maps (stop-bit termination), ASCII stop-bit strings, Unicode, byte vectors, decimals, JSON escaping
- `test_decoder_compiler` — Template compiler: valid compilation, duplicate IDs, missing IDs, invalid XML, empty templates, sequences, decimals, dictionary collision detection, reference rejection, operator/type validation, SHA-256 provenance
- `test_decoder_session` — Session: single message, template-ID reuse, first-message no-ID, unknown ID, trailing bytes, bytes_consumed, reset, optional null
- `test_decoder_operators` — Operators: default, copy, increment, constant
- `test_decoder_sequences` — Sequences: simple 2-entry, empty
- `test_decoder_rollback` — Rollback: copy state, template-ID, fingerprint determinism
- `test_decoder_cli` — CLI: valid hex, invalid hex, missing templates
- `test_decoder_limits` — Limits: max_message_bytes, hard ceiling clamping, pmap limit, string limit, nullable non-canonical, signed max-width (INT32_MAX/MIN), cursor restore, session independence

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

## Non-goals

- No SPECTRA UDP packet framing or message-boundary guessing
- No socket or multicast connections
- No exchange sequence/gap policy
- No A/B merge or deduplication
- No Snapshot/Incremental recovery
- No normalized market events or order-book reconstruction
- No FIX/TWIME session or order sending

## Security

This tool performs no network access. No credentials, secrets, or private connection data are used or stored. All test fixtures are synthetic.
