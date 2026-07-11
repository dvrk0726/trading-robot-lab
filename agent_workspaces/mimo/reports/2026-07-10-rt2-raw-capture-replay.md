# RT-2 Implementation Report — Raw Segment Format and Synthetic Capture/Replay

Date: 2026-07-11  
Branch: mimo/issue-18-rt2-raw-capture-replay  
Pull Request: #20  
Implementation commit: `088ceef` (Round 10 code)  
Implementation CI: #68 (run 29143755544): ALL GREEN 7/7  
Executor: MiMo Code

## Summary

Implemented the first offline raw-market-data source-of-truth layer under `cpp/moex_raw/`. Creates versioned immutable `.mxraw` segments with deterministic synthetic data. No network access, no FAST decode, no real capture.

## Round 10 Corrections

Owner Acceptance Round 10 found one reporting defect: `cmd_replay()` never populated `RawSegmentReport.segment_indexes` or `RawSegmentReport.segment_sizes` after `validate_stream_set()`, causing `generate_text_report()` to print "Segments: 0" and JSON output to contain empty arrays.

### 1. Replay report segment_indexes/segment_sizes
After `validate_stream_set()` returns validated, canonically sorted `metas` and `target->segment_paths` (both sorted by segment_index), iterate and populate `report.segment_indexes` from `metas[i].segment_index` and `report.segment_sizes` from `std::filesystem::file_size(target->segment_paths[i])`. Explicit error return on file_size failure — no silent zero.

### 2. End-to-end rotated replay report test
Added test in `test_cli.cpp`: synth 10 records with `--max-records 3 --payload-size 32`, replay text/JSON, assert exactly 4 segments with indexes `[0,1,2,3]`, four positive sizes, `Records: 10`, `Payload Bytes: 320`, non-empty 64-char replay_sha256, `overall_status=valid`.

### 3. Preserved invariants
18 test executables, `/WX`, `-Werror`, Release-active CHECK macros, current scope and non-goals unchanged.

## Round 9 Corrections

All Round 9 blockers addressed:

### 1. Partial-positive hash EOF tests — stage-correct injection
Previous tests truncated the backing data buffer, causing footer seek to fail before reaching the content/file hash stages. Fixed by keeping the full backing buffer and injecting partial-EOF behavior by stage:
- Added `partial_content_hash_eof` and `partial_file_hash_eof` flags to `ScriptedFileHandle`
- First read in stage returns 1 byte (positive partial), second read returns 0 (premature EOF)
- Content-hash test asserts `SegmentStatus::IoError`, exact path, `"IO_ERROR"` code with `"SHA-256"` message, and `content_hash_read_count == 2`
- File-hash test asserts `SegmentStatus::IoError`, exact path, `"IO_ERROR"` code with `"file SHA-256"` message, and `file_hash_read_count == 2`

### 2. End-to-end CLI test with paths containing spaces
Added test creating a directory named `"path with spaces"` and JSON path `"my report.json"`. Runs real `synth`, `inspect --json-out`, and `replay` through the quoted paths. Verifies success, JSON existence, and strict parser acceptance.

### 3. Handoff synchronization
Updated AI_CONTEXT.md, PROJECT_STATE.md, ROADMAP.md, MiMo report, PR description and Issue #18 with actual implementation SHA and CI run.

## Previous Round Corrections

### 1. IFileHandle abstraction
Extended `IFileSystem` with `file_size()`, `open_read()`, `open_write()`. Added `IFileHandle` interface with `read()`, `write()`, `seek()`, `flush()`, `close()`. `DefaultFileHandle` wraps `std::FILE*`. Writer uses `IFileHandle` for all I/O. Reader uses `IFileSystem` for file_size and `IFileHandle` for reads. Production defaults use real filesystem.

### 2. Release-active fault injection tests
Short write, flush failure, close failure, rename failure all produce `Failed` writer state with no finalized `.mxraw`. Short-read mock returns proper error. FILE_TOO_LARGE mock rejects before allocation/read.

### 3. FILE_TOO_LARGE sparse reader test
`MockFileSystem` returns `kMaxSegmentBytes+1` for `file_size()`. `validate_segment()` returns `Unsupported`/`FILE_TOO_LARGE` before opening file or allocating memory.

### 4. Real capture_utc_ns collection
`validate_segment()` collects `first_utc_ns`/`last_utc_ns` from records with `kRecordFlagUtcValid` during the main streaming validation pass. Zero semantics when no valid UTC records (both values = 0). Propagated through `validate_stream_set()`. Used in single-file and stream reports.

### 5. Multi-stream top-level fields
For directory with multiple stream sets, singular top-level stream metadata/index/hash/timestamp/count fields remain empty/zero. Authoritative data only in deterministic `stream_sets[]`. For single stream set, top-level filled completely.

### 6. One-segment aggregate hashes
`content_sha256`/`file_sha256` populated with segment values for single-segment streams.

### 7. Issue path preservation
`RawValidationIssue` gets specific `source`/`path` at creation time in `validate_segment()`. Not replaced by first segment's path by caller.

### 8. JSON strict parser tests
Independent test-only JSON parser in `test_round5.cpp` verifies: numeric types (not strings), arrays, nested `stream_sets`, issue `source`/`path`, escaping quotes/backslashes/newlines/control chars, and deterministic ordering.

### 9. Writer prospective validation
`next_capture_index`, `record_count`, `total_payload_bytes` computed before write. Only committed after successful write. `start_capture_index=UINT64_MAX` negative test proves no mutation/final publication.

### Additional fixes
- **Classification**: `.mxraw.partial` → `Partial` for incomplete files; malformed finalized `.mxraw` → `Corrupt`/`Truncated`/`Unsupported`
- **Rotation fix**: Moved `should_rotate()` check before prospective state computation so `record_count_` reflects post-rotation reset
- **CI update**: Test inventory updated from 16 to 17 for both Windows/MSVC and Linux/GCC

## Context Read

- [x] `AI_CONTEXT.md`
- [x] `PROJECT_STATE.md`
- [x] `ROADMAP.md`
- [x] `SECURITY.md`
- [x] `docs/mimo_developer_workflow.md`
- [x] `docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md`
- [x] `docs/moex/MOEX_REALTIME_ARCHITECTURE.md`
- [x] `decisions/ADR-0003-cpp-qsh-ordlog-data-engine.md`
- [x] `decisions/ADR-0004-moex-vpts-certification-gate.md`
- [x] `tasks/RT-1-fast-config-template-inspector/00_OVERVIEW.md`
- [x] `tasks/RT-2-raw-capture-replay-contract/00_OVERVIEW.md`
- [x] `tasks/RT-2-raw-capture-replay-contract/01_REQUIREMENTS.md`
- [x] `tasks/RT-2-raw-capture-replay-contract/02_TEST_PLAN.md`
- [x] `tasks/RT-2-raw-capture-replay-contract/03_ACCEPTANCE.md`

## What Was Implemented

- C++20/CMake module `cpp/moex_raw/` with library, CLI and 18 test executables
- v1 binary segment contract: `MXRAWV1\0` preamble, segment metadata, `REC1` packet records, `MXENDV1\0` footer
- Pure C++ CRC32C (Castagnoli) with known vectors
- Pure C++ SHA-256 with streaming context for incremental hashing
- Little-endian serialization primitives with checked add/multiply overflow detection
- UTF-8 string validation (128-byte limit, no embedded NUL, proper encoding)
- `IFileHandle`/`IFileSystem` abstraction for deterministic I/O injection
- `RawSegmentWriter` with `.mxraw.partial` -> finalized lifecycle via `IFileHandle`
- Writer prospective state validation (checked arithmetic before write, commit only after success)
- Writer metadata validation before file creation (all IDs, hashes, enums, strings, header size)
- Hard 64 GiB segment cap enforced regardless of rotation policy
- `write_length_string` rejects oversized strings (no silent truncation)
- Bounded streaming reader/validator with checked arithmetic via `IFileHandle`
- Real capture_utc_ns collection from records with kRecordFlagUtcValid (zero semantics when none)
- Canonical filename parsing and filename/content identity comparison
- Stream-set validation: numeric sorting, duplicate/missing detection, full metadata/hash equality, monotonic timestamp across boundaries
- Directory grouping by `(session_id, source_id, channel_id)` with full candidate reporting
- Per-stream independent summaries in text and JSON reports (multi-stream: no singular top-level fields)
- One-segment aggregate hashes populated from segment values
- Issue path preservation at creation time
- Expanded report schema with format_version, source metadata, provenance hashes, per-stream summaries
- Deterministic replay callback with `MXREPLAY1\0` canonical digest (single streaming SHA-256 context)
- `replay_from_stream_set()` for explicit session selection
- `replay_from_directory()` deprecated as safe wrapper that fails on any ambiguity
- CLI: `moex-raw synth`, `moex-raw inspect`, `moex-raw replay` with strict numeric/hex validation
- Status classification: unsupported, partial (`.mxraw.partial` only), truncated, corrupt, I/O error
- Portable 64-bit file position
- Independent golden MXREPLAY1 digest test
- Strict JSON parser test with type/array/escape/ordering verification

## What Was Intentionally Not Implemented

- No socket, multicast or real UDP/TCP capture
- No FAST binary decode (deferred to RT-3)
- No exchange sequence extraction from payload
- No A/B deduplication or recovery
- No book building
- No database/object-storage integration
- No pcap/pcapng dependency
- No FIX/TWIME or order sending
- No production enablement

## Files Changed (Round 5)

```text
M  cpp/moex_raw/include/moex_raw/raw_segment.hpp         (IFileHandle, IFileSystem extended, validate_segment/stream_set signatures)
M  cpp/moex_raw/include/moex_raw/raw_replay.hpp          (replay_stream IFileSystem* param)
M  cpp/moex_raw/src/raw_segment_writer.cpp               (IFileHandle, prospective validation)
M  cpp/moex_raw/src/raw_segment_reader.cpp               (IFileSystem, UTC collection, classification, issue paths)
M  cpp/moex_raw/src/raw_replay.cpp                       (IFileSystem threading)
M  cpp/moex_raw/src/main.cpp                             (multi-stream top-level, UTC bounds, one-segment hashes, issue paths)
M  cpp/moex_raw/tests/test_round3.cpp                    (MockFileSystem extended for IFileHandle)
A  cpp/moex_raw/tests/test_round5.cpp                    (fault injection, FILE_TOO_LARGE, UINT64_MAX, classification, UTC, JSON parser)
M  cpp/moex_raw/CMakeLists.txt                           (+test_round5)
M  .github/workflows/ci.yml                              (test inventory 16→17)
```

## Local Test Results (Round 10)

```text
RT-2 (18/18 passed, Windows Release):
  test_crc32c .............. Passed
  test_endian .............. Passed
  test_strings ............. Passed
  test_header_contract .... Passed
  test_record_contract .... Passed
  test_footer_validation .. Passed
  test_writer_lifecycle .... Passed
  test_rotation ............ Passed
  test_stream_set .......... Passed
  test_replay .............. Passed
  test_cli ................. Passed
  test_json_report ......... Passed
  test_resource_safety ..... Passed
  test_golden_record_layout  Passed
  test_content_sha256_e2e .. Passed
  test_round3 .............. Passed
  test_round5 .............. Passed
  test_round6 .............. Passed

RT-1 (6/6 passed, no regression)
QSH/M10X (20/20 passed, no regression)
Python (3/3 passed)
Hygiene: PASS
```

## GitHub Actions

```text
CI #68 (run 29143755544): ALL GREEN — 7/7 jobs passed
  C++ MOEX RAW Windows/MSVC (18 tests): PASSED
  C++ MOEX RAW Linux/GCC (18 tests): PASSED
  C++ MOEX FAST inspector Windows (6 tests): PASSED
  C++ MOEX FAST inspector Linux (6 tests): PASSED
  C++ QSH M10X regression (20 tests): PASSED
  Python tests and contracts: PASSED
  Repository hygiene: PASSED
```

## Safety and Architecture Check

- [x] No direct work in `main`
- [x] No force-push
- [x] No auto-merge or merge
- [x] No broker/exchange connection added
- [x] No live trading enabled
- [x] Every OrderIntent still requires RiskEngine
- [x] Trading Lab still cannot send real orders
- [x] QSH semantics unchanged
- [x] `strategy_ready` gating not weakened
- [x] No unrelated architecture expansion
- [x] `-Werror` / `/WX` preserved
- [x] Release CHECK macros active

## Final Handoff

```text
Issue moved to: READY_FOR_REVIEW
PR created: yes (PR #20)
Next task started: no
Merge performed: no
```

## Final Status

```text
READY_FOR_REVIEW
```
