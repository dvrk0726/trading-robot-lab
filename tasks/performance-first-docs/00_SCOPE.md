# Performance-first documentation scope

Issue: #36

This bootstrap file exists only to open the draft PR. It must be deleted by the implementation commit so the final PR diff contains exactly:

- AI_CONTEXT.md
- PROJECT_STATE.md
- ROADMAP.md
- README.md

Required outcome:

- C++20 is the latency-critical realtime/hot-path language.
- Python remains outside the hot path for research, reports, UI and offline tooling.
- Performance claims require measured Release benchmarks.
- CI-2 caching is postponed until CI time or cost becomes a measured development bottleneck.
- RT-4 research/specification is the next gate; RT-4 implementation remains not started and not authorized.

No code, CI, routing, ruleset, benchmark or RT-4 implementation changes.
