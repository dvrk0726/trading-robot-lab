# RT-4 MOEX FAST source update — 2026-07-15

**Issues:** #38, #40  
**Scope:** documentation and source provenance only  
**Security:** no credentials, external/private IP addresses, VPN endpoints or raw market-data packets are recorded

## 1. Purpose

This document records evidence obtained after the historical RT-3 source audit dated 2026-07-11 and the later MOEX access/connectivity checks performed on 2026-07-15.

The historical file `docs/rt3_moex_fast_authoritative_source_audit.md` remains unchanged because it correctly records what the official MOEX endpoints returned on its audit date. This update records later endpoint contents and Stage 0 connectivity checks without rewriting history.

## 2. Source hierarchy

1. Official MOEX SPECTRA FAST specification and test files.
2. FIX FAST 1.1 only for base FAST semantics not fully specified by MOEX.
3. Third-party implementations only as cross-check.

Primary official directories:

```text
https://ftp.moex.com/pub/FAST/Spectra/test/
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/
https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/
```

MOEX support also supplied the same directories using `ftp://` links. The repository records the public resource locations only; no private connection details are stored.

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

Evidence status as of 2026-07-15:

```text
4-byte preamble: confirmed
contains MsgSeqNum(34): confirmed
same tag in FAST body: confirmed
external preamble endian: unresolved in official text
```

The official XML cannot define this value because the preamble is outside the FAST body.

A public MOEX-specific implementation was found to interpret the four bytes as little-endian. This is cross-check evidence only and is not accepted as normative.

### 5.1 Superseded: former explicit LittleEndian/BigEndian and Gate B AutoVerify decision

The former RT-4 decision (Gate A framing supports explicit `LittleEndian` and `BigEndian`; no default value; Gate B compares both interpretations with decoded tag 34; ambiguous or neither-match cases fail closed) is superseded.

The local no-packet connectivity test (section 8) did not verify endian because no UDP market-data packet reached the process during the bounded check. Endian was later resolved independently by written MOEX support.

### 5.2 Written MOEX support clarification — 2026-07-16

Written MOEX support confirmed (paraphrased, no personal information or ticket identifiers stored):

- the UDP FAST preamble is four bytes and uses little-endian byte order;
- MsgSeqNum value 1 is encoded as `01 00 00 00`;
- the same little-endian rule applies to SPECTRA T0, T1 and production feeds;
- the numeric preamble value is guaranteed to equal decoded FAST MsgSeqNum tag 34;
- this concerns the UDP multicast preamble, not the TCP Historical Replay length prefix.

Gate A now uses fixed little-endian decoding with no runtime byte-order selector, alternative production byte-order path or automatic endian discovery. Gate B consumes the fixed little-endian external value from A1 and compares it numerically against decoded tag 34.

The accepted C++ representation remains `std::uint32_t` and modulo-2^32 sequence arithmetic. The written MOEX support did not separately repeat the word "unsigned".

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

Stage 0 intentionally uses only gap/statistics/order checking. Raw and decoded dump modes are not used.

## 7. T0 configuration inspection

The current T0 configuration parsed successfully.

Observed structure includes separate physical A and B multicast connections. Examples of public exchange-group ranges and ports:

```text
Feed A multicast groups: 239.195.12.x, ports 480xx
Feed B multicast groups: 239.195.140.x, ports 490xx
```

The configuration includes Instrument Replay, Instrument Incremental and ordinary Incremental feeds.

A and B are treated as physical copies of one logical sequence, not primary and backup.

## 8. Initial Stage 0 connectivity test

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

Observed result before MOEX access confirmation:

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
- this result is not evidence of a framing defect.

## 9. Connection-service boundary

Credentials for these services are not FAST multicast credentials:

- SPECTRA Terminal;
- trading FIX;
- FIX Drop Copy;
- TWIME.

They must not be inserted into `fast_sensor`, project commands, Issues, PRs or repository files.

## 10. MOEX access confirmation and official VPN instruction

MOEX support later confirmed that access had been opened for the registered external static IPv4 and supplied a Windows VPN instruction.

The instruction specifies:

```text
connection type: workplace VPN over the existing Internet connection
VPN type: PPTP
data encryption: optional; connection may proceed without encryption
authentication examples: CHAP and MS-CHAP v2 enabled, PAP disabled
```

The support reply stated that credentials may be left empty. The general instruction contains a legacy example using arbitrary placeholder values. The per-customer support reply has priority for this test.

The VPN endpoint and registered external IPv4 are private connection data and are not stored.

## 11. Bounded VPN diagnostics

The following read-only or connection-attempt evidence was obtained from the registered home static IPv4:

```text
current external IPv4 matches the address registered with MOEX
Windows remote-access result: error 807
TCP connection test to VPN endpoint, port 1723: failed
TcpTestSucceeded: False
```

Because TCP 1723 fails, the connection does not reach PPTP authentication or the later GRE data-tunnel stage. FAST reception is not tested while this control connection is unavailable.

Local checks:

```text
Windows Firewall profiles: enabled
DefaultOutboundAction: Allow for Domain, Private and Public
explicit enabled outbound block rules targeting VPN endpoint: none found
registered antivirus products: Windows Defender only
third-party antivirus/network filter: not found
```

One unrelated explicit outbound block rule was inspected. It applies only to a fixed list of remote addresses; the VPN endpoint is not in that list.

A mobile-network test also returned failure, but it is not acceptance evidence because MOEX access control is tied to the registered home external IPv4.

Current interpretation:

- Windows VPN parameters match the supplied MOEX instruction;
- the registered home external IPv4 matches the submitted address;
- normal Windows outbound policy does not explain the failure;
- the unresolved boundary is the path to the PPTP control endpoint or the MOEX-side server/ACL;
- router, firewall and system configuration must not be changed speculatively;
- a detailed follow-up was sent to MOEX support and a reply is pending.

## 12. Repository checkpoint

```text
RT-4 specification Issue #38: closed completed
RT-4 specification PR #39: merged
reviewed PR head: afd128a49584fce1131323ac7b19e5b5d7b1997a
main merge SHA: 136293ede211619b7d9198d85ed3afb0f2577514
post-merge main CI #189: success
RT-4 Gate A: completed in Draft PR #52
accepted technical checkpoint: 105f7d878833e30ee92644c312d0e94cb632b87d
CI #234, run ID 29526060857, success, 6 jobs
98 internal Gate A tests
Final Architecture Review pending; Ready not authorized; merge not authorized
```

## 13. Security and repository policy

Never commit:

```text
logins
passwords
API keys
VPN endpoints or profiles
external or private connection addresses
.env or credential files
real raw market-data captures
packet dumps containing live/test exchange payloads
```

Synthetic byte vectors are allowed. Official public XML hashes, public documentation URLs and public multicast group ranges from MOEX configuration are allowed.

## 14. Consequences for RT-4

- RT-4 specification is complete and merged.
- Gate A is implemented and documented in Draft PR #52.
- Accepted technical checkpoint: 105f7d878833e30ee92644c312d0e94cb632b87d.
- External preamble endian is resolved by written MOEX support (2026-07-16): fixed little-endian.
- Gate B consumes the fixed little-endian value from A1 and compares numerically against decoded tag 34; mismatch fails closed.
- Gate C owns Snapshot + buffered Incremental recovery.
- Every architecture review must re-check current official MOEX files.
- Final Architecture Review is pending. Ready and merge are not authorized.
- This documentation change does not authorize Ready, merge, Gate B/C/D, RT-5, RT-6, CI-2 or production/live trading.
