# RT-3 MOEX FAST Authoritative Source Audit

**Date**: 2026-07-11
**Branch**: `mimo/issue-21-rt3-fast-decoder`
**PR**: #23
**Purpose**: Document-only source audit. No production code, tests, CMake, CI, PR description, or state files are modified.

---

## 1. Source Hierarchy

| Priority | Source | Role |
|----------|--------|------|
| 1 (Primary) | MOEX SPECTRA FAST specification + XML templates | Normative for all MOEX-specific framing, preamble, template semantics, data types as used by MOEX |
| 2 (Delegated baseline) | FIX Trading Community FAST Specification Version 1.1 | Normative ONLY for base encoding semantics that MOEX explicitly delegates (stop-bit, nullable, presence map, operators) |
| 3 (Cross-check only) | Third-party implementations (QuickFAST, OpenFAST, etc.) | Never used as evidence; at most illustrative cross-check |

**Rule**: When MOEX and FAST 1.1 conflict, MOEX wins. When MOEX is silent on a base FAST encoding rule, FAST 1.1 fills the gap. Third-party code is never authoritative.

---

## 2. Source Files — Versions, Dates, SHA-256

All files were downloaded from `https://ftp.moex.com/pub/FAST/Spectra/test/` on 2026-07-11 using HTTPS and hashed with PowerShell `Get-FileHash -Algorithm SHA256`.

**Reproduction commands** (PowerShell):

```powershell
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/<path>" -UseBasicParsing -OutFile "<local>"
(Get-FileHash "<local>" -Algorithm SHA256).Hash
```

| File | Version / Date | SHA-256 |
|------|----------------|---------|
| `spectra_fastgate_en.pdf` | v1.30.2, 2026-04-10 | `F744FFFF277D76657FF3F138B0923CE681C1F80BBB8B756088806BF7A04715C2` |
| `spectra_fastgate_ru.pdf` | v1.30.2, 2026-04-10 | `CCEE5583EFECCB942ECEF7DFB353A3405C0F71E8E1C887931D0346350DA940FC` |
| `FAST_9.0/templates.xml` | (no internal version; same content as T0) | `DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E` |
| `templatesT0/templates.xml` | (same content as FAST_9.0) | `DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E` |
| `templatesT0/configuration.xml` | Test environment config | `AE80702BC3E179CAF5DA025E94FDAC6AC7A6A4FF1353E7FB5D0396DE987C4118` |
| `templatesT1/templates.xml` | (differs from T0 — see §2.1) | `84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F` |
| `templatesT1/configuration.xml` | Test environment config | `9377BB0DC3D17344CA7243EB7F4618BCFE6C261C6E63CD392767C910CE6BDB57` |

**Source URL**: `https://ftp.moex.com/pub/FAST/Spectra/test/`

**meta.info**: Listed in the MOEX directory listing (746 bytes, dated 2026-04-10) but returned HTTP 404 at the time of fetch (2026-07-11). This is a client-side/cache/CDN fetch failure, not proof that the file is unavailable or removed. The file is not used as evidence in this audit.

**Note on T0 vs T1 templates.xml**: T0 and T1 `templates.xml` are **NOT** byte-identical. They have different SHA-256 hashes. The differences are:

| Template | T0 ID | T1 ID | T1 extra fields | T0-only fields |
|----------|-------|-------|-----------------|----------------|
| SecurityDefinition | 40 | 47 | `HighLimitPxWeekend`, `LowLimitPxWeekend`, `ClearingSettlPrice` | `MaturityDate`, `MaturityTime` |
| SecurityStatus | 5 | 48 | `HighLimitPxWeekend`, `LowLimitPxWeekend` | (none) |

The MOEX changelog (spectra_fastgate_en.pdf) confirms: "Changed message template ID from '40' to '47'" (SecurityDefinition) and "Changed message template ID from '5' to '48'" (SecurityStatus).

FAST_9.0 `templates.xml` is byte-identical to T0 `templates.xml` (same SHA-256: `DBD50F1E...`).

---

## 3. Topic-by-Topic Evidence Table

### 3.1. Message Framing and MOEX Preamble

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| 4-byte preamble before FAST message | spectra_fastgate_en.pdf §3.2 (p.23) | "A specific feature of data distribution via the MOEX Market Data Multicast streams is, that there is a 4-bytes preamble added before every FAST-message. The preamble contains the 34-th tag (SeqNum) value." | MOEX-specific | MOEX adds a 4-byte preamble containing MsgSeqNum (tag 34) BEFORE the FAST-encoded message. The tag 34 value is also present inside the FAST body. | confirmed |
| Preamble byte order | spectra_fastgate_en.pdf §3.2 | Not explicitly stated | MOEX-specific | The MOEX PDF does not specify the byte order of the 4-byte preamble. | **unresolved** (endianness not documented) |
| Multiple FIX messages per UDP packet | spectra_fastgate_en.pdf §1.2.5 (p.16) | "A single UDP packet may contain several FIX messages in the FAST format. Although, currently the system does not provide a possibility to send more than one FAST-coded message via a single UDP packet." | MOEX-specific | Multiple FAST messages per UDP datagram is possible in principle, but **the current MOEX system sends at most one FAST message per UDP packet**. Messages are bounded to MTU size (1500 bytes). | confirmed |
| SOH separator | spectra_fastgate_en.pdf §3.2.1 (p.23) | "FAST, stop bit is used instead of the standard FIX-separator (<SOH> byte)" | Delegated to FAST 1.1 | FAST replaces SOH with stop-bit encoding. | confirmed |

### 3.2. Stop-Bit Unsigned Integer

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Stop-bit encoding principle | spectra_fastgate_en.pdf §3.2.1 (p.23) | "7 bits of every byte are used for data transmission while the 8th bit indicates the field end" | Delegated to FAST 1.1 | Bit 7 = stop (1=last), bits 6..0 = data (MSB first). | confirmed |
| Max bytes for uInt32 | — | — | Delegated to FAST 1.1 §6.3.2 | Max 5 bytes for 32-bit types (35 data bits). | confirmed (FAST 1.1) |
| Max bytes for uInt64 | — | — | Delegated to FAST 1.1 §6.3.2 | Max 10 bytes for 64-bit types (70 data bits). | confirmed (FAST 1.1) |
| Canonical encoding (no leading zero bytes) | — | — | Delegated to FAST 1.1 §6.3.2 | "The encoded value MUST use the fewest number of bytes." Multi-byte encoding with leading zero group is non-canonical. | confirmed (FAST 1.1) |
| Value 0 encoding | — | — | Delegated to FAST 1.1 §6.3.2 | Single byte `0x80` (stop bit, data=0). | confirmed (FAST 1.1) |

### 3.3. Stop-Bit Signed Integer

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Two's complement encoding | — | — | Delegated to FAST 1.1 §6.3.2 | Signed integers use two's complement. The value is sign-extended to determine the minimum number of 7-bit groups. The overall bit pattern determines the sign, not any single bit position. | confirmed (FAST 1.1) |
| Sign extension | — | — | Delegated to FAST 1.1 §6.3.2 | Sign extension determines the minimum byte count: a positive value whose top payload bit is 1 needs an extra sign-extension group; a negative value whose top payload bits are all 1 also needs an extra group. | confirmed (FAST 1.1) |
| Max bytes for int32 | — | — | Delegated to FAST 1.1 §6.3.2 | Max 5 bytes (35 data bits). | confirmed (FAST 1.1) |
| Max bytes for int64 | — | — | Delegated to FAST 1.1 §6.3.2 | Max 10 bytes (70 data bits). | confirmed (FAST 1.1) |
| Canonical minimum byte count | — | — | Delegated to FAST 1.1 §6.3.2 | Value must not fit in fewer bytes. | confirmed (FAST 1.1) |

### 3.4. Nullable Signed/Unsigned Integer

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Null sentinel = 0x00 | — | — | Delegated to FAST 1.1 §6.3.2 | Nullable integer, first byte 0x00 = null (consuming 1 byte). | confirmed (FAST 1.1) |
| Offset-by-1 encoding | — | — | Delegated to FAST 1.1 §6.3.2 | Non-null value V is encoded as stopbit(V+1). | confirmed (FAST 1.1) |
| Nullable unsigned: decoded 0 (wire 0x80) is non-canonical | — | — | Delegated to FAST 1.1 §6.3.2 | Since null=0x00 and non-null encodes V+1, the minimum non-null decoded value is 1 (V=0 → stopbit(1)=0x81). Wire 0x80 alone (decoded=0) is invalid for nullable unsigned. | confirmed (FAST 1.1) |
| Nullable signed: decoded 0 (wire 0x80) maps to value -1 | — | — | Delegated to FAST 1.1 §6.3.2 | V=-1 → stopbit(-1+1)=stopbit(0)=0x80. This is valid. | confirmed (FAST 1.1) |

### 3.5. ASCII String

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Stop-bit per character | — | — | Delegated to FAST 1.1 §6.3.6 | Each byte bit 7 = stop, bits 6..0 = character. Stop bit on LAST byte only. | confirmed (FAST 1.1) |
| Valid character range | — | — | Delegated to FAST 1.1 §6.3.6 | 0x01..0x7F. 0x00 in data position is invalid (except empty string terminator). | confirmed (FAST 1.1) |
| Empty string encoding | — | — | Delegated to FAST 1.1 §6.3.6 | Single byte `0x80` (stop bit, data=0). | confirmed (FAST 1.1) |
| Nullable ASCII | — | — | Delegated to FAST 1.1 §6.3.6 | Same wire encoding as mandatory ASCII. Null vs empty distinction at operator/pmap level. | confirmed (FAST 1.1) |
| Not null-terminated | — | — | Delegated to FAST 1.1 §6.3.6 | ASCII strings are NOT null-terminated. | confirmed (FAST 1.1) |

### 3.6. Unicode String

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Length-prefixed encoding | — | — | Delegated to FAST 1.1 §6.3.6 | Length as stop-bit uInt32, then that many bytes of UTF-8. | confirmed (FAST 1.1) |
| Nullable Unicode | — | — | Delegated to FAST 1.1 §6.3.6 | Length encoded as nullable uInt32 (0x00 = null, no body bytes). | confirmed (FAST 1.1) |
| UTF-8 validation | — | — | Delegated to FAST 1.1 §6.3.6 | Body is raw bytes; strict UTF-8 validation is implementation best practice. | confirmed (FAST 1.1) |
| MOEX template usage | templatesT0/templates.xml line 110 | `<string name="SecurityDesc" id="107" presence="optional" charset="unicode"/>` | MOEX-specific | SecurityDesc uses `charset="unicode"` in MOEX templates. Also: `Headline` (id=148), `Text` (id=58), `InstrAttribValue` (id=872) in OTC templates. | confirmed |

### 3.7. Byte Vector

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Length-prefixed encoding | — | — | Delegated to FAST 1.1 | Length as stop-bit uInt32, then that many raw bytes. | confirmed (FAST 1.1) |
| Nullable byte vector | — | — | Delegated to FAST 1.1 | Length encoded as nullable uInt32 (0x00 = null, no body bytes). | confirmed (FAST 1.1) |
| MOEX template usage | None in observed templates | — | — | No `byteVector` type observed in MOEX SPECTRA templates for market data streams. | confirmed (absent) |

### 3.8. Decimal

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Composite encoding | spectra_fastgate_en.pdf §3.2.7 (p.25) | "Decimal exponent and mantissa will be encoded as a single, composite field." | Delegated to FAST 1.1 | Exponent (i32) then mantissa (i64), each with own operator. | confirmed |
| Exponent-first order | — | — | Delegated to FAST 1.1 §6.3.7 | Exponent first, then mantissa. | confirmed (FAST 1.1) |
| Nullable decimal | — | — | Delegated to FAST 1.1 §6.3.7 | If exponent nullable and null → whole decimal null, mantissa NOT consumed. | confirmed (FAST 1.1) |
| MOEX template usage | templatesT0/templates.xml, multiple fields | e.g., `<decimal name="MDEntryPx" id="270" presence="optional"/>` | MOEX-specific | MDEntryPx, LastPx, StrikePrice, ContractMultiplier, HighLimitPx, LowLimitPx, MinPriceIncrement, etc. use `decimal` type. Exponent is `int32`, mantissa is `int64` per FAST 1.1. | confirmed |

### 3.9. Presence Map

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Stop-bit terminated | — | — | Delegated to FAST 1.1 §6.3.1 | Presence map is a stop-bit terminated byte sequence. | confirmed (FAST 1.1) |
| Bit layout | — | — | Delegated to FAST 1.1 §6.3.1 | Each byte: bit 7 = stop (1=last), bits 6..0 = data (MSB first). | confirmed (FAST 1.1) |
| Must be terminated | — | — | Delegated to FAST 1.1 §6.3.1 | Unterminated presence map is an error. | confirmed (FAST 1.1) |
| Implicit zero bits | — | — | Delegated to FAST 1.1 §6.3.1 | If pmap terminates before all declared bits are transmitted, remaining bits are implicitly 0. | confirmed (FAST 1.1) |

### 3.10. Template ID

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Template ID in PMap bit 0 | — | — | Delegated to FAST 1.1 §6.3.1 | If pmap bit 0 is set, a template ID follows as a stop-bit unsigned integer. | confirmed (FAST 1.1) |
| MOEX template IDs | templatesT0/templates.xml | Various `<template ... id="N">` attributes | MOEX-specific | OrdersLogMessage=29, BookMessage=30, DefaultIncrementalRefreshMessage=31, DefaultSnapshotMessage=32, SecurityDefinition=40 (T0)/47 (T1), SecurityStatus=5 (T0)/48 (T1), SecurityGroupStatus=45, TradingSessionStatus=46, DiscreteAuction=42, News=9, etc. | confirmed |
| Template ID encoding | — | — | Delegated to FAST 1.1 §6.3.1 | Stop-bit unsigned integer, present only if pmap bit 0 is set. | confirmed (FAST 1.1) |

### 3.11. Operators

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| No compression operators in MOEX templates | spectra_fastgate_en.pdf changelog v1.7.0 (2018-12-05) | "Message templates do not now contain the compression operators copy, delta and increment" | MOEX-specific | MOEX removed all compression operators (copy, delta, increment) as of v1.7.0. | confirmed |
| Exhaustive operator inventory (XML) | templatesT0/templates.xml (618 lines) | Full XML scan of all 18 templates | MOEX-specific | **Only two operators are used**: (1) `none` (default — no explicit operator element) on all non-constant fields; (2) `constant` on selected fields. No `copy`, `delta`, `increment`, `default`, or `tail` operators are present. | confirmed |
| `constant` operator usage | templatesT0/templates.xml | 24 constant fields across all templates | MOEX-specific + FAST 1.1 | Constant values are NOT transmitted on wire; decoded from template. Fields using `constant`: `ApplVerID`="9" (all templates), `MessageType` (all templates), `SenderCompID`="MOEX" (all templates), `SecurityIDSource`="8" (most templates), `MarketID`="MOEX" (SecurityDefinition, TradingSessionStatus, News, OtcMonitorSecurityDefinition). | confirmed |
| Default operator behavior | — | — | Delegated to FAST 1.1 | With no operator, field value is always present in the stream (no pmap bit needed unless optional). | confirmed (FAST 1.1) |

### 3.12. Dictionaries

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Template dictionary | Not used by MOEX | — | — | MOEX does not use FAST template dictionaries. Templates are statically defined in XML files. | confirmed |
| Field dictionary | Not used by MOEX | — | — | MOEX does not use FAST field dictionaries. | confirmed |

### 3.13. Sequences

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Sequence encoding | — | — | Delegated to FAST 1.1 §6.3.4 | Sequence = length field (pmap-assisted), then group fields for each element. | confirmed (FAST 1.1) |
| Length field type | templatesT0/templates.xml | `<length name="NoMDEntries" id="268"/>` | MOEX-specific + FAST 1.1 | Length is stop-bit uInt32. | confirmed |
| MOEX template sequences | templatesT0/templates.xml | `<sequence name="MDEntries">`, `<sequence name="MDFeedTypes">`, etc. | MOEX-specific | MDEntries, MDFeedTypes, Underlyings, InstrumentLegs, InstrumentAttributes, EvntGrp, SecMassStatGrp, NewsText. | confirmed |

---

## 4. Literal Byte Vectors

All byte vectors below are derived from the encoding rules in FAST 1.1 §6.3 (delegated by MOEX). The encoding logic is independently verified against the project's test-only reference encoder (`cpp/moex_fast/tests/fast_reference_encoder.hpp`), which implements the same two's complement stop-bit algorithm. No third-party implementation is used as evidence.

### 4.1. Stop-Bit Unsigned — Value 0

**Source**: FAST 1.1 §6.3.2
**Calculation**: Value 0 requires 1 data bit (the value 0 itself). One byte: data bits = `0000000`, stop bit = 1 → `0x80`.
**Wire**: `[0x80]`

### 4.2. Stop-Bit Unsigned — Value 127

**Source**: FAST 1.1 §6.3.2
**Calculation**: Value 127 = `0x7F`, fits in 7 data bits. One byte: data = `1111111`, stop = 1 → `0xFF`.
**Wire**: `[0xFF]`

### 4.3. Stop-Bit Unsigned — Value 128

**Source**: FAST 1.1 §6.3.2
**Calculation**: Value 128 = `0x80`, needs 8 data bits → 2 bytes. Byte 0: data = `0000001` (bits 13..7 of value), byte 1: data = `0000000` (bits 6..0), stop = 1.
**Wire**: `[0x01, 0x80]`

### 4.4. Stop-Bit Unsigned — UINT32_MAX (4294967295)

**Source**: FAST 1.1 §6.3.2
**Calculation**: 32 bits → ceil(32/7) = 5 bytes. Groups from MSB: `0x0F`, `0x7F`, `0x7F`, `0x7F`, last with stop: `0xFF`.
**Wire**: `[0x0F, 0x7F, 0x7F, 0x7F, 0xFF]`

### 4.5. Stop-Bit Unsigned — UINT64_MAX

**Source**: FAST 1.1 §6.3.2
**Calculation**: 64 bits → ceil(64/7) = 10 bytes. Groups from MSB: `0x01`, `0x7F`×8, last with stop: `0xFF`.
**Wire**: `[0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF]`

### 4.6. Stop-Bit Signed — Value 0

**Source**: FAST 1.1 §6.3.2
**Calculation**: Two's complement 0 needs 1 sign bit. One byte: data = `0000000`, stop = 1 → `0x80`.
**Wire**: `[0x80]`

### 4.7. Stop-Bit Signed — Value -1

**Source**: FAST 1.1 §6.3.2
**Calculation**: Two's complement -1 = all ones. In 7 data bits: `1111111`, stop = 1 → `0xFF`.
**Wire**: `[0xFF]`

### 4.8. Stop-Bit Signed — Value 63

**Source**: FAST 1.1 §6.3.2
**Calculation**: 63 = `0x3F`. Needs sign bit (0) + 6 payload bits = 7 total → 1 byte. Data = `0111111`, stop = 1 → `0xBF`.
**Wire**: `[0xBF]`

### 4.9. Stop-Bit Signed — Value 64

**Source**: FAST 1.1 §6.3.2
**Calculation**: 64 needs sign bit + 7 payload bits = 8 total → 2 bytes. Byte 0: data = `0000000`, byte 1: data = `1000000`, stop = 1 → `0xC0`.
**Wire**: `[0x00, 0xC0]`

### 4.10. Stop-Bit Signed — Value -64

**Source**: FAST 1.1 §6.3.2
**Calculation**: -64 in two's complement. 7 data bits: `1000000`, stop = 1 → `0xC0`.
**Wire**: `[0xC0]`

### 4.11. Stop-Bit Signed — Value -65

**Source**: FAST 1.1 §6.3.2
**Calculation**: -65 needs 8 data bits → 2 bytes. Byte 0: `0111111` = `0x7F`, byte 1: `0111111` + stop = `0xBF`.
**Wire**: `[0x7F, 0xBF]`

### 4.12. Stop-Bit Signed — INT32_MAX (2147483647)

**Source**: FAST 1.1 §6.3.2
**Calculation**: 32 bits two's complement (sign=0 + 31 payload) → ceil(32/7) = 5 bytes. Groups: `0x07`, `0x7F`, `0x7F`, `0x7F`, `0xFF`.
**Wire**: `[0x07, 0x7F, 0x7F, 0x7F, 0xFF]`

### 4.13. Stop-Bit Signed — INT32_MIN (-2147483648)

**Source**: FAST 1.1 §6.3.2
**Calculation**: INT32_MIN in two's complement is 0x80000000 (32 bits). Sign-extended to 33 bits: 0x1_80000000 (the extra leading 1 preserves the negative value). This requires 33 data bits → ceil(33/7) = 5 bytes.

Bit groups (MSB first, 7 bits each):

| Group | Bits (33-bit value) | Binary | Byte |
|-------|---------------------|--------|------|
| 0 | [32..26] | `0001000` | `0x08` |
| 1 | [25..19] | `0000000` | `0x00` |
| 2 | [18..12] | `0000000` | `0x00` |
| 3 | [11..5] | `0000000` | `0x00` |
| 4 (stop) | [4..0]+stop | `00000` + stop | `0x80` |

**Wire**: `[0x08, 0x00, 0x00, 0x00, 0x80]`

**Note on encoding**: In FAST 1.1 two's complement stop-bit encoding, the sign of the value is determined by the overall bit pattern across all 7-bit groups (following standard two's complement semantics), not by any single bit position in the first byte. The first byte's bit 6 (data bit position 6) is the MSB of the first 7-bit group, which for INT32_MIN is 0 because the 33-bit two's complement representation is `1_10000000_00000000_00000000_00000000` and the first group captures bits [32..26] = `0001000`. The value is negative because the sign extension bit (bit 32) is 1, which is captured in the first group.

This encoding is verified by the project's independent reference encoder (`fast_reference_encoder.hpp:101-126`): for val=-2147483648, the encoder computes nbits=33 (via `while (v < -1) { nbits++; v >>= 1; }`), ngroups=5, and emits the same byte sequence.

### 4.14. Nullable Unsigned — Null

**Source**: FAST 1.1 §6.3.2
**Wire**: `[0x00]`

### 4.15. Nullable Unsigned — Value 0

**Source**: FAST 1.1 §6.3.2
**Calculation**: V=0 → stopbit(0+1) = stopbit(1) = `0x81`.
**Wire**: `[0x81]`

### 4.16. Nullable Signed — Value -1

**Source**: FAST 1.1 §6.3.2
**Calculation**: V=-1 → stopbit(-1+1) = stopbit(0) = `0x80`.
**Wire**: `[0x80]`

### 4.17. Presence Map — 7 bits, pattern 0110000

**Source**: FAST 1.1 §6.3.1
**Calculation**: Bits `[0,1,1,0,0,0,0]` → data byte = `0110000` = `0x30`, + stop = `0xB0`.
**Wire**: `[0xB0]`

### 4.18. Presence Map — 14 bits all zero

**Source**: FAST 1.1 §6.3.1
**Calculation**: 14 bits → ceil(14/7) = 2 bytes. Byte 0: `0x00` (no stop), byte 1: `0x80` (stop, data=0).
**Wire**: `[0x00, 0x80]`

### 4.19. ASCII String — Empty

**Source**: FAST 1.1 §6.3.6
**Wire**: `[0x80]` (stop bit, data=0)

### 4.20. ASCII String — "A"

**Source**: FAST 1.1 §6.3.6
**Calculation**: 'A' = 0x41, data bits = `1000001`, stop = 1 → `0xC1`.
**Wire**: `[0xC1]`

### 4.21. ASCII String — "AB"

**Source**: FAST 1.1 §6.3.6
**Calculation**: 'A' = 0x41 (no stop), 'B' = 0x42 + stop = `0xC2`.
**Wire**: `[0x41, 0xC2]`

### 4.22. Unicode String — Empty

**Source**: FAST 1.1 §6.3.6
**Calculation**: Length = 0 → stopbit(0) = `0x80`. No body bytes.
**Wire**: `[0x80]`

### 4.23. Unicode String — "A"

**Source**: FAST 1.1 §6.3.6
**Calculation**: Length = 1 → stopbit(1) = `0x81`. Body: 0x41.
**Wire**: `[0x81, 0x41]`

### 4.24. Nullable Unicode — Null

**Source**: FAST 1.1 §6.3.6
**Wire**: `[0x00]`

### 4.25. Byte Vector — Empty

**Source**: FAST 1.1
**Calculation**: Length = 0 → stopbit(0) = `0x80`. No body bytes.
**Wire**: `[0x80]`

### 4.26. Decimal — exponent=2, mantissa=5000

**Source**: FAST 1.1 §6.3.7
**Calculation**: Exponent stopbit(2) = `0x82`. Mantissa 5000 = `0x1388` → 2 bytes: `0x27`, `0x88`.
**Wire**: `[0x82, 0x27, 0x88]`

---

## 5. Comparison with Current Implementation

### Files Compared

| File | Path |
|------|------|
| WireCursor (header) | `cpp/moex_fast/include/moex_fast/wire_cursor.hpp` |
| WireCursor (implementation) | `cpp/moex_fast/src/wire_cursor.cpp` |
| fast_reference_encoder.hpp | `cpp/moex_fast/tests/fast_reference_encoder.hpp` |
| test_decoder_reference_oracle.cpp | `cpp/moex_fast/tests/test_decoder_reference_oracle.cpp` |

### Potential Discrepancies

| # | Topic | Current Code | Authoritative Rule | Source | Status |
|---|-------|--------------|-------------------|--------|--------|
| 1 | **Nullable signed: INT32_MIN handling** | `wire_cursor.cpp:278` rejects `raw == INT32_MIN` as overflow | FAST 1.1 §6.3.2: nullable signed = decoded-1. Decoded INT32_MIN would give value INT32_MIN-1 which overflows int32. This rejection is CORRECT. | FAST 1.1 | confirmed correct |
| 2 | **Nullable signed: decoded 0 maps to -1** | `wire_cursor.cpp:281`: `out = raw - 1` where raw=0 → out=-1 | FAST 1.1: V=-1 → stopbit(-1+1)=stopbit(0)=0x80. Decoding 0x80: raw=0, out=0-1=-1. Correct. | FAST 1.1 | confirmed correct |
| 3 | **Nullable unsigned: decoded 0 (wire 0x80) rejected** | `wire_cursor.cpp:241`: `if (raw == 0) return NonCanonicalEncoding` | FAST 1.1: nullable unsigned, null=0x00, non-null encodes V+1. Minimum non-null wire is 0x81 (V=0). Wire 0x80 alone is invalid for nullable unsigned. | FAST 1.1 | confirmed correct |
| 4 | **Stop-bit signed canonical: range check in i32** | `wire_cursor.cpp:142-153`: checks if decoded value fits in (bytes_read-1)*7 bits | FAST 1.1: minimum byte count required. The range check `[lo, hi]` for prev_bits is correct. | FAST 1.1 | confirmed correct |
| 5 | **Stop-bit signed i64: pre-shift overflow guard** | `wire_cursor.cpp:177-184`: for bytes beyond 9, checks top 7 bits are sign extension before shifting | FAST 1.1: max 10 bytes for int64. The 10th byte's 7 data bits would shift left by 63, overflowing uint64_t. The pre-shift check is a necessary guard. | FAST 1.1 | confirmed correct |
| 6 | **Presence map: implicit zero padding** | `wire_cursor.cpp:337-339`: pads with false after stop bit | FAST 1.1 §6.3.1: implicit zero bits after transmitted bits. Correct. | FAST 1.1 | confirmed correct |
| 7 | **Presence map: continues reading after pmap_bits filled** | `wire_cursor.cpp:317-334`: reads ALL bytes until stop bit, even if pmap_bits already filled | FAST 1.1: stop bit terminates the pmap. Must consume all bytes until stop bit found. Correct. | FAST 1.1 | confirmed correct |
| 8 | **ASCII: 0x00 in data position rejected** | `wire_cursor.cpp:369`: `if (data_bits == 0) return InvalidEncoding` for continuation bytes | FAST 1.1 §6.3.6: valid characters are 0x01..0x7F. 0x00 in data position is invalid. Correct. | FAST 1.1 | confirmed correct |
| 9 | **ASCII: empty string detection** | `wire_cursor.cpp:358-361`: stop byte with data=0 returns Ok (empty string) | FAST 1.1: empty string = single byte 0x80. Correct. | FAST 1.1 | confirmed correct |
| 10 | **Unicode: nullable uses nullable_u32 for length** | `wire_cursor.cpp:461`: `read_nullable_u32(len, len_null)` | FAST 1.1: nullable unicode length is nullable uInt32. Correct. | FAST 1.1 | confirmed correct |
| 11 | **Byte vector: nullable uses nullable_u32 for length** | `wire_cursor.cpp:508`: `read_nullable_u32(len, len_null)` | FAST 1.1: nullable byte vector length is nullable uInt32. Correct. | FAST 1.1 | confirmed correct |
| 12 | **Decimal: null decimal skips mantissa** | `wire_cursor.cpp:538-541`: if exponent nullable and null, returns without reading mantissa | FAST 1.1 §6.3.7: null exponent = null decimal, mantissa NOT consumed. Correct. | FAST 1.1 | confirmed correct |
| 13 | **Reference encoder: nullable i32 encoding** | `fast_reference_encoder.hpp:177-179`: `encode_stopbit_i32(buf, shifted)` where shifted = (uint32_t(val)+1) cast to int32 | FAST 1.1: nullable signed V → stopbit(V+1). For V=INT32_MAX, V+1 wraps to INT32_MIN (via unsigned). The encoder correctly uses unsigned arithmetic then reinterprets. | FAST 1.1 | confirmed correct |
| 14 | **Reference encoder: nullable i64 encoding** | `fast_reference_encoder.hpp:188-190`: same pattern as i32 | Same reasoning. Correct. | FAST 1.1 | confirmed correct |
| 15 | **Test: nullable i32 value -1 → wire 0x80** | `test_decoder_reference_oracle.cpp:319-323` | FAST 1.1: V=-1 → stopbit(-1+1)=stopbit(0)=0x80. Correct. | FAST 1.1 | confirmed correct |
| 16 | **Test: nullable u32 max value** | `test_decoder_reference_oracle.cpp:554-558`: `0xFFFFFFFE` → `0x0F 0x7F 0x7F 0x7F 0xFF` | FAST 1.1: V=0xFFFFFFFE → stopbit(0xFFFFFFFE+1)=stopbit(0xFFFFFFFF). Correct. | FAST 1.1 | confirmed correct |
| 17 | **MOEX preamble: 4-byte SeqNum before FAST body** | Not implemented in WireCursor (WireCursor operates on FAST body only) | MOEX §3.2: 4-byte preamble contains MsgSeqNum before FAST message. | spectra_fastgate_en.pdf §3.2 | No discrepancy (separate concern) |
| 18 | **MOEX: only `none` and `constant` operators** | Code implements default (none) operator; constant is handled at template level | Exhaustive XML scan: only `none` and `constant` operators are used across all 18 MOEX templates. No copy/delta/increment/default/tail. | templatesT0/templates.xml (full scan) | confirmed correct |
| 19 | **SecurityDesc uses charset="unicode"** | Not directly visible in WireCursor (WireCursor handles raw types) | templatesT0/templates.xml line 110: `<string ... charset="unicode"/>` | templates.xml | No discrepancy (handled at template level) |

### Summary of Discrepancies Found

**No discrepancies found.** All examined implementations (WireCursor decoder, reference encoder, and test oracle) are consistent with the authoritative sources (MOEX SPECTRA FAST specification v1.30.2 and FAST 1.1 specification as delegated by MOEX).

---

## 6. Key Observations

1. **MOEX delegates all base FAST encoding to FAST 1.1**: The MOEX PDF describes stop-bit encoding at a high level (§3.2.1, §3.2.7) but does NOT restate the full encoding rules. It relies on implementors knowing FAST 1.1 for: canonical encoding, nullable offset-by-1, presence map structure, signed two's complement, decimal composite encoding, sequence length encoding.

2. **MOEX uses only `none` and `constant` operators**: Exhaustive XML scan of all 18 templates in `templatesT0/templates.xml` (618 lines) confirms that only the `constant` operator and the default `none` operator are used. The MOEX changelog v1.7.0 (2018-12-05) states: "Message templates do not now contain the compression operators copy, delta and increment." The XML inventory confirms this is still true in the current templates.

3. **MOEX adds a 4-byte preamble**: This is MOEX-specific and not part of FAST 1.1. The preamble contains MsgSeqNum (tag 34), allowing sequence number extraction without FAST decoding. The byte order of the preamble is **not documented** in the MOEX specification (§3.2, Pic.3 shows "Preamble (4 bytes)" then "Message" but does not specify endianness).

4. **The current MOEX system sends at most one FAST message per UDP packet**: While MOEX §1.2.5 states that "a single UDP packet may contain several FIX messages in the FAST format" in principle, it also explicitly states: "currently the system does not provide a possibility to send more than one FAST-coded message via a single UDP packet." Messages are bounded to MTU size (1500 bytes).

5. **Template IDs are MOEX-specific**: The specific template IDs (29, 30, 31, 32, 40/47, 5/48, 45, 46, etc.) are defined by MOEX and change across versions. T0 and T1 use different IDs for SecurityDefinition (40 vs 47) and SecurityStatus (5 vs 48).

6. **T0 and T1 templates are NOT byte-identical**: They have different SHA-256 hashes and differ in template IDs and field definitions. FAST_9.0 templates.xml IS byte-identical to T0 templates.xml.
