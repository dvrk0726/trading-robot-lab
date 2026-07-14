# Future Rewrite Notes

Date: 2026-07-09
Status: active engineering log

## Purpose

Record lessons, mistakes, bottlenecks and ideas discovered while building the first MOEX FAST/FIX platform, so a later production rewrite does not repeat avoidable design errors.

This is not a task dump. Add only findings that can affect architecture, correctness, performance, reliability, operability or certification.

## Entry format

```text
Date:
Area:
Observed problem or idea:
Evidence/measurement:
Why it matters:
Current workaround:
Recommended future design:
Priority: low / medium / high / critical
Status: open / accepted / implemented / rejected
Related files/issues:
```

## Rules

```text
1. Separate measured facts from guesses.
2. Include evidence: benchmark, log, packet sample, failure case or specification reference.
3. Do not rewrite the system immediately for every note.
4. Promote accepted architectural decisions into an ADR.
5. Promote actionable near-term work into ROADMAP.md or an approved task.
6. Never store credentials, raw market data or private connection details here.
```

## Initial notes

### NOTE-001 — Raw data must remain reproducible

```text
Area: data capture
Observation: decoded formats and interpretation rules will change during development.
Future design: preserve immutable raw capture with checksums and enough provenance to replay it through newer decoders.
Priority: critical
Status: accepted
```

### NOTE-002 — Measure before production optimization

```text
Area: performance
Observation: actual ORDERS-LOG peak and burst rates are not yet known.
Future design: build benchmark counters first, then size CPU, queues, disks and network from T0/production measurements with headroom.
Priority: critical
Status: accepted
```

### NOTE-003 — Databases must not block capture

```text
Area: reliability
Observation: analytical databases can slow down, restart or become unavailable.
Future design: raw capture is independent; ClickHouse/PostgreSQL are downstream consumers and must not be synchronous dependencies of UDP receive.
Priority: critical
Status: accepted
```

### NOTE-004 — Legacy or third-party source-specific semantics must not leak into official contracts

```text
Area: data contracts
Observation: retired or third-party data sources can carry source-specific flags, enum layouts and semantics that differ from the official MOEX SPECTRA specification.
Future design: official MOEX normalized events and order-book contracts must be derived only from the live specification; legacy or third-party source-specific flags and semantics must never be embedded in official contracts or reused as a common enum layer.
Priority: critical
Status: accepted
```

### NOTE-005 — First implementation should stay simple

```text
Area: system architecture
Observation: adding many languages, brokers, queues and microservices early increases failure modes before traffic is measured.
Future design: begin with one C++ collector process plus offline Python research; split services only after profiling and operational evidence.
Priority: high
Status: accepted
```
