# RT-1 Implementation Report — FAST Configuration/Template Inspector

Date: 2026-07-10 (Round 6 corrections)  
Branch: feat/rt-1-fast-config-inspector  
Pull Request: #16  
Head: `f4ecf9a1478bbdbc7a8b7931caa82f7bff9b8e90`  
CI run: (pending)  
Executor: MiMo Code

## Summary

Implemented a local C++20/CMake CLI tool that reads MOEX SPECTRA `configuration.xml` and `templates.xml`, validates their structure, and produces a deterministic inspection report. No network access is performed.

## Round 5 Corrections Applied

All blocking corrections from Architecture/Review Round 5 have been addressed:

1. **FAST `<length>` WireType fix.** Mapped standard FAST `<length>` element to `WireType::uInt32` in `element_to_wire_type()` and `parse_wire_type()`. Previously received `WireType::Unknown`, causing 16 false "Unknown wire type" warnings on real MOEX files. Preserves `is_sequence_length=true`, FIX id, global field order, and parent_sequence.

2. **Version-aware template compatibility profiles.** Added `spectra-1.29` (current published FAST 9.0 artifact: SecurityDefinition ID 40) and `spectra-1.30` (newer spec: SecurityDefinition ID 47, SecurityStatus ID 48). Does not replace ID 40 with 47 globally — each profile has its own required template set.

3. **Automatic profile detection.** `auto` mode (default) infers the profile from template ID/name evidence in the supplied `templates.xml`: ID 40 + SecurityDefinition → spectra-1.29; ID 47 + SecurityDefinition + ID 48 + SecurityStatus → spectra-1.30. Ambiguous or inconsistent profiles produce clear compatibility warnings/errors.

4. **Profile reporting.** JSON and text reports now include: `detected_profile`, `profile_evidence`, `compatibility_status` (compatible/unknown/mismatch). Strict mode fails on true profile mismatch.

5. **`--profile` CLI option.** Optional explicit override: `--profile auto|spectra-1.29|spectra-1.30`. Documented in help text.

6. **Release-active tests.** Added assertion that `<length name="NoMDEntries" id="268"/>` receives WireType::uInt32 with no "Unknown wire type" issue. Strict synthetic valid input produces `overall_status=="valid"` with zero issues.

7. **Profile test fixtures and tests.** Created `synthetic_templates_130.xml` for spectra-1.30 profile. Added 11 new tests: length wire type, strict valid, auto-detection (1.29/1.30), explicit override, mismatch warning/error, text report, mixed profile negative, wrong name negative.

## Round 6 Corrections Applied

All blocking corrections from Architecture/Review Round 6 have been addressed:

1. **Auto-detection always runs.** `detect_profile()` is called unconditionally. The actual `detected_profile` and `detection_evidence` are always populated from artifact contents, regardless of CLI override.

2. **Separated profile fields.** Report now contains four distinct profile fields:
   - `detected_profile` — actual auto-detected value from template evidence
   - `detection_evidence` (JSON: `profile_evidence`) — actual detection rationale
   - `requested_profile` — CLI request (`"auto"` or explicit)
   - `selected_profile` — profile used for validation (same as detected in auto, or override)

3. **Explicit override does not hide ambiguity.** Ambiguous or internally inconsistent artifacts always produce `compatibility_status=mismatch` and strict-mode `invalid`, regardless of `--profile` override. The report never claims an artifact was detected as 1.30 merely because the operator requested 1.30.

4. **spectra-1.29 detection rejects conflicting 1.30 evidence.** Requires ID 40 SecurityDefinition AND absence of ID 48 SecurityStatus AND absence of ID 47 SecurityDefinition. Wrong-name ID 47/48 identities are included in inconsistency evidence. Mixed artifacts (ID 40 + ID 48, ID 40 + wrong-name ID 47 + ID 48) are detected as `ambiguous`.

5. **CLI validates `--profile` values.** Unsupported values are rejected with a clear error message and non-zero exit code before inspection runs.

6. **Round 6 test matrix.** Added 6 new tests:
   - 1.29 templates + ID 48 SecurityStatus => ambiguous/mismatch
   - ID 40 + wrong-name ID 47 + ID 48 => ambiguous/mismatch
   - Explicit spectra-1.29 override on ambiguous artifact => mismatch; strict invalid
   - Explicit spectra-1.30 override on ambiguous artifact => mismatch; strict invalid
   - Override preserves actual detected profile and evidence separately
   - Unsupported CLI `--profile` value => CLI failure
   - CLI test for unsupported `--profile` value

7. **Existing positive tests preserved.** All 1.29, 1.30, and FAST length tests from Round 5 continue to pass.

## Round 5 Corrections Applied (retained)

All blocking corrections from Architecture/Review Round 4 have been addressed:

1. **Portable temp helper.** Replaced Windows-only `std::system("if not exist ... mkdir")` in `test_helpers.hpp` with C++20 `std::filesystem::create_directories`. Added error checks for directory creation, file open, and write failures. Fixes Linux GCC `-Werror=unused-result` build failure (CI run #17 root cause).

2. **Duplicate endpoint test.** `test_true_duplicate_endpoint()` now calls `run_inspector()` instead of only `parse_configuration_xml()`, and asserts that a `Duplicate endpoint` issue is emitted by the validator.

3. **Linux GCC build fix.** Eliminates the `-Werror=unused-result` error on `std::system()` return value compilation on GCC/Linux.

## Round 3 Corrections Applied (retained)

All blocking corrections from Architecture/Review Round 3 have been addressed:

1. **Corrected MOEX configuration semantics.** `FeedGroup::name` is now `MarketDataGroup@feedType` (the logical group identifier: `FUT-INFO`, `ORDERS-LOG`). `FeedGroup::label` stores the human-readable description separately. `FeedEndpoint::endpoint_role` is set from `connection/type` (the endpoint role: `Incremental`, `Snapshot`, `Historical Replay`, etc.). Previously these concepts were inverted.

2. **Corrected FAST presence semantics.** Absent `presence` attribute now correctly defaults to mandatory (matching real MOEX templates). `presence="optional"` means optional. Unsupported presence values produce an explicit warning issue. Previously, only `presence="mandatory"` was treated as mandatory, which inverted the semantics for official templates.

3. **Required template ID/name pair validation.** The validator now checks all 7 required pairs: `29 OrdersLogMessage`, `30 BookMessage`, `31 DefaultIncrementalRefreshMessage`, `32 DefaultSnapshotMessage`, `40 SecurityDefinition`, `45 SecurityGroupStatus`, `46 TradingSessionStatus`. Both ID and expected name are validated; mismatches produce issues.

4. **Synthetic fixtures are semantically faithful.** The configuration fixture has one `MarketDataGroup` for `FUT-INFO` (label: "Futures defintion") and one for `ORDERS-LOG` (label: "Full orders log"). Connection types (`Incremental`, `Snapshot`, `Historical Replay`, etc.) are inside each group's connections. Labels are deliberately different from feedType. Templates use absent presence for mandatory fields and `presence="optional"` for optional fields, matching real MOEX convention.

5. **Validates missing feedType and connection/type.** Empty `MarketDataGroup@feedType` produces an error and the group is skipped. Empty `connection/type` produces an error.

6. **Exact duplicate endpoint detection.** Duplicates are identified by full key: group feedType + endpoint role + protocol + source IP + destination IP + port + feed ID.

7. **Removed tracked runtime-generated fixture files.** 22 generated XML files removed from git tracking. Tests now create temporary files only in `build/temp/` (gitignored via `build/`). Only authored deterministic fixtures (`synthetic_configuration.xml`, `synthetic_templates.xml`) remain tracked.

8. **Unknown presence values produce issues.** Unsupported presence attribute values (e.g., `presence="constant"`) generate explicit warning issues instead of being silently accepted.

## Build Commands

```powershell
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release --parallel 4
```

## Test Commands

```powershell
ctest --test-dir build/moex_fast -C Release --output-on-failure
```

## Built Executable

```powershell
build\moex_fast\Release\moex-fast-inspect.exe --configuration <path\T0-configuration.xml> --templates <path\FAST_9.0-templates.xml> --json-out integration_report.json --strict
```

## Test Results

### Windows/MSVC

```
Compiler: MSVC 19.42.34436.0
Platform: Windows 10 x64

6/6 tests passed:
  - test_template_parser ............. Passed — 17 tests
  - test_config_parser ............... Passed — 24 tests
  - test_provenance .................. Passed — 7 tests
  - test_deterministic_report ........ Passed — 31 tests (6 new Round 6)
  - test_resource_safety ............. Passed — 8 tests
  - test_cli ......................... Passed — 12 tests (1 new Round 6)
```

### Existing QSH/M10X Regression

```
20/20 tests passed (no regression)
```

## Test Coverage Summary

### Template Parser (17 tests)
- Valid templates parse (7 required templates)
- Template names match specification (all 7 ID/name pairs)
- Template fields (name, type, FIX tag, constant)
- Mandatory/optional presence (absent=mandatory, optional=optional)
- Sequence fields and length
- Sequence nesting preserved (parent_sequence tracking)
- Field order monotonic (no resets at sequence boundaries)
- Malformed XML, missing root, duplicate ID, non-numeric ID, missing ID
- Empty templates, file not found
- Unknown element reported (not silently discarded)
- Unknown presence value reported
- Issue source is Template

### Configuration Parser (24 tests)
- Valid configuration parse (2 groups: FUT-INFO, ORDERS-LOG)
- FeedType is group ID (name = feedType, label separate)
- Endpoint role per connection (from connection/type)
- Endpoint role is NOT feedType (role comes from connection/type, not group ID)
- Feed endpoints A/B designation
- Endpoint attributes
- Malformed/missing root/empty/not-found configuration
- UDP/TCP protocol detection
- Port zero/negative/overflow/non-numeric rejected
- Issue source is Configuration
- TCP Historical Replay (multiple IPs)
- Unknown protocol detected
- Missing UDP src-ip/feed detected
- TCP no feed is OK
- True duplicate endpoint
- Missing feedType detected (error, group skipped)
- Missing connection/type detected (error)

### Provenance (7 tests)
- SHA-256 stability/change detection
- File size, path recording
- No raw XML/credentials in report
- Independent validation status (template errors don't affect configuration validation_ok)

### Deterministic Report (31 tests)
- Deterministic JSON, schema version, status
- Strict vs non-strict, template ordering
- JSON valid syntax, text output
- Required templates and feeds in JSON
- Required check results populated (7 template + 7 feed checks)
- Required template pair names include ID and name
- Endpoint role in JSON
- Label and feedType in JSON
- Parent sequence in JSON
- **Round 5:** length wire type (no Unknown wire type issues)
- **Round 5:** strict valid synthetic (valid with zero issues)
- **Round 5:** profile auto-detected spectra-1.29
- **Round 5:** profile auto-detected spectra-1.30
- **Round 5:** profile explicit override (updated for new fields)
- **Round 5:** profile mismatch warning
- **Round 5:** profile mismatch strict mode error
- **Round 5:** profile in text report
- **Round 5:** mixed profile negative test
- **Round 5:** wrong name profile negative test
- **Round 5:** NoMDEntries length — no Unknown wire type issue
- **Round 6:** 1.29 + ID 48 SecurityStatus => ambiguous/mismatch
- **Round 6:** ID 40 + wrong-name ID 47 + ID 48 => ambiguous/mismatch
- **Round 6:** explicit spectra-1.29 on ambiguous => mismatch/invalid
- **Round 6:** explicit spectra-1.30 on ambiguous => mismatch/invalid
- **Round 6:** override preserves actual detection evidence
- **Round 6:** unsupported --profile CLI value rejected

### Resource Safety (8 tests)
- Empty/truncated file, large template/field count
- Output write failure, invalid XML variants
- JSON escape handling, wire type names

### CLI Integration (12 tests)
- `--help`, no args, missing configuration/templates/files
- Valid input with/without JSON, strict/non-strict mode
- Invalid output path, unknown argument
- **Round 6:** unsupported `--profile` value exits non-zero

## JSON Contract

```json
{
  "schema_version": "1.0",
  "inspector_version": "0.1.0",
  "detected_profile": "spectra-1.29|spectra-1.30|unknown|ambiguous",
  "profile_evidence": "actual auto-detection evidence from artifact contents",
  "requested_profile": "auto|spectra-1.29|spectra-1.30",
  "selected_profile": "spectra-1.29|spectra-1.30 (profile used for validation)",
  "compatibility_status": "compatible|unknown|mismatch",
  "templates_file": { "path", "file_name", "file_size", "sha256", "parse_ok", "validation_ok" },
  "configuration_file": { "path", "file_name", "file_size", "sha256", "parse_ok", "validation_ok" },
  "required_templates": [{ "name", "present", "severity" }],
  "required_feeds": [{ "name", "present", "severity" }],
  "templates": [{ "id", "name", "fields": [...] }],
  "feed_groups": [{ "feedType", "label", "market_id", "endpoints": [{ "endpoint_role", "protocol", ... }] }],
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
    inspect_types.hpp    — Data types with IssueSource, RequiredCheckResult, FeedGroup.label
    xml_parser.hpp       — XML parsing interface
    inspector.hpp        — Main inspector logic
    report.hpp           — Report generation
    sha256.hpp           — Pure C++ SHA-256 interface
  src/
    inspect_types.cpp    — WireType helpers
    xml_parser.cpp       — XML parsing with correct MOEX semantics
    inspector.cpp        — Independent validation, required checks, duplicate detection
    report.cpp           — JSON/text with feedType, label, endpoint_role
    sha256.cpp           — Pure C++ SHA-256 (no OpenSSL)
    main.cpp             — CLI entry point
  tests/
    test_helpers.hpp     — CHECK macros + temp file helpers
    test_template_parser.cpp (17 tests)
    test_config_parser.cpp (24 tests)
    test_provenance.cpp (7 tests)
    test_deterministic_report.cpp (31 tests)
    test_resource_safety.cpp (8 tests)
    test_cli.cpp (11 tests)
    fixtures/
      synthetic_configuration.xml  — Authored deterministic fixture
      synthetic_templates.xml      — Authored deterministic fixture
```

## XML Dependency

- **pugixml** v1.14 (MIT license)
- Fetched at build time via CMake FetchContent
- No runtime dependency; compiled statically

## CI Evidence

CI run #24 on commit `37e13e9` (code `1d8b12a703ba4860262210ff430cb7ff10c5d2f6`):
- C++ MOEX FAST inspector Windows (6 tests): PASS
- C++ MOEX FAST inspector Linux (6 tests): PASS
- C++ QSH M10X regression (20 tests): PASS
- Python tests and contracts: PASS
- Repository hygiene: PASS

## Integration Check

Status: **pending** — Owner real-file rerun required after the Round 5 `<length>` WireType fix. The previous owner run showed 16 false warnings from `<length>` fields that are now resolved. The `<length>` correction and profile auto-detection must be verified against the official MOEX artifact.

Owner-run command for integration check:

```powershell
build\moex_fast\Release\moex-fast-inspect.exe --configuration <path\T0-configuration.xml> --templates <path\FAST_9.0-templates.xml> --json-out integration_report.json --strict
```

## Known Limitations

- Only parses MOEX SPECTRA XML format
- Does not decode FAST binary wire data
- Does not connect to any network endpoint
- No integration test with official MOEX XML files (requires owner-provided files)

## Security

- No credentials, official XML, or network access
- Synthetic fixtures only (RFC 5737 addresses)
- Generated build files gitignored
- Runtime test files in build/temp (gitignored)

## Main Protection

Option B active. MiMo does not merge PRs or push to main.

## Files Changed (Round 6)

```
MODIFIED: cpp/moex_fast/CMakeLists.txt (MOEX_FAST_INSPECT_PATH for test_deterministic_report)
MODIFIED: cpp/moex_fast/include/moex_fast/inspect_types.hpp (detection_evidence, requested_profile, selected_profile)
MODIFIED: cpp/moex_fast/src/inspector.cpp (detect_profile: 1.29 rejects ID 48/wrong-name; profile logic: always auto-detect, separate fields)
MODIFIED: cpp/moex_fast/src/main.cpp (--profile CLI validation)
MODIFIED: cpp/moex_fast/src/report.cpp (requested_profile, selected_profile in JSON/text)
MODIFIED: cpp/moex_fast/tests/test_cli.cpp (test_invalid_profile_value)
MODIFIED: cpp/moex_fast/tests/test_deterministic_report.cpp (6 new Round 6 tests, updated existing override test)
```

## Files Changed (Round 5)

```
MODIFIED: cpp/moex_fast/include/moex_fast/inspect_types.hpp (detected_profile, profile_evidence, compatibility_status in InspectionReport)
MODIFIED: cpp/moex_fast/include/moex_fast/inspector.hpp (profile option in InspectorOptions)
MODIFIED: cpp/moex_fast/src/inspect_types.cpp (length → uInt32 in parse_wire_type)
MODIFIED: cpp/moex_fast/src/xml_parser.cpp (length → uInt32 in element_to_wire_type)
MODIFIED: cpp/moex_fast/src/inspector.cpp (profile detection, version-aware required pairs)
MODIFIED: cpp/moex_fast/src/report.cpp (profile info in JSON and text)
MODIFIED: cpp/moex_fast/src/main.cpp (--profile CLI option)
MODIFIED: cpp/moex_fast/tests/test_template_parser.cpp (length WireType, FIX id, parent_sequence assertions)
MODIFIED: cpp/moex_fast/tests/test_deterministic_report.cpp (11 new Round 5 tests)
MODIFIED: cpp/moex_fast/tests/test_resource_safety.cpp (length → uInt32 in wire type test)
ADDED:    cpp/moex_fast/tests/fixtures/synthetic_templates_130.xml (spectra-1.30 profile fixture)
MODIFIED: AI_CONTEXT.md
MODIFIED: PROJECT_STATE.md
MODIFIED: ROADMAP.md
MODIFIED: agent_workspaces/mimo/reports/2026-07-10-rt1-fast-config-template-inspector.md
```

## Files Changed (Round 4)

```
MODIFIED: cpp/moex_fast/tests/test_helpers.hpp (portable temp helper with std::filesystem)
MODIFIED: cpp/moex_fast/tests/test_config_parser.cpp (duplicate test uses run_inspector)
MODIFIED: AI_CONTEXT.md
MODIFIED: PROJECT_STATE.md
MODIFIED: ROADMAP.md
MODIFIED: agent_workspaces/mimo/reports/2026-07-10-rt1-fast-config-template-inspector.md
```

## Files Changed (Round 3)

```
MODIFIED: cpp/moex_fast/include/moex_fast/inspect_types.hpp (FeedGroup.label, FeedEndpoint.endpoint_role, unknown_presence)
MODIFIED: cpp/moex_fast/src/xml_parser.cpp (correct grouping, presence, validation)
MODIFIED: cpp/moex_fast/src/inspector.cpp (required pairs, duplicate detection)
MODIFIED: cpp/moex_fast/src/report.cpp (feedType, label, endpoint_role in output)
MODIFIED: cpp/moex_fast/tests/test_helpers.hpp (temp file helpers)
MODIFIED: cpp/moex_fast/tests/test_template_parser.cpp (presence semantics, temp files)
MODIFIED: cpp/moex_fast/tests/test_config_parser.cpp (endpoint_role, temp files, new tests)
MODIFIED: cpp/moex_fast/tests/test_deterministic_report.cpp (endpoint_role, label, strict fixture)
MODIFIED: cpp/moex_fast/tests/test_provenance.cpp (temp files)
MODIFIED: cpp/moex_fast/tests/test_resource_safety.cpp (temp files)
MODIFIED: cpp/moex_fast/tests/fixtures/synthetic_configuration.xml (correct MOEX semantics)
MODIFIED: cpp/moex_fast/tests/fixtures/synthetic_templates.xml (absent presence = mandatory)
REMOVED FROM TRACKING: 22 runtime-generated fixture files (now in build/temp)
MODIFIED: AI_CONTEXT.md
MODIFIED: PROJECT_STATE.md
MODIFIED: ROADMAP.md
MODIFIED: agent_workspaces/mimo/reports/2026-07-10-rt1-fast-config-template-inspector.md
```
