# Project State

Дата обновления: 2026-07-15  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: RT-4 Gate A1 UDP framing setup — Issue #42 / Draft PR #43

## Архитектурные границы

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry требует VPTS/certification и решения Owner.
Secrets, private connection data and raw market data не хранятся в Git.
```

## Performance-first development policy

```text
C++20 owns every latency-critical realtime/hot-path component:
  MOEX packet/framing, sequencing and recovery, FAST decoding,
  normalized realtime market-data events, future L3/L2 books,
  realtime state, RiskEngine, OrderManager and future runtime
  execution components.
Python is not used in the hot path; it is used for research,
  analysis, reports, UI, orchestration and offline tooling.
Correctness, deterministic behavior and fail-closed validation
  are mandatory and cannot be sacrificed for speed.
Performance claims require measured Release benchmarks.
Avoid speculative abstractions, compatibility layers and
  unnecessary allocations in the critical path.
```

## Завершённые этапы

### RT-1 — DONE

```text
Issue #14
Implementation PR #16
Merge commit: ab74f560c1bcf9d09ae7bdfb8552c745928fd022
Offline MOEX configuration/templates inspector
Windows/Linux Release tests
```

### RT-2 — DONE

```text
Issue #18
Specification PR #19
Implementation PR #20
Merge commit: 060371112d921c1c1f4055cfbdb99049bdf8a2af
.mxraw v1 raw segments and deterministic replay
Owner local Release acceptance passed
```

RT-2 does not decode FAST and does not infer exchange sequence from `capture_index`.

### RT-3 — DONE

```text
Issue #21: closed completed
Implementation PR #23: merged
Final reviewed PR head: a1443d3f909151d327b83042e43c1cc4c04cc732
Merge commit on main: 377618c360c165d88dde4cfe0cee87f8747cba03
Pre-merge CI #156: success
Post-merge main CI #157: success
Owner-local Windows Release acceptance: inventory 15/15, PASS
```

Accepted implementation: specialized MOEX SPECTRA T0/T1 decoder, not a general-purpose FAST 1.1 engine.

Historical accepted RT-3 compile/test profiles:

```text
T0 SHA-256:
DBD50F1E0BECC2B2EBD9DAC8E4C6609BA1538566811B610CDE9B6DD3E7F66A8E

T1 SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
```

Current official endpoint contents are tracked separately by RT-4.

Required RT-3 scope:

```text
one bounded FAST message body
template ID and previous-template-ID reuse
presence maps
ordinary/nullable integer primitives
ASCII and Unicode strings
exact decimal
field without operator
constant
sequences and single length instruction
limits, reset, deterministic errors and transactional rollback
T0/T1 official XML compilation
Windows/Linux Release tests
```

Excluded and fail-closed:

```text
default/copy/increment/delta/tail
generic field dictionaries/scopes/keys
user-defined dictionaries
typeRef/templateRef/groupRef
reference resolution/cycles
generic groups outside accepted inventory
byteVector
decimal component operators
historical profile compatibility
```

### CI-1 — DONE

```text
Required-check-preserving routing
Docs-only smoke PR #32
Main SHA: 0699a533a1ed44a9d47e05b049aca4061bebaac0
Post-merge CI #165: success
```

### QSH retirement — DONE

```text
Issue #33
PR #34 merged
Main merge SHA: 7c05cfb979cd0144be508e41a6f3a6229bfab1cb
Post-merge CI #175: success
```

QSH/QScalp/OrdLog product support, old QSH L3/L2 book, Trading Lab QSH integration, tombstone job, `run_qsh` and QSH routing are retired and absent. `*.qsh` remains only a raw-market-data safety ban.

### Performance-first documentation — DONE

```text
Issue #36: closed completed
PR #37: merged
Main merge SHA: 7a23f57eab119df98e4cea7eaf239ad504d4bb88
CI-2 caching: POSTPONED
RT-4 selected as next gate
```

### RT-4 specification — DONE

```text
Issue #38: closed completed
PR #39: merged
Final reviewed PR head: afd128a49584fce1131323ac7b19e5b5d7b1997a
Main merge SHA: 136293ede211619b7d9198d85ed3afb0f2577514
Post-merge main CI #189: success
```

Authoritative files:

```text
docs/rt4_spectra_framing_sequencing_recovery_spec.md
docs/rt4_moex_fast_source_update_2026-07-15.md
```

Approved architecture structure:

```text
Gate A: UDP framing, A/B sequencing, bounded reordering,
        gap detection, explicit monotonic timeout and fail-closed
Gate B: RT-2 .mxraw + RT-3 integration, tag-34 verification,
        one-time preamble endian AutoVerify
Gate C: Snapshot + buffered Incremental recovery
Gate D: Windows/Linux Release performance and production evidence
```

Current official-source evidence:

```text
MOEX SPECTRA FAST document: v1.30.2, 2026-04-10
T0 configuration SHA-256:
AE80702BC3E179CAF5DA025E94FDAC6AC7A6A4FF1353E7FB5D0396DE987C4118
Current T0 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
Current T1 templates SHA-256:
84FACBF784676FD1A0442297F45DB4D3BBA11AE938618F082BEABEF62A782A3F
External UDP preamble byte order: unresolved in official text
```

The 2026-07-11 RT-3 source audit remains historical evidence. The 2026-07-15 endpoint update is recorded separately by RT-4.

Gate A uses explicit LittleEndian or BigEndian with no default. Gate B may compare both values with decoded tag 34, fail closed on ambiguity and lock one byte order after verification.

### RT-4 post-merge state sync — DONE

```text
Issue #40: closed completed
PR #41: merged
Final reviewed PR head: 6789fb3621d70465114a32d2b146562e7f6809e8
Main merge SHA: acb74763e7dd395f210ac738c425c7d544a6cb51
Post-merge main CI #194: success
```

## MOEX access and connectivity state

```text
MOEX support confirmed FAST access activation for the registered external static IPv4.
Official Windows VPN instruction: PPTP; data encryption optional.
Current home external IPv4 matches the registered address.
Windows VPN: remote-access error 807.
TCP 1723 to supplied VPN endpoint: unreachable from registered home connection.
Windows Firewall default outbound policy: Allow.
Explicit enabled outbound block rule targeting endpoint: not found.
Registered antivirus: Windows Defender only.
Third-party antivirus/network filter: not found.
MOEX support follow-up: pending.
```

The VPN endpoint, external/private IP addresses, credentials, VPN profiles and raw/decoded market-data packets are not stored.

This connectivity state does not prove a framing defect and does not authorize production acceptance.

## Current gate — RT-4 Gate A1 setup

```text
Issue #42: open
Draft PR #43: open
Branch: mimo/issue-42-rt4-a1-udp-framing
Base main SHA: acb74763e7dd395f210ac738c425c7d544a6cb51
Setup files: AI_CONTEXT.md, PROJECT_STATE.md, ROADMAP.md
C++ implementation: not authorized
CI implementation changes: not authorized
MiMo: not authorized
Merge: not authorized
```

Approved A1 implementation boundary after separate Owner authorization:

```text
one current UDP datagram
4-byte external MsgSeqNum preamble
explicit LittleEndian or BigEndian; no default
exactly one borrowed FAST body beginning at byte 4
bounded validation and stable FrameCode result
no payload copy and no heap allocation
one Release-active framing CTest
MOEX FAST inventory 15 -> 16 without changing required-check job names
```

Deterministic framing error precedence:

```text
invalid limits or invalid byte-order enum -> InvalidConfig
payload size 0..3                       -> DatagramTooShort
payload size 4                          -> EmptyFastBody
payload size > max_datagram_bytes       -> DatagramTooLarge
payload size 5..max                     -> Ok
```

A1 excludes A/B sequencing, serial arithmetic, duplicate suppression, reorder storage, gap deadlines, sockets, `.mxraw`, RT-3 integration, AutoVerify, SequenceReset, Snapshot recovery and benchmarks.

## Sequence

```text
Issue #42 / Draft PR #43 docs-only setup
-> docs-only CI
-> Architecture Review of setup state
-> separate Owner authorization for A1 implementation and MiMo
-> one bounded implementation commit in existing branch/PR
-> full CI
-> Architecture Review
-> separate Owner merge authorization
-> post-merge main CI verification
```

CI-2 caching is POSTPONED, not started and not authorized.

Future normalized events and the new L3/L2 book are designed from official MOEX SPECTRA data; no automatic reuse of the old QSH code.
