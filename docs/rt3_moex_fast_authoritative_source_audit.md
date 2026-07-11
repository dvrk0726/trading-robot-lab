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

### 2.1. FIX FAST 1.1 Specification (Normative Delegated Baseline)

| Attribute | Value |
|-----------|-------|
| Title | FAST Specification Version 1.1 |
| Publisher | FIX Trading Community (fixtrading.org) |
| Published | 2006-12-20 (last updated on fixtrading.org: 2017-07-28) |
| Download page | `https://fixtrading.org/packages/fast-specification-version-1-1/` |
| Direct download | `https://fixtrading.org/packages/fast-specification-version-1-1/?wpdmdl=2582` |
| Filename | `FAST Specification Version 1.1.pdf` (472.97 KB) |
| SHA-256 | `5047CED8D6A06195EAB777C12E09B41C139EADCFCA87BCCFBC4DACAAB6E297B3` |
| Total pages | 44 |

**Reproduction command** (PowerShell):

```powershell
Invoke-WebRequest -Uri "https://fixtrading.org/packages/fast-specification-version-1-1/?wpdmdl=2582" -UseBasicParsing -OutFile "$env:TEMP\FAST_Specification_v1.1.pdf"
(Get-FileHash "$env:TEMP\FAST_Specification_v1.1.pdf" -Algorithm SHA256).Hash
# Expected: 5047CED8D6A06195EAB777C12E09B41C139EADCFCA87BCCFBC4DACAAB6E297B3
```

### 2.2. MOEX SPECTRA Source Files

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
| `templatesT1/templates.xml` | (differs from T0 — see §2.3) | `84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F` |
| `templatesT1/configuration.xml` | Test environment config | `9377BB0DC3D17344CA7243EB7F4618BCFE6C261C6E63CD392767C910CE6BDB57` |

**Source URL**: `https://ftp.moex.com/pub/FAST/Spectra/test/`

**meta.info**: Listed in the MOEX directory listing (746 bytes, dated 2026-04-10) but returned HTTP 404 at the time of fetch (2026-07-11). The cause of the 404 is **unresolved** — no evidence distinguishes a removed file, a CDN/cache issue, a client-side fetch problem, or an access restriction. The file is not used as evidence in this audit.

### 2.3. T0 vs T1 Template Differences

T0 and T1 `templates.xml` are **NOT** byte-identical. They have different SHA-256 hashes. The differences are:

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
| Stop-bit encoding principle | spectra_fastgate_en.pdf §3.2.1 (p.23) + FAST 1.1 §10.2 (p.21) | MOEX: "7 bits of every byte are used for data transmission while the 8th bit indicates the field end". FAST 1.1: "If the bit is not set, the next byte belongs to the entity, otherwise it is the last byte. The seven bits following the stop bit are significant data bits." | Delegated to FAST 1.1 | Bit 7 = stop (1=last), bits 6..0 = data (MSB first). | confirmed |
| Max bytes for uInt32 | FAST 1.1 §10.6.1 (p.23) | "Integers are represented as stop bit encoded entities." Type bounds: uInt32 = [0, 4294967295] (§6.2.1, p.10) | Delegated to FAST 1.1 | Max 5 bytes for 32-bit types (35 data bits). | confirmed (FAST 1.1) |
| Max bytes for uInt64 | FAST 1.1 §6.2.1 (p.10) | Type bounds: uInt64 = [0, 18446744073709551615] | Delegated to FAST 1.1 | Max 10 bytes for 64-bit types (70 data bits). | confirmed (FAST 1.1) |
| Canonical encoding (no leading zero bytes) | FAST 1.1 §10.6.1 (p.23) | "An integer is overlong if the entity value still represents the same integer after removing seven or more of the most significant bits." | Delegated to FAST 1.1 | Multi-byte encoding with leading zero group is non-canonical (overlong). | confirmed (FAST 1.1) |
| Value 0 encoding | FAST 1.1 Appendix 3.1.2 Ex.2 (p.34) | Mandatory uInt32, input 0 → FAST `0x80` | Delegated to FAST 1.1 | Single byte `0x80` (stop bit, data=0). | confirmed (FAST 1.1, p.34) |

### 3.3. Stop-Bit Signed Integer

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Two's complement encoding | FAST 1.1 §10.6.1.1 (p.23) | "The entity value is a two's complement integer representation [TWOC]. The most significant data bit of the entity value is the sign bit." | Delegated to FAST 1.1 | Signed integers use two's complement. The overall bit pattern determines the sign, not any single bit position in the first byte. | confirmed (FAST 1.1, p.23) |
| Sign extension example | FAST 1.1 §10.6.1.1 NOTE (p.23) | "if the value to encode is 64, which has the binary representation 01000000, the stop bit encoding must be 0x00 0xC0. If we did not have the extra seven leading zero bits, the most significant bit, which is also the sign bit, would be one, and thus the encoding would incorrectly represent -64." | Delegated to FAST 1.1 | Sign extension determines the minimum byte count: a positive value whose top payload bit is 1 needs an extra sign-extension group; a negative value whose top payload bits are all 1 also needs an extra group. | confirmed (FAST 1.1, p.23) |
| Max bytes for int32 | FAST 1.1 §6.2.1 (p.10) | Type bounds: int32 = [-2147483648, 2147483647] | Delegated to FAST 1.1 | Max 5 bytes (35 data bits). | confirmed (FAST 1.1) |
| Max bytes for int64 | FAST 1.1 §6.2.1 (p.10) | Type bounds: int64 = [-9223372036854775808, 9223372036854775807] | Delegated to FAST 1.1 | Max 10 bytes (70 data bits). | confirmed (FAST 1.1) |
| Canonical minimum byte count | FAST 1.1 §10.6.1 (p.23) | "An integer is overlong if the entity value still represents the same integer after removing seven or more of the most significant bits." | Delegated to FAST 1.1 | Value must not fit in fewer bytes. | confirmed (FAST 1.1) |
| Negative example | FAST 1.1 Appendix 3.1.1 Ex.6 (p.33) | Int32, input -8193 → native `0xFF 0xDF 0xFF` → FAST `0x73 0x3F 0xFF` | Delegated to FAST 1.1 | Three bytes: `0111_0011 0011_1111 1111_1111`. Entity value = `111_0011 011_1111 111_1111` = sign-extended two's complement of -8193. | confirmed (FAST 1.1, p.33) |

### 3.4. Nullable Signed/Unsigned Integer

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Nullable NULL entity value | FAST 1.1 §10.4 (p.21) | "All nullable types are constructed in such a way that NULL is represented as a 7-bit entity value where all bits are zero." | Delegated to FAST 1.1 | The nullable NULL **entity value** is 7 zero bits (`0x00`). | confirmed (FAST 1.1, p.21) |
| Nullable NULL wire encoding for integers | FAST 1.1 §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33) | §10.6.1: "The NULL representation of a nullable integer is a 7-bit entity value where all bits are zero." Example: optional uInt32, input null → FAST `0x80` (10000000) | Delegated to FAST 1.1 | On the wire, nullable integer NULL is encoded as `0x80` (stop bit set, 7 data bits all zero). The entity value `0x00` becomes `0x80` when stop-bit encoded. | confirmed (FAST 1.1, p.23 + p.33) |
| Offset-by-1 encoding | FAST 1.1 §10.6.1 (p.23) | "If an integer is nullable, every non-negative integer is incremented by 1 before it is encoded." | Delegated to FAST 1.1 | **Only non-negative** values are incremented by 1 before encoding. Negative signed values are encoded directly (not incremented). NULL (wire `0x80`) is therefore distinct from every non-null encoding. | confirmed (FAST 1.1, p.23) |
| Nullable unsigned: decoded 0 (wire 0x81) is value 0 | FAST 1.1 Appendix 3.1.2 Ex.1 (p.33) | Optional uInt32, input 0 → FAST `0x81` (10000001). "Increment by 1 since field is optional" | Delegated to FAST 1.1 | V=0 → stopbit(0+1) = stopbit(1) = `0x81`. Wire `0x80` alone (entity value 0, decoded as raw=0) is the NULL sentinel, not a valid non-null value for nullable unsigned. | confirmed (FAST 1.1, p.33) |
| Nullable signed: value -1 encoding | FAST 1.1 §10.6.1 (p.23) | "every **non-negative** integer is incremented by 1 before it is encoded." V=-1 is negative → NOT incremented → stopbit(-1) = `0xFF`. | Delegated to FAST 1.1 | NULL (`0x80`) and value -1 (`0xFF`) are **distinct** on the wire. The offset-by-1 rule applies only to non-negative values; negative values use direct two's complement stop-bit encoding. **Defect**: `wire_cursor.cpp:268` checks `data_[pos_] == 0x00` for nullable NULL instead of checking the stop-bit decoded value == 0 (wire `0x80`). `wire_cursor.cpp:281` applies `out = raw - 1` unconditionally instead of only for raw > 0. `fast_reference_encoder.hpp:171` pushes `0x00` for nullable NULL instead of encoding entity value `0x00` with stop bit (`0x80`), and `:177-179` applies `val+1` to all signed values instead of only non-negative ones. | confirmed (FAST 1.1, p.23); code defects documented |

### 3.5. ASCII String

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Stop-bit per character | FAST 1.1 §10.6.3 (p.23-24) | "An ASCII String is represented as a stop bit encoded entity. The entity value is interpreted as a sequence of 7-bit ASCII characters." | Delegated to FAST 1.1 | Each byte: bit 7 = stop (1=last), bits 6..0 = character. Stop bit on LAST byte only. | confirmed (FAST 1.1, p.23) |
| Valid character range | FAST 1.1 §10.6.3 (p.23-24) | The entity value is "a sequence of 7-bit ASCII characters." | Delegated to FAST 1.1 | 0x01..0x7F per character. 0x00 in data position is the zero-preamble, not a valid character. | confirmed (FAST 1.1, p.23) |
| Empty string encoding (non-nullable) | FAST 1.1 Appendix 3.1.3 Ex.2 (p.34) | Mandatory string, input "" (zero length) → FAST `0x80` | Delegated to FAST 1.1 | Single byte `0x80` (stop bit, data=0). | confirmed (FAST 1.1, p.34) |
| Nullable ASCII NULL | FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34) | §10.6.3: "If there are no remaining bits after removing the preamble the value represents the NULL string." The p.24 table shows **entity values** (not wire bytes): entity value `0x00` nullable = NULL. Appendix 3.1.3 Ex.1 (p.34): optional string, input null → FAST `0x80` (10000000). | Delegated to FAST 1.1 | On the wire, nullable ASCII NULL is `0x80` — entity value `0x00` (zero-preamble) stop-bit encoded. This is a completed stop-bit wire sequence. Note: the p.24 table lists entity values; the wire encoding adds the stop bit. | confirmed (FAST 1.1, p.24 + p.34) |
| Nullable ASCII empty string | FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34) | The p.24 table: entity value `0x00 0x00` nullable = Empty String. Appendix 3.1.3 Ex.1 (p.34): optional string, input "" → FAST `0x00 0x80` (00000000 10000000). | Delegated to FAST 1.1 | On the wire, nullable ASCII empty string is `0x00 0x80` — entity value `0x00 0x00` (zero-preamble + zero-preamble = empty non-nullable string) stop-bit encoded. Both bytes form a completed stop-bit wire sequence (second byte has stop bit). | confirmed (FAST 1.1, p.24 + p.34) |
| Not null-terminated | FAST 1.1 §10.6.3 (p.23-24) | "An ASCII String is represented as a stop bit encoded entity." No null terminator mentioned. | Delegated to FAST 1.1 | ASCII strings are NOT null-terminated. | confirmed (FAST 1.1, p.23-24) |

### 3.6. Unicode String

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Length-prefixed encoding | FAST 1.1 §10.6.4 (p.24) | "A Unicode String is represented as a Byte Vector containing the UTF-8 encoded representation of the string." + §10.6.5: "a byte vector field is represented as an Unsigned Integer size preamble followed by the specified number of raw bytes." | Delegated to FAST 1.1 | Length as stop-bit uInt32, then that many bytes of UTF-8. | confirmed (FAST 1.1, p.24) |
| Nullable Unicode | FAST 1.1 §10.6.4 (p.24) | "If a Unicode String is nullable, it is represented by a nullable Byte Vector." + §10.6.5: "A nullable byte vector has a nullable size preamble. The NULL byte vector is represented by a NULL size preamble." | Delegated to FAST 1.1 | Length encoded as nullable uInt32 (NULL = no body bytes). | confirmed (FAST 1.1, p.24) |
| UTF-8 validation | FAST 1.1 §10.6.4 (p.24) | "UTF-8 is defined in the Unicode 3.2 [UNICODE] standard." | Delegated to FAST 1.1 | Body is raw bytes; strict UTF-8 validation is implementation best practice. | confirmed (FAST 1.1, p.24) |
| MOEX template usage | templatesT0/templates.xml line 110 | `<string name="SecurityDesc" id="107" presence="optional" charset="unicode"/>` | MOEX-specific | SecurityDesc uses `charset="unicode"` in MOEX templates. Also: `Headline` (id=148), `Text` (id=58), `InstrAttribValue` (id=872) in OTC templates. | confirmed |

### 3.7. Byte Vector

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Length-prefixed encoding | FAST 1.1 §10.6.5 (p.24) | "a byte vector field is represented as an Unsigned Integer size preamble followed by the specified number of raw bytes. Each byte in the data part has eight significant data bits. As a consequence, the data part is not stop bit encoded." | Delegated to FAST 1.1 | Length as stop-bit uInt32, then that many raw bytes (8 bits each, no stop bits in data). | confirmed (FAST 1.1, p.24) |
| Nullable byte vector | FAST 1.1 §10.6.5 (p.24) | "A nullable byte vector has a nullable size preamble. The NULL byte vector is represented by a NULL size preamble." | Delegated to FAST 1.1 | Length encoded as nullable uInt32 (NULL = no body bytes). | confirmed (FAST 1.1, p.24) |
| MOEX template usage | None in observed templates | — | — | No `byteVector` type observed in MOEX SPECTRA templates for market data streams. | confirmed (absent) |

### 3.8. Decimal

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Composite encoding | spectra_fastgate_en.pdf §3.2.7 (p.25) + FAST 1.1 §10.6.2 (p.23) | MOEX: "Decimal exponent and mantissa will be encoded as a single, composite field." FAST 1.1: "A scaled number is represented as a Signed Integer exponent followed by a Signed Integer mantissa." | Delegated to FAST 1.1 | Exponent (i32) then mantissa (i64), each with own operator. | confirmed |
| Exponent-first order | FAST 1.1 §10.6.2 (p.23) | "A scaled number is represented as a Signed Integer exponent followed by a Signed Integer mantissa." | Delegated to FAST 1.1 | Exponent first, then mantissa. | confirmed (FAST 1.1, p.23) |
| Nullable decimal | FAST 1.1 §10.6.2 (p.23) | "If a scaled number is nullable, the exponent is nullable and the mantissa is non-nullable. A NULL scaled number is represented as a NULL exponent. The mantissa is present in the stream iff the exponent is not NULL." | Delegated to FAST 1.1 | If exponent nullable and null → whole decimal null, mantissa NOT consumed. | confirmed (FAST 1.1, p.23) |
| MOEX template usage | templatesT0/templates.xml, multiple fields | e.g., `<decimal name="MDEntryPx" id="270" presence="optional"/>` | MOEX-specific | MDEntryPx, LastPx, StrikePrice, ContractMultiplier, HighLimitPx, LowLimitPx, MinPriceIncrement, etc. use `decimal` type. Exponent is `int32`, mantissa is `int64` per FAST 1.1. | confirmed |

### 3.9. Presence Map

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Stop-bit terminated | FAST 1.1 §10.5 (p.21) | "A presence map is represented as a stop bit encoded entity." | Delegated to FAST 1.1 | Presence map is a stop-bit terminated byte sequence. | confirmed (FAST 1.1, p.21) |
| Bit layout | FAST 1.1 §10.2 (p.21) | "the most significant bit in each byte indicates whether the next byte is part of the entity. If the bit is not set, the next byte belongs to the entity, otherwise it is the last byte. The seven bits following the stop bit are significant data bits." | Delegated to FAST 1.1 | Each byte: bit 7 = stop (1=last), bits 6..0 = data (MSB first). | confirmed (FAST 1.1, p.21) |
| Must be terminated | FAST 1.1 §10.5 (p.21) | "Logically a presence map has an infinite suffix of zeroes." + "It is a reportable error [ERR R7] if a presence map is overlong." | Delegated to FAST 1.1 | Unterminated presence map is an error. | confirmed (FAST 1.1, p.21) |
| Implicit zero bits | FAST 1.1 §10.5 (p.21) | "This makes it possible to truncate a presence map that ends in a sequence where the bits are all zero. The length of the remaining part must be a multiple of seven." | Delegated to FAST 1.1 | If pmap terminates before all declared bits are transmitted, remaining bits are implicitly 0. | confirmed (FAST 1.1, p.21) |

### 3.10. Template ID

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Template ID in PMap bit 0 | FAST 1.1 §10.3 (p.21) + §10.5 (p.21) | "A template identifier is represented as an Unsigned Integer in the stream." + "The segment has a template identifier either if it is a message segment, or if the segment appears as the result of a dynamic template reference instruction. A template identifier is encoded as if a copy operator was specified." | Delegated to FAST 1.1 | If pmap bit 0 is set, a template ID follows as a stop-bit unsigned integer. | confirmed (FAST 1.1, p.21) |
| MOEX template IDs | templatesT0/templates.xml | Various `<template ... id="N">` attributes | MOEX-specific | OrdersLogMessage=29, BookMessage=30, DefaultIncrementalRefreshMessage=31, DefaultSnapshotMessage=32, SecurityDefinition=40 (T0)/47 (T1), SecurityStatus=5 (T0)/48 (T1), SecurityGroupStatus=45, TradingSessionStatus=46, DiscreteAuction=42, News=9, etc. | confirmed |
| Template ID encoding | FAST 1.1 §10.3 (p.21) | "A template identifier is represented as an Unsigned Integer in the stream." | Delegated to FAST 1.1 | Stop-bit unsigned integer, present only if pmap bit 0 is set. | confirmed (FAST 1.1, p.21) |

### 3.11. Operators

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| No compression operators in MOEX templates | spectra_fastgate_en.pdf changelog v1.7.0 (2018-12-05) | "Message templates do not now contain the compression operators copy, delta and increment" | MOEX-specific | MOEX removed all compression operators (copy, delta, increment) as of v1.7.0. | confirmed |
| Exhaustive operator inventory | See §7 below | Reproducible inventory with exact commands and counts | MOEX-specific | `constant` is the only explicit operator element (70 fields per template set). All remaining fields have no operator element — 323 in T0, 326 in T1 (see §7 for exact namespace-safe counts and reproduction commands). `none` is a project designation for the absence of a field operator, not a FAST operator. No `copy`, `delta`, `increment`, `default`, or `tail` operators are present. 19 templates per template set. | confirmed (see §7) |
| Default operator behavior | FAST 1.1 §10.5.1 (p.22) | "If a field is mandatory and has no field operator, it will not occupy any bit in the presence map and its value must always appear in the stream." | Delegated to FAST 1.1 | With no operator, field value is always present in the stream (no pmap bit needed unless optional). | confirmed (FAST 1.1, p.22) |
| Operator types defined | FAST 1.1 §6.3 (p.12) | `fieldOp = constant | \default | copy | increment | delta | tail` | Delegated to FAST 1.1 | Six operator types defined in FAST 1.1. MOEX uses only `constant` as an explicit operator element; `none` is a project designation for the absence of a field operator, not a FAST operator. | confirmed (FAST 1.1, p.12) |
| Dictionaries | FAST 1.1 §6.3.1 (p.12-13) | Three predefined dictionaries: `global`, `template`, `type`. Plus user-defined. | Delegated to FAST 1.1 | `constant` is the only explicit operator element in MOEX templates; `none` is a project designation for the absence of a field operator, not a FAST operator. Neither `constant` nor absent-operator accesses any dictionary. No dictionary-dependent operators (copy, increment, delta, default, tail) are present. | confirmed (FAST 1.1, p.12-13) |

### 3.12. Dictionaries

Per FAST 1.1 §6.3.1 (p.12-13), three predefined dictionaries exist: `global`, `template`, and `type`, plus user-defined dictionaries. MOEX XML template definitions are not the same as the FAST `template` dictionary — the former is a static XML file, the latter is a runtime dictionary accessed by dictionary-dependent operators.

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Predefined `global` dictionary | Not accessed by MOEX operators | — | — | No dictionary-dependent operator (copy, increment, delta, default, tail) is present. `constant` does not access dictionaries. | confirmed |
| Predefined `template` dictionary | Not accessed by MOEX operators | — | — | No dictionary-dependent operator is present. MOEX XML template definitions are a different concept from the FAST `template` dictionary. | confirmed |
| Predefined `type` dictionary | Not accessed by MOEX operators | — | — | No dictionary-dependent operator is present. | confirmed |
| User-defined dictionaries | Not used by MOEX | — | — | No `dictionary` attribute appears in any MOEX template. | confirmed |

### 3.13. Sequences

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Sequence encoding | FAST 1.1 §6.2.5 (p.11-12) | "A sequence has an associated length field containing an unsigned integer indicating the number of encoded elements." | Delegated to FAST 1.1 | Sequence = length field (pmap-assisted), then group fields for each element. | confirmed (FAST 1.1, p.11) |
| Length field type | FAST 1.1 §6.2.5 (p.12) + templatesT0/templates.xml | "The length field has a name, is of type uInt32 and can have a field operator." + `<length name="NoMDEntries" id="268"/>` | MOEX-specific + FAST 1.1 | Length is stop-bit uInt32. | confirmed |
| MOEX template sequences | templatesT0/templates.xml | `<sequence name="MDEntries">`, `<sequence name="MDFeedTypes">`, etc. | MOEX-specific | MDEntries, MDFeedTypes, Underlyings, InstrumentLegs, InstrumentAttributes, EvntGrp, SecMassStatGrp, NewsText. | confirmed |

---

## 4. Literal Byte Vectors

All byte vectors below are derived from the encoding rules in FAST 1.1 §10.2-§10.7 (delegated by MOEX). The encoding logic is independently cross-checked against the project's test-only reference encoder (`cpp/moex_fast/tests/fast_reference_encoder.hpp`), which implements the same two's complement stop-bit algorithm. **Note**: The reference encoder has confirmed defects for nullable integer NULL encoding (#13), nullable signed offset-by-1 (#14, #15), INT32_MIN sign extension (#21), and INT64_MIN sign extension (#29) — see §5 for details.

**Important note on nullable integer wire encoding**: Per FAST 1.1 §10.6.1 (p.23) and Appendix 3.1.2 Example 1 (p.33), nullable integer NULL is encoded on the wire as `0x80` (stop bit set, 7 data bits all zero). The entity value is `0x00` (7 zero bits), which becomes `0x80` when stop-bit encoded. Per §10.6.1, only non-negative values are incremented by 1; negative signed values are encoded directly. **Defect in reference encoder**: `fast_reference_encoder.hpp` pushes `0x00` for nullable integer NULL instead of `0x80`, and applies `val+1` to all signed values instead of only non-negative ones. **Defect in production decoder**: `wire_cursor.cpp` checks `data_[pos_] == 0x00` for nullable NULL detection instead of reading the stop-bit encoded value and checking for decoded 0 (wire `0x80`), and applies `raw - 1` unconditionally for nullable signed instead of only for positive decoded values.

### 4.1. Stop-Bit Unsigned — Value 0

**Source**: FAST 1.1 §10.2 (p.21) + Appendix 3.1.2 Ex.2 (p.34)
**Calculation**: Value 0 requires 1 data bit (the value 0 itself). One byte: data bits = `0000000`, stop bit = 1 → `0x80`.
**Wire**: `[0x80]`
**Official example**: Mandatory uInt32, input 0 → FAST `0x80` (p.34)

### 4.2. Stop-Bit Unsigned — Value 127

**Source**: FAST 1.1 §10.2 (p.21)
**Calculation**: Value 127 = `0x7F`, fits in 7 data bits. One byte: data = `1111111`, stop = 1 → `0xFF`.
**Wire**: `[0xFF]`

### 4.3. Stop-Bit Unsigned — Value 128

**Source**: FAST 1.1 §10.2 (p.21)
**Calculation**: Value 128 = `0x80`, needs 8 data bits → 2 bytes. Byte 0: data = `0000001` (bits 13..7 of value), byte 1: data = `0000000` (bits 6..0), stop = 1.
**Wire**: `[0x01, 0x80]`

### 4.4. Stop-Bit Unsigned — UINT32_MAX (4294967295)

**Source**: FAST 1.1 §10.2 (p.21)
**Calculation**: 32 bits → ceil(32/7) = 5 bytes. Groups from MSB: `0x0F`, `0x7F`, `0x7F`, `0x7F`, last with stop: `0xFF`.
**Wire**: `[0x0F, 0x7F, 0x7F, 0x7F, 0xFF]`

### 4.5. Stop-Bit Unsigned — UINT64_MAX

**Source**: FAST 1.1 §10.2 (p.21)
**Calculation**: 64 bits → ceil(64/7) = 10 bytes. Groups from MSB: `0x01`, `0x7F`×8, last with stop: `0xFF`.
**Wire**: `[0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF]`

### 4.6. Stop-Bit Signed — Value 0

**Source**: FAST 1.1 §10.6.1.1 (p.23) + Appendix 3.1.1 (p.33)
**Calculation**: Two's complement 0 needs 1 sign bit. One byte: data = `0000000`, stop = 1 → `0x80`.
**Wire**: `[0x80]`

### 4.7. Stop-Bit Signed — Value -1

**Source**: FAST 1.1 §10.6.1.1 (p.23)
**Calculation**: Two's complement -1 = all ones. In 7 data bits: `1111111`, stop = 1 → `0xFF`.
**Wire**: `[0xFF]`

### 4.8. Stop-Bit Signed — Value 63

**Source**: FAST 1.1 §10.6.1.1 (p.23)
**Calculation**: 63 = `0x3F`. Needs sign bit (0) + 6 payload bits = 7 total → 1 byte. Data = `0111111`, stop = 1 → `0xBF`.
**Wire**: `[0xBF]`

### 4.9. Stop-Bit Signed — Value 64

**Source**: FAST 1.1 §10.6.1.1 NOTE (p.23)
**Calculation**: Per the spec NOTE: "if the value to encode is 64, which has the binary representation 01000000, the stop bit encoding must be 0x00 0xC0." 64 needs sign bit + 7 payload bits = 8 total → 2 bytes. Byte 0: data = `0000000`, byte 1: data = `1000000`, stop = 1 → `0xC0`.
**Wire**: `[0x00, 0xC0]`
**Official example**: FAST 1.1 p.23 NOTE: encoding of 64 = `0x00 0xC0`

### 4.10. Stop-Bit Signed — Value -64

**Source**: FAST 1.1 §10.6.1.1 (p.23)
**Calculation**: -64 in two's complement. 7 data bits: `1000000`, stop = 1 → `0xC0`.
**Wire**: `[0xC0]`

### 4.11. Stop-Bit Signed — Value -65

**Source**: FAST 1.1 §10.6.1.1 (p.23)
**Calculation**: -65 needs 8 data bits → 2 bytes. Byte 0: `0111111` = `0x7F`, byte 1: `0111111` + stop = `0xBF`.
**Wire**: `[0x7F, 0xBF]`

### 4.12. Stop-Bit Signed — INT32_MAX (2147483647)

**Source**: FAST 1.1 §10.6.1.1 (p.23)
**Calculation**: 32 bits two's complement (sign=0 + 31 payload) → ceil(32/7) = 5 bytes. Groups: `0x07`, `0x7F`, `0x7F`, `0x7F`, `0xFF`.
**Wire**: `[0x07, 0x7F, 0x7F, 0x7F, 0xFF]`

### 4.13. Stop-Bit Signed — INT32_MIN (-2147483648)

**Source**: FAST 1.1 §10.6.1.1 (p.23) + §6.2.1 (p.10)
**Calculation**: INT32_MIN = -2147483648. The 32-bit two's complement pattern is `0x80000000`. A stop-bit entity value is always a multiple of 7 bits; 4 bytes (28 data bits) covers only [-134217728, 134217727], so 5 bytes (35 data bits) is required. The 35-bit two's complement of -2147483648 is obtained by sign-extending the 32-bit pattern: bits [34..32] = `111` (sign extension), bit 31 = `1`, bits [30..0] = `0`. The entity value is `1111000_0000000_0000000_0000000_0000000`. The sign bit (bit 34) = 1 → negative.

Bit groups (MSB first, 7 bits each) from the 35-bit two's complement entity value:

| Group | Bits (35-bit value) | Binary | Byte |
|-------|---------------------|--------|------|
| 0 | [34..28] | `1111000` | `0x78` |
| 1 | [27..21] | `0000000` | `0x00` |
| 2 | [20..14] | `0000000` | `0x00` |
| 3 | [13..7] | `0000000` | `0x00` |
| 4 (stop) | [6..0]+stop | `0000000` + stop | `0x80` |

**Wire**: `[0x78, 0x00, 0x00, 0x00, 0x80]`

**Derivation from normative rule**: Per FAST 1.1 §10.6.1.1 (p.23): "The entity value is a two's complement integer representation [TWOC]. The most significant data bit of the entity value is the sign bit." The stop-bit encoding must ensure the value is not overlong (§10.6.1, p.23). With 4 bytes (28 data bits), the sign bit (bit 27) would be 0 → positive, which is wrong for -2147483648. With 5 bytes (35 data bits), sign-extending the 32-bit pattern `0x80000000` yields bits [34..31] = `1111`, bit 30..0 = `0...0`. The sign bit (bit 34) = 1 → negative. This is the minimum correct encoding. Overlong check: removing the top 7 data bits yields `0000000_0000000_0000000_0000000` = 0 (positive) ≠ -2147483648, so the encoding is not overlong.

**Cross-check**: The project's reference encoder (`fast_reference_encoder.hpp:101-126`) produces `[0x08, 0x00, 0x00, 0x00, 0x80]` for INT32_MIN, which is **incorrect**: group 0 = `0x08` = `0001000` → sign bit (bit 34) = 0 → positive. The encoder uses `memcpy` into `uint32_t` and extracts 7-bit groups directly, which loses the sign-extension bits [34..32]. The correct output is `[0x78, 0x00, 0x00, 0x00, 0x80]`. This is a **defect** in the reference encoder.

### 4.14. Nullable Unsigned — Null

**Source**: FAST 1.1 §10.4 (p.21) + §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33)
**Entity value**: 7-bit entity value where all bits are zero = `0x00`
**Wire**: `[0x80]` (entity value `0x00` stop-bit encoded: stop bit = 1, data = `0000000`)
**Official example**: Optional uInt32, input null → FAST `0x80` (10000000) (p.33)

### 4.15. Nullable Unsigned — Value 0

**Source**: FAST 1.1 §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33)
**Calculation**: V=0 → stopbit(0+1) = stopbit(1) = `0x81`.
**Wire**: `[0x81]`
**Official example**: Optional uInt32, input 0 → FAST `0x81` (10000001). "Increment by 1 since field is optional" (p.33)

### 4.16. Nullable Signed — Value -1

**Source**: FAST 1.1 §10.6.1 (p.23)
**Calculation**: Per §10.6.1 (p.23): "every **non-negative** integer is incremented by 1 before it is encoded." V=-1 is negative → NOT incremented. stopbit(-1) = two's complement -1 in 7 data bits = `1111111`, + stop bit = `0xFF`.
**Wire**: `[0xFF]`
**Note**: NULL (wire `0x80`) and value -1 (wire `0xFF`) are **distinct**. The current reference encoder and production decoder incorrectly apply the +1 shift to all signed values including negatives, which would produce wire `0x80` for -1 (same as NULL) — this is a defect (see §3.4, §5).

### 4.17. Presence Map — 7 bits, pattern 0110000

**Source**: FAST 1.1 §10.5 (p.21)
**Calculation**: Bits `[0,1,1,0,0,0,0]` → data byte = `0110000` = `0x30`, + stop = `0xB0`.
**Wire**: `[0xB0]`

### 4.18. Presence Map — All Zero (Minimal Encoding)

**Source**: FAST 1.1 §10.5 (p.21)
**Calculation**: Per §10.5 (p.21): "This makes it possible to truncate a presence map that ends in a sequence where the bits are all zero." An all-zero presence map is minimally encoded as a single byte with stop bit and 7 zero data bits. The encoding `[0x00, 0x80]` (14 zero bits + stop) is **overlong** because removing the top 7 zero bits yields the same value (all zeros).
**Wire**: `[0x80]` (7 zero data bits + stop bit; minimal non-overlong encoding)

### 4.19. ASCII String — Empty (non-nullable)

**Source**: FAST 1.1 Appendix 3.1.3 Ex.2 (p.34)
**Wire**: `[0x80]` (stop bit, data=0)
**Official example**: Mandatory string, input "" (zero length) → FAST `0x80` (p.34)

### 4.20. ASCII String — "A"

**Source**: FAST 1.1 §10.6.3 (p.23)
**Calculation**: 'A' = 0x41, data bits = `1000001`, stop = 1 → `0xC1`.
**Wire**: `[0xC1]`

### 4.21. ASCII String — "AB"

**Source**: FAST 1.1 §10.6.3 (p.23)
**Calculation**: 'A' = 0x41 (no stop), 'B' = 0x42 + stop = `0xC2`.
**Wire**: `[0x41, 0xC2]`

### 4.22. Nullable ASCII — Null

**Source**: FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34)
**Calculation**: The p.24 table lists **entity values** (not wire bytes). Entity value `0x00` nullable = NULL. Stop-bit encoding of entity value `0x00` (7 zero data bits + stop bit) = `0x80`.
**Wire**: `[0x80]` (completed stop-bit wire sequence)
**Official example**: Appendix 3.1.3 Ex.1 (p.34): optional string, input null → FAST `0x80` (10000000).

### 4.23. Nullable ASCII — Empty String

**Source**: FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34)
**Calculation**: Entity value `0x00 0x00` nullable = Empty String. Stop-bit encoding: first byte data=`0000000` (no stop) = `0x00`, second byte data=`0000000` (stop) = `0x80`.
**Wire**: `[0x00, 0x80]` (completed stop-bit wire sequence; second byte carries stop bit)
**Official example**: Appendix 3.1.3 Ex.1 (p.34): optional string, input "" → FAST `0x00 0x80` (00000000 10000000).

### 4.24. Unicode String — Empty

**Source**: FAST 1.1 §10.6.4-§10.6.5 (p.24)
**Calculation**: Length = 0 → stopbit(0) = `0x80`. No body bytes.
**Wire**: `[0x80]`

### 4.25. Unicode String — "A"

**Source**: FAST 1.1 §10.6.4-§10.6.5 (p.24)
**Calculation**: Length = 1 → stopbit(1) = `0x81`. Body: 0x41.
**Wire**: `[0x81, 0x41]`

### 4.26. Nullable Unicode — Null

**Source**: FAST 1.1 §10.6.4-§10.6.5 (p.24)
**Wire**: Nullable uInt32 length = NULL → `0x80` (per §10.6.5: "The NULL byte vector is represented by a NULL size preamble" which is the nullable uInt32 NULL = stopbit-encoded entity value 0x00 = 0x80)

### 4.27. Byte Vector — Empty

**Source**: FAST 1.1 §10.6.5 (p.24) + Appendix 3.1.4 Ex.2 (p.35)
**Calculation**: Length = 0 → stopbit(0) = `0x80`. No body bytes.
**Wire**: `[0x80]`
**Official example**: Mandatory byteVector, zero length → FAST length `0x80` (p.35)

### 4.28. Decimal — exponent=2, mantissa=5000

**Source**: FAST 1.1 §10.6.2 (p.23)
**Calculation**: Exponent stopbit(2) = `0x82`. Mantissa 5000 = `0x1388` → 2 bytes: `0x27`, `0x88`.
**Wire**: `[0x82, 0x27, 0x88]`

### 4.29. Stop-Bit Signed — INT64_MIN (-9223372036854775808)

**Source**: FAST 1.1 §10.6.1.1 (p.23) + §6.2.1 (p.10)
**Calculation**: INT64_MIN = -9223372036854775808. The 64-bit two's complement pattern is `0x8000000000000000`. A stop-bit entity value is always a multiple of 7 bits; 9 bytes (63 data bits) covers only [-4611686018427387904, 4611686018427387903], so 10 bytes (70 data bits) is required. The 70-bit two's complement of INT64_MIN is obtained by sign-extending the 64-bit pattern: bits [69..64] = `111111` (sign extension), bit 63 = `1`, bits [62..0] = `0`. The sign bit (bit 69) = 1 → negative.

Bit groups (MSB first, 7 bits each) from the 70-bit two's complement entity value:

| Group | Bits (70-bit value) | Binary | Byte |
|-------|---------------------|--------|------|
| 0 | [69..63] | `1111111` | `0x7F` |
| 1 | [62..56] | `0000000` | `0x00` |
| 2 | [55..49] | `0000000` | `0x00` |
| 3 | [48..42] | `0000000` | `0x00` |
| 4 | [41..35] | `0000000` | `0x00` |
| 5 | [34..28] | `0000000` | `0x00` |
| 6 | [27..21] | `0000000` | `0x00` |
| 7 | [20..14] | `0000000` | `0x00` |
| 8 | [13..7] | `0000000` | `0x00` |
| 9 (stop) | [6..0]+stop | `0000000` + stop | `0x80` |

**Wire**: `[0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80]`

**Derivation from normative rule**: Per FAST 1.1 §10.6.1.1 (p.23): "The entity value is a two's complement integer representation [TWOC]. The most significant data bit of the entity value is the sign bit." The stop-bit encoding must ensure the value is not overlong (§10.6.1, p.23). With 9 bytes (63 data bits), sign-extending the 64-bit pattern `0x8000000000000000` to 63 bits yields bit 62 = 1, bits [61..0] = 0, but this represents -4611686018427387904, not INT64_MIN. With 10 bytes (70 data bits), sign-extending yields bits [69..63] = `1111111` = `0x7F` (sign bit = 1 → negative). This is the minimum correct encoding. Overlong check: removing the top 7 data bits yields a 63-bit positive value (sign bit = 0) ≠ INT64_MIN, so the encoding is not overlong.

**Production decoder defect**: `wire_cursor.cpp:178` computes `sign = (raw >> 63) & 1` before the 10th byte, but after 9 bytes `raw` holds only 63 data bits in positions [62..0] — bit 63 is always 0. Therefore `sign` is always 0, `expected` is always `0x00`, and the guard at line 181 rejects INT64_MIN (bits [63..57] = `0x7F`) as IntegerOverflow. This is a **defect** in the production decoder.

**Reference encoder defect**: `fast_reference_encoder.hpp:128-147` copies the 64-bit signed value into `uint64_t raw` via `memcpy` and extracts 7-bit groups via `(raw >> (g * 7)) & 0x7F`. For INT64_MIN, group 0 (bits [69..63]) requires bits [69..64] which don't exist in `uint64_t`. The encoder produces group 0 = `0x00` (from bit 63 = 0 and implicit zeros above) instead of `0x7F`. This is a **defect** in the reference encoder.

### 4.30. Stop-Bit Signed — INT64_MAX (9223372036854775807)

**Source**: FAST 1.1 §10.6.1.1 (p.23) + §6.2.1 (p.10)
**Calculation**: INT64_MAX = 9223372036854775807. The 64-bit two's complement pattern is `0x7FFFFFFFFFFFFFFF`. 10 bytes (70 data bits) is required because the value's sign bit (0) plus 63 payload bits = 64 bits, which exceeds 9 bytes (63 data bits). The 70-bit two's complement sign-extends with 0: bits [69..64] = `000000`, bit 63 = `0`, bits [62..0] = all `1`. The sign bit (bit 69) = 0 → positive.

Bit groups (MSB first, 7 bits each) from the 70-bit two's complement entity value:

| Group | Bits (70-bit value) | Binary | Byte |
|-------|---------------------|--------|------|
| 0 | [69..63] | `0000000` | `0x00` |
| 1 | [62..56] | `1111111` | `0x7F` |
| 2 | [55..49] | `1111111` | `0x7F` |
| 3 | [48..42] | `1111111` | `0x7F` |
| 4 | [41..35] | `1111111` | `0x7F` |
| 5 | [34..28] | `1111111` | `0x7F` |
| 6 | [27..21] | `1111111` | `0x7F` |
| 7 | [20..14] | `1111111` | `0x7F` |
| 8 | [13..7] | `1111111` | `0x7F` |
| 9 (stop) | [6..0]+stop | `1111111` + stop | `0xFF` |

**Wire**: `[0x00, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF]`

**Derivation from normative rule**: Same as §4.29 but for a positive value. With 10 bytes, group 0 = `0x00` (sign bit = 0 → positive). Overlong check: removing the top 7 zero data bits yields a 63-bit value whose sign bit (bit 62) = 1 → negative, which ≠ INT64_MAX, so the encoding is not overlong.

**Cross-check**: The reference encoder produces the correct encoding for INT64_MAX because all significant bits fit within 64 bits (bit 63 = 0, no sign-extension bits needed above bit 63).

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
| 1 | **Nullable signed: INT32_MIN handling** | `wire_cursor.cpp:278` rejects `raw == INT32_MIN` as overflow | FAST 1.1 §10.6.1 (p.23): only non-negative values are incremented. Negative values are encoded/decoded directly. INT32_MIN as a decoded raw value is negative → value = raw = INT32_MIN (no decrement). The overflow guard exists only because the code unconditionally applies `raw - 1`. | FAST 1.1, p.23 | **confirmed discrepancy** (production decoder) |
| 2 | **Nullable signed: unconditional raw-1** | `wire_cursor.cpp:281`: `out = raw - 1` applied to all decoded values | FAST 1.1 §10.6.1 (p.23): only non-negative integers are incremented by 1. Negative values are NOT incremented. The decoder should apply `raw - 1` only for raw > 0; for raw < 0, value = raw directly. NULL is detected by decoded raw == 0 (wire `0x80`). | FAST 1.1, p.23 | **confirmed discrepancy** (production decoder) |
| 3 | **Nullable unsigned: NULL detection and wire 0x80 rejection** | `wire_cursor.cpp:232`: NULL via `data_[pos_] == 0x00`; `:241`: rejects `raw == 0` as NonCanonical | FAST 1.1 §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33): nullable unsigned NULL wire = `0x80` (entity value 0x00, stop-bit encoded). The code checks byte `0x00` for NULL (wrong byte), then rejects wire `0x80` (the correct NULL encoding) as NonCanonical. | FAST 1.1, p.23 + p.33 | **confirmed discrepancy** (production decoder) |
| 4 | **Stop-bit signed canonical: range check in i32** | `wire_cursor.cpp:142-153`: checks if decoded value fits in (bytes_read-1)*7 bits | FAST 1.1 §10.6.1 (p.23): "An integer is overlong if the entity value still represents the same integer after removing seven or more of the most significant bits." The range check `[lo, hi]` for prev_bits is correct. | FAST 1.1, p.23 | confirmed correct |
| 5 | **Stop-bit signed i64: pre-shift overflow guard** | `wire_cursor.cpp:177-184`: for bytes beyond 9, checks top 7 bits are sign extension before shifting | FAST 1.1: max 10 bytes for int64. The 10th byte's 7 data bits would shift left by 63, overflowing uint64_t. The pre-shift check is a necessary guard. **Defect**: At line 178, `sign = (raw >> 63) & 1` computes the sign from bit 63 of `raw`, but after 9 bytes `raw` holds only 63 data bits in positions [62..0] — bit 63 is always 0. Therefore `sign` is always 0, `expected` is always `0x00`, and the guard rejects any 10-byte value where bits [63..57] are not all-zero. A correct 10-byte negative value (e.g. INT64_MIN, where bits [63..57] = `1111111` = `0x7F`) is rejected as IntegerOverflow. See §4.29, §4.30 for literal derivations. | FAST 1.1, p.10 + p.23 | **confirmed discrepancy** (production decoder) |
| 6 | **Presence map: implicit zero padding** | `wire_cursor.cpp:337-339`: pads with false after stop bit | FAST 1.1 §10.5 (p.21): "Logically a presence map has an infinite suffix of zeroes." | FAST 1.1, p.21 | confirmed correct |
| 7 | **Presence map: continues reading after pmap_bits filled** | `wire_cursor.cpp:317-334`: reads ALL bytes until stop bit, even if pmap_bits already filled | FAST 1.1: stop bit terminates the pmap. Must consume all bytes until stop bit found. Correct. | FAST 1.1, p.21 | confirmed correct |
| 8 | **ASCII: 0x00 in data position rejected** | `wire_cursor.cpp:369`: `if (data_bits == 0) return InvalidEncoding` for continuation bytes | FAST 1.1 §10.6.3 (p.23-24): 7-bit ASCII characters. 0x00 in data position is the zero-preamble, not a valid character in a data position. Correct. | FAST 1.1, p.23-24 | confirmed correct |
| 9 | **ASCII: empty string detection** | `wire_cursor.cpp:358-361`: stop byte with data=0 returns Ok (empty string) | FAST 1.1 Appendix 3.1.3 Ex.2 (p.34): mandatory string, input "" → FAST `0x80`. Correct. | FAST 1.1, p.34 | confirmed correct |
| 10 | **Unicode: nullable uses nullable_u32 for length** | `wire_cursor.cpp:461`: `read_nullable_u32(len, len_null)` | FAST 1.1 §10.6.4-§10.6.5 (p.24): nullable Unicode is nullable Byte Vector. NULL size preamble = NULL string. Call-site structure matches the spec. However, `read_nullable_u32` itself has confirmed defects (#3, #22): NULL detection checks byte `0x00` instead of stop-bit decoded 0, and rejects wire `0x80` as NonCanonical. End-to-end nullable Unicode behavior is therefore non-compliant. | FAST 1.1, p.24 | **Structurally correct only** (call structure matches spec; depends on broken nullable u32 primitive) |
| 11 | **Byte vector: nullable uses nullable_u32 for length** | `wire_cursor.cpp:508`: `read_nullable_u32(len, len_null)` | FAST 1.1 §10.6.5 (p.24): "A nullable byte vector has a nullable size preamble. The NULL byte vector is represented by a NULL size preamble." Call-site structure matches the spec. However, `read_nullable_u32` has confirmed defects (#3, #22). End-to-end nullable byte vector behavior is therefore non-compliant. | FAST 1.1, p.24 | **Structurally correct only** (call structure matches spec; depends on broken nullable u32 primitive) |
| 12 | **Decimal: null decimal skips mantissa** | `wire_cursor.cpp:538-541`: if exponent nullable and null, returns without reading mantissa | FAST 1.1 §10.6.2 (p.23): "A NULL scaled number is represented as a NULL exponent. The mantissa is present in the stream iff the exponent is not NULL." Call-site structure matches the spec. However, the exponent is a nullable signed i32, which has confirmed defects (#1, #2, #22): NULL detection checks byte `0x00`, `raw-1` is applied unconditionally, and INT32_MIN is rejected. End-to-end nullable decimal behavior is therefore non-compliant. | FAST 1.1, p.23 | **Structurally correct only** (call structure matches spec; depends on broken nullable signed i32 primitive) |
| 13 | **Reference encoder: nullable integer NULL uses 0x00** | `fast_reference_encoder.hpp:153,162,171,186`: `buf.push_back(0x00)` for null | FAST 1.1 §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33): nullable integer NULL wire encoding = `0x80` (entity value 0x00, stop-bit encoded). The reference encoder emits the entity value `0x00` instead of the wire encoding `0x80`. | FAST 1.1, p.23 + p.33 | **confirmed discrepancy** (reference encoder) |
| 14 | **Reference encoder: nullable signed applies val+1 to all values** | `fast_reference_encoder.hpp:177-179`: shifts all signed values by +1 | FAST 1.1 §10.6.1 (p.23): "every **non-negative** integer is incremented by 1." Negative values should be encoded directly (not incremented). The encoder applies `val+1` to all signed values including negatives, producing the same wire code as NULL for value -1. | FAST 1.1, p.23 | **confirmed discrepancy** (reference encoder) |
| 15 | **Reference encoder: nullable i64 applies val+1 to all values** | `fast_reference_encoder.hpp:188-190`: same pattern as i32 | Same defect as #14 for 64-bit. | FAST 1.1, p.23 | **confirmed discrepancy** (reference encoder) |
| 16 | **Test: nullable i32 value -1 → wire 0x80** | `test_decoder_reference_oracle.cpp:319-323` | FAST 1.1 §10.6.1 (p.23): V=-1 is negative → NOT incremented → stopbit(-1) = `0xFF`. The test expects `0x80` (same as NULL), which is incorrect per the spec rule that only non-negative values are shifted. | FAST 1.1, p.23 | **confirmed discrepancy** (test expectation) |
| 17 | **Test: nullable u32 max value** | `test_decoder_reference_oracle.cpp:554-558`: `0xFFFFFFFE` → `0x0F 0x7F 0x7F 0x7F 0xFF` | FAST 1.1: V=0xFFFFFFFE → stopbit(0xFFFFFFFE+1)=stopbit(0xFFFFFFFF). Correct. | FAST 1.1, p.23 | confirmed correct |
| 18 | **MOEX preamble: 4-byte SeqNum before FAST body** | Not implemented in WireCursor (WireCursor operates on FAST body only) | MOEX §3.2: 4-byte preamble contains MsgSeqNum before FAST message. | spectra_fastgate_en.pdf §3.2 | No discrepancy (separate concern) |
| 19 | **MOEX: only `constant` as explicit operator** | Code implements default (no-operator) behavior; constant is handled at template level | Exhaustive XML scan: `constant` is the only explicit operator element across all 19 MOEX templates (70 constant fields, 0 copy/delta/increment/default/tail). Fields without an operator element use the project designation `none`. | templatesT0/templates.xml (full scan, see §7) | confirmed correct |
| 20 | **SecurityDesc uses charset="unicode"** | Not directly visible in WireCursor (WireCursor handles raw types) | templatesT0/templates.xml line 110: `<string ... charset="unicode"/>` | templates.xml | no discrepancy (handled at template level) |
| 21 | **Reference encoder: INT32_MIN encoding produces wrong vector** | `fast_reference_encoder.hpp:119-125`: extracts 7-bit groups from `uint32_t` via `memcpy` | FAST 1.1 §10.6.1.1 (p.23): INT32_MIN in 35-bit two's complement has sign bit (bit 34) = 1. Group 0 = `1111000` = `0x78`. The encoder extracts from `uint32_t` (32 bits), losing sign-extension bits [34..32], producing group 0 = `0001000` = `0x08` (sign bit = 0 → positive). **Note**: The same sign-extension defect affects `encode_stopbit_i64` (`:128-147`) for 64-bit negative values requiring 10 bytes: it extracts groups from `uint64_t` via `memcpy`, losing bits [69..64]. See §4.30 for INT64_MIN literal derivation. | FAST 1.1, p.23 | **confirmed discrepancy** (reference encoder, 32-bit and 64-bit) |
| 22 | **Production decoder: nullable NULL detection via byte 0x00** | `wire_cursor.cpp:232,268,288`: `if (data_[pos_] == 0x00)` for nullable NULL | FAST 1.1 §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33): nullable integer NULL wire = `0x80` (entity value 0x00 stop-bit encoded). The code checks for byte `0x00` (not a valid stop-bit entity) instead of reading the stop-bit value and checking for decoded 0 (wire `0x80`). | FAST 1.1, p.23 + p.33 | **confirmed discrepancy** (production decoder) |
| 23 | **Signed i64: decoder rejects valid 10-byte negative values (INT64_MIN)** | `wire_cursor.cpp:177-184`: `sign = (raw >> 63) & 1`; `top7 = raw >> 57`; `expected = sign ? 0x7F : 0x00` | FAST 1.1 §10.6.1.1 (p.23): INT64_MIN = -9223372036854775808 requires 10 bytes. After 9 bytes, `raw` holds 63 data bits in positions [62..0] — bit 63 is always 0. Therefore `sign = (raw >> 63) & 1` is always 0, `expected` is always `0x00`, and the guard rejects 10-byte values where bits [63..57] ≠ `0x00`. The correct 10-byte encoding of INT64_MIN has bits [63..57] = `0x7F` (sign extension), which is rejected as IntegerOverflow. See §4.29 for literal derivation. | FAST 1.1, p.10 + p.23 | **confirmed discrepancy** (production decoder) |
| 24 | **Unsigned canonical: u32 accepts overlong multi-byte encodings** | `wire_cursor.cpp:61`: `if (bytes_read > 1 && result <= 0x7Fu)` returns NonCanonical | FAST 1.1 §10.6.1 (p.23): "An integer is overlong if the entity value still represents the same integer after removing seven or more of the most significant bits." The check `result <= 0x7Fu` only rejects values that fit in 7 bits (1 byte). It does NOT reject values that fit in fewer bytes than used. Example: `[00 01 80]` (3 bytes) decodes to value 128, which passes `128 > 0x7F`, but canonical encoding is `[01 80]` (2 bytes). | FAST 1.1, p.23 | **confirmed discrepancy** (production decoder) |
| 25 | **Unsigned canonical: u64 accepts overlong multi-byte encodings** | `wire_cursor.cpp:91`: `if (bytes_read > 1 && result <= 0x7F)` returns NonCanonical | Same defect as #24 for 64-bit. The check only rejects values ≤ 0x7F (fitting in 1 byte). Overlong multi-byte encodings of larger values are accepted. | FAST 1.1, p.23 | **confirmed discrepancy** (production decoder) |
| 26 | **Presence map: no ERR R7 overlong check; test accepts [00 80]** | `wire_cursor.cpp:307-342`: `read_presence_map` has no overlong check; `test_decoder_primitives.cpp:357-371`: explicitly accepts `[0x00, 0x80]` as valid Ok | FAST 1.1 §10.5 (p.21): "It is a reportable error [ERR R7] if a presence map is overlong." Canonical all-zero pmap is `[0x80]` (1 byte). `[0x00, 0x80]` (2 bytes) is overlong: removing the top 7 zero data bits leaves the same all-zero value. `read_presence_map` performs no overlong check. The active test at line 364 asserts `Ok` for `[00 80]`, treating overlong as valid. | FAST 1.1, p.21 | **confirmed discrepancy** (production decoder + test) |
| 27 | **Nullable ASCII: always sets is_null=false; decodes NULL as empty; rejects valid empty** | `wire_cursor.cpp:377-380`: `is_null = false; return read_ascii_string(out, max_bytes);` | FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34): nullable ASCII NULL wire = `[0x80]` (entity value `0x00`, stop-bit encoded); empty wire = `[0x00, 0x80]` (entity value `0x00 0x00`, stop-bit encoded). `read_nullable_ascii` always sets `is_null = false` and delegates to `read_ascii_string`: (a) NULL `[80]` is decoded as non-null empty string (data_bits=0, stop → Ok empty); (b) empty `[00 80]` is rejected as InvalidEncoding (zero preamble in continuation byte). | FAST 1.1, p.24 + p.34 | **confirmed discrepancy** (production decoder) |
| 28 | **Nullable ASCII: no active primitive tests** | `test_decoder_primitives.cpp`: `main()` test list (lines 570-584) has no `test_nullable_ascii` | FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34): The approved RT-3 test plan requires separate nullable ASCII NULL and empty tests. No `test_nullable_ascii` function exists; `read_nullable_ascii` is never called. | FAST 1.1, p.24 + p.34 | **confirmed discrepancy** (test gap) |
| 29 | **Reference encoder: encode_stopbit_i64 sign-extension defect for 10-byte values** | `fast_reference_encoder.hpp:128-147`: `std::memcpy(&raw, &val, 8)` then `(raw >> (g * 7)) & 0x7F` | FAST 1.1 §10.6.1.1 (p.23): INT64_MIN in 70-bit two's complement has 6 sign-extension bits in positions [69..64] that don't exist in `uint64_t`. The encoder copies the 64-bit pattern into `uint64_t raw` and extracts groups via right-shift; bits [69..64] are implicitly zero. For INT64_MIN, group 0 should be `1111111` = `0x7F` (7 sign-extension bits) but the encoder produces `0000000` = `0x00` (bit 63 = 0, lost bits [69..64]). The same defect affects any 64-bit negative value requiring 10 bytes. See §4.30 for literal derivation. | FAST 1.1, p.23 | **confirmed discrepancy** (reference encoder)

### Summary of Discrepancies

**Confirmed discrepancies (17)**:
- **#1, #2, #22** (production decoder `wire_cursor.cpp`): Nullable signed decode applies `raw - 1` unconditionally instead of only for non-negative decoded values. Nullable NULL detection checks byte `0x00` instead of stop-bit decoded value 0 (wire `0x80`). INT32_MIN is rejected as overflow due to the unconditional decrement.
- **#3** (production decoder): Nullable unsigned NULL detection checks byte `0x00` instead of stop-bit decoded 0; rejects wire `0x80` (the correct NULL encoding) as NonCanonical.
- **#5** (production decoder): Signed i64 pre-shift overflow guard computes sign from `raw >> 63` (always 0 after 9 bytes), rejecting valid 10-byte negative values like INT64_MIN.
- **#24, #25** (production decoder): Unsigned u32/u64 canonical check only rejects `result <= 0x7F`; accepts overlong multi-byte encodings (e.g. `[00 01 80]` for 128 instead of canonical `[01 80]`).
- **#26** (production decoder + test): `read_presence_map` has no ERR R7 overlong check; test at `test_decoder_primitives.cpp:364` explicitly accepts overlong `[00 80]` as valid.
- **#27** (production decoder): `read_nullable_ascii` always sets `is_null = false`, decodes NULL `[80]` as non-null empty, rejects valid empty `[00 80]` at zero preamble.
- **#28** (test gap): No `test_nullable_ascii` function or `read_nullable_ascii` calls exist in `test_decoder_primitives.cpp`.
- **#13** (reference encoder): Nullable integer NULL encoded as `0x00` instead of `0x80`.
- **#14, #15** (reference encoder): Nullable signed i32/i64 applies `val+1` to all values instead of only non-negative values.
- **#16** (test expectation): Test expects nullable signed -1 → wire `0x80`, but per the spec rule (only non-negative values incremented), -1 should encode as `0xFF`.
- **#21** (reference encoder): INT32_MIN encoding produces `[0x08, 0x00, 0x00, 0x00, 0x80]` (sign bit = 0, positive) instead of the correct `[0x78, 0x00, 0x00, 0x00, 0x80]` due to missing sign-extension in the `uint32_t` extraction.
- **#23** (production decoder): Signed i64 decoder rejects valid 10-byte negative values (INT64_MIN) because the pre-shift sign check uses `raw >> 63` which is always 0 after 9 accumulated bytes.
- **#29** (reference encoder): `encode_stopbit_i64` extracts 7-bit groups from `uint64_t` via `memcpy`, losing bits [69..64] needed for sign-extension of 64-bit negative values requiring 10 bytes.

**Structurally correct only (3)**:
- **#10** (nullable Unicode): Call structure matches spec (`read_nullable_u32` for length), but `read_nullable_u32` has confirmed defects (#3, #22).
- **#11** (nullable byte vector): Call structure matches spec (`read_nullable_u32` for length), but `read_nullable_u32` has confirmed defects (#3, #22).
- **#12** (nullable decimal): Call structure matches spec (nullable exponent, skip mantissa if null), but exponent is nullable signed i32 with confirmed defects (#1, #2, #22).

**Unresolved items (2)**:
- **Preamble endianness**: The MOEX specification does not specify the byte order of the 4-byte preamble (§3.2, spectra_fastgate_en.pdf).
- **meta.info fetch**: The cause of the HTTP 404 when fetching `meta.info` from the MOEX FTP is unresolved (see §2.2).

**Confirmed correct (7 of 35)**: Items #4, #6, #7, #8, #9, #17, #19 — confirmed correct against the authoritative sources (FAST 1.1 specification and MOEX SPECTRA specification v1.30.2).

**No discrepancy (2)**: Items #18, #20 — not discrepancies: #18 is a separate concern not handled by WireCursor; #20 is handled at the template level and does not represent a code discrepancy. All categories are mutually exclusive; no item appears in more than one category.

---

## 6. Key Observations

1. **MOEX delegates all base FAST encoding to FAST 1.1**: The MOEX PDF describes stop-bit encoding at a high level (§3.2.1, §3.2.7) but does NOT restate the full encoding rules. It relies on implementors knowing FAST 1.1 for: canonical encoding, nullable offset-by-1, presence map structure, signed two's complement, decimal composite encoding, sequence length encoding.

2. **MOEX uses only `constant` as an explicit operator element**: Exhaustive namespace-safe XML scan of all 19 templates in `templatesT0/templates.xml` (618 lines) confirms that `constant` is the only explicit operator element (70 fields). All other fields have no operator element — `none` is a project designation for this absence, not a FAST operator. The MOEX changelog v1.7.0 (2018-12-05) states: "Message templates do not now contain the compression operators copy, delta and increment." The XML inventory confirms this is still true in the current templates.

3. **MOEX adds a 4-byte preamble**: This is MOEX-specific and not part of FAST 1.1. The preamble contains MsgSeqNum (tag 34), allowing sequence number extraction without FAST decoding. The byte order of the preamble is **not documented** in the MOEX specification (§3.2, Pic.3 shows "Preamble (4 bytes)" then "Message" but does not specify endianness).

4. **The current MOEX system sends at most one FAST message per UDP packet**: While MOEX §1.2.5 states that "a single UDP packet may contain several FIX messages in the FAST format" in principle, it also explicitly states: "currently the system does not provide a possibility to send more than one FAST-coded message via a single UDP packet." Messages are bounded to MTU size (1500 bytes).

5. **Template IDs are MOEX-specific**: The specific template IDs (29, 30, 31, 32, 40/47, 5/48, 45, 46, etc.) are defined by MOEX and change across versions. T0 and T1 use different IDs for SecurityDefinition (40 vs 47) and SecurityStatus (5 vs 48).

6. **T0 and T1 templates are NOT byte-identical**: They have different SHA-256 hashes and differ in template IDs and field definitions. FAST_9.0 templates.xml IS byte-identical to T0 templates.xml.

7. **Nullable integer NULL wire encoding and offset-by-1 rule**: The official FAST 1.1 specification (§10.6.1, p.23 + Appendix 3.1.2 Example 1, p.33) defines nullable integer NULL as entity value `0x00` (7 zero bits), which when stop-bit encoded is `0x80` on the wire. The offset-by-1 rule applies only to **non-negative** values; negative signed values are encoded directly. Therefore NULL (`0x80`) and value -1 (`0xFF`) are distinct on the wire. Both the reference encoder and the production decoder have defects: the encoder pushes `0x00` for NULL and applies `val+1` universally; the decoder checks byte `0x00` for NULL and applies `raw-1` universally (see §5 entries #1–3, #13–16, #22).

8. **INT32_MIN encoding**: The correct encoding is `[0x78, 0x00, 0x00, 0x00, 0x80]`, derived from the 35-bit two's complement representation (FAST 1.1 §10.6.1.1, p.23). The 32-bit pattern `0x80000000` sign-extended to 35 bits yields bits [34..31] = `1111`, giving group 0 = `1111000` = `0x78` (sign bit = 1 → negative). The reference encoder produces `[0x08, 0x00, 0x00, 0x00, 0x80]` (sign bit = 0 → positive) due to extracting from `uint32_t` without sign extension — this is a defect (see §5 entry #21).

9. **INT64_MIN encoding and decoder defect**: The correct encoding is `[0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80]`, derived from the 70-bit two's complement representation (FAST 1.1 §10.6.1.1, p.23). The production decoder's signed i64 pre-shift guard (`wire_cursor.cpp:178`) computes `sign = (raw >> 63) & 1` before the 10th byte, but after 9 bytes bit 63 is always 0 — this incorrectly rejects INT64_MIN as IntegerOverflow (see §5 entry #23). The reference encoder (`fast_reference_encoder.hpp:128-147`) extracts groups from `uint64_t` via `memcpy`, losing bits [69..64] needed for sign extension — this produces `0x00` instead of `0x7F` for group 0 (see §5 entry #29).

10. **Unsigned canonical encoding incomplete**: `read_stopbit_u32/u64` rejects multi-byte encodings only when `result <= 0x7F` (fitting in 1 byte). This fails to reject overlong encodings where the value fits in fewer bytes than used. Example: `[00 01 80]` (3 bytes, value 128) is accepted, but canonical is `[01 80]` (2 bytes). The signed canonical check (`wire_cursor.cpp:207-218`) uses a proper range check and does not have this defect (see §5 entries #24, #25).

11. **Presence map overlong (ERR R7) not enforced**: `read_presence_map` (`wire_cursor.cpp:307-342`) performs no overlong check. Per FAST 1.1 §10.5 (p.21), a non-canonical presence map is a reportable error (ERR R7). The canonical all-zero pmap is `[0x80]` (1 byte); `[0x00, 0x80]` (2 bytes) is overlong. The active test at `test_decoder_primitives.cpp:364` accepts `[00 80]` as valid (see §5 entry #26).

12. **Nullable ASCII non-compliant**: `read_nullable_ascii` (`wire_cursor.cpp:377-380`) always sets `is_null = false` and delegates to `read_ascii_string`. Per FAST 1.1 §10.6.3 (p.24) + Appendix 3.1.3 Ex.1 (p.34), nullable ASCII NULL wire = `[0x80]` and empty wire = `[0x00, 0x80]`. The current code decodes NULL as non-null empty and rejects valid empty at the zero preamble. No nullable ASCII tests exist (see §5 entries #27, #28).

---

## 7. Reproducible Operator and Dictionary Inventory

The following inventory is hash-bound and reproducible. Each template set was downloaded from `https://ftp.moex.com/pub/FAST/Spectra/test/` and hashed with `Get-FileHash -Algorithm SHA256`.

### 7.1. Reproduction Commands

The following PowerShell commands download all three template files, verify their SHA-256 hashes, and produce the exact counts reported in §7.2.

```powershell
# === Download and hash verification ===
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/templates.xml" -UseBasicParsing -OutFile "t0.xml"
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/templates.xml" -UseBasicParsing -OutFile "t1.xml"
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/FAST_9.0/templates.xml" -UseBasicParsing -OutFile "f9.xml"

(Get-FileHash "t0.xml" -Algorithm SHA256).Hash  # Expected: DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E
(Get-FileHash "t1.xml" -Algorithm SHA256).Hash  # Expected: 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
(Get-FileHash "f9.xml" -Algorithm SHA256).Hash  # Expected: DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

# === Namespace-safe per-file counts ===
# The MOEX templates.xml declares xmlns="http://www.fixprotocol.org/ns/fast/td/1.1".
# XPath without a namespace prefix silently matches nothing in a namespace-aware XML
# document. The script registers the namespace with XmlNamespaceManager using prefix "x"
# and queries //x:template, //x:constant, etc.
foreach ($f in @("t0.xml","t1.xml","f9.xml")) {
  $xml = [xml](Get-Content $f -Raw)
  $ns = $xml.DocumentElement.NamespaceURI
  $nsm = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
  if ($ns) { $nsm.AddNamespace("x", $ns) }
  $xp = if ($ns) { "x:" } else { "" }

  $templates = $xml.SelectNodes("//${xp}template", $nsm).Count
  $constant  = $xml.SelectNodes("//${xp}constant", $nsm).Count
  $copy      = $xml.SelectNodes("//${xp}copy", $nsm).Count
  $delta     = $xml.SelectNodes("//${xp}delta", $nsm).Count
  $increment = $xml.SelectNodes("//${xp}increment", $nsm).Count
  $default   = $xml.SelectNodes("//${xp}default", $nsm).Count
  $tail      = $xml.SelectNodes("//${xp}tail", $nsm).Count
  $dictAttrs = $xml.SelectNodes("//*[(@dictionary)]", $nsm).Count

  # Count field-type elements and no-operator fields
  $fieldTypes = @('uInt32','int32','uInt64','int64','string','decimal','byteVector','sequence','length')
  $fieldCount = 0; $noOpCount = 0
  foreach ($t in $fieldTypes) {
    foreach ($n in $xml.SelectNodes("//${xp}$t", $nsm)) {
      $fieldCount++
      $hasOp = $false
      foreach ($child in $n.ChildNodes) {
        if ($child.LocalName -in @('constant','copy','delta','increment','default','tail')) { $hasOp = $true; break }
      }
      if (-not $hasOp) { $noOpCount++ }
    }
  }

  "$f : ns='$ns' templates=$templates lines=$(($raw = Get-Content $f; $raw.Count)) constant=$constant copy=$copy delta=$delta increment=$increment default=$default tail=$tail dictionary_attrs=$dictAttrs fields=$fieldCount no_operator=$noOpCount"
}
```

**Actual output** (2026-07-12):

```
t0.xml : ns='http://www.fixprotocol.org/ns/fast/td/1.1' templates=19 lines=618 constant=70 copy=0 delta=0 increment=0 default=0 tail=0 dictionary_attrs=0 fields=393 no_operator=323
t1.xml : ns='http://www.fixprotocol.org/ns/fast/td/1.1' templates=19 lines=619 constant=70 copy=0 delta=0 increment=0 default=0 tail=0 dictionary_attrs=0 fields=396 no_operator=326
f9.xml : ns='http://www.fixprotocol.org/ns/fast/td/1.1' templates=19 lines=618 constant=70 copy=0 delta=0 increment=0 default=0 tail=0 dictionary_attrs=0 fields=393 no_operator=323
```

**Note on namespace**: All three template files declare `xmlns="http://www.fixprotocol.org/ns/fast/td/1.1"` on the root `<templates>` element. XPath `//template` (without a namespace prefix) matches zero nodes in a namespace-aware parser. The script registers the namespace as prefix `x` and queries `//x:template`, `//x:constant`, etc. This is why the previous (namespace-unaware) inventory reported 307 no-operator fields (wrong) while the namespace-safe inventory reports 323 for T0/FAST_9.0 and 326 for T1 (correct).

### 7.2. Inventory Results

| Metric | templatesT0 | templatesT1 | FAST_9.0 |
|--------|-------------|-------------|----------|
| SHA-256 | `DBD50F1E...` | `84FACBF7...` | `DBD50F1E...` |
| Lines | 618 | 619 | 618 |
| Templates | 19 | 19 | 19 |
| `constant` fields | 70 | 70 | 70 |
| `copy` | 0 | 0 | 0 |
| `delta` | 0 | 0 | 0 |
| `increment` | 0 | 0 | 0 |
| `default` | 0 | 0 | 0 |
| `tail` | 0 | 0 | 0 |
| `dictionary` attributes | 0 | 0 | 0 |
| Total fields | 393 | 396 | 393 |
| No-operator fields | 323 | 326 | 323 |
| T0 == FAST_9.0 | — | — | byte-identical |

**T0/T1 reconciliation**: T1 has 3 more total fields (396 vs 393) and 3 more no-operator fields (326 vs 323) than T0. The difference table (§2.3) accounts for exactly +3 net no-operator fields:

| Template | T0 → T1 | T0 fields | T1 fields | T0 no-op | T1 no-op | Difference |
|----------|---------|-----------|-----------|----------|----------|------------|
| SecurityDefinition | id 40 → 47 | 83 | 84 | 78 | 79 | removed `MaturityDate` (uInt32, no-op), `MaturityTime` (uInt32, no-op); added `HighLimitPxWeekend` (decimal, no-op), `LowLimitPxWeekend` (decimal, no-op), `ClearingSettlPrice` (decimal, no-op). Net: +1 field, +1 no-op |
| SecurityStatus | id 5 → 48 | 14 | 16 | 10 | 12 | added `HighLimitPxWeekend` (decimal, no-op), `LowLimitPxWeekend` (decimal, no-op). Net: +2 fields, +2 no-op |
| All other 17 templates | (unchanged) | 296 | 296 | 235 | 235 | 0 |

**Grand total**: T0 323 no-op + 70 constant = 393 fields; T1 326 no-op + 70 constant = 396 fields. Difference: +3 no-operator fields, matching the §2.3 table exactly.

### 7.3. Template Names (T0/FAST_9.0)

All 19 templates in `templatesT0/templates.xml`:

1. DefaultIncrementalRefreshMessage
2. DefaultSnapshotMessage
3. SecurityDefinition
4. SecurityDefinitionUpdateReport
5. SecurityStatus
6. SecurityMassStatus
7. SecurityGroupStatus
8. Heartbeat
9. SequenceReset
10. TradingSessionStatus
11. DiscreteAuction
12. News
13. OrdersLogMessage
14. BookMessage
15. OtcMonitorIncrementalRefreshMessage
16. OtcMonitorSnapshotMessage
17. OtcMonitorSecurityDefinition
18. Logon
19. Logout

### 7.4. Dictionary Usage

Per FAST 1.1 §6.3.1 (p.12-13), three predefined dictionaries exist: `global`, `template`, `type`, plus user-defined dictionaries. MOEX XML template definitions are a different concept from the FAST `template` dictionary. The inventory confirms:

- **Predefined `global` dictionary**: no dictionary-dependent operator accesses it
- **Predefined `template` dictionary**: no dictionary-dependent operator accesses it
- **Predefined `type` dictionary**: no dictionary-dependent operator accesses it
- **User-defined dictionaries**: no `dictionary` attribute in any template
- **`dictionary` attributes**: 0 across all template sets

`constant` is the only explicit operator element; `none` is a project designation for the absence of a field operator, not a FAST operator. Neither accesses any dictionary.
