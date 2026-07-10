# RT-2 Implementation Report — Raw Segment Format and Synthetic Capture/Replay

Date: 2026-07-10  
Branch: mimo/issue-18-rt2-raw-capture-replay  
Pull Request: #20  
Implementation commit: `8e9a61ef26d99a2b47b2d05fa354952797e46ec2`  
Executor: MiMo Code

## Summary

Implemented the first offline raw-market-data source-of-truth layer under `cpp/moex_raw/`. Creates versioned immutable `.mxraw` segments with deterministic synthetic data. No network access, no FAST decode, no real capture.

## Round 3 Corrections

All 10 Round 3 blockers addressed:

1. **Complete stream-set validation**: Added `parse_canonical_filename()` for strict canonical filename parsing. `validate_stream_set()` now parses filenames, compares filename identity with content identity, sorts numerically by parsed segment index, rejects duplicate/missing indexes, compares all metadata fields (feed_group, endpoint_role, source_label, clock_domain, transport, source_side) and all three provenance hashes, and enforces monotonic timestamp across segment boundaries.

2. **Per-stream directory inspection**: `cmd_inspect()` now generates independent `RawStreamSummary` entries per stream set in both text and JSON reports. Each stream has its own metadata, counts, hashes and status. Independent streams are not merged.

3. **Strict replay ambiguity**: `matches.size() != 1` after selectors now always fails as ambiguous (same-session different source/channel included). `replay_from_directory()` is deprecated but kept as a safe wrapper that fails on any ambiguity.

4. **Partial file blocks replay**: `.mxraw.partial` files cause non-zero replay exit. `inspect --strict` fails on partial files. Non-strict inspect reports partial as warning.

5. **Writer metadata validation**: `validate_metadata()` called before file creation in `open()`. Validates non-zero IDs/hashes/time, supported enums, UTF-8/no-NUL/128-byte strings, non-empty required strings, exact header <=4096. `write_length_string()` returns false on oversized strings (no silent truncation).

6. **Hard 64 GiB cap**: Enforced regardless of `max_segment_bytes=0`. Reader rejects files above 64 GiB. Checked arithmetic for segment/capture indexes, counts, bytes. CLI rejects negative/signed/whitespace numeric strings.

7. **Status classification fixed**: Unsupported version returns `SegmentStatus::Unsupported`. Footer magic checked at correct position (start of 92-byte footer, not EOF-8). Distinguishes unsupported, partial, truncated, corrupt and I/O error.

8. **Expanded report schema**: Added `format_version`, full source metadata (clock_domain, transport, source_side, all three hashes), per-stream summaries (`stream_sets[]`), actual timestamp bounds, issue source/path.

9. **Replay summary digest**: `ReplayResult.summary.replay_sha256` populated via single streaming SHA-256 context in `replay_stream()`. Hard-coded independently derived MXREPLAY1 golden digest test. Rotation-invariance and metadata/payload change detection tests.

10. **Fault/resource tests**: Filename mismatch, metadata/hash mismatch, duplicate/missing indexes, cross-segment monotonic, same-session multiple channels/sources, valid+partial semantics, 64 GiB rejection, checked arithmetic, callback stop, portable handling.

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

- C++20/CMake module `cpp/moex_raw/` with library, CLI and 16 test executables
- v1 binary segment contract: `MXRAWV1\0` preamble, segment metadata, `REC1` packet records, `MXENDV1\0` footer
- Pure C++ CRC32C (Castagnoli) with known vectors
- Pure C++ SHA-256 with streaming context for incremental hashing
- Little-endian serialization primitives with checked add/multiply overflow detection
- UTF-8 string validation (128-byte limit, no embedded NUL, proper encoding)
- `RawSegmentWriter` with `.mxraw.partial` -> finalized lifecycle
- Writer metadata validation before file creation (all IDs, hashes, enums, strings, header size)
- Hard 64 GiB segment cap enforced regardless of rotation policy
- `write_length_string` rejects oversized strings (no silent truncation)
- Bounded streaming reader/validator with checked arithmetic
- Canonical filename parsing and filename/content identity comparison
- Stream-set validation: numeric sorting, duplicate/missing detection, full metadata/hash equality, monotonic timestamp across boundaries
- Directory grouping by `(session_id, source_id, channel_id)` with full candidate reporting
- Per-stream independent summaries in text and JSON reports
- Expanded report schema with format_version, source metadata, provenance hashes, per-stream summaries
- Deterministic replay callback with `MXREPLAY1\0` canonical digest (single streaming SHA-256 context)
- `replay_from_stream_set()` for explicit session selection
- `replay_from_directory()` deprecated as safe wrapper that fails on any ambiguity
- CLI: `moex-raw synth`, `moex-raw inspect`, `moex-raw replay` with strict numeric/hex validation
- Status classification: unsupported, partial, truncated, corrupt, I/O error
- Portable 64-bit file position
- Independent golden MXREPLAY1 digest test

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

## Files Changed

```text
M  cpp/moex_raw/include/moex_raw/strings.hpp             (Round 3: write_length_string returns bool)
M  cpp/moex_raw/include/moex_raw/raw_types.hpp           (Round 3: ParsedFilename, RawStreamSummary, report fields)
M  cpp/moex_raw/include/moex_raw/raw_replay.hpp          (Round 3: deprecated replay_from_directory comment)
M  cpp/moex_raw/src/strings.cpp                          (Round 3: write_length_string overflow check)
M  cpp/moex_raw/src/raw_types.cpp                        (Round 3: parse_canonical_filename, serialize_header checks)
M  cpp/moex_raw/src/raw_segment_writer.cpp               (Round 3: validate_metadata, hard 64 GiB cap, checked arithmetic)
M  cpp/moex_raw/src/raw_segment_reader.cpp               (Round 3: filename parsing, sorting, metadata/hash equality, monotonic, classification)
M  cpp/moex_raw/src/raw_replay.cpp                       (Round 3: streaming SHA-256 in replay_stream, strict ambiguity)
M  cpp/moex_raw/src/raw_report.cpp                       (Round 3: per-stream summaries, expanded schema)
M  cpp/moex_raw/src/main.cpp                             (Round 3: per-stream inspect, strict replay, partial blocking, number validation)
M  cpp/moex_raw/tests/test_resource_safety.cpp           (Round 3: canonical filename for corrupt test)
A  cpp/moex_raw/tests/test_round3.cpp                    (Round 3: golden digest, filename mismatch, sorting, dup/missing, ambiguity, partial, metadata validation, 64 GiB, classification, report schema, callback stop)
M  cpp/moex_raw/CMakeLists.txt                           (Round 3: +test_round3)
```

## Commands Run

```powershell
# Build RT-2
cmake -S cpp/moex_raw -B build/moex_raw -A x64
cmake --build build/moex_raw --config Release --parallel 2

# Test RT-2
ctest --test-dir build/moex_raw -C Release --output-on-failure

# Build and test RT-1 (regression)
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release --parallel 2
ctest --test-dir build/moex_fast -C Release --output-on-failure

# Build and test QSH/M10X (regression)
cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release --parallel 2
ctest --test-dir build/qsh_ingest -C Release --output-on-failure

# Python and hygiene
python -m pytest -q
python tools/check_repository_hygiene.py
```

## Local Test Results (Round 3)

```text
RT-2 (16/16 passed, Windows Release):
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

RT-1 (6/6 passed, no regression):
  test_template_parser ..... Passed
  test_config_parser ....... Passed
  test_provenance .......... Passed
  test_deterministic_report  Passed
  test_resource_safety ..... Passed
  test_cli ................. Passed

QSH/M10X (20/20 passed, no regression)

Python (3/3 passed)
Shared schemas (5/5 valid)
Repository hygiene: PASS (276 files)
```

## Compiler

```text
MSVC 19.42.34436.0
Windows SDK 10.0.22621.0
CMake 4.3
C++20 / Release / x64
```

## GitHub Actions

```text
CI #50 (run 29110786126): ALL GREEN — 7/7 jobs passed
  C++ MOEX RAW Windows/MSVC (16 tests): PASSED
  C++ MOEX RAW Linux/GCC (16 tests): PASSED
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
