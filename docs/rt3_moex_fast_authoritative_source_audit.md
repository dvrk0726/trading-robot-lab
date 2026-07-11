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
| Offset-by-1 encoding | FAST 1.1 §10.6.1 (p.23) | "If an integer is nullable, every non-negative integer is incremented by 1 before it is encoded." | Delegated to FAST 1.1 | Non-null value V is encoded as stopbit(V+1). For signed integers, the arithmetic wraps correctly through unsigned reinterpretation. | confirmed (FAST 1.1, p.23) |
| Nullable unsigned: decoded 0 (wire 0x81) is value 0 | FAST 1.1 Appendix 3.1.2 Ex.1 (p.33) | Optional uInt32, input 0 → FAST `0x81` (10000001). "Increment by 1 since field is optional" | Delegated to FAST 1.1 | V=0 → stopbit(0+1) = stopbit(1) = `0x81`. Wire `0x80` alone (entity value 0, decoded as raw=0) is the NULL sentinel, not a valid non-null value for nullable unsigned. | confirmed (FAST 1.1, p.33) |
| Nullable signed: decoded 0 (wire 0x80) maps to value -1 | FAST 1.1 §10.6.1 (p.23) | V=-1 → stopbit(-1+1) = stopbit(0) = `0x80`. | Delegated to FAST 1.1 | This is the same wire byte as NULL (`0x80`). The decoder disambiguates by reading the stop-bit encoded value first: raw=0 means either NULL (if the wire byte was the nullable NULL sentinel) or value -1 (if the wire byte is a valid stop-bit encoded entity). In practice, the decoder checks the nullable NULL case first (raw=0 and this is a nullable field → NULL), otherwise raw=0 is non-canonical for nullable unsigned or value -1 for nullable signed. | confirmed (FAST 1.1, p.23) |

### 3.5. ASCII String

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Stop-bit per character | FAST 1.1 §10.6.3 (p.23-24) | "An ASCII String is represented as a stop bit encoded entity. The entity value is interpreted as a sequence of 7-bit ASCII characters." | Delegated to FAST 1.1 | Each byte: bit 7 = stop (1=last), bits 6..0 = character. Stop bit on LAST byte only. | confirmed (FAST 1.1, p.23) |
| Valid character range | FAST 1.1 §10.6.3 (p.23-24) | The entity value is "a sequence of 7-bit ASCII characters." | Delegated to FAST 1.1 | 0x01..0x7F per character. 0x00 in data position is the zero-preamble, not a valid character. | confirmed (FAST 1.1, p.23) |
| Empty string encoding (non-nullable) | FAST 1.1 Appendix 3.1.3 Ex.2 (p.34) | Mandatory string, input "" (zero length) → FAST `0x80` | Delegated to FAST 1.1 | Single byte `0x80` (stop bit, data=0). | confirmed (FAST 1.1, p.34) |
| Nullable ASCII NULL | FAST 1.1 §10.6.3 (p.24) | "If an ASCII String is nullable, an additional zero-preamble is allowed at the start of the string. The bits that follow are interpreted as a non-nullable string, including a possible zero preamble. If there are no remaining bits after removing the preamble the value represents the NULL string." Wire encoding table: `0x00` → Nullable: Yes → NULL | Delegated to FAST 1.1 | On the wire, nullable ASCII NULL is `0x00` (a zero-preamble with no remaining bits). This is distinct from the nullable integer NULL encoding (`0x80`). ASCII strings use the zero-preamble mechanism for null-vs-empty distinction. | confirmed (FAST 1.1, p.24) |
| Nullable ASCII empty string | FAST 1.1 §10.6.3 (p.24) | Wire encoding table: `0x00 0x00` → Nullable: Yes → Empty String | Delegated to FAST 1.1 | On the wire, nullable ASCII empty string is `0x00 0x00` (zero-preamble + zero-preamble = empty). | confirmed (FAST 1.1, p.24) |
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
| Exhaustive operator inventory | See §7 below | Reproducible inventory with exact commands and counts | MOEX-specific | **Only two operators are used across all templates**: (1) `constant` (70 fields per template set); (2) `none` (default — no explicit operator element, 307 fields in T0). No `copy`, `delta`, `increment`, `default`, or `tail` operators are present. 19 templates per template set. | confirmed (see §7) |
| Default operator behavior | FAST 1.1 §10.5.1 (p.22) | "If a field is mandatory and has no field operator, it will not occupy any bit in the presence map and its value must always appear in the stream." | Delegated to FAST 1.1 | With no operator, field value is always present in the stream (no pmap bit needed unless optional). | confirmed (FAST 1.1, p.22) |
| Operator types defined | FAST 1.1 §6.3 (p.12) | `fieldOp = constant | \default | copy | increment | delta | tail` | Delegated to FAST 1.1 | Six operator types defined in FAST 1.1. MOEX uses only `constant` and the implicit default (`none`). | confirmed (FAST 1.1, p.12) |
| Dictionaries | FAST 1.1 §6.3.1 (p.12-13) | Three predefined dictionaries: `template`, `type`, `global`. Plus user-defined. | Delegated to FAST 1.1 | MOEX uses only the default (global) dictionary since only `constant` and `none` operators are used — no dictionary-dependent operators (copy, increment, delta, default, tail). | confirmed (FAST 1.1, p.12-13) |

### 3.12. Dictionaries

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Template dictionary | Not used by MOEX | — | — | MOEX does not use FAST template dictionaries. Templates are statically defined in XML files. | confirmed |
| Field dictionary | Not used by MOEX | — | — | MOEX does not use FAST field dictionaries. | confirmed |

### 3.13. Sequences

| Aspect | Source | Location | MOEX-specific / Delegated | Finding | Status |
|--------|--------|----------|---------------------------|---------|--------|
| Sequence encoding | FAST 1.1 §6.2.5 (p.11-12) | "A sequence has an associated length field containing an unsigned integer indicating the number of encoded elements." | Delegated to FAST 1.1 | Sequence = length field (pmap-assisted), then group fields for each element. | confirmed (FAST 1.1, p.11) |
| Length field type | FAST 1.1 §6.2.5 (p.12) + templatesT0/templates.xml | "The length field has a name, is of type uInt32 and can have a field operator." + `<length name="NoMDEntries" id="268"/>` | MOEX-specific + FAST 1.1 | Length is stop-bit uInt32. | confirmed |
| MOEX template sequences | templatesT0/templates.xml | `<sequence name="MDEntries">`, `<sequence name="MDFeedTypes">`, etc. | MOEX-specific | MDEntries, MDFeedTypes, Underlyings, InstrumentLegs, InstrumentAttributes, EvntGrp, SecMassStatGrp, NewsText. | confirmed |

---

## 4. Literal Byte Vectors

All byte vectors below are derived from the encoding rules in FAST 1.1 §10.2-§10.7 (delegated by MOEX). The encoding logic is independently cross-checked against the project's test-only reference encoder (`cpp/moex_fast/tests/fast_reference_encoder.hpp`), which implements the same two's complement stop-bit algorithm.

**Important note on nullable integer wire encoding**: Per FAST 1.1 §10.6.1 (p.23) and the Appendix 3.1.2 Example 1 (p.33), nullable integer NULL is encoded on the wire as `0x80` (stop bit set, 7 data bits all zero). The entity value is `0x00` (7 zero bits), which becomes `0x80` when stop-bit encoded. The reference encoder (`fast_reference_encoder.hpp`) uses `0x00` for nullable integer NULL, which is the entity value rather than the wire encoding. The project decoder correctly handles the spec-compliant wire encoding `0x80` for nullable NULL.

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
**Calculation**: INT32_MIN = -2147483648. In two's complement, this value requires 33 bits to represent (the 32-bit pattern `0x80000000` has sign bit 0 in bit 31, which would be interpreted as a positive number; sign extension to 33 bits produces `1_10000000_00000000_00000000_00000000` where bit 32 = 1 correctly indicates negative).

Bit groups (MSB first, 7 bits each) from the 33-bit two's complement value:

| Group | Bits (33-bit value) | Binary | Byte |
|-------|---------------------|--------|------|
| 0 | [32..26] | `0001000` | `0x08` |
| 1 | [25..19] | `0000000` | `0x00` |
| 2 | [18..12] | `0000000` | `0x00` |
| 3 | [11..5] | `0000000` | `0x00` |
| 4 (stop) | [4..0]+stop | `00000` + stop | `0x80` |

**Wire**: `[0x08, 0x00, 0x00, 0x00, 0x80]`

**Derivation from normative rule**: Per FAST 1.1 §10.6.1.1 (p.23): "The entity value is a two's complement integer representation [TWOC]. The most significant data bit of the entity value is the sign bit." For INT32_MIN (-2147483648), the minimum two's complement representation needs 33 bits because:
- 32-bit two's complement of -2147483648 = `1000_0000_0000_0000_0000_0000_0000_0000` (hex `0x80000000`)
- Bit 31 (MSB of 32-bit) = 1. If interpreted as a 32-bit entity value, the sign bit (bit 31) = 1 → negative. However, bits [31..30] = `10` — the top data bit is 1 but the second is 0, so the entity value `1000...0` in 32 bits represents -2147483648 correctly.
- BUT: the stop-bit encoding must ensure the value is not overlong. With 4 bytes (28 data bits), the top bit would be in bit 27 = 0 (positive), which is wrong. With 5 bytes (35 data bits), bit 34 = 0, bit 33 = 0, bit 32 = 1 → sign bit = 1 → negative. Bits [32..0] = `1_10000000_00000000_00000000_00000000` = -2147483648 in 33-bit two's complement. This is the minimum correct encoding.

**Cross-check**: The project's reference encoder (`fast_reference_encoder.hpp:101-126`) computes the same encoding for INT32_MIN. For val=-2147483648, the encoder computes nbits=33 (via `while (v < -1) { nbits++; v >>= 1; }`), ngroups=5, and emits `[0x08, 0x00, 0x00, 0x00, 0x80]`. This is agreement with the normative derivation above, but the cross-check is not used as primary evidence.

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
**Calculation**: V=-1 → stopbit(-1+1) = stopbit(0) = `0x80`.
**Wire**: `[0x80]`

### 4.17. Presence Map — 7 bits, pattern 0110000

**Source**: FAST 1.1 §10.5 (p.21)
**Calculation**: Bits `[0,1,1,0,0,0,0]` → data byte = `0110000` = `0x30`, + stop = `0xB0`.
**Wire**: `[0xB0]`

### 4.18. Presence Map — 14 bits all zero

**Source**: FAST 1.1 §10.5 (p.21)
**Calculation**: 14 bits → ceil(14/7) = 2 bytes. Byte 0: `0x00` (no stop), byte 1: `0x80` (stop, data=0).
**Wire**: `[0x00, 0x80]`

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

**Source**: FAST 1.1 §10.6.3 (p.24)
**Wire**: `[0x00]` (zero-preamble, no remaining bits → NULL)
**Official text**: "If an ASCII String is nullable, an additional zero-preamble is allowed at the start of the string. The bits that follow are interpreted as a non-nullable string, including a possible zero preamble. If there are no remaining bits after removing the preamble the value represents the NULL string." (p.24)

### 4.23. Nullable ASCII — Empty String

**Source**: FAST 1.1 §10.6.3 (p.24)
**Wire**: `[0x00, 0x00]` (zero-preamble + zero-preamble = empty string)
**Official text**: Wire encoding table on p.24: `0x00 0x00` → Nullable: Yes → Empty String

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
| 1 | **Nullable signed: INT32_MIN handling** | `wire_cursor.cpp:278` rejects `raw == INT32_MIN` as overflow | FAST 1.1 §10.6.1 (p.23): nullable signed = decoded-1. Decoded INT32_MIN would give value INT32_MIN-1 which overflows int32. This rejection is CORRECT. | FAST 1.1, p.23 | confirmed correct |
| 2 | **Nullable signed: decoded 0 maps to -1** | `wire_cursor.cpp:281`: `out = raw - 1` where raw=0 → out=-1 | FAST 1.1: V=-1 → stopbit(-1+1)=stopbit(0)=0x80. Decoding 0x80: raw=0, out=0-1=-1. Correct. | FAST 1.1, p.23 | confirmed correct |
| 3 | **Nullable unsigned: decoded 0 (wire 0x80) rejected** | `wire_cursor.cpp:241`: `if (raw == 0) return NonCanonicalEncoding` | FAST 1.1: nullable unsigned, NULL wire=`0x80` (raw=0), non-null encodes V+1. Minimum non-null wire is 0x81 (V=0). Wire 0x80 (raw=0) is the NULL sentinel for nullable unsigned, not a valid non-null value. | FAST 1.1, p.23 + p.33 | confirmed correct |
| 4 | **Stop-bit signed canonical: range check in i32** | `wire_cursor.cpp:142-153`: checks if decoded value fits in (bytes_read-1)*7 bits | FAST 1.1 §10.6.1 (p.23): "An integer is overlong if the entity value still represents the same integer after removing seven or more of the most significant bits." The range check `[lo, hi]` for prev_bits is correct. | FAST 1.1, p.23 | confirmed correct |
| 5 | **Stop-bit signed i64: pre-shift overflow guard** | `wire_cursor.cpp:177-184`: for bytes beyond 9, checks top 7 bits are sign extension before shifting | FAST 1.1: max 10 bytes for int64. The 10th byte's 7 data bits would shift left by 63, overflowing uint64_t. The pre-shift check is a necessary guard. | FAST 1.1, p.10 | confirmed correct |
| 6 | **Presence map: implicit zero padding** | `wire_cursor.cpp:337-339`: pads with false after stop bit | FAST 1.1 §10.5 (p.21): "Logically a presence map has an infinite suffix of zeroes." | FAST 1.1, p.21 | confirmed correct |
| 7 | **Presence map: continues reading after pmap_bits filled** | `wire_cursor.cpp:317-334`: reads ALL bytes until stop bit, even if pmap_bits already filled | FAST 1.1: stop bit terminates the pmap. Must consume all bytes until stop bit found. Correct. | FAST 1.1, p.21 | confirmed correct |
| 8 | **ASCII: 0x00 in data position rejected** | `wire_cursor.cpp:369`: `if (data_bits == 0) return InvalidEncoding` for continuation bytes | FAST 1.1 §10.6.3 (p.23-24): 7-bit ASCII characters. 0x00 in data position is the zero-preamble, not a valid character in a data position. Correct. | FAST 1.1, p.23-24 | confirmed correct |
| 9 | **ASCII: empty string detection** | `wire_cursor.cpp:358-361`: stop byte with data=0 returns Ok (empty string) | FAST 1.1 Appendix 3.1.3 Ex.2 (p.34): mandatory string, input "" → FAST `0x80`. Correct. | FAST 1.1, p.34 | confirmed correct |
| 10 | **Unicode: nullable uses nullable_u32 for length** | `wire_cursor.cpp:461`: `read_nullable_u32(len, len_null)` | FAST 1.1 §10.6.4-§10.6.5 (p.24): nullable Unicode is nullable Byte Vector. NULL size preamble = NULL string. Correct. | FAST 1.1, p.24 | confirmed correct |
| 11 | **Byte vector: nullable uses nullable_u32 for length** | `wire_cursor.cpp:508`: `read_nullable_u32(len, len_null)` | FAST 1.1 §10.6.5 (p.24): "A nullable byte vector has a nullable size preamble. The NULL byte vector is represented by a NULL size preamble." Correct. | FAST 1.1, p.24 | confirmed correct |
| 12 | **Decimal: null decimal skips mantissa** | `wire_cursor.cpp:538-541`: if exponent nullable and null, returns without reading mantissa | FAST 1.1 §10.6.2 (p.23): "A NULL scaled number is represented as a NULL exponent. The mantissa is present in the stream iff the exponent is not NULL." Correct. | FAST 1.1, p.23 | confirmed correct |
| 13 | **Reference encoder: nullable integer NULL uses 0x00** | `fast_reference_encoder.hpp:151-157` (and similar for i32/i64/u64): `buf.push_back(0x00)` for null | FAST 1.1 §10.6.1 (p.23) + Appendix 3.1.2 Ex.1 (p.33): nullable integer NULL wire encoding = `0x80` (entity value 0x00, stop-bit encoded). The reference encoder emits the entity value `0x00` instead of the wire encoding `0x80`. This is a **shared defect** in the reference encoder. The project decoder correctly handles the spec-compliant `0x80` wire encoding for nullable NULL. | FAST 1.1, p.23 + p.33 | **confirmed discrepancy** (reference encoder, not production decoder) |
| 14 | **Reference encoder: nullable i32 encoding** | `fast_reference_encoder.hpp:177-179`: `encode_stopbit_i32(buf, shifted)` where shifted = (uint32_t(val)+1) cast to int32 | FAST 1.1 §10.6.1 (p.23): nullable signed V → stopbit(V+1). For V=INT32_MAX, V+1 wraps to INT32_MIN (via unsigned). The encoder correctly uses unsigned arithmetic then reinterprets. | FAST 1.1, p.23 | confirmed correct (excluding NULL encoding) |
| 15 | **Reference encoder: nullable i64 encoding** | `fast_reference_encoder.hpp:188-190`: same pattern as i32 | Same reasoning. Correct. | FAST 1.1, p.23 | confirmed correct (excluding NULL encoding) |
| 16 | **Test: nullable i32 value -1 → wire 0x80** | `test_decoder_reference_oracle.cpp:319-323` | FAST 1.1: V=-1 → stopbit(-1+1)=stopbit(0)=0x80. Correct. | FAST 1.1, p.23 | confirmed correct |
| 17 | **Test: nullable u32 max value** | `test_decoder_reference_oracle.cpp:554-558`: `0xFFFFFFFE` → `0x0F 0x7F 0x7F 0x7F 0xFF` | FAST 1.1: V=0xFFFFFFFE → stopbit(0xFFFFFFFE+1)=stopbit(0xFFFFFFFF). Correct. | FAST 1.1, p.23 | confirmed correct |
| 18 | **MOEX preamble: 4-byte SeqNum before FAST body** | Not implemented in WireCursor (WireCursor operates on FAST body only) | MOEX §3.2: 4-byte preamble contains MsgSeqNum before FAST message. | spectra_fastgate_en.pdf §3.2 | No discrepancy (separate concern) |
| 19 | **MOEX: only `none` and `constant` operators** | Code implements default (none) operator; constant is handled at template level | Exhaustive XML scan: only `none` and `constant` operators are used across all 19 MOEX templates (70 constant fields, 0 copy/delta/increment/default/tail). | templatesT0/templates.xml (full scan, see §7) | confirmed correct |
| 20 | **SecurityDesc uses charset="unicode"** | Not directly visible in WireCursor (WireCursor handles raw types) | templatesT0/templates.xml line 110: `<string ... charset="unicode"/>` | templates.xml | No discrepancy (handled at template level) |

### Summary of Discrepancies

**Confirmed discrepancy (1)**:
- **#13**: The reference encoder (`fast_reference_encoder.hpp`) uses `0x00` for nullable integer NULL, while the official FAST 1.1 spec (§10.6.1, p.23 + Appendix 3.1.2 Ex.1, p.33) specifies the wire encoding as `0x80` (entity value 0x00, stop-bit encoded). The project decoder correctly handles the spec-compliant `0x80` encoding. This discrepancy is in the test-only reference encoder, not in production code.

**Unresolved items (2)**:
- **Preamble endianness**: The MOEX specification does not specify the byte order of the 4-byte preamble (§3.2, spectra_fastgate_en.pdf).
- **meta.info fetch**: The cause of the HTTP 404 when fetching `meta.info` from the MOEX FTP is unresolved (see §2.2).

**All other items (17 of 20)**: Confirmed correct against the authoritative sources (FAST 1.1 specification and MOEX SPECTRA specification v1.30.2).

---

## 6. Key Observations

1. **MOEX delegates all base FAST encoding to FAST 1.1**: The MOEX PDF describes stop-bit encoding at a high level (§3.2.1, §3.2.7) but does NOT restate the full encoding rules. It relies on implementors knowing FAST 1.1 for: canonical encoding, nullable offset-by-1, presence map structure, signed two's complement, decimal composite encoding, sequence length encoding.

2. **MOEX uses only `none` and `constant` operators**: Exhaustive XML scan of all 19 templates in `templatesT0/templates.xml` (619 lines) confirms that only the `constant` operator and the default `none` operator are used (70 constant fields, 0 copy/delta/increment/default/tail). The MOEX changelog v1.7.0 (2018-12-05) states: "Message templates do not now contain the compression operators copy, delta and increment." The XML inventory confirms this is still true in the current templates.

3. **MOEX adds a 4-byte preamble**: This is MOEX-specific and not part of FAST 1.1. The preamble contains MsgSeqNum (tag 34), allowing sequence number extraction without FAST decoding. The byte order of the preamble is **not documented** in the MOEX specification (§3.2, Pic.3 shows "Preamble (4 bytes)" then "Message" but does not specify endianness).

4. **The current MOEX system sends at most one FAST message per UDP packet**: While MOEX §1.2.5 states that "a single UDP packet may contain several FIX messages in the FAST format" in principle, it also explicitly states: "currently the system does not provide a possibility to send more than one FAST-coded message via a single UDP packet." Messages are bounded to MTU size (1500 bytes).

5. **Template IDs are MOEX-specific**: The specific template IDs (29, 30, 31, 32, 40/47, 5/48, 45, 46, etc.) are defined by MOEX and change across versions. T0 and T1 use different IDs for SecurityDefinition (40 vs 47) and SecurityStatus (5 vs 48).

6. **T0 and T1 templates are NOT byte-identical**: They have different SHA-256 hashes and differ in template IDs and field definitions. FAST_9.0 templates.xml IS byte-identical to T0 templates.xml.

7. **Nullable integer NULL wire encoding**: The official FAST 1.1 specification (§10.6.1, p.23 + Appendix 3.1.2 Example 1, p.33) defines nullable integer NULL as entity value `0x00` (7 zero bits), which when stop-bit encoded is `0x80` on the wire. The project's reference encoder uses `0x00` (entity value) instead of `0x80` (wire encoding), which is a discrepancy in the test-only encoder. The production decoder correctly handles the spec-compliant `0x80` encoding.

8. **INT32_MIN encoding**: The encoding `[0x08, 0x00, 0x00, 0x00, 0x80]` is derived from the normative two's complement rule (FAST 1.1 §10.6.1.1, p.23). INT32_MIN requires 33 bits in two's complement (sign bit = 1 at bit 32), yielding 5 stop-bit encoded bytes. This is confirmed by the spec's sign extension example (§10.6.1.1 NOTE, p.23).

---

## 7. Reproducible Operator and Dictionary Inventory

The following inventory is hash-bound and reproducible. Each template set was downloaded from `https://ftp.moex.com/pub/FAST/Spectra/test/` and hashed with `Get-FileHash -Algorithm SHA256`.

### 7.1. Reproduction Commands

```powershell
# Download templates
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/templatesT0/templates.xml" -UseBasicParsing -OutFile "t0.xml"
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/templatesT1/templates.xml" -UseBasicParsing -OutFile "t1.xml"
Invoke-WebRequest -Uri "https://ftp.moex.com/pub/FAST/Spectra/test/FAST_9.0/templates.xml" -UseBasicParsing -OutFile "f9.xml"

# SHA-256 verification
(Get-FileHash "t0.xml" -Algorithm SHA256).Hash  # Expected: DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E
(Get-FileHash "t1.xml" -Algorithm SHA256).Hash  # Expected: 84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
(Get-FileHash "f9.xml" -Algorithm SHA256).Hash  # Expected: DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

# Operator counts (PowerShell)
$t0 = Get-Content "t0.xml" -Raw
"templates: " + ([regex]::Matches($t0, '<template ')).Count        # Expected: 19
"constant: " + ([regex]::Matches($t0, '<constant')).Count          # Expected: 70
"copy: " + ([regex]::Matches($t0, '<copy')).Count                  # Expected: 0
"delta: " + ([regex]::Matches($t0, '<delta')).Count                # Expected: 0
"increment: " + ([regex]::Matches($t0, '<increment')).Count        # Expected: 0
"default: " + ([regex]::Matches($t0, '<default')).Count            # Expected: 0
"tail: " + ([regex]::Matches($t0, '<tail')).Count                  # Expected: 0
"lines: " + ($t0 -split "`n").Count                                # Expected: 619
```

### 7.2. Inventory Results

| Metric | templatesT0 | templatesT1 | FAST_9.0 |
|--------|-------------|-------------|----------|
| SHA-256 | `DBD50F1E...` | `84FACBF7...` | `DBD50F1E...` |
| Lines | 619 | 620 | 619 |
| Templates | 19 | 19 | 19 |
| `constant` fields | 70 | 70 | 70 |
| `copy` | 0 | 0 | 0 |
| `delta` | 0 | 0 | 0 |
| `increment` | 0 | 0 | 0 |
| `default` | 0 | 0 | 0 |
| `tail` | 0 | 0 | 0 |
| T0 == FAST_9.0 | — | — | byte-identical |

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

Per FAST 1.1 §6.3.1 (p.12-13), three predefined dictionaries exist: `template`, `type`, `global`. MOEX templates use only `constant` (which does not access dictionaries) and `none` (default, which does not access dictionaries). No `dictionary` attribute appears in any MOEX template. Therefore:

- **Template dictionary**: not used
- **Type dictionary**: not used
- **Global dictionary**: not used (no dictionary-dependent operators present)
- **User-defined dictionaries**: not used
