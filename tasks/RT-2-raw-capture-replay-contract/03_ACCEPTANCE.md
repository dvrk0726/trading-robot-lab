# RT-2 Acceptance Criteria

RT-2 is accepted only when every mandatory item below is satisfied. Green CI alone is insufficient if the binary contract, lifecycle or tests violate the specification.

## 1. Scope and architecture

- [ ] Implementation is confined to offline synthetic raw segment write/read/validate/replay.
- [ ] No socket, multicast, MOEX connection, FAST decode, A/B merge/dedup, recovery, book building, FIX/TWIME or order sending was added.
- [ ] The module does not depend on Python, a database, dashboard or strategy process.
- [ ] Payload is treated as opaque bytes.
- [ ] `capture_index` is documented and implemented as local source-stream order, not exchange sequence.
- [ ] A replayable stream set is keyed by `(session_id, source_id, channel_id)`.
- [ ] Multi-stream timestamp merge is not implemented in RT-2.
- [ ] Existing RT-1 and QSH semantics are unchanged.

## 2. Portable v1 binary contract

- [ ] Magic bytes are exactly `MXRAWV1\0`, `REC1` and `MXENDV1\0`.
- [ ] `format_version=1`, record header size `44` and footer size `92` are enforced.
- [ ] v1 format/footer flags are zero; only record flag bit 0 is accepted.
- [ ] Exact field order, widths, enums, limits and byte order are documented.
- [ ] Integers are manually encoded little-endian.
- [ ] No raw C++ struct/ABI serialization is used.
- [ ] Header is at most 4096 bytes; strings are valid bounded UTF-8.
- [ ] Non-zero session/source IDs and required hashes are validated.
- [ ] Zero-length payload and 1 MiB maximum payload behave exactly as specified.
- [ ] Record framing preserves exact payload bytes and timestamp-validity semantics.
- [ ] Footer contains counts, totals, index bounds and `content_sha256` over pre-footer bytes.
- [ ] Whole-file SHA-256 remains separately named provenance.
- [ ] CRC32C and SHA-256 use known reviewed vectors.

## 3. Writer safety

- [ ] Writer uses `.mxraw.partial` until successful finalization.
- [ ] Canonical lowercase filename includes session, source, channel and fixed-width segment index.
- [ ] Existing partial/final files are never overwritten by default.
- [ ] Empty segment finalization is rejected.
- [ ] Append/reopen/re-finalize after finalization is rejected.
- [ ] A record is counted only after its complete successful write.
- [ ] Failed record write does not advance index, count or byte totals.
- [ ] Finalization writes footer, flushes, closes and same-directory renames.
- [ ] Rename/finalize failure leaves explicit error and does not claim success.
- [ ] Failed/unfinished partial files are not silently deleted or replayed.
- [ ] Core serialized output does not depend on current time, random generation or environment.

## 4. Deterministic rotation

- [ ] Rotation by record limit is deterministic.
- [ ] Rotation by byte limit is deterministic and includes header/footer overhead.
- [ ] Segment size above 64 GiB is rejected.
- [ ] Boundary record is neither lost nor duplicated.
- [ ] Segment and capture indexes remain contiguous.
- [ ] A record too large for an empty legal segment is rejected.
- [ ] Identical explicit metadata/policy/records produce byte-identical segment sets.
- [ ] Changing only rotation limits does not change logical `replay_sha256`.

## 5. Reader and validator

- [ ] Reader streams records and does not load the complete segment/stream set into memory.
- [ ] All untrusted lengths/offsets are checked before allocation or seeking.
- [ ] Wrong magic/version/size/flags/reserved/enums/UTF-8 are rejected.
- [ ] Filename identity is compared with content identity.
- [ ] Truncation at header, record, payload, checksum and footer boundaries is detected.
- [ ] Payload/record/footer CRC32C failures are detected.
- [ ] Content SHA-256 and footer totals are verified.
- [ ] Extra bytes after the footer are rejected.
- [ ] Partial, truncated, corrupt, unsupported and I/O failures are distinguishable.
- [ ] Stream-set validation detects duplicate/missing indexes, overlap, metadata/hash mismatch and monotonic-time regression.
- [ ] Directory inspection groups independent stream sets instead of merging them.

## 6. Replay

- [ ] Replay emits only validated finalized records.
- [ ] Segment ordering uses parsed numeric index and capture ordering is exact.
- [ ] Multiple streams require explicit source/channel selection.
- [ ] Callback receives byte-identical payloads and immutable required metadata.
- [ ] Callback failure stops replay with explicit status.
- [ ] No sleep, pacing, network send or decode occurs.
- [ ] `MXREPLAY1\0` canonical digest framing matches the requirements exactly.
- [ ] Fixed synthetic vector has an independently derived expected digest.
- [ ] Digest is identical on Windows and Linux.
- [ ] Digest changes when logical metadata, record metadata or payload changes.
- [ ] Digest is invariant to segment rotation boundaries.

## 7. CLI and report

- [ ] Synthetic generation, inspection and replay are accessible through documented CLI commands.
- [ ] Help, missing arguments, invalid values and unknown options have correct non-zero behavior.
- [ ] No overwrite is the default.
- [ ] Strict validation returns non-zero for trust-affecting findings.
- [ ] Ambiguous multi-stream replay fails until a stream selector is supplied.
- [ ] JSON is valid, deterministic and versioned.
- [ ] Reports include stream key, hashes, counts, timestamp/index bounds, partial findings, issues and overall status.
- [ ] Reports do not include payload bytes, private addresses, ports or credentials.
- [ ] Expected user/input failures do not escape as uncaught exceptions.

## 8. Tests and CI

- [ ] All task tests remain active in Release.
- [ ] Tests are self-contained and order-independent.
- [ ] Windows/MSVC RT-2 build/tests pass.
- [ ] Linux/GCC RT-2 build/tests pass.
- [ ] Existing RT-1 6/6 executables pass.
- [ ] Existing QSH/M10X 20/20 tests pass.
- [ ] Python tests and shared contracts pass.
- [ ] Repository hygiene passes.
- [ ] Corruption, truncation, overflow, short I/O and callback-failure tests exist.
- [ ] At least one independently derived golden byte/hash vector prevents writer-reader common-mode validation.

## 9. Security and repository hygiene

- [ ] No pcap/pcapng or raw MOEX packet data committed.
- [ ] No official private XML, real endpoint data, credentials or login identifiers committed.
- [ ] Generated `.mxraw` and `.partial` files remain outside Git except a tiny explicitly reviewed synthetic golden fixture if absolutely required.
- [ ] No binaries, build directories or large generated reports committed.
- [ ] Hygiene checker covers new generated raw extensions/paths.

## 10. Documentation and handoff

- [ ] `cpp/moex_raw/README.md` documents exact format, lifecycle, naming, rotation, stream selection, replay digest, build/usage and limitations.
- [ ] MiMo report exists at `agent_workspaces/mimo/reports/<date>-rt2-raw-capture-replay.md`.
- [ ] Report includes Issue, branch, commit SHA, PR, changed files, commands and exact results.
- [ ] Report distinguishes implementation-commit evidence from later evidence/docs commits.
- [ ] Issue and PR metadata contain actual final head and latest CI.
- [ ] MiMo stops at `READY_FOR_REVIEW` and does not merge or start RT-3.

## Mandatory stop conditions

MiMo must stop and return the Issue to Architecture/Review if any of these arise:

```text
the format requires undocumented ABI/native struct serialization;
a required field cannot be encoded deterministically across platforms;
production network capture appears necessary to complete RT-2;
real MOEX data or private endpoint metadata appears necessary for tests;
writer cannot distinguish complete write from partial/failed write;
reader would allocate from unchecked file lengths;
checksum/truncation failure would be accepted as valid;
scope expands into FAST decoding, A/B merge, recovery or books;
existing RT-1 or QSH tests require semantic weakening.
```

## Owner review gate

RT-2 has no graphical UI. Owner review is a short local demonstration using synthetic data:

```text
1. generate deterministic multi-segment synthetic capture;
2. inspect it and show valid stream/hash/count summaries;
3. replay it and show the expected digest;
4. regenerate with different rotation limits and show the same replay digest;
5. mutate/truncate a copy and show explicit rejection;
6. show ambiguous multi-stream replay rejection and explicit selection;
7. verify no network connection is opened.
```

Architecture/Review must inspect code, tests, format documentation and CI before the owner demonstration.