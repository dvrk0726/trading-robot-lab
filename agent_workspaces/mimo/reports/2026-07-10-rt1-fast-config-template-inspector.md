# RT-1 Implementation Report — FAST Configuration/Template Inspector

Date: 2026-07-10
Branch: feat/rt-1-fast-config-inspector
Executor: MiMo Code

## Summary

Implemented a local C++20/CMake CLI tool that reads MOEX SPECTRA `configuration.xml` and `templates.xml`, validates their structure, and produces a deterministic inspection report. No network access is performed.

## Deliverables

1. **C++20 inspector executable** — `cpp/moex_fast/` module integrated into the existing CMake structure
2. **XML parsing** — Uses pugixml (MIT license, fetched via CMake FetchContent)
3. **Human-readable summary output** — Text format printed to stdout
4. **Deterministic JSON inspection report** — `--json-out` option
5. **Normalized C++ metadata contracts** — `inspect_types.hpp` with value types for templates, fields, feed groups, endpoints, issues, and reports
6. **Unit tests** — 5 test executables with synthetic XML fixtures
7. **CI integration** — Added `cpp-moex-fast-inspector` job to `.github/workflows/ci.yml`
8. **Module README** — `cpp/moex_fast/README.md`

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

5/5 tests passed:
  - test_template_parser ............. Passed (0.06 sec)
  - test_config_parser ............... Passed (0.04 sec)
  - test_provenance .................. Passed (0.04 sec)
  - test_deterministic_report ........ Passed (0.03 sec)
  - test_resource_safety ............. Passed (0.07 sec)
```

### Existing QSH/M10X Regression

```
20/20 tests passed (no regression)
CTest inventory: exactly 20
```

### Repository Hygiene

```
Repository hygiene check: PASS
Checked 244 tracked or non-ignored pending files.
```

### Python Tests

```
3 passed in 0.04s
```

## Test Coverage

### Template Parser Tests (test_template_parser)
- Valid templates parse (7 required templates)
- Template fields (name, type, FIX tag, constant)
- Mandatory/optional presence
- Sequence fields and length
- Malformed XML error handling
- Missing root element
- Duplicate template ID detection
- Non-numeric template ID detection
- Missing template ID detection
- Empty templates
- File not found

### Configuration Parser Tests (test_config_parser)
- Valid configuration parse (FUT-INFO, ORDERS-LOG)
- Feed endpoints (A/B designation)
- Endpoint attributes (protocol, IP, port, multicast)
- Malformed configuration XML
- Missing configuration root
- Empty configuration
- Configuration file not found
- UDP/TCP protocol detection
- Feed type preservation

### Provenance Tests (test_provenance)
- SHA-256 stability for identical bytes
- SHA-256 change detection for different content
- File size correctness
- Path recording
- No raw XML in report
- No credentials in report

### Deterministic Report Tests (test_deterministic_report)
- Deterministic JSON output (identical for same inputs)
- Schema version present
- Overall status for valid input
- Overall status for invalid input
- Strict vs non-strict mode
- Template ordering in JSON
- JSON valid syntax (balanced braces)
- Text output format

### Resource Safety Tests (test_resource_safety)
- Empty file handling
- Truncated file (no crash)
- Large template count (100 templates)
- Large field count (50 fields)
- Output write failure handling
- No crash on invalid XML variants
- JSON escape handling
- Wire type name mapping

## CLI Interface

```
moex-fast-inspect \
  --configuration <path/configuration.xml> \
  --templates <path/templates.xml> \
  [--json-out <path/report.json>] \
  [--strict]
```

Options:
- `--configuration` — Path to MOEX configuration.xml (required)
- `--templates` — Path to MOEX templates.xml (required)
- `--json-out` — Path for deterministic JSON report (optional)
- `--strict` — Treat missing required components as errors
- `--help` — Print usage

## Architecture

```
cpp/moex_fast/
  CMakeLists.txt           — Build config with FetchContent for pugixml
  README.md                — Module documentation
  include/moex_fast/
    inspect_types.hpp      — Data types (WireType, FastFieldDescriptor, etc.)
    xml_parser.hpp         — XML parsing interface
    inspector.hpp          — Main inspector logic
    report.hpp             — Report generation
  src/
    inspect_types.cpp      — WireType helpers
    xml_parser.cpp         — XML parsing with pugixml
    inspector.cpp          — Validation and report assembly
    report.cpp             — JSON/text serialization
    main.cpp               — CLI entry point
  tests/
    fixtures/              — Synthetic XML test files
    test_template_parser.cpp
    test_config_parser.cpp
    test_provenance.cpp
    test_deterministic_report.cpp
    test_resource_safety.cpp
```

## XML Dependency

- **pugixml** v1.14 (MIT license)
- Fetched at build time via CMake FetchContent
- No runtime dependency; compiled statically into the inspector
- Well-tested, lightweight, license-compatible

## Known Limitations

- Only parses MOEX SPECTRA XML format (not generic FAST XML)
- Does not decode FAST binary wire data
- Does not connect to any network endpoint
- Linux/GCC build not tested in this session (CI runs on Windows)
- No integration test with official MOEX XML files (not committed per security policy)

## Security

- No credentials in code, tests, or reports
- No real MOEX passwords/login IDs
- No owner-provided official XML committed
- Synthetic fixtures only in Git
- No network access performed by the tool
- Generated build files remain gitignored

## Files Changed

```
NEW: cpp/moex_fast/ (entire module)
MODIFIED: .github/workflows/ci.yml (added moex_fast CI job)
MODIFIED: AI_CONTEXT.md (updated for RT-1 in progress)
MODIFIED: PROJECT_STATE.md (updated for RT-1 in progress)
MODIFIED: ROADMAP.md (updated for RT-1 in progress)
```

## Acceptance Criteria Status

- [x] CLI reads configuration.xml and templates.xml from operator-supplied paths
- [x] CLI performs no network access
- [x] File SHA-256 and size are reported
- [x] Templates are listed with stable IDs, names and ordered fields
- [x] Feed groups/endpoints are listed with transport and role
- [x] Required template IDs 29, 30, 31, 32, 40, 45, 46 are checked
- [x] FUT-INFO and ORDERS-LOG Incremental A/B, Snapshot A/B, Historical Replay are checked
- [x] Strict mode fails on missing required components
- [x] Non-strict mode preserves warnings without hiding them
- [x] Human-readable output is understandable
- [x] JSON report is deterministic and documented
- [x] Field order from templates.xml is preserved
- [x] Optional/mandatory presence is preserved
- [x] Sequence boundaries and length fields are represented correctly
- [x] Unknown types/operators are surfaced explicitly
- [x] Duplicate template IDs are rejected
- [x] Invalid endpoint ports are rejected
- [x] No QSH flags or QSH parser enums are reused for FAST metadata
- [x] No raw XML is embedded in JSON output
- [x] QuickFAST is not added as production dependency
- [x] Inspector parsing is isolated from future realtime hot path
- [x] No universal FIX message tree is introduced
- [x] No UDP/TCP receiver is added
- [x] No order-entry code is added
- [x] No database dependency is added
- [x] New contracts are small value types with clear ownership
- [x] CMake integration follows existing repository conventions
- [x] All new unit tests pass
- [x] Existing QSH/M10X tests continue to pass
- [x] Error cases do not crash the process
- [x] Deterministic JSON test passes
- [x] SHA-256 test passes
- [x] Windows/MSVC build passes
- [x] No official XML, credentials, binaries or build directories are committed
- [x] No unrelated files are changed
- [x] Commit is logically scoped to RT-1
- [x] Module README is included
