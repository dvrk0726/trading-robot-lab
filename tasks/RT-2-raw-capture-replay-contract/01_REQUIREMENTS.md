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

`capture_index` is the writer-assigned local order of accepted records in one logical source stream.

It is not:

```text
FAST MsgSeqNum;
exchange packet sequence;
order-log revision;
TCP byte offset;
A/B deduplication key.
```

RT-2 treats payload bytes as opaque. Future decoders may derive exchange-level sequence information without modifying the raw segment.

A replayable stream set is identified by:

```text
(session_id, source_id, channel_id)
```

One broader capture session may contain multiple stream sets. Multi-stream timestamp merge, A/B deduplication and sequencing are deferred to later stages.

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

Hard v1 limits:

```text
header_size <= 4096 bytes;
UTF-8 string <= 128 encoded bytes;
payload_size <= 1 MiB;
finalized segment size <= 64 GiB;
all size/offset arithmetic checked before use.
```

### 4.1 Preamble

The first bytes are exactly:

```text
8-byte magic: ASCII bytes `MXRAWV1\0`;
format_version: u16 = 1;
header_size: u32;
format_flags: u32 = 0 for v1.
```

The reader rejects wrong magic, unsupported version, impossible header size and any non-zero v1 `format_flags` bit.

### 4.2 Segment metadata

After the preamble, serialize these fields in exactly this order:

```text
session_id: 16 opaque caller-provided bytes;
segment_index: u64;
start_capture_index: u64;
created_utc_ns: u64;
clock_domain: u8;
transport: u8;
source_side: u8;
reserved: u8 = 0;
source_id: u64;
channel_id: u64;
configuration_sha256: 32 bytes;
templates_sha256: 32 bytes;
endpoint_fingerprint_sha256: 32 bytes;
feed_group: u16 length + UTF-8 bytes;
endpoint_role: u16 length + UTF-8 bytes;
source_label: u16 length + UTF-8 bytes.
```

Exact enum values:

```text
clock_domain:
  1 = synthetic
  2 = system_monotonic_receive
  3 = hardware_receive

transport:
  0 = synthetic
  1 = udp
  2 = tcp

source_side:
  0 = none
  1 = A
  2 = B
```

Validation rules:

```text
session_id must not be all zero;
created_utc_ns must be non-zero;
source_id and channel_id must be non-zero;
all three SHA-256 values must not be all zero;
feed_group and endpoint_role must be non-empty;
source_label may be empty;
strings must be valid UTF-8, contain no NUL and fit the 128-byte limit;
reserved byte must be zero;
unsupported enum values are errors;
header_size must equal the exact serialized header length.
```

A segment contains one logical source stream only. The endpoint fingerprint is a privacy-safe SHA-256 of caller-normalized endpoint identity. RT-2 fixtures and reports must not require real IP addresses or ports.

### 4.3 Packet record

Each record consists of a fixed 44-byte header, opaque payload and trailing checksum.

Exact record header order:

```text
record_magic: 4 ASCII bytes `REC1`;
record_header_size: u16 = 44;
record_flags: u16;
record_size: u32 = 44 + payload_size + 4;
capture_index: u64;
capture_utc_ns: u64;
capture_monotonic_ns: u64;
payload_size: u32;
payload_crc32c: u32;
payload: payload_size bytes;
record_crc32c: u32 over all record bytes before this final field.
```

`record_magic` is serialized as bytes `0x52 0x45 0x43 0x31`; implementations must not depend on a native multi-character integer literal.

Record flags:

```text
bit 0 = utc_timestamp_valid;
bits 1..15 = 0 in v1.
```

Any unknown v1 record flag is an error.

Required rules:

```text
capture_index increases by exactly one for every accepted record in the stream;
capture_monotonic_ns is non-decreasing within and across segments;
capture_utc_ns is interpreted only when utc_timestamp_valid is set;
record_size exactly matches header + payload + trailing checksum;
payload bytes are preserved exactly;
zero-length payload is valid and has CRC32C value 0;
payload_size may not exceed 1 MiB;
reader checks every length/offset before allocation or seek.
```

### 4.4 Finalization footer

A valid finalized segment ends with an exact 92-byte footer:

```text
8-byte magic: ASCII bytes `MXENDV1\0`;
footer_size: u32 = 92;
footer_flags: u32 = 0;
record_count: u64;
first_capture_index: u64;
last_capture_index: u64;
total_payload_bytes: u64;
data_bytes_before_footer: u64;
content_sha256: 32 bytes over every byte before the footer;
footer_crc32c: u32 over the first 88 footer bytes.
```

The writer must not finalize an empty segment. The reader distinguishes finalized, partial, truncated, corrupt and unsupported files.

A report also computes whole-file SHA-256 for provenance. The stored `content_sha256` is not a whole-file hash and must always use that exact name.

## 5. Checksums

Use:

```text
CRC32C (Castagnoli) for payload, record and footer checks;
SHA-256 for segment content and whole-file provenance.
```

Known vectors are mandatory, including:

```text
CRC32C("") = 0x00000000;
CRC32C("123456789") = 0xE3069283.
```

Reuse/refactor the existing pure C++ SHA-256 implementation only if RT-1 behavior and tests remain unchanged. No checksum failure may be downgraded to success in strict validation.

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
return success only after the complete record is written;
on finalize: write footer, flush, close, then rename within the same directory;
rename target must not already exist;
failed finalize leaves the partial file and an explicit error;
a finalized writer cannot append, finalize twice or reopen the file;
the module never silently deletes forensic partial files.
```

Use same-directory rename for the publish step. Document that filesystem durability beyond successful flush/close/rename is not claimed by RT-2.

The core API receives all serialized metadata explicitly. It must not generate random IDs or read the current time internally.

## 7. Deterministic naming and rotation

Canonical lowercase file name:

```text
<session-id-32hex>_src<source-id-16hex>_ch<channel-id-16hex>_seg<segment-index-16hex>.mxraw
```

The partial path appends `.partial` to the complete final name.

Implement rotation by configured limits only:

```text
max_records_per_segment > 0;
max_segment_bytes includes header, records and footer;
max_segment_bytes <= 64 GiB;
limit must fit header + one legal record + footer.
```

Before a new record would exceed either limit:

1. finalize the current non-empty segment;
2. increment `segment_index` by one;
3. create the next segment with the next `start_capture_index`;
4. write the boundary record to the new segment.

A single record that cannot fit in an empty segment is rejected. Time-based rotation, compression and background upload are deferred.

Identical caller metadata, rotation policy and records must produce byte-identical files and names.

## 8. Reader and validation

The reader streams records; loading the complete file or stream set into memory is forbidden.

Validate at minimum:

```text
actual file size and bounded offsets;
all exact magic/version/header/footer constants;
flags, reserved byte, UTF-8 and enum values;
record magic and exact record size;
payload, record and footer CRC32C;
content SHA-256;
footer counts and byte totals;
contiguous capture_index within a segment;
non-decreasing monotonic timestamp;
no bytes after the footer.
```

Stream-set validation for one `(session_id, source_id, channel_id)` additionally checks:

```text
unique segment_index;
segment index parsed from content and filename;
segments sorted numerically by parsed index;
same session/source metadata and hashes;
contiguous segment_index;
contiguous capture_index across segments;
non-decreasing monotonic timestamp across segments;
no duplicate or missing finalized segment in the supplied set;
partial files reported separately and never replayed as valid data.
```

A directory may contain multiple stream sets. Inspection groups them independently. Replay of a directory with multiple streams requires explicit stream selection or fails as ambiguous; RT-2 does not merge streams by timestamp.

## 9. Deterministic replay

Replay emits validated records through a callback/view API in parsed `segment_index, capture_index` order.

Required behavior:

```text
validation occurs before or during replay;
no network send;
no sleep/pacing;
no payload decode;
callback receives immutable metadata and bounded payload view;
callback failure stops replay and is reported;
summary includes records, payload bytes, first/last index and digest.
```

Canonical `replay_sha256` framing is exactly:

```text
ASCII bytes `MXREPLAY1\0`
session_id (16 bytes)
source_id (u64 little-endian)
channel_id (u64 little-endian)
clock_domain (u8)
transport (u8)
source_side (u8)
configuration_sha256 (32 bytes)
templates_sha256 (32 bytes)
endpoint_fingerprint_sha256 (32 bytes)
feed_group (u16 length + UTF-8 bytes)
endpoint_role (u16 length + UTF-8 bytes)
source_label (u16 length + UTF-8 bytes)
for every record in replay order:
  record_flags (u16 little-endian)
  capture_index (u64 little-endian)
  capture_utc_ns (u64 little-endian)
  capture_monotonic_ns (u64 little-endian)
  payload_size (u32 little-endian)
  payload bytes
```

Segment boundaries and file names are deliberately excluded, so changing only the rotation policy does not change replay identity. Replaying identical logical records and metadata must produce the same digest on Windows and Linux.

## 10. CLI

Provide one or more executables with equivalent commands:

```text
synth   — create deterministic synthetic segments from explicit metadata/seed;
inspect — validate one segment or directory and optionally write JSON;
replay  — validate one selected stream set and compute replay summary/digest.
```

Required CLI behavior:

```text
--help works;
missing/unknown arguments fail non-zero;
invalid values fail before file creation;
no overwrite by default;
strict inspect returns non-zero for trust-affecting findings;
directory replay with multiple streams requires source/channel selection;
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
session id in canonical lowercase hex;
logical source metadata;
stream grouping key;
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
bounded file/header/string/payload sizes;
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
exact format constants and byte order;
writer lifecycle;
file naming and rotation;
stream-set validation;
replay digest framing;
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
generated .mxraw/.partial files outside a tiny authored fixture expressly approved by review;
binaries or build directories.
```

Prefer tests that generate binary segments at runtime. If a golden fixture is required, keep it tiny, synthetic, documented and covered by the hygiene checker.