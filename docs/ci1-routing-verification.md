# CI-1 routing verification

This document records the acceptance check for CI-1 required-check-preserving job routing.

Expected pull-request behavior for this documentation-only change:

- `Repository hygiene` runs and succeeds.
- The six expensive Python/C++ jobs are skipped at job level.
- Existing required-check names remain present and do not block merge.
- Pushes to `main` continue to run the full seven-job matrix.

Verified base before creating this check:

```text
main: 9201dfa9119868eb414832832838831adf96bcfd
post-merge CI: #163 success
```
