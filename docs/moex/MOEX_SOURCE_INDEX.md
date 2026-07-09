# MOEX_SOURCE_INDEX

Date: 2026-07-09
Status: source index / living document

## Purpose

This file is the controlled index of MOEX-related official sources used by the project.

It is not a dump of manuals.

Rules:

```text
- Do not commit full PDFs, ZIP distributions, templates or raw market data unless explicitly approved.
- Store only links, document purpose, reading status and project-relevant notes.
- Extract important implementation notes into focused docs after review.
- Keep credentials, local configs and raw captures out of Git.
```

## Reading workflow

```text
1. Add source to this index.
2. Read one source or one logical section at a time.
3. Extract only project-relevant facts.
4. Write a focused summary doc if needed.
5. Convert summary into implementation tasks only after owner approval.
```

## Source status legend

```text
not_started  = source identified but not reviewed
in_progress  = being read / partially summarized
summarized   = key points extracted into project docs
blocked      = source unavailable or requires access
superseded   = older source kept only for history
```

---

## 1. MOEX SPECTRA test access

| Field | Value |
|---|---|
| Source | https://www.moex.com/s438 |
| Type | Web page |
| Area | SPECTRA test contour access |
| Status | in_progress |
| Priority | high |

### Purpose

Used to understand how to request MOEX SPECTRA test access and what protocols/environments are available.

### Key extraction targets

```text
- who can request test access;
- T0 vs T1 test stands;
- internet vs dedicated-channel access;
- how to request login/password;
- what to specify in the request: FIX/FAST;
- whether FAST Full_orders_log is available on test contour;
- what steps are required before production use.
```

### Notes

```text
Use this source before any real MOEX connectivity work.
Do not request or store credentials in Git.
```

---

## 2. General MOEX test access

| Field | Value |
|---|---|
| Source | https://www.moex.com/s328 |
| Type | Web page |
| Area | General test access / development environment |
| Status | not_started |
| Priority | medium |

### Purpose

Used to understand the general MOEX test-access model for software development and debugging.

### Key extraction targets

```text
- what test access is for;
- which markets/protocols are covered;
- whether test data differs from real production market data;
- support process and contact points.
```

---

## 3. MOEX SPECTRA FAST test directory

| Field | Value |
|---|---|
| Source | https://ftp.moex.com/pub/FAST/Spectra/test/ |
| Type | Public FTP/HTTP directory |
| Area | FAST market data test documentation and templates |
| Status | not_started |
| Priority | high |

### Known contents

```text
spectra_fastgate_ru.pdf
spectra_fastgate_en.pdf
FAST_9.0/templates.xml
templatesT0/configuration.xml
templatesT0/templates.xml
templatesT1/configuration.xml
templatesT1/templates.xml
fast_sensor.zip
meta.info
```

### Purpose

Used to prepare the FAST market-data collector and decoder for SPECTRA test access.

### Key extraction targets

```text
- FAST session/feed model;
- incremental vs snapshot channels;
- message replay / instrument replay;
- sequence numbers and recovery;
- templates.xml message definitions;
- configuration.xml multicast groups/ports/channels;
- which template version is current for T0;
- data model needed for normalized market events.
```

### Notes

```text
Do not commit downloaded ZIP/PDF/XML files by default.
If templates/configuration are needed for development, keep local copies outside Git or commit only minimal derived notes after owner approval.
```

---

## 4. MOEX SPECTRA FIX test docs

| Field | Value |
|---|---|
| Source | https://ftp.moex.com/pub/FIX/Spectra/test/docs/ |
| Type | Public FTP/HTTP directory |
| Area | FIX order-entry test documentation |
| Status | not_started |
| Priority | high |

### Known contents

```text
spectra_fixgate_ru.pdf
spectra_fixgate_en.pdf
meta.info
back/
```

### Purpose

Used to prepare the FIX test client for SPECTRA test order-entry and later certification readiness.

### Key extraction targets

```text
- Logon / HeartBeat / Logout;
- session sequence handling;
- Resend Request / recovery;
- NewOrderSingle;
- OrderCancelRequest;
- OrderCancelReplaceRequest;
- OrderStatusRequest;
- MassCancel;
- ExecutionReport;
- reject messages and error handling;
- required tags for SPECTRA futures/options;
- test-only safety restrictions.
```

### Notes

```text
FIX client work must remain test-only until explicit owner approval and certification gate completion.
```

---

## 5. MOEX FAST Gate product page

| Field | Value |
|---|---|
| Source | https://www.moex.com/s441 |
| Type | Web page |
| Area | FAST Gate production market data |
| Status | in_progress |
| Priority | high |

### Purpose

Used to understand production FAST Gate access for real-time market data.

### Key extraction targets

```text
- what FAST Gate provides;
- SPECTRA bandwidth/network requirements;
- production access requirements;
- market data agreement requirements;
- difference between test FAST and production FAST.
```

---

## 6. MOEX Full_orders_log product page

| Field | Value |
|---|---|
| Source | https://www.moex.com/a588 |
| Type | Web page |
| Area | Full order log / unlimited-depth order book data |
| Status | in_progress |
| Priority | high |

### Purpose

Used to evaluate whether production Full_orders_log is the correct source for current real order-book data collection.

### Key extraction targets

```text
- whether it provides all anonymous SPECTRA transactions;
- whether it allows unlimited-depth order book reconstruction;
- whether it includes deleted/moved orders;
- pricing;
- contract/licensing requirements;
- how it relates to FAST Gate tariffs.
```

### Notes

```text
This is likely the main production data source for current real microstructure research.
It is separate from production order-entry.
```

---

## 7. MOEX SIMBA product page

| Field | Value |
|---|---|
| Source | https://www.moex.com/s3320 |
| Type | Web page |
| Area | Low-latency market data / HFT-oriented feed |
| Status | not_started |
| Priority | low for MVP |

### Purpose

Used only for later comparison with FAST Gate.

### Current decision

```text
SIMBA is deferred.
Do not start MVP from SIMBA or colocation.
Initial path is SPECTRA test FIX/FAST -> production FAST + Full_orders_log -> paper trading.
```

---

## 8. MOEX VPTS certification procedure

| Field | Value |
|---|---|
| Source | porjadok-sertifikacii-vpts-2023.pdf |
| Type | Uploaded PDF / official procedure document |
| Area | Production trading software certification gate |
| Status | summarized |
| Priority | high for production trading, low for market-data-only collector |

### Purpose

Used to define the mandatory certification gate before any production order sending.

### Extracted project rule

```text
No production order sending until VPTS certification requirements are implemented, tested and explicitly approved by the owner.
```

### Important extracted requirements

```text
- interaction logging is mandatory;
- recovery from network failure/restart/exchange restart is mandatory;
- SPECTRA software must run through a full trading day on test polygon;
- SPECTRA FIX scenarios include Logon/Heartbeat, reconnect, order placement, cancellation, replacement, status request and mass cancel;
- DropCopy ExecutionReport verification is required if DropCopy is used;
- production trading is blocked by owner gate and certification/compliance readiness.
```

### Related project decision

```text
decisions/ADR-0004-moex-vpts-certification-gate.md
```

---

## 9. Public qsh_parser OrdLog issue

| Field | Value |
|---|---|
| Source | https://github.com/AndrewKhodyakov/qsh_parser/issues/3 |
| Type | GitHub issue |
| Area | Historical QSH / OrdLog parsing clue |
| Status | summarized |
| Priority | medium |

### Purpose

Used as a clue that points to the official QSH v4 specification and confirms that the public parser originally did not support OrderLog.

### Extracted notes

```text
- qsh_parser supported QSH v4;
- it supported Quotes and Deals, but not OrderLog at the time;
- issue points to QSH specification: https://www.qscalp.ru/store/qsh.pdf;
- gzip compression behavior is documented in the QSH spec.
```

---

## 10. QSH v4 specification

| Field | Value |
|---|---|
| Source | https://www.qscalp.ru/store/qsh.pdf |
| Type | Public PDF |
| Area | Historical QSH format / OrdLog flags |
| Status | summarized partially |
| Priority | high for QSH parser |

### Purpose

Used to validate QSH v4 stream and OrdLog flag semantics.

### Extracted notes

```text
- QSH files may be gzip-compressed while keeping .qsh extension;
- OrderLog stream type exists separately from Quotes and Deals;
- 0x94 in OrdLog flags decodes as Add + Buy + Quote, not TxEnd;
- TxEnd is bit 10 / 0x0400;
- Quote is bit 7 / 0x0080.
```

### Related implementation

```text
cpp/qsh_ingest/include/qsh/qsh_types.hpp
cpp/qsh_ingest/src/main.cpp
cpp/qsh_ingest/tests/test_quote_modes.cpp
```

---

## Next reading order

Recommended sequence:

```text
1. spectra_fastgate_ru.pdf
2. templatesT0/configuration.xml
3. templatesT0/templates.xml
4. spectra_fixgate_ru.pdf
5. MOEX SPECTRA test access page
6. Full_orders_log product page
7. VPTS certification procedure only when approaching order-entry/certification work
```

## Next summary docs to create after reading

Only after owner approval:

```text
docs/moex/fast_spectra_notes.md
docs/moex/fast_spectra_templates_notes.md
docs/moex/fix_spectra_notes.md
docs/moex/spectra_test_access_notes.md
docs/moex/full_orders_log_production_data_notes.md
```
