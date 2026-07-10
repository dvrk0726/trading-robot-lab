# moex_fast — FAST Configuration and Template Inspector

Local C++20 CLI tool for inspecting MOEX SPECTRA `configuration.xml` and `templates.xml`.

## Purpose

Parse and validate FAST template definitions and feed configuration from MOEX SPECTRA XML files. Produces a human-readable summary and deterministic JSON inspection report. No network access is performed.

## Build

```bash
cmake -S cpp/moex_fast -B build/moex_fast
cmake --build build/moex_fast --config Release
```

On Windows with MSVC:

```powershell
cmake -S cpp/moex_fast -B build/moex_fast -A x64
cmake --build build/moex_fast --config Release
```

## Usage

```bash
moex-fast-inspect \
  --configuration <path/configuration.xml> \
  --templates <path/templates.xml> \
  [--json-out <path/report.json>] \
  [--strict]
```

### Options

- `--configuration` — Path to MOEX configuration.xml (required)
- `--templates` — Path to MOEX templates.xml (required)
- `--json-out` — Path for deterministic JSON report output (optional)
- `--strict` — Treat missing required components as errors instead of warnings
- `--help` — Print usage

## Report

The JSON report contains:

- Schema version and inspector version
- Input file provenance (path, size, SHA-256)
- Template metadata (ID, name, ordered fields with types)
- Feed group metadata (endpoints, protocols, ports)
- Validation issues (warnings and errors)
- Overall status: `valid`, `warning`, or `invalid`

## Strict vs Non-strict

- **Non-strict (default)**: Missing required templates or feed components produce warnings
- **Strict**: Missing required templates or feed components produce errors and set overall status to `invalid`

## Known Limitations

- Only parses MOEX SPECTRA XML format
- Does not decode FAST binary wire data
- Does not connect to any network endpoint
- Synthetic fixtures used for testing; real MOEX XML files are not committed

## Security

This tool performs no network access. No credentials, secrets, or private connection data are used or stored.
