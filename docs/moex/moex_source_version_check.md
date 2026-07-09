# MOEX Source Version Check

Date: 2026-07-09
Status: first-pass directory/version check

## Purpose

This document tracks MOEX public source versions so the project does not accidentally implement against stale protocol files.

This is not a protocol summary and not a copy of MOEX files.

## Checked directories

### FAST SPECTRA test root

Source:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/
```

Observed contents from directory listing:

```text
backup/                  modified 2026-01-26
FAST_8.6/                modified 2025-06-20
FAST_9.0/                modified 2025-09-09
fast_sensor.zip          modified 2022-11-01, size 4,999,380
meta.info                modified 2026-04-10, size 746
spectra_fastgate_en.pdf  modified 2026-04-10, size 556,004
spectra_fastgate_ru.pdf  modified 2026-04-10, size 588,929
templatesT0/             modified 2021-09-24
templatesT1/             modified 2024-04-22
```

Key interpretation:

```text
The Russian/English FAST PDF and root meta.info are newer than templatesT0.
FAST_9.0 is newer than templatesT0.
Therefore, before implementing a decoder, compare the public T0 templates with FAST_9.0/templates.xml and the current PDF.
```

### FAST_9.0 directory

Source:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/FAST_9.0/
```

Observed contents from directory listing:

```text
Spectra_fastgate-1.29.0.0.1188.zip  modified 2025-09-09, size 907,483
templates.xml                       modified 2025-09-09, size 28,575
```

Key interpretation:

```text
FAST_9.0/templates.xml may be the newer template set than templatesT0/templates.xml uploaded earlier.
It must be compared before writing production-quality decoder code.
```

## Already parsed sources

### templatesT0/configuration.xml

Status:

```text
parsed from owner-uploaded file
```

Important result:

```text
T0 configuration contains ORDERS-LOG with Incremental A/B, Snapshot A/B and Historical Replay TCP.
```

Related summary:

```text
docs/moex/fast_spectra_t0_templates_notes.md
```

### templatesT0/templates.xml

Status:

```text
parsed from owner-uploaded file
```

Important result:

```text
OrdersLogMessage id=29 and BookMessage id=30 are present.
```

Related summary:

```text
docs/moex/fast_spectra_t0_templates_notes.md
```

### spectra_fastgate_ru.pdf

Status:

```text
first-pass summarized
```

Related summary:

```text
docs/moex/fast_spectra_notes.md
```

### spectra_fixgate_ru.pdf

Status:

```text
first-pass summarized
```

Related summary:

```text
docs/moex/fix_spectra_notes.md
```

## Not yet parsed / needs owner upload or direct access

The following files were identified but not parsed in full yet:

```text
FAST_9.0/templates.xml
FAST root meta.info
FIX docs meta.info
templatesT1/configuration.xml
templatesT1/templates.xml
```

Reason:

```text
Directory listings are accessible, but file bodies were not reliably fetched in the current assistant environment.
```

## Current implementation rule

```text
Do not start real FAST decoder implementation until template version mismatch risk is resolved.
```

Allowed now:

```text
- continue documentation;
- build source index;
- draft architecture;
- prepare local dry-run config shape;
- use synthetic test messages.
```

Blocked until version check:

```text
- hardcoded binary decoder against one template version;
- production-quality OrdersLogMessage parser;
- assumptions that templatesT0/templates.xml is the final current schema.
```

## Next owner upload request

To finish version check, upload:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/FAST_9.0/templates.xml
https://ftp.moex.com/pub/FAST/Spectra/test/meta.info
https://ftp.moex.com/pub/FIX/Spectra/test/docs/meta.info
```

Optional later:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/configuration.xml
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/templates.xml
```

## Practical conclusion

The project already has enough information to confirm the path:

```text
SPECTRA test access -> FAST ORDERS-LOG collector -> normalized data -> paper trading later
```

But it must resolve source/template version alignment before writing the real decoder core.
