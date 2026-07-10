# RT-2 Test Plan

## 1. Test rules

All checks must remain active in `Release`; plain `assert(...)` is insufficient when `NDEBUG` is defined.

Tests must:

```text
run on Windows/MSVC and Linux/GCC;
use synthetic/documentation-only metadata and payloads;
create binary files in a unique temporary build directory;
clean or isolate files between cases;
not depend on test order;
not require network, administrator rights or external services;
not commit generated .mxraw or .partial files;
fail with clear diagnostics.
```

Use explicit deterministic IDs, timestamps, hashes and seeds. No test may depend on wall clock, `random_device`, locale, host endianness or directory enumeration order.

## 2. Serialization primitives

Required cases:

```text
little-endian u16/u32/u64 exact bytes;
read/write round-trip for every primitive;
checked addition/multiplication overflow rejected;
bounded UTF-8 round-trip;
invalid UTF-8 rejected;
embedded NUL rejected;
128-byte string accepted, 129-byte string rejected;
CRC32C("") = 0x00000000;
CRC32C("123456789") = 0xE3069283;
SHA-256 known vectors retained/passed;
unsupported enum rejected;
non-zero reserved byte rejected;
non-zero format/footer flags rejected;
record flag bits 1..15 rejected.
```

No serializer test may use raw C++ struct dumps.

## 3. Deterministic header contract

Create one fixed v1 metadata fixture in code and verify:

```text
magic bytes exactly `MXRAWV1\0`;
format_version = 1;
header_size equals exact serialized length and is <= 4096;
format_flags = 0;
all numeric fields are little-endian;
16-byte non-zero session id preserved;
segment/start indexes and created_utc_ns preserved;
clock/transport/side enum byte values preserved;
source_id and channel_id preserved;
three non-zero SHA-256 values preserved;
feed_group/endpoint_role/source_label framing preserved;
repeated serialization is byte-identical;
Windows and Linux produce the same expected header SHA-256.
```

Negative header cases:

```text
all-zero session id;
zero created_utc_ns;
zero source_id/channel_id;
all-zero required SHA-256;
empty feed_group or endpoint_role;
wrong header_size;
header_size above 4096;
unsupported enum;
invalid string length/UTF-8/NUL.
```

Hardcode at least one reviewed expected digest or exact byte vector so a writer and reader with the same defect cannot validate each other accidentally.

## 4. Packet record contract

Required positive cases:

```text
record magic exact bytes `REC1`;
record_header_size = 44;
zero-length payload with CRC32C = 0;
single normal payload;
maximum legal 1 MiB payload;
exact payload bytes including 0x00 and 0xFF;
utc_timestamp_valid off with capture_utc_ns ignored;
utc_timestamp_valid on;
equal/non-decreasing monotonic timestamps accepted;
sequential capture_index values;
record_size = 44 + payload_size + 4;
record fitting exact segment-byte boundary.
```

Required negative cases:

```text
payload above 1 MiB;
wrong record magic;
record_header_size not 44;
unknown record flag bit;
record_size smaller/larger than actual;
payload_size inconsistent with record_size;
payload CRC32C mismatch;
record CRC32C mismatch;
capture_index duplicate, backward or skipped;
monotonic timestamp decreases;
truncated fixed header;
truncated payload;
missing trailing record checksum;
checked size arithmetic overflow.
```

## 5. Writer lifecycle

Test each state transition independently:

```text
Created -> Open -> append -> Finalized;
Created/Open -> Failed;
partial exists before finalize;
final exists only after successful finalize;
finalized segment cannot append or reopen;
finalize twice rejected;
finalize empty segment rejected;
existing partial path rejected;
existing final path rejected;
failed record write does not advance capture_index or totals;
failed finalize leaves partial file and explicit failure;
writer destruction never silently publishes unfinished partial;
no implicit overwrite or delete;
core API output is unchanged when wall clock/environment changes.
```

Where filesystem fault injection is impractical, isolate file operations behind a narrow testable abstraction and simulate short write, flush, close and rename failures.

## 6. Footer and segment validation

Positive validation:

```text
footer magic exact bytes `MXENDV1\0`;
footer_size = 92;
footer_flags = 0;
record count and payload total match;
first/last capture index match;
data_bytes_before_footer exact;
content_sha256 exact over bytes before footer;
footer CRC32C exact over first 88 footer bytes;
whole-file SHA-256 deterministic;
no bytes after footer.
```

Mutation tests change one byte/field at a time and prove rejection:

```text
wrong file magic;
unsupported format version;
impossible header size;
invalid enum/reserved/flags;
invalid string length/UTF-8;
wrong footer magic/size/flags;
wrong record count;
wrong first/last index;
wrong payload/byte total;
wrong content SHA-256;
wrong footer CRC32C;
extra byte after footer;
missing footer;
truncation at header, record, payload, record-checksum and footer boundaries.
```

Validator classifications must include:

```text
valid finalized;
partial/unfinalized;
truncated;
corrupt;
unsupported;
I/O error.
```

## 7. Deterministic naming and rotation

Verify canonical lowercase name:

```text
<session-id-32hex>_src<source-id-16hex>_ch<channel-id-16hex>_seg<segment-index-16hex>.mxraw
```

Required cases:

```text
partial path appends `.partial`;
uppercase/malformed/non-canonical names rejected or explicitly reported;
content identity matches filename identity;
rotation exactly at max_records;
rotation before max_segment_bytes would be exceeded;
max_segment_bytes above 64 GiB rejected;
limit too small for header + one record + footer rejected;
record fitting exact byte boundary accepted;
record exceeding empty-segment limit rejected;
segment_index increments by one;
start_capture_index continues exactly;
indexes 2 and 10 are parsed numerically, not trusted from enumeration order;
repeated run with identical input produces byte-identical file set;
changed payload/metadata changes relevant hashes.
```

Rotation must not lose, duplicate or reorder the boundary record.

## 8. Multi-segment stream-set validation

Positive case for one `(session_id, source_id, channel_id)`:

```text
at least three finalized segments;
same logical source metadata/hashes;
contiguous segment indexes;
contiguous capture indexes;
non-decreasing monotonic timestamps across boundaries;
numeric parsed ordering;
aggregate counts and payload bytes correct.
```

Negative cases:

```text
duplicate segment_index;
missing segment_index;
filename/content index mismatch;
mixed session_id;
mixed source_id or channel_id;
mixed incompatible source metadata or hashes;
first capture index not continuous;
overlapping capture range;
monotonic timestamp decreases across segments;
partial file present;
corrupt middle segment;
lexical/directory order intentionally differs from parsed index order.
```

Directory grouping tests:

```text
two valid independent stream sets are reported separately;
inspection succeeds with separate summaries;
replay without stream selection fails as ambiguous;
replay with explicit source_id/channel_id selects only that stream;
partial files are reported and never replayed.
```

## 9. Replay correctness

Create deterministic synthetic records with varied payload lengths/bytes and rotate them into multiple segments.

Verify:

```text
callback count equals accepted record count;
callback order equals capture_index order;
all metadata fields match;
every payload is byte-identical;
first/last indexes and payload total match;
replay_sha256 equals a reviewed expected value;
repeated replay gives the same digest;
Windows and Linux give the same digest;
changing only rotation limits leaves replay_sha256 unchanged;
changing logical metadata/record metadata/payload changes replay_sha256;
callback-requested stop returns explicit aborted status;
no callback occurs after validation failure;
no network or sleep occurs.
```

Add an independent test implementation of the documented `MXREPLAY1\0` framing. It must not call the production digest-framing helper.

## 10. CLI tests

For the selected CLI shape, cover equivalent behavior:

```text
--help and subcommand help;
no args;
unknown command/option;
missing required path/metadata;
invalid session id/hash/enum/number;
invalid rotation limits;
valid deterministic synth;
inspect valid single file;
inspect directory with one and multiple stream sets;
inspect partial/truncated/corrupt input;
replay selected valid stream;
ambiguous directory replay rejected without stream selector;
replay corrupt stream rejected;
JSON output created;
invalid output path;
existing output/segment not overwritten;
strict exit-code behavior;
console/JSON contains no payload bytes/private endpoint data.
```

CLI tests must use portable process execution and null-device handling (`NUL` vs `/dev/null`) or avoid shell redirection entirely.

## 11. JSON report tests

Verify parseable valid JSON and stable required fields:

```text
schema/tool/format versions;
operation;
session id canonical lowercase hex;
logical source metadata and stream grouping key;
segment indexes and sizes;
content/file hashes;
counts/totals/index/timestamp bounds;
partial findings;
issue severity/code/source;
replay digest;
overall status.
```

Verify deterministic key/array order. Payload bytes, raw dumps, addresses, ports and credentials must be absent.

## 12. Resource-safety tests

Required adversarial cases:

```text
file larger than 64 GiB represented by sparse/mock input;
huge header_size;
huge payload_size/record_size/string length;
near-u64 offset overflow;
record count inconsistent with file size;
many unrelated directory entries;
read-only/output failure;
rename target race/already exists;
short-read simulation;
short-write/flush/close/rename simulation;
callback failure.
```

Tests must prove rejection before unbounded allocation.

## 13. Baseline regression matrix

Windows/local:

```powershell
cmake -S cpp/moex_raw -B build/moex_raw -A x64
cmake --build build/moex_raw --config Release
ctest --test-dir build/moex_raw -C Release --output-on-failure

cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release
ctest --test-dir build/moex_fast -C Release --output-on-failure

cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure

python -m pytest -q
python shared/schemas/validate_examples.py
python tools/check_repository_hygiene.py
```

Linux CI:

```bash
cmake -S cpp/moex_raw -B build/moex_raw -DCMAKE_BUILD_TYPE=Release
cmake --build build/moex_raw --parallel 2
ctest --test-dir build/moex_raw --output-on-failure

cmake -S cpp/moex_fast -B build/moex_fast -DCMAKE_BUILD_TYPE=Release
cmake --build build/moex_fast --parallel 2
ctest --test-dir build/moex_fast --output-on-failure
```

Expected:

```text
RT-2 task tests: all pass;
RT-1: 6/6 executables pass;
QSH/M10X: 20/20 pass;
Python/contracts: pass;
repository hygiene: pass.
```

If local Linux is unavailable, GitHub Actions Linux evidence is mandatory. Failures may not be hidden or declared unrelated without reviewer approval.