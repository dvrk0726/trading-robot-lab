# RT-2 Requirements

## 1. Build and placement

Use C++20 and CMake. Prefer `cpp/moex_raw/` with a small library, CLI and tests unless repository inspection proves a cleaner compatible location.

Required platforms:

```text
Windows / MSVC Release;
Linux / GCC Release;
no QuickFAST;
no pcap library;
no database or Python runtime dependency;
no network dependency.
```

Existing RT-1 and QSH/M10X behavior must remain unchanged.

## 2. Public value types

Create small value types equivalent to:

```text
RawSessionMetadata
RawSourceMetadata
RawPacketRecord
RawSegmentRotationPolicy
RawSegmentWriter
RawSegmentReader
RawReplaySummary
RawValidationIssue
RawSegmentReport
```

Avoid dynamic polymorphism unless justified. Expected input errors must use explicit result/error values or caught exceptions translated at the API/CLI boundary.

## 3. Required semantic separation

`capture_index` is the writer-assigned local order of accepted records in one session.

It is not:

```text
FAST MsgSeqNum;
exchange packet sequence;
order-log revision;
TCP byte offset;
A/B deduplication key.
```

RT-2 treats payload bytes as opaque. Future decoders may derive exchange-level sequence information without modifying the raw segment.

## 4. v1 segment layout

Use a custom binary format with extension `.mxraw` and temporary extension `.mxraw.partial`.

Logical layout:

```text
[preamble + segment metadata]
[record 0]
[record 1]
...
[record N-1]
[finalization footer]
```

All integer fields are fixed-width unsigned integers encoded little-endian. Signed native time types, bitfields, compiler packing and raw `sizeof(struct)` persistence are forbidden.

### 4.1 Preamble

The first bytes must contain:

```text
8-byte magic: ASCII "MXRAWV1" followed by NUL;
format_version: u16 = 1;
header_size: u32;
format_flags: u32;
```

The reader must reject wrong magic, unsupported version, impossible header size and unknown mandatory flags.

### 4.2 Segment metadata

The v1 header must serialize fields in a documented fixed order:

```text
session_id: 16 opaque caller-provided bytes;
segment_index: u64;
start_capture_index: u64;
created_utc_ns: u64;
clock_domain: enum;
transport: enum { synthetic, udp, tcp };
source_side: enum { none, A, B };
source_id: u64;
channel_id: u64;
configuration_sha256: 32 bytes;
templates_sha256: 32 bytes;
endpoint_fingerprint_sha256: 32 bytes;
feed_group: bounded UTF-8;
endpoint_role: bounded UTF-8;
source_label: bounded UTF-8;
```

Strings use `u16 length + exact UTF-8 bytes` in the listed order. Maximum encoded length per string is 128 bytes. Invalid UTF-8, embedded NUL, overlong values and unsupported enum values are errors.

A segment represents one logical source/channel. Different source identities use different segments.

The endpoint fingerprint is a privacy-safe SHA-256 of caller-normalized endpoint identity. RT-2 fixtures and reports must not require real IP addresses or ports.

### 4.3 Packet record

Each record must contain a documented fixed header, opaque payload and checksum:

```text
record_magic: u32 constant for v1;
record_header_size: u16;
record_flags: u16;
record_size: u32;
capture_index: u64;
capture_utc_ns: u64;
capture_monotonic_ns: u64;
payload_size: u32;
payload_crc32c: u32;
payload: payload_size bytes;
record_crc32c: u32 over the serialized record excluding record_crc32c itself.
```

Required rules:

```text
capture_index strictly increases by one for every accepted record;
capture_monotonic_ns is non-decreasing;
capture_utc_ns is informational and may move because of clock correction;
record_size must exactly match header + payload + trailing checksum;
payload bytes are preserved exactly;
payload_size zero is allowed only if explicitly documented and tested;
hard payload ceiling is 1 MiB;
reader never allocates from untrusted length before checking all bounds.
```

Record flags must include an explicit `utc_timestamp_valid` bit. Unknown mandatory record flags are rejected; unknown optional flags are reported.

### 4.4 Finalization footer

A valid finalized segment ends with:

```text
8-byte magic: ASCII "MXENDV1" followed by NUL;
footer_size: u32;
footer_flags: u32;
record_count: u64;
first_capture_index: u64;
last_capture_index: u64;
total_payload_bytes: u64;
data_bytes_before_footer: u64;
content_sha256: 32 bytes over every byte before the footer;
footer_crc32c: u32 over the serialized footer excluding footer_crc32c.
```

The writer must not finalize an empty segment. The reader must distinguish finalized, partial, truncated, corrupt and unsupported files.

A report also computes whole-file SHA-256 for provenance. The stored `content_sha256` is not a whole-file hash and must be named accordingly.

## 5. Checksums

Use:

```text
CRC32C for per-record and footer corruption localization;
SHA-256 for segment content and file provenance.
```

Implementations must use known test vectors. Reuse/refactor the existing pure C++ SHA-256 implementation only if RT-1 behavior and tests remain unchanged.

No checksum failure may be downgraded to success in strict validation.

## 6. Writer lifecycle

Required states:

```text
Created -> Open -> Finalized
Created/Open -> Failed
```

Rules:

```text
write only to `<final-name>.partial`;
create without overwriting an existing partial or final path;
write header before accepting records;
return success only after the full record is written;
on finalize: write footer, flush, close, then rename within the same directory;
rename target must not already exist;
failed finalize leaves the partial file and an explicit error;
a finalized writer cannot append, finalize twice or reopen the file;
the module never silently deletes forensic partial files.
```

Use same-directory rename for the publish step. Document that OS/filesystem durability beyond successful flush/close/rename is not claimed by RT-2.

## 7. Deterministic rotation

Implement rotation by configured limits only:

```text
max_records_per_segment > 0;
max_segment_bytes large enough for header, one legal record and footer.
```

Before writing a record that would exceed a limit:

1. finalize the current non-empty segment;
2. increment `segment_index`;
3. create the next segment with the next `start_capture_index`;
4. write the record there.

A single record that cannot fit in an empty segment is rejected. Time-based rotation, compression and background upload are deferred.

Final naming must be deterministic from caller-provided session identity and segment index. The core writer must not inject random IDs or current time into serialized output.

## 8. Reader and validation

The reader must stream records; loading the complete file or payload set into memory is forbidden.

Validate at minimum:

```text
file size and bounded offsets;
magic/version/header/footer sizes;
UTF-8 and enum values;
record magic and exact record size;
payload and record CRC32C;
footer CRC32C;
content SHA-256;
footer counts and byte totals;
contiguous capture_index within a segment;
non-decreasing monotonic timestamp;
no bytes after the footer.
```

Directory/session validation additionally checks:

```text
unique segment_index;
segments sorted numerically, never lexically by accident;
same session_id and compatible source metadata;
contiguous capture_index across segments;
no duplicate or missing finalized segment in the supplied set;
partial files reported separately and never replayed as valid data.
```

## 9. Deterministic replay

Replay emits validated records through a callback/view API in `segment_index, capture_index` order.

Required behavior:

```text
validation occurs before or during replay;
no network send;
no sleep/pacing in RT-2;
no payload decode;
callback receives metadata plus a bounded payload view;
callback failure stops replay and is reported;
replay summary includes records, payload bytes, first/last index and digest.
```

Define `replay_sha256` over a documented canonical serialization of session/source identity, capture metadata and payload bytes. Replaying identical valid segments must produce the identical digest on Windows and Linux.

## 10. CLI

Provide one or more executables with equivalent commands:

```text
synth   — create deterministic synthetic segments from explicit metadata/seed;
inspect — validate one segment or a directory/session and optionally write JSON;
replay  — validate and compute deterministic replay summary/digest.
```

Required CLI behavior:

```text
--help works;
missing/unknown arguments fail non-zero;
invalid values fail before file creation;
no overwrite by default;
strict inspect returns non-zero for warnings that affect trust;
console and JSON never include payload bytes;
expected file errors do not produce uncaught exception text.
```

Exact executable/subcommand names may be finalized after repository inspection.

## 11. JSON report

Document a stable JSON schema containing at minimum:

```text
schema_version;
tool_version;
operation;
input paths/file names;
format version;
session id in canonical non-secret form;
source logical metadata;
segment indexes and file sizes;
content and whole-file SHA-256;
record counts and payload byte totals;
first/last capture indexes;
timestamp bounds;
partial-file findings;
issues with severity/code/source;
replay_sha256 when replayed;
overall_status: valid / warning / invalid.
```

Do not include payload bytes, raw packet dumps, private addresses, ports or credentials.

## 12. Resource and error safety

Hard requirements:

```text
bounded string/header/payload sizes;
checked integer addition and multiplication;
no offset wraparound;
no allocation directly from unchecked file values;
no uncaught expected-input exceptions;
no append to finalized files;
no silent partial write success;
no silent skipped records;
no success after checksum mismatch or truncation.
```

## 13. Repository hygiene and documentation

Add a module README documenting:

```text
format and byte order;
writer lifecycle;
rotation;
validation and replay digest;
build and CLI examples;
known limitations;
no-network statement;
private/raw-data handling.
```

Never commit:

```text
pcap/pcapng;
raw MOEX payloads;
official private configuration/templates;
real addresses/ports/login data;
generated .mxraw/.partial files outside tiny authored fixtures expressly approved by review;
binaries or build directories.
```

Prefer tests that generate binary segments at runtime. If a golden fixture is required, keep it tiny, synthetic, documented and covered by the hygiene checker.