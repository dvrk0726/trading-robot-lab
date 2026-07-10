# RT-1 Implementation Report — FAST Configuration/Template Inspector

Date: 2026-07-10 (Round 3 corrections)  
Branch: feat/rt-1-fast-config-inspector  
Pull Request: #16  
Executor: MiMo Code

## Summary

Implemented a local C++20/CMake CLI tool that reads MOEX SPECTRA `configuration.xml` and `templates.xml`, validates their structure, and produces a deterministic inspection report. No network access is performed.

## Round 3 Corrections Applied

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
cmake -S cpp/moex_fast -B cpp/moex_fast/build
cmake --build cpp/moex_fast/build --config Release
```

## Test Commands

```powershell
ctest --test-dir cpp/moex_fast/build -C Release --output-on-failure
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
  - test_deterministic_report ........ Passed — 14 tests
  - test_resource_safety ............. Passed — 8 tests
  - test_cli ......................... Passed — 11 tests
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

### Deterministic Report (14 tests)
- Deterministic JSON, schema version, status
- Strict vs non-strict, template ordering
- JSON valid syntax, text output
- Required templates and feeds in JSON
- Required check results populated (7 template + 7 feed checks)
- Required template pair names include ID and name
- Endpoint role in JSON
- Label and feedType in JSON
- Parent sequence in JSON

### Resource Safety (8 tests)
- Empty/truncated file, large template/field count
- Output write failure, invalid XML variants
- JSON escape handling, wire type names

### CLI Integration (11 tests)
- `--help`, no args, missing configuration/templates/files
- Valid input with/without JSON, strict/non-strict mode
- Invalid output path, unknown argument

## JSON Contract

```json
{
  "schema_version": "1.0",
  "inspector_version": "0.1.0",
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
    test_deterministic_report.cpp (14 tests)
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

## Integration Check

No official owner-provided XML available locally. Integration verification against the official T0 configuration.xml was not performed. The synthetic fixture uses the same structural hierarchy as the official file per the review description.

Owner-run command for integration check:

```powershell
moex-fast-inspect --configuration <path-to-T0-configuration.xml> --templates <path-to-FAST_9.0-templates.xml> --json-out integration_report.json --strict
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
