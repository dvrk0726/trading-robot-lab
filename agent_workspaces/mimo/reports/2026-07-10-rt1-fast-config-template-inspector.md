# RT-1 Implementation Report — FAST Configuration/Template Inspector

Date: 2026-07-10 (corrections round)  
Branch: feat/rt-1-fast-config-inspector  
Pull Request: #16  
Commit SHA: 3ccd2f4b7dc35bcdde76f30f4fb84e63cc64c5d6  
Executor: MiMo Code

## Summary

Implemented a local C++20/CMake CLI tool that reads MOEX SPECTRA `configuration.xml` and `templates.xml`, validates their structure, and produces a deterministic inspection report. No network access is performed.

## Corrections Applied (CHANGES_REQUIRED round)

All 9 blocking corrections from the Architecture/Review inspection have been addressed:

1. **Tests use CHECK macros, not assert** — All test executables now use `CHECK()` macros that remain active in Release builds. Added `test_helpers.hpp` with `CHECK`, `CHECK_EQ`, `CHECK_NE`, `CHECK_MSG` that print file:line and exit(1) on failure.

2. **Feed roles per endpoint** — `feed_type` moved from `FeedGroup` to `FeedEndpoint`. Each source/endpoint carries its own role (Incremental, Snapshot, Historical Replay, etc.). Group-level `feed_type` removed.

3. **Field order and sequence nesting** — Global monotonic field order counter continues across sequences (no reset). `parent_sequence` field added to `FastFieldDescriptor` to track nesting. Sequence length field correctly identified by element name.

4. **Unknown elements/operators reported** — Parser now reports unknown XML elements with `IssueSource::Template`. Known FAST operators (constant, default, copy, etc.) are recognized; truly unknown elements produce explicit warnings.

5. **Port validation** — Strict parsing: rejects non-numeric, zero, negative, and >65535 values. Uses `strtol` on the original text string before narrowing to `uint16_t`.

6. **Independent validation statuses** — `IssueSource` enum tracks whether each issue came from template or configuration parsing. `validation_ok` for each file only considers issues from its own source.

7. **Linux SHA-256** — Replaced OpenSSL dependency with a pure C++ SHA-256 implementation (`sha256.cpp`/`sha256.hpp`). No external library needed on Linux.

8. **JSON contract** — Added `required_templates` and `required_feeds` arrays to JSON output. Each entry has `name`, `present`, and `severity`. `feed_type` appears inside endpoint objects.

9. **State files updated** — AI_CONTEXT.md (duplicate RT-1 section removed), PROJECT_STATE.md, ROADMAP.md all reflect CHANGES_REQUIRED → corrections applied.

## New CLI Test Added

`test_cli.cpp` — 11 CLI integration tests:
- `--help` exits 0
- No arguments exits non-zero
- Missing `--configuration` exits non-zero
- Missing `--templates` exits non-zero
- Missing files exits non-zero
- Valid input without `--json-out` exits 0
- Valid input with `--json-out` exits 0 and writes valid JSON
- `--strict` mode with valid input exits 0
- Non-strict mode exits 0
- Invalid output path exits non-zero
- Unknown argument exits non-zero

## Build Commands

```powershell
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release
```

## Test Commands

```powershell
ctest --test-dir build/moex_fast -C Release --output-on-failure
```

## Test Results

### Windows/MSVC

```
Compiler: MSVC 19.42.34436.0
Platform: Windows 10 x64

6/6 tests passed:
  - test_template_parser ............. Passed (0.04 sec) — 15 tests
  - test_config_parser ............... Passed (0.03 sec) — 15 tests
  - test_provenance .................. Passed (0.04 sec) — 7 tests
  - test_deterministic_report ........ Passed (0.03 sec) — 12 tests
  - test_resource_safety ............. Passed (0.04 sec) — 8 tests
  - test_cli ......................... Passed (0.25 sec) — 11 tests
```

### Existing QSH/M10X Regression

```
20/20 tests passed (no regression)
```

### Repository Hygiene

```
Repository hygiene check: PASS
Checked 257 tracked or non-ignored pending files.
```

### Python Tests

```
3 passed in 0.03s
```

### Contract Validation

```
All 5 examples valid.
```

## Test Coverage Summary

### Template Parser (15 tests)
- Valid templates parse (7 required templates)
- Template fields (name, type, FIX tag, constant)
- Mandatory/optional presence
- Sequence fields and length
- **Sequence nesting preserved** (parent_sequence tracking)
- **Field order monotonic** (no resets at sequence boundaries)
- Malformed XML, missing root, duplicate ID, non-numeric ID, missing ID
- Empty templates, file not found
- **Unknown element reported** (not silently discarded)
- **Issue source is Template**

### Configuration Parser (15 tests)
- Valid configuration parse
- **Feed type per endpoint** (Incremental, Snapshot, Historical Replay)
- Feed endpoints A/B designation
- Endpoint attributes
- Malformed/missing root/empty/not-found configuration
- UDP/TCP protocol detection
- **Port zero rejected**
- **Port negative rejected**
- **Port overflow rejected** (>65535)
- **Port non-numeric rejected**
- **Issue source is Configuration**
- **TCP Historical Replay**

### Provenance (7 tests)
- SHA-256 stability/change detection
- File size, path recording
- No raw XML/credentials in report
- **Independent validation status** (template errors don't affect configuration validation_ok)

### Deterministic Report (12 tests)
- Deterministic JSON, schema version, status
- Strict vs non-strict, template ordering
- JSON valid syntax, text output
- **Required templates and feeds in JSON**
- **Required check results populated** (7 template + 7 feed checks)
- **Feed type in endpoint JSON**
- **Parent sequence in JSON**

### Resource Safety (8 tests)
- Empty/truncated file, large template/field count
- Output write failure, invalid XML variants
- JSON escape handling, wire type names

### CLI Integration (11 tests)
- `--help`, no args, missing configuration/templates/files
- Valid input with/without JSON, strict/non-strict mode
- Invalid output path, unknown argument

## JSON Contract

The JSON report now includes:

```json
{
  "schema_version": "1.0",
  "inspector_version": "0.1.0",
  "templates_file": { "path", "file_name", "file_size", "sha256", "parse_ok", "validation_ok" },
  "configuration_file": { "path", "file_name", "file_size", "sha256", "parse_ok", "validation_ok" },
  "required_templates": [{ "name", "present", "severity" }],
  "required_feeds": [{ "name", "present", "severity" }],
  "templates": [{ "id", "name", "fields": [...] }],
  "feed_groups": [{ "name", "market_id", "endpoints": [{ "feed_type", "protocol", ... }] }],
  "issues": [{ "severity", "source", "message" }],
  "overall_status": "valid|warning|invalid"
}
```

## Architecture

```
cpp/moex_fast/
  CMakeLists.txt
  README.md
  include/moex_fast/
    inspect_types.hpp    — Data types with IssueSource, RequiredCheckResult
    xml_parser.hpp       — XML parsing interface
    inspector.hpp        — Main inspector logic
    report.hpp           — Report generation
    sha256.hpp           — Pure C++ SHA-256 interface
  src/
    inspect_types.cpp    — WireType helpers
    xml_parser.cpp       — XML parsing with field order, nesting, port validation
    inspector.cpp        — Independent validation, required checks
    report.cpp           — JSON/text with required_templates/feeds
    sha256.cpp           — Pure C++ SHA-256 (no OpenSSL)
    main.cpp             — CLI entry point
  tests/
    test_helpers.hpp     — CHECK macros for Release-active assertions
    test_template_parser.cpp (15 tests)
    test_config_parser.cpp (15 tests)
    test_provenance.cpp (7 tests)
    test_deterministic_report.cpp (12 tests)
    test_resource_safety.cpp (8 tests)
    test_cli.cpp (11 tests)
    fixtures/            — Synthetic XML test files
```

## XML Dependency

- **pugixml** v1.14 (MIT license)
- Fetched at build time via CMake FetchContent
- No runtime dependency; compiled statically

## Known Limitations

- Only parses MOEX SPECTRA XML format
- Does not decode FAST binary wire data
- Does not connect to any network endpoint
- Linux/GCC build not tested locally (pure C++ SHA-256 removes OpenSSL dependency)
- No integration test with official MOEX XML files

## Security

- No credentials, official XML, or network access
- Synthetic fixtures only
- Generated build files gitignored

## Files Changed

```
NEW: cpp/moex_fast/include/moex_fast/sha256.hpp
NEW: cpp/moex_fast/src/sha256.cpp
NEW: cpp/moex_fast/tests/test_cli.cpp
NEW: cpp/moex_fast/tests/test_helpers.hpp
NEW: cpp/moex_fast/tests/cli_test_config.hpp.in
MODIFIED: cpp/moex_fast/include/moex_fast/inspect_types.hpp
MODIFIED: cpp/moex_fast/src/xml_parser.cpp
MODIFIED: cpp/moex_fast/src/inspector.cpp
MODIFIED: cpp/moex_fast/src/report.cpp
MODIFIED: cpp/moex_fast/CMakeLists.txt
MODIFIED: cpp/moex_fast/tests/test_template_parser.cpp
MODIFIED: cpp/moex_fast/tests/test_config_parser.cpp
MODIFIED: cpp/moex_fast/tests/test_provenance.cpp
MODIFIED: cpp/moex_fast/tests/test_deterministic_report.cpp
MODIFIED: cpp/moex_fast/tests/test_resource_safety.cpp
MODIFIED: AI_CONTEXT.md
MODIFIED: PROJECT_STATE.md
MODIFIED: ROADMAP.md
```
