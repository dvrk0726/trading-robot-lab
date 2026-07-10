# RT-2 Implementation Report — Raw Segment Format and Synthetic Capture/Replay

Date: 2026-07-10  
Branch: mimo/issue-18-rt2-raw-capture-replay  
Pull Request: #20  
Implementation commit: (Round 2 — pending push)  
Executor: MiMo Code

## Summary

Implemented the first offline raw-market-data source-of-truth layer under `cpp/moex_raw/`. Creates versioned immutable `.mxraw` segments with deterministic synthetic data. No network access, no FAST decode, no real capture.

## Round 2 Corrections

All 7 Round 2 blockers addressed:

1. **GCC sign-compare fix**: Cast `int` literal to `std::size_t` in `strings.cpp:71` to resolve `-Werror=sign-compare` on GCC 13.
2. **Single SHA-256 replay context**: Removed `full_buf` and second replay pass. Initialize one `SHA256Ctx` with canonical `MXREPLAY1` metadata prefix, update it for each record in the single streaming callback, finalize after successful replay.
3. **Directory discovery**: `group_stream_sets()` now reports every `.mxraw` and `.mxraw.partial` candidate. Unreadable, malformed, corrupt, unsupported and partial entries produce explicit issues. Corrupt entries in discovery cause non-zero replay exit.
4. **Session selection contract**: Added `replay_from_stream_set()` accepting fully resolved `StreamSetInfo`. CLI uses full `(session_id, source_id, channel_id)` key. Removed ignored second replay.
5. **64-bit file position**: Replaced `std::fseek`/`std::ftell` with `std::filesystem::file_size` + portable `fseek64`/`ftell64` (uses `_fseeki64`/`_ftelli64` on Windows, `fseek` on Linux) for 64 GiB limit support.
6. **Short-read completion checks**: File SHA-256 loop now returns `IoError` on short read instead of silently breaking. Content SHA-256 loop already required exact completion.
7. **New tests**: Resource boundedness (1000-record replay with no accumulation), directory discovery (corrupt, partial, two-session ambiguity), CLI e2e (valid+corrupt, valid+partial, ambiguity rejection), explicit session selection via `replay_from_stream_set`.

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

- C++20/CMake module `cpp/moex_raw/` with library, CLI and 15 test executables
- v1 binary segment contract: `MXRAWV1\0` preamble, segment metadata, `REC1` packet records, `MXENDV1\0` footer
- Pure C++ CRC32C (Castagnoli) with known vectors: `CRC32C("")=0x00000000`, `CRC32C("123456789")=0xE3069283`
- Pure C++ SHA-256 with streaming context for incremental hashing
- Little-endian serialization primitives with checked add/multiply overflow detection
- UTF-8 string validation (128-byte limit, no embedded NUL, proper encoding)
- `RawSegmentWriter` with `.mxraw.partial` -> finalized lifecycle via same-directory rename
- Deterministic rotation by `max_records_per_segment` and `max_segment_bytes`
- `RawSegmentReader`/validator with bounded streaming parsing
- Stream-set validation: contiguous segment/capture indexes, metadata consistency
- Directory grouping by `(session_id, source_id, channel_id)` with full candidate reporting
- Deterministic replay callback with `MXREPLAY1\0` canonical digest framing (single SHA-256 context)
- `replay_from_stream_set()` for explicit session selection without ambiguity
- CLI: `moex-raw synth`, `moex-raw inspect`, `moex-raw replay`
- JSON/text report generation with stable key order
- Portable 64-bit file position for 64 GiB limit support

## What Was Intentionally Not Implemented

- No socket, multicast or real UDP/TCP capture (non-goal per spec)
- No FAST binary decode (deferred to RT-3)
- No exchange sequence extraction from payload
- No A/B deduplication or recovery (deferred to RT-4)
- No book building
- No database/object-storage integration
- No pcap/pcapng dependency
- No FIX/TWIME or order sending
- No production enablement

## Files Changed

```text
A  cpp/moex_raw/CMakeLists.txt
A  cpp/moex_raw/README.md
A  cpp/moex_raw/include/moex_raw/crc32c.hpp
A  cpp/moex_raw/include/moex_raw/endian.hpp
A  cpp/moex_raw/include/moex_raw/file_position.hpp          (Round 2)
M  cpp/moex_raw/include/moex_raw/raw_replay.hpp             (Round 2: +replay_from_stream_set)
A  cpp/moex_raw/include/moex_raw/raw_report.hpp
M  cpp/moex_raw/include/moex_raw/raw_segment.hpp            (Round 2: group_stream_sets signature)
A  cpp/moex_raw/include/moex_raw/raw_types.hpp
A  cpp/moex_raw/include/moex_raw/sha256.hpp
A  cpp/moex_raw/include/moex_raw/strings.hpp
A  cpp/moex_raw/src/crc32c.cpp
A  cpp/moex_raw/src/endian.cpp
M  cpp/moex_raw/src/main.cpp                                (Round 2: single SHA256, discovery issues)
M  cpp/moex_raw/src/raw_replay.cpp                          (Round 2: 64-bit seek, stream_set API)
A  cpp/moex_raw/src/raw_report.cpp
M  cpp/moex_raw/src/raw_segment_reader.cpp                  (Round 2: 64-bit, short-read, discovery)
A  cpp/moex_raw/src/raw_segment_writer.cpp
A  cpp/moex_raw/src/raw_types.cpp
A  cpp/moex_raw/src/sha256.cpp
M  cpp/moex_raw/src/strings.cpp                             (Round 2: sign-compare fix)
M  cpp/moex_raw/tests/test_cli.cpp                          (Round 2: +corrupt/partial/ambiguity tests)
A  cpp/moex_raw/tests/test_crc32c.cpp
A  cpp/moex_raw/tests/test_endian.cpp
A  cpp/moex_raw/tests/test_footer_validation.cpp
A  cpp/moex_raw/tests/test_header_contract.cpp
A  cpp/moex_raw/tests/test_json_report.cpp
A  cpp/moex_raw/tests/test_record_contract.cpp
A  cpp/moex_raw/tests/test_replay.cpp
M  cpp/moex_raw/tests/test_resource_safety.cpp              (Round 2: +bounded memory, discovery, sessions)
A  cpp/moex_raw/tests/test_rotation.cpp
M  cpp/moex_raw/tests/test_stream_set.cpp                   (Round 2: +partial reporting)
A  cpp/moex_raw/tests/test_strings.cpp
A  cpp/moex_raw/tests/test_writer_lifecycle.cpp
M  .gitignore
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

## Local Test Results (Round 2)

```text
RT-2 (15/15 passed, Windows Release):
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
Repository hygiene: PASS (275 files)
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
CI #42: failed in Linux/GCC build (sign-compare) — fixed in Round 2
Awaiting new CI run after push
```

## Repository Hygiene Evidence

- [x] `python tools/check_repository_hygiene.py` passed
- [x] No `.env`, keys, credentials or personal data
- [x] No official XML or owner connection parameters
- [x] No QSH/FAST/pcap/raw market data
- [x] No databases, binaries or build directories
- [x] No oversized tracked files
- [x] `.mxraw` and `.mxraw.partial` added to `.gitignore`

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
