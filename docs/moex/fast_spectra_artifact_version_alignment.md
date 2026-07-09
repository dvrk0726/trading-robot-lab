# FAST SPECTRA Artifact Version Alignment

Date: 2026-07-09
Status: verified from owner-uploaded artifacts

## Purpose

This document records version/alignment findings from locally downloaded MOEX FAST SPECTRA artifacts.

Goal: reduce implementation mistakes caused by mixing outdated PDFs, templates and generated distributions.

This is a derived note only.

Do not commit the raw PDFs, ZIP files, XML templates or binaries unless explicitly approved.

## Uploaded artifacts checked

```text
fast_9_templates.xml
Spectra_fastgate-1.29.0.0.1188.zip
fast_sensor.zip
spectra_fastgate_ru.pdf
spectra_fastgate_ru (1).pdf
```

Previously uploaded and used for comparison:

```text
templates.xml        from templatesT0/templates.xml
configuration.xml    from templatesT0/configuration.xml
```

## Template comparison result

### templatesT0/templates.xml vs FAST_9.0/templates.xml

Result:

```text
identical byte-for-byte
```

Both files:

```text
size: 28,575 bytes
sha256: dbd50f1e0becc2b2ebd9dac8e4c6609ba1538566811b610cde9b6dd3e7f66a8e
```

Conclusion:

```text
The uploaded FAST_9.0/templates.xml does not introduce a different message schema compared with the previously uploaded templatesT0/templates.xml.
```

Practical consequence:

```text
The existing notes based on templatesT0/templates.xml remain valid for the currently uploaded FAST_9.0/templates.xml.
```

## Important template ids confirmed

The current uploaded templates define the key IDs needed by the project:

```text
OrdersLogMessage             id=29
BookMessage                  id=30
DefaultIncrementalRefresh    id=31
DefaultSnapshot              id=32
SecurityDefinition           id=40
SecurityStatus               id=5
SecurityMassStatus           id=37
SecurityGroupStatus          id=45
TradingSessionStatus         id=46
DiscreteAuction              id=42
Heartbeat                    id=6
SequenceReset                id=7
Logon                        id=1000
Logout                       id=1001
```

Key decision:

```text
Decoder must load templates from file and must not hardcode old message IDs from older PDFs.
```

## PDF versions found

### Older standalone PDF

```text
file: spectra_fastgate_ru.pdf
version: 1.24.1
page title date: 20.01.2025
sha256: 437699b7ded6a264e586a33697de0bc323d2e78d575889a3c0e7b6171d41f87a
```

This PDF is older and contains older template-id mapping for some instrument/session messages.

Examples from older 1.24.1:

```text
SecurityDefinition id=38
TradingSessionStatus id=8
DiscreteAuction id=26
```

Do not use this version as the implementation baseline.

### ZIP distribution PDF

```text
file: Spectra_fastgate-1.29.0.0.1188.zip / Spectra_fastgate/spectra_fastgate_ru.pdf
spec version: 1.29.0
package build version: 1.29.0.0.1188
package build date: 2025-09-04 03-19-59
sha256(pdf): cd19d2a8334f381f6024df117de5654202126ff3fec9db8d865999cf38fa3698
```

Important 1.29.0 changes include:

```text
TradingSessionStatus msg id changed from 41 to 46.
SecurityDefinition id already changed to 40 in 1.27.0.
SecurityGroupStatus id changed to 45 in 1.28.0.
TradingSessionStatus gained SettlSessBegin and ClrSessBegin.
```

### Newer standalone PDF

```text
file: spectra_fastgate_ru (1).pdf
version: 1.29.1
page title date: 19.11.2025
sha256: 27674419e2f58b1e56149690290607f9223d73c1e4c98cbc0640e6d7ff197f9f
```

Important 1.29.1 change:

```text
MOEX Board references were removed from the specification.
```

Practical conclusion:

```text
Use the newer 1.29.1 PDF as the conceptual/specification baseline where available.
Use the uploaded FAST_9.0/templates.xml as the actual decoder template file.
Do not use old 1.24.1 message IDs for implementation.
```

## ZIP distribution contents

### Spectra_fastgate-1.29.0.0.1188.zip

Contains only documentation package artifacts:

```text
Spectra_fastgate/
Spectra_fastgate/spectra_fastgate_ru.pdf
Spectra_fastgate/meta.info
Spectra_fastgate/spectra_fastgate_en.pdf
```

Important:

```text
This ZIP is documentation, not a runtime library or collector implementation.
```

### fast_sensor.zip

Contains FAST sensor binaries:

```text
fast_sensor/el7-x86_64/fast_sensor-1.30.0.1337
fast_sensor/win-x86_64/fast_sensor.exe
```

Current interpretation:

```text
fast_sensor is a utility/binary package, not the main development baseline.
Do not commit its binaries.
Do not build the collector around black-box binaries until their purpose is documented.
```

## Current source-of-truth hierarchy

For future implementation, use this priority:

```text
1. Actual templates.xml supplied for the target environment.
2. Actual configuration.xml supplied for the target environment.
3. Newest matching FAST PDF specification.
4. Older PDFs only as historical reference.
```

For the currently uploaded artifacts:

```text
schema/template baseline: fast_9_templates.xml / templatesT0/templates.xml
spec baseline: spectra_fastgate_ru (1).pdf, version 1.29.1
connection/channel baseline: templatesT0/configuration.xml
```

## Implementation safety rules

```text
1. Do not hardcode template ids from older PDFs.
2. Do not assume PDF version and templates are always synchronized.
3. Always log loaded template file name and SHA-256 at collector startup.
4. Always log loaded configuration file name and SHA-256 at collector startup.
5. Separate FAST MDFlags from historical QSH order_flags.
6. Prefer template-driven decoding over hand-written fixed binary layouts.
7. Keep raw MOEX artifacts outside Git unless explicitly approved.
```

## Current blocker status

Previous blocker:

```text
Need to compare FAST_9.0/templates.xml with templatesT0/templates.xml.
```

Status:

```text
resolved: they are identical byte-for-byte.
```

Remaining open questions:

```text
1. Can the project receive the T0 multicast streams over normal internet after MOEX test access approval?
2. Does MOEX provide a current configuration.xml together with test credentials, and can it differ from the public one?
3. What exactly is fast_sensor intended to validate: connectivity, packet loss, or feed health?
4. Should first collector MVP decode directly from UDP or first capture PCAP and decode offline?
```

## Related docs

```text
docs/moex/MOEX_SOURCE_INDEX.md
docs/moex/fast_spectra_notes.md
docs/moex/fast_spectra_t0_templates_notes.md
docs/moex/moex_source_version_check.md
```
