# RT-4 MOEX FAST source update — 2026-07-15

**Issue:** #38  
**Scope:** documentation and source provenance only  
**Security:** no credentials, external/private IP addresses or raw market-data packets are recorded

## 1. Purpose

This document records evidence obtained after the historical RT-3 source audit dated 2026-07-11.

The historical file `docs/rt3_moex_fast_authoritative_source_audit.md` remains unchanged because it correctly records what the official MOEX endpoints returned on its audit date. This update records later endpoint contents and Stage 0 connectivity checks without rewriting history.

## 2. Source hierarchy

1. Official MOEX SPECTRA FAST specification and test files.
2. FIX FAST 1.1 only for base FAST semantics not fully specified by MOEX.
3. Third-party implementations only as cross-check.

Primary directory:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/
```

## 3. Current normative document

```text
file: spectra_fastgate_en.pdf
version: 1.30.2
date: 2026-04-10
historical audit SHA-256:
F744FFFF277D76657FF3F138B0923CE681C1F80BBB8B756088806BF7A04715C2
```

Relevant confirmed rules:

- the current UDP transport adds a 4-byte preamble before each FAST message;
- the preamble contains tag 34 `MsgSeqNum`;
- tag 34 is also present inside the FAST message;
- current MOEX sends at most one FAST-coded message per UDP datagram;
- A and B are equal copies of one logical feed;
- clients suppress duplicate sequence numbers and tolerate bounded reordering;
- an unresolved gap requires recovery;
- TCP Historical Replay uses a 4-byte message-length prefix, not the UDP sequence preamble;
- Snapshot cycles and `LastFragment` govern later recovery processing.

The document does not explicitly state the byte order of the external UDP preamble.

## 4. Fresh official file verification

The Owner downloaded current files directly from the official MOEX endpoints and calculated SHA-256 locally on 2026-07-15.

### T0 configuration

```text
URL:
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/configuration.xml

SHA-256:
AE80702BC3E179CAF5DA025E94FDAC6AC7A6A4FF1353E7FB5D0396DE987C4118
```

This hash is unchanged from the 2026-07-11 RT-3 audit.

### Current T0 templates

```text
URL:
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/templates.xml

SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

### Current T1 templates

```text
URL:
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/templates.xml

SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

### Interpretation

On 2026-07-15 the official T0 and T1 template endpoints returned byte-identical files.

The RT-3 audit recorded this earlier state on 2026-07-11:

```text
T0 templates:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 templates:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Therefore:

- the old audit is valid historical evidence for its date;
- it is stale as a statement of current endpoint equality;
- no claim is made about the internal MOEX deployment event that caused the endpoint contents to converge;
- RT-4 must re-hash official files at every material source review.

## 5. UDP preamble byte order

Evidence status:

```text
4-byte preamble: confirmed
contains MsgSeqNum(34): confirmed
same tag in FAST body: confirmed
external preamble endian: unresolved in official text
```

The official XML cannot define this value because the preamble is outside the FAST body.

A public MOEX-specific implementation was found to interpret the four bytes as little-endian. This is cross-check evidence only and is not accepted as normative.

RT-4 decision:

- Gate A framing supports explicit `LittleEndian` and `BigEndian`;
- no default value is allowed;
- Gate B may compare both interpretations with decoded tag 34;
- ambiguous or neither-match cases fail closed;
- production acceptance requires a live T0/T1 packet, an official vector or written MOEX confirmation.

## 6. Official fast_sensor inspection

Official archive contents inspected locally:

```text
Linux binary
Windows x86_64 binary
```

No source code, configuration XML or template XML was included in the archive.

Verified Windows binary:

```text
fast_sensor version 1.30.0.1337
```

Relevant supported modes from built-in help:

- gap analyser;
- statistics collector;
- packet order and duplicate checks;
- raw packet dump;
- decoded message dump;
- TCP recovery.

The Stage 0 test intentionally used only gap/statistics/order checking. Raw and decoded dump modes were not used.

## 7. T0 configuration inspection

The current T0 configuration parsed successfully.

Observed structure includes separate physical A and B multicast connections. Examples of address ranges and ports are recorded only at the non-private exchange-group level:

```text
Feed A multicast groups: 239.195.12.x, ports 480xx
Feed B multicast groups: 239.195.140.x, ports 490xx
```

The configuration includes Instrument Replay, Instrument Incremental and ordinary Incremental feeds.

A and B are treated as physical copies of one logical sequence, not primary and backup.

## 8. Stage 0 connectivity test

The Owner confirmed locally:

- one active Ethernet IPv4 interface;
- Windows network profile `Private`;
- IPv4 Internet connectivity;
- active IPv4 multicast route `224.0.0.0/4`.

The actual local IP, gateway and registered external static IPv4 are deliberately omitted.

Safe test form:

```powershell
fast_sensor.exe -g -s --check_order `
  --config .\fast_sensor_t0\configuration.xml `
  --templates .\fast_sensor_t0\templates.xml `
  --bind_address <LOCAL_INTERFACE_IPV4>
```

Observed result:

```text
configuration accepted
subscriptions attempted
packets count = 0
speed = 0
```

Interpretation:

- the binary started correctly;
- the T0 configuration and templates were accepted;
- no UDP market-data packet reached the process during the bounded check;
- no FAST message was decoded;
- preamble endian could not be verified;
- MOEX support activation or routing for the registered external static IPv4 remained pending.

This result is not evidence of a framing defect.

## 9. Connection-service boundary

Credentials for these services are not FAST multicast credentials:

- SPECTRA Terminal;
- trading FIX;
- FIX Drop Copy;
- TWIME.

They must not be inserted into `fast_sensor`, project commands, Issues, PRs or repository files.

The FAST test depends on the service and routing assigned by MOEX to the registered external static IPv4. Support confirmation remains pending.

## 10. Security and repository policy

Never commit:

```text
logins
passwords
API keys
VPN profiles or credentials
external or private connection addresses
.env or credential files
real raw market-data captures
packet dumps containing live/test exchange payloads
```

Synthetic byte vectors are allowed. Official public XML hashes and public multicast group ranges from MOEX configuration are allowed.

## 11. Consequences for RT-4

- Gate A can be specified and implemented synthetically after separate implementation authorization.
- Gate A cannot declare production endian acceptance without additional evidence.
- Gate B owns one-time tag-34 byte-order verification.
- Gate C owns Snapshot + buffered Incremental recovery.
- Every architecture review must re-check current official MOEX files.
- Waiting for MOEX support does not authorize code, MiMo or a later gate.
