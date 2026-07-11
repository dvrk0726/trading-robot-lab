# RT-3 Acceptance Criteria

RT-3 is accepted only when all criteria below are satisfied. A green CI run alone is not sufficient.

## 1. Scope and repository placement

```text
C++20 decoder is integrated under cpp/moex_fast/;
existing RT-1 inspector remains available and compatible;
no network, multicast, packet-framing, recovery, books or order-entry code is added;
no private/official raw data or credentials enter Git;
implementation is in a separate mimo/issue-21-* branch and PR;
MiMo does not merge or start RT-4.
```

## 2. Template compiler

```text
decoder-specific immutable compiled template tree exists;
operators, initial values, dictionaries, nesting and sequence lengths are preserved;
wire-affecting unsupported XML is rejected explicitly;
duplicate ids/keys, invalid combinations and unresolved/cyclic references fail compilation;
template SHA-256 and compiler version are retained;
existing RT-1 six-test behavior is not weakened.
```

## 3. Primitive correctness

The implementation passes literal independent vectors for:

```text
presence maps;
uInt32/uInt64 and nullable forms;
int32/int64 and nullable forms;
ASCII, Unicode and byte vectors;
decimal exponent/mantissa;
null versus empty;
truncation, overflow and non-canonical encodings.
```

No primitive reads beyond the supplied span or allocates from an unchecked length.

## 4. Template-ID state

```text
explicit id, previous-id reuse and id changes work;
missing previous id and unknown id fail explicitly;
truncated/non-canonical ids fail at the correct offset;
failed messages do not change previous-template-id state;
reset clears template-id state.
```

## 5. Operator matrix

The implementation provides an explicit documented supported matrix and active tests for all accepted profile combinations of:

```text
none;
constant;
default;
copy;
increment;
delta;
tail.
```

Presence-bit, nullable, initial-value and dictionary behavior follows FIX FAST normative semantics. Unsupported combinations fail template compilation instead of being approximated.

## 6. Dictionaries and transactional decode

```text
canonical dictionary keys prevent cross-scope/type collisions;
state is isolated per DecoderSession;
compiled templates are safely shareable;
every failed decode leaves dictionaries and template-id state unchanged;
decode_exact trailing bytes cause rollback;
failures inside later sequence entries roll back earlier provisional updates;
reset clears all session dictionaries.
```

Rollback is proven by before/after state fingerprints or equivalent deterministic evidence, followed by successful continuation decoding.

## 7. Groups, sequences and exact values

```text
mandatory/optional groups decode correctly;
null optional sequence differs from present empty sequence;
entry order and field order are preserved;
entry presence maps are handled;
sequence and total-node limits are enforced before allocation/iteration;
decimals remain exact exponent/mantissa pairs;
no decoder-internal conversion to binary floating point occurs.
```

## 8. Public API and error contract

```text
decode_one returns bytes_consumed for one message prefix;
decode_exact rejects trailing bytes;
expected malformed input returns explicit status/issues, not uncaught exceptions;
issues include stable code, byte offset, template id when known and deterministic field path;
DecodedMessage owns its values independently of input/session lifetime;
DecoderSession is explicitly one ordered logical stream and not thread-safe.
```

## 9. Hard limits and safety

All hard limits from `01_REQUIREMENTS.md` are implemented and actively tested:

```text
1 MiB message/value ceiling as applicable;
64-byte presence-map ceiling;
template/field/nesting/sequence/node ceilings;
checked cursor and size arithmetic;
no allocation before length/count validation;
no unbounded recursion or loop;
no unbounded dictionary key creation.
```

Boundary tests must exercise overflow paths without requiring huge real allocations.

## 10. Deterministic output and CLI

A built `moex-fast-decode` executable provides exact-one-message offline decode from hex or binary file.

Acceptance requires:

```text
deterministic text and strict JSON;
versioned schema;
templates and input SHA-256 provenance;
template id/name and bytes_consumed;
ordered decoded tree;
explicit null/value-source representation;
exact decimal integers;
lowercase byte-vector hex;
stable issue codes/offsets/paths;
quoted paths with spaces;
no private endpoint or full local path leak by default.
```

Repeated identical runs on Windows and Linux produce semantically identical canonical JSON.

## 11. Independent evidence

The test suite includes all of:

```text
hard-coded literal primitive/message vectors;
separate test-only reference encoder/oracle;
deterministic differential cases with fixed seed;
corruption/truncation mutations;
transaction rollback tests.
```

Comparing two production functions that share the same logic does not count as independent evidence.

## 12. RT-2 integration

A repository-safe integration test proves:

```text
synthetic `.mxraw` records carry exactly one FAST message each by fixture contract;
RT-2 replay callback passes payload bytes to one DecoderSession;
three stateful records decode in capture order;
malformed payload returns an explicit decode error;
bad payload does not mutate decoder state;
no packet framing, exchange sequence, A/B or recovery behavior is claimed.
```

RT-3 production decoder remains a span-based codec and does not depend on `.mxraw` filesystem parsing.

## 13. Regression gates

Final PR head must have green evidence for:

```text
RT-3 decoder Windows/MSVC Release: exact documented inventory, all pass;
RT-3 decoder Linux/GCC Release: exact same inventory, all pass;
RT-1 inspector Windows/Linux: 6/6;
RT-2 raw/replay Windows/Linux: 18/18;
QSH/M10X: 20/20;
Python/contracts: PASS;
repository hygiene: PASS.
```

Warnings-as-errors remain enabled. Release-active checks must not be replaced by standard `assert` that disappears under `NDEBUG`.

## 14. Documentation and handoff

The implementation PR must accurately update:

```text
cpp/moex_fast/README.md;
AI_CONTEXT.md;
PROJECT_STATE.md;
ROADMAP.md;
agent_workspaces/mimo/reports/<date>-rt3-fast-decoder.md;
PR description and Issue #21 status.
```

Documentation must state:

```text
exact supported operator/type matrix;
explicit unsupported constructs;
implementation SHA;
implementation CI;
final head SHA;
final CI;
exact test counts;
commands and results;
non-goals and RT-4 block.
```

No stale `Pending`, old SHA, old CI number or unsupported capability claim is acceptable.

## 15. Architecture review gate

Before owner review, Architecture/Review must inspect:

```text
template compiler and operator matrix;
all primitive bounds/overflow logic;
dictionary keying and transaction journal;
sequence/group recursion and limits;
error offsets/paths;
CLI JSON determinism;
independent vectors/oracle;
RT-2 integration boundary;
all changed files and CI evidence.
```

Any correctness ambiguity in nullable/operator semantics is blocking.

## 16. Owner-local acceptance

Owner acceptance uses the exact reviewed PR head and includes:

```text
clean Release configure/build;
all RT-3 CTest tests;
golden exact CLI decode;
stateful two-message template-id/copy/increment decode;
truncated or corrupt message rejection;
proof that a failed decode does not alter subsequent state;
RT-2 synthetic replay into RT-3 decoder;
git HEAD and clean status verification.
```

Expected owner demo outcomes:

```text
valid vectors decode with exact expected values;
stateful second message reuses prior template/dictionary state;
corrupt/truncated input returns non-zero and stable issue details;
subsequent known-good message proves rollback;
RT-2 replay preserves record order and decoder result order;
no network connection is made.
```

## 17. Merge and completion

```text
Owner explicitly authorizes merge;
expected PR head SHA is used during merge;
post-merge CI on main is green;
Issue #21 is then moved to DONE;
only after DONE may RT-4 architecture preparation begin.
```

Neither MiMo nor automation may merge the implementation PR. Auto-merge remains disabled.