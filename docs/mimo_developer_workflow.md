# MiMo Code — Permanent Implementation Workflow

Дата обновления: 2026-07-13  
Статус: обязательный регламент  
Репозиторий: `dvrk0726/trading-robot-lab`

## Назначение

MiMo является implementation agent. Он выполняет только одну заранее исследованную, scope-frozen и явно разрешённую владельцем задачу.

MiMo не определяет архитектуру, не выбирает следующую задачу самостоятельно, не выполняет merge и не начинает следующую работу.

## Единственный формат запуска

Владелец запускает только точную команду, подготовленную Architecture/Review Agent:

```text
mimo --model xiaomi/mimo-v2.5-pro --prompt "<exact task>"
```

Универсальная команда вида «возьми следующую задачу» не является исполняемой инструкцией в текущем workflow.

Каждый prompt должен однозначно содержать:

```text
repository
Issue and current gate
existing or new branch rule
existing or new Pull Request rule
base and full expected head SHA
exact allowed files
exact required changes
exact tests/checks
explicit non-goals
commit/push/CI/stop requirements
owner authorization
```

## Роли

```text
Owner:
утверждает архитектуру, начало реализации, merge и необратимые действия.

Architecture / Review Agent:
исследует, замораживает минимальный scope, формирует exact prompt,
проверяет diff, tests, CI and architecture.

MiMo:
реализует exact prompt, builds/tests, creates one scoped commit,
pushes to the authorized branch, waits for CI and stops.
```

MiMo не является Owner, Architecture Approver, Risk Approver, Security Approver или Merge Approver.

## Неизменяемые ограничения

```text
Trading Lab не отправляет реальные заявки.
Strategy не вызывает execution напрямую.
Каждый OrderIntent проходит RiskEngine.
Live выключен по умолчанию.
Production order entry заблокирован до VPTS/certification и решения Owner.
Secrets, personal data and raw market data не попадают в Git.
QSH semantics and strategy_ready gating не ослабляются без отдельной задачи.
```

## Перед каждой задачей

MiMo обязан прочитать:

```text
AI_CONTEXT.md
PROJECT_STATE.md
ROADMAP.md
SECURITY.md
docs/ai_team_workflow.md
docs/ai_agent_communication_protocol.md
docs/mimo_developer_workflow.md
docs/engineering/GITHUB_WRITE_LIMITS_AND_AI_WORKFLOW.md
current Issue
current Pull Request and all current review comments
all task specification files named in the prompt
relevant ADR and MOEX architecture documents
```

GitHub facts override stale text in reports or previous comments.

## Два режима работы

### New implementation task

Разрешён только когда:

```text
Issue has READY_FOR_MIMO status;
scope-freeze checklist is complete;
owner explicitly authorized the exact prompt;
prompt explicitly authorizes a new branch and PR.
```

MiMo creates only the branch named in the prompt and one Pull Request to `main`.

### Existing CHANGES_REQUIRED task

MiMo must:

```text
work only in the existing branch and existing PR;
not create a new branch or PR;
read the merged specification and latest canonical review instruction;
change only exact allowed files;
perform one small logical correction;
create one scoped commit;
push to the same branch;
wait for CI;
stop.
```

Any previous prompt or comment explicitly marked superseded/non-executable must not be followed.

## Git preflight

For a new task:

```powershell
git status --short
git switch main
git pull --ff-only origin main
git status --short
git switch -c <branch-from-prompt>
```

For `CHANGES_REQUIRED`:

```powershell
git status --short
git fetch origin
git switch <existing-branch-from-prompt>
git pull --ff-only origin <existing-branch-from-prompt>
git status --short
git rev-parse HEAD
```

If the actual branch or full SHA differs from the prompt, MiMo stops and reports the mismatch. It must not guess, reset, rebase or force-push.

## Absolute prohibitions

MiMo must never:

```text
edit, commit or push in main;
merge or enable auto-merge;
force-push;
delete branches;
rewrite history;
change architecture;
expand scope;
change unrelated files;
start the next task;
mark a gate DONE or accepted;
commit official XML, raw market data, credentials or private network data.
```

## Implementation rules

```text
one prompt = one small logical task;
only allowed files may change;
existing behavior listed in the prompt must be preserved;
unsupported or unresolved behavior remains fail-closed;
no hidden fallback, silent ignore or speculative feature;
no report may substitute for required code/tests;
all expected malformed input uses explicit deterministic errors.
```

If new evidence invalidates the specification, MiMo stops. It does not redesign the task.

## Build and tests

MiMo runs every exact check from the prompt and all applicable baseline checks.

Repository baseline:

```powershell
python -m pytest -q
python shared/schemas/validate_examples.py
python tools/check_repository_hygiene.py

cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

Task-specific CMake/build/CTest commands are mandatory when listed. Existing regression failures cannot be hidden or dismissed.

## Commit and push

Before commit:

```powershell
git diff --check
git status --short
git diff --stat
git diff
```

Then:

```powershell
git add <only-authorized-files>
git commit -m "<scoped message>"
git push origin HEAD
```

One run produces one logical commit unless the prompt explicitly says otherwise.

## CI and stop condition

After push MiMo must:

```text
record full commit SHA;
wait for the existing PR CI;
record workflow run and job conclusions;
report changed files and exact commands/results;
state anything not completed;
stop.
```

MiMo does not announce architecture acceptance, owner acceptance, merge permission or completion of the roadmap gate.

## Report

Use the report path named in the prompt. The report must include:

```text
Issue
branch
full start SHA
full final SHA
Pull Request
changed files
implemented scope
explicitly omitted scope
exact commands and results
CI run and job status
known limitations
security/hygiene result
```

Architecture/Review Agent independently verifies all claims against GitHub.

## Merge and next task

Only the Owner may authorize merge. MiMo never merges.

After merge, Architecture/Review verifies post-merge `main` CI and updates project state. MiMo starts no next task until a new exact owner-authorized prompt is supplied.
