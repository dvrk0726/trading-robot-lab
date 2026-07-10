# RT-2 Acceptance Criteria

RT-2 is accepted only when every mandatory item below is satisfied. Green CI alone is insufficient if the binary contract, lifecycle or tests violate the specification.

## 1. Scope and architecture

- [ ] Implementation is confined to offline synthetic raw segment write/read/validate/replay.
- [ ] No socket, multicast, MOEX connection, FAST decode, book building, FIX/TWIME or order sending was added.
- [ ] The module does not depend on Python, a database, dashboard or strategy process.
- [ ] Payload is treated as opaque bytes.
- [ ] `capture_index` is clearly documented as local capture order, not exchange sequence.
- [ ] Existing RT-1 and QSH semantics are unchanged.

## 2. Portable binary contract

- [ ] v1 magic, version, field order, widths, byte order and limits are documented.
- [ ] Integers are manually encoded little-endian.
- [ ] No raw C++ struct/ABI serialization is used.
- [ ] Header strings are bounded, valid UTF-8 and serialized deterministically.
- [ ] Session/source metadata includes required hashes and logical source identity.
- [ ] Record framing preserves exact payload bytes and explicit timestamp validity.
- [ ] Footer contains counts, byte totals, index bounds and content SHA-256.
- [ ] CRC32C and SHA-256 use known test vectors.

## 3. Writer safety

- [ ] Writer uses `.mxraw.partial` until successful finalization.
- [ ] Existing partial/final files are never overwritten by default.
- [ ] Empty segment finalization is rejected.
- [ ] Append/finalize after finalization is rejected.
- [ ] A record is counted only after its complete successful write.
- [ ] Finalization writes footer, flushes, closes and same-directory renames.
- [ ] Rename/finalize failure leaves explicit error and does not claim success.
- [ ] Failed/unfinished partial files are not silently deleted or replayed.

## 4. Rotation

- [ ] Rotation by record limit is deterministic.
- [ ] Rotation by byte limit is deterministic.
- [ ] Boundary record is neither lost nor duplicated.
- [ ] Segment and capture indexes remain contiguous.
- [ ] A record too large for an empty legal segment is rejected.
- [ ] Identical explicit metadata and records produce byte-identical segment sets.

## 5. Reader and validator

- [ ] Reader streams records and does not load the complete segment/session into memory.
- [ ] All untrusted lengths/offsets are checked before allocation or seeking.
- [ ] Wrong magic/version/flags/enums/UTF-8 are rejected or explicitly reported as specified.
- [ ] Truncation at header, record, payload and footer boundaries is detected.
- [ ] Payload/record/footer CRC32C failures are detected.
- [ ] Content SHA-256 and footer totals are verified.
- [ ] Extra bytes after the footer are rejected.
- [ ] Partial, truncated, corrupt, unsupported and I/O failures are distinguishable.
- [ ] Multi-segment validation detects duplicates, gaps, overlap and mixed session/source metadata.

## 6. Replay

- [ ] Replay emits only validated finalized records.
- [ ] Segment ordering is numeric and capture ordering is exact.
- [ ] Callback receives byte-identical payloads and required metadata.
- [ ] Callback failure stops replay with explicit status.
- [ ] No sleep, pacing, network send or decode occurs.
- [ ] `replay_sha256` framing is documented.
- [ ] Fixed synthetic test vector has a reviewed expected replay digest.
- [ ] The digest is identical on Windows and Linux.

## 7. CLI and report

- [ ] Synthetic generation, inspection and replay are accessible through documented CLI commands.
- [ ] Help, missing arguments, invalid values and unknown options have correct non-zero behavior.
- [ ] No overwrite is the default.
- [ ] Strict validation returns non-zero for trust-affecting findings.
- [ ] JSON is valid, deterministic and versioned.
- [ ] Reports include hashes, counts, index bounds, issues and overall status.
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
- [ ] Hygiene checker covers any new generated raw extensions/paths.

## 10. Documentation and handoff

- [ ] `cpp/moex_raw/README.md` documents format, lifecycle, rotation, replay digest, build/usage and limitations.
- [ ] MiMo report exists at `agent_workspaces/mimo/reports/<date>-rt2-raw-capture-replay.md`.
- [ ] Report includes Issue, branch, commit SHA, PR, changed files, commands and exact results.
- [ ] Report distinguishes implementation commit evidence from later evidence/docs commits.
- [ ] Issue and PR metadata contain the actual final head and latest CI.
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
checksum or truncation failure would be accepted as valid;
scope expands into FAST decoding, A/B dedup, recovery or books;
existing RT-1 or QSH tests require semantic weakening.
```

## Owner review gate

RT-2 has no graphical UI. Owner review consists of a short local demonstration using synthetic data:

```text
1. generate deterministic multi-segment synthetic capture;
2. inspect it and show `valid` with hashes/counts;
3. replay it and show the expected digest;
4. mutate/truncate a copy and show explicit rejection;
5. verify no network connection is opened.
```

Architecture/Review must inspect code, tests, format documentation and CI before the owner demonstration.