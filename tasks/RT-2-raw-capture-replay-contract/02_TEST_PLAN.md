# RT-2 Test Plan

## 1. Test rules

All checks must remain active in `Release`; plain `assert(...)` is insufficient when `NDEBUG` is defined.

Tests must:

```text
run on Windows/MSVC and Linux/GCC;
use synthetic/documentation-only metadata and payloads;
create binary files in a temporary build directory;
remove or isolate temporary files between cases;
not depend on test order;
not require network, administrator rights or external services;
not commit generated .mxraw or .partial files;
fail with clear diagnostics.
```

Use deterministic caller-provided IDs, timestamps, hashes and seeds. No test may depend on wall clock, random_device, locale, host endianness or directory enumeration order.

## 2. Serialization primitives

Required cases:

```text
little-endian u16/u32/u64 exact bytes;
read/write round-trip for every primitive;
checked-add overflow rejected;
bounded UTF-8 round-trip;
invalid UTF-8 rejected;
embedded NUL rejected;
128-byte string accepted, 129-byte string rejected;
CRC32C known vector "123456789" = 0xE3069283;
SHA-256 known vectors retained/passed;
unsupported enum rejected;
unknown mandatory flags rejected;
optional unknown flags reported.
```

No serializer test may use raw C++ struct dumps.

## 3. Deterministic header contract

Create a fixed v1 metadata fixture in code and verify:

```text
magic bytes exactly `MXRAWV1\0`;
format version and header size at documented offsets;
all numeric values are little-endian;
16-byte session id preserved;
three SHA-256 fields preserved;
logical feed/role/source strings preserved;
repeated serialization is byte-identical;
Windows and Linux produce the same expected header SHA-256.
```

Hardcode at least one reviewed expected digest or exact byte vector so a writer and reader with the same bug cannot validate each other accidentally.

## 4. Packet record contract

Required positive cases:

```text
single normal payload;
minimum payload allowed by the selected v1 rule;
maximum legal payload;
exact payload bytes including 0x00 and 0xFF;
utc_timestamp_valid on/off;
non-decreasing equal monotonic timestamps accepted;
sequential capture_index values;
record exact-boundary size.
```

Required negative cases:

```text
payload above 1 MiB;
wrong record magic;
record_header_size too small/large;
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
create -> open -> append -> finalize;
partial exists before finalize;
final exists only after successful finalize;
finalized segment cannot append;
finalize twice rejected;
finalize empty segment rejected;
existing partial path rejected;
existing final path rejected;
failed record write does not advance capture_index;
failed finalize leaves partial file and reports failure;
writer destruction does not silently publish an unfinished partial;
no implicit overwrite or delete.
```

Where filesystem fault injection is impractical, isolate file operations behind a narrow testable abstraction and simulate short write/flush/rename failure.

## 6. Footer and segment validation

Positive validation:

```text
correct footer magic/size;
record count and payload total match;
first/last capture index match;
data_bytes_before_footer exact;
content_sha256 exact;
footer CRC32C exact;
whole-file SHA-256 deterministic;
no bytes after footer.
```

Mutation tests must modify one byte/field at a time and prove rejection:

```text
wrong file magic;
unsupported format version;
impossible header size;
invalid string length/UTF-8;
invalid enum or mandatory flag;
wrong footer magic/size;
wrong record count;
wrong first/last index;
wrong payload/byte total;
wrong content SHA-256;
wrong footer CRC32C;
extra byte after footer;
missing footer;
truncation at header, record and footer boundaries.
```

The validator must classify at least:

```text
valid finalized;
partial/unfinalized;
truncated;
corrupt;
unsupported;
I/O error.
```

## 7. Deterministic rotation

Use small limits to force rotation.

Required cases:

```text
rotation exactly at max_records;
rotation before max_segment_bytes would be exceeded;
record fitting exactly to byte boundary accepted;
record exceeding empty-segment limit rejected;
segment_index increments by one;
start_capture_index continues exactly;
file names sort correctly for segment indexes 2 and 10;
repeated run with identical inputs produces byte-identical segment set;
changed payload/metadata changes the relevant hashes.
```

Verify that rotation does not lose, duplicate or reorder the boundary record.

## 8. Multi-segment session validation

Required positive case:

```text
at least three finalized segments;
same session/source metadata;
contiguous segment indexes;
contiguous capture indexes;
numeric segment ordering;
aggregate counts and payload bytes correct.
```

Required negative cases:

```text
duplicate segment_index;
missing segment_index in supplied session;
mixed session_id;
mixed incompatible source metadata;
first capture index not continuous;
overlapping capture index range;
partial file present;
corrupt middle segment;
lexical file order intentionally differs from numeric order.
```

Partial files are reported but never replayed as valid records.

## 9. Replay correctness

Create a deterministic synthetic source containing varied payload lengths and bytes, rotate it into multiple segments, validate and replay.

Verify:

```text
callback count equals written count;
callback order equals capture_index order;
all metadata fields match;
every payload is byte-identical;
first/last indexes and payload total match;
replay_sha256 equals a reviewed expected value;
repeated replay gives the same digest;
Windows and Linux give the same digest;
callback-requested stop returns explicit aborted status;
no callback occurs after validation failure;
no network or sleep occurs.
```

Add a differential test where the expected replay digest is independently computed from the documented canonical digest framing rather than by reusing the production replay function.

## 10. CLI tests

For the selected CLI shape, cover equivalent behavior:

```text
--help / subcommand help;
no args;
unknown command/option;
missing required path/metadata;
invalid session id/hash/number;
invalid rotation limits;
valid deterministic synth;
inspect valid file and directory;
inspect partial/truncated/corrupt input;
replay valid session;
replay corrupt session rejected;
JSON output created;
invalid output path;
existing output/segment not overwritten;
strict exit-code behavior;
console/JSON contains no payload bytes.
```

CLI tests must use portable quoting and null-device handling (`NUL` vs `/dev/null`) or avoid shell redirection entirely.

## 11. JSON report tests

Verify valid JSON parsing and stable required fields:

```text
schema/tool/format versions;
operation;
logical source metadata;
segment indexes and sizes;
content and file hashes;
counts/totals/index bounds;
partial findings;
issue severity/code/source;
replay digest;
overall status.
```

Verify deterministic key/array order for identical inputs. Payload bytes, raw packet dumps, addresses, ports and credentials must be absent.

## 12. Resource-safety tests

Required adversarial cases:

```text
huge header_size;
huge payload_size;
huge record_size;
huge string length;
near-u64 offset overflow;
record count inconsistent with file size;
malformed directory with many unrelated files;
read-only output directory;
rename target race/already exists;
short-read simulation;
short-write simulation;
callback failure.
```

Tests must prove the reader rejects values before unbounded allocation.

## 13. Baseline regression matrix

MiMo must run and report:

```powershell
# RT-2 Windows and Linux tests through CI
cmake -S cpp/moex_raw -B build/moex_raw -A x64
cmake --build build/moex_raw --config Release
ctest --test-dir build/moex_raw -C Release --output-on-failure

# Existing RT-1
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release
ctest --test-dir build/moex_fast -C Release --output-on-failure

# Existing QSH/M10X
cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure

python -m pytest -q
python shared/schemas/validate_examples.py
python tools/check_repository_hygiene.py
```

Expected baseline:

```text
RT-2 task-specific tests: all pass;
RT-1: 6/6 executables pass;
QSH/M10X: 20/20 pass;
Python/contracts: pass;
repository hygiene: pass.
```

If local Linux is unavailable, GitHub Actions Linux evidence is mandatory. Failures may not be hidden or declared unrelated without reviewer approval.