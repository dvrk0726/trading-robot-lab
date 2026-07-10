# moex_raw — MXRaw Segment Format and Synthetic Capture/Replay

Version: 0.1.0  
Status: implementation  
Issue: #18

## Overview

Offline raw-market-data source-of-truth layer for the MOEX realtime contour. Creates versioned immutable `.mxraw` segments with deterministic synthetic data. No network, no FAST decode, no real capture.

## Binary Contract — v1

### File Layout

```text
[header: preamble + segment metadata]
[record 0]
[record 1]
...
[record N-1]
[footer: finalization]
```

### Constants

```text
Magic:       MXRAWV1\0 (8 bytes)
Format ver:  1 (u16)
Record magic: REC1 (4 bytes)
Footer magic: MXENDV1\0 (8 bytes)
Replay magic: MXREPLAY1\0 (10 bytes)
Record header: 44 bytes
Footer:      92 bytes
Max payload: 1 MiB
Max header:  4096 bytes
Max string:  128 UTF-8 bytes
Max segment: 64 GiB
```

### Byte Order

All integers are fixed-width unsigned, little-endian. No raw C++ struct dumps.

### Enums

```text
clock_domain: 1=synthetic, 2=system_monotonic_receive, 3=hardware_receive
transport:    0=synthetic, 1=udp, 2=tcp
source_side:  0=none, 1=A, 2=B
```

### Record Flags

```text
bit 0 = utc_timestamp_valid
bits 1..15 = 0 in v1
```

### Checksums

```text
CRC32C (Castagnoli): payload, record, footer
SHA-256: content (pre-footer), whole-file provenance, replay digest
```

Known vectors:
- CRC32C("") = 0x00000000
- CRC32C("123456789") = 0xE3069283

## Writer Lifecycle

```text
Created -> Open -> Finalized
Created/Open -> Failed
```

- Writes to `<name>.mxraw.partial`
- Finalize: footer, flush, close, same-directory rename
- No append after finalize
- No empty segment finalize
- No overwrite of existing files

## File Naming

```text
<session-id-32hex>_src<source-id-16hex>_ch<channel-id-16hex>_seg<segment-index-16hex>.mxraw
```

## Rotation

Deterministic by record count or byte limit. Segment index increments by one. Capture index continues across segments.

## Stream Set

Keyed by `(session_id, source_id, channel_id)`. Validates:
- Contiguous segment indexes
- Contiguous capture indexes across segments
- Non-decreasing monotonic timestamps
- Same session/source metadata

## Replay

Emits validated records through callback. Canonical `replay_sha256` uses `MXREPLAY1\0` framing excluding segment boundaries.

## CLI

```text
moex-raw synth   --out <dir> [--session/--source/--channel/--records/--segments/--payload-size]
moex-raw inspect --input <path> [--json-out] [--strict]
moex-raw replay  --input <dir> [--source/--channel] [--json-out]
```

## Build

```powershell
cmake -S cpp/moex_raw -B build/moex_raw -A x64
cmake --build build/moex_raw --config Release
ctest --test-dir build/moex_raw -C Release --output-on-failure
```

## Limitations

- Synthetic data only; no real network capture
- No FAST decode
- No A/B deduplication or recovery
- No book building
- No database/object-storage integration
- Single-threaded writer

## Security

- No network connections
- No credentials or private data
- Generated `.mxraw` files outside Git (build directory)
- Synthetic metadata only
