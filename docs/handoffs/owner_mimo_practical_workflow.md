# Owner / ChatGPT / MiMo Practical Workflow

Date: 2026-07-09
Repo: `dvrk0726/trading-robot-lab`
Audience: owner and any AI agent helping the owner

## Roles

### Owner

The owner does not need to inspect code manually on every step. The owner mainly:

```text
1. opens Developer PowerShell for VS 2022;
2. goes to the repo folder;
3. pulls latest task file from GitHub;
4. starts MiMo with the exact task filename;
5. after MiMo finishes, checks git status/log;
6. sends screenshots/results to ChatGPT for review;
7. saves changes with mimo_save.ps1 only if MiMo left local changes.
```

### ChatGPT

ChatGPT:

```text
1. creates focused task files in GitHub, e.g. M10S_...md;
2. reviews screenshots from owner;
3. verifies actual GitHub commits, changed files, and reports;
4. tells owner whether the result is acceptable;
5. creates the next task only after reviewing the previous result;
6. keeps research direction controlled and prevents unsafe shortcuts.
```

### MiMo

MiMo:

```text
1. reads the task file;
2. edits code/docs/scripts locally;
3. builds and runs tests;
4. runs real-sample validation if local QSH exists;
5. updates the long report;
6. may commit/push by itself or leave local changes.
```

## Owner setup

Open:

```text
Developer PowerShell for VS 2022
```

Go to repo:

```powershell
cd C:\ProjectsHFT\trading-robot-lab
```

Check current state:

```powershell
git status --short
git log --oneline -1
```

## How to start a new task

When ChatGPT creates a task file, for example:

```text
M10S_COUNTER_FLAG_SEMANTICS_AND_BOOK_IMPACT.md
```

Owner runs:

```powershell
git pull
mimo --model xiaomi/mimo-v2.5-pro --prompt "Выполни M10S_COUNTER_FLAG_SEMANTICS_AND_BOOK_IMPACT.md"
```

Use the exact filename that ChatGPT provides.

## What to do after MiMo finishes

Run:

```powershell
git status --short
git log --oneline -1
```

Then send ChatGPT a screenshot.

## How to interpret git status

### Case A: working tree clean

If output says:

```text
Nothing to commit. Working tree clean.
```

or `git status --short` prints nothing, then there are no local unsaved changes.

If `git log --oneline -1` shows a new commit with:

```text
HEAD -> main, origin/main, origin/HEAD
```

then MiMo already pushed to GitHub. Do not run `mimo_save.ps1` again.

### Case B: there are modified files

If `git status --short` shows lines like:

```text
M  agent_workspaces/mimo/reports/...
M  cpp/qsh_ingest/src/main.cpp
?? cpp/qsh_ingest/tests/test_new_file.cpp
```

then MiMo left local changes. Save them with the command ChatGPT provides, usually:

```powershell
.\tools\mimo_save.ps1 "Short commit message"
```

Example:

```powershell
.\tools\mimo_save.ps1 "Investigate Counter flag semantics and book impact"
```

After that, run again:

```powershell
git status --short
git log --oneline -1
```

Send ChatGPT a screenshot.

## What `mimo_save.ps1` does

The helper script:

```text
1. stages current changes;
2. commits them;
3. rebases on latest origin/main before push;
4. pushes to GitHub.
```

Warnings like this are usually fine:

```text
LF will be replaced by CRLF the next time Git touches it
```

Do not stop because of those warnings unless the script fails.

## What screenshots to send ChatGPT

Always send the terminal screenshot showing:

```text
git status --short
git log --oneline -1
mimo_save output, if used
```

Good screenshot contents:

```text
commit hash
commit message
whether push completed
whether working tree is clean
```

ChatGPT will then verify GitHub directly.

## When to ask ChatGPT before continuing

Ask ChatGPT before running the next task when:

```text
MiMo says tests failed
MiMo says build failed
git status shows unexpected files under data/raw or data/reports
MiMo adds broker/live-trading code
MiMo says L2 strategy-ready YES while crossed_book_snapshots > 0
MiMo changes default behavior without evidence
MiMo suggests skipping diagnostics and going to strategy
```

## Files that must not be committed

Never commit:

```text
data/raw/
data/reports/
.env
*.qsh
*.csv generated reports
*.json generated reports
*.exe
*.dll
keys / tokens / credentials
```

If such files appear in `git status --short`, stop and ask ChatGPT.

## Current practical loop

The normal loop is:

```text
1. ChatGPT creates M10X task in GitHub.
2. Owner runs git pull.
3. Owner starts MiMo with exact task filename.
4. MiMo works.
5. Owner runs git status --short and git log --oneline -1.
6. If dirty: owner runs tools/mimo_save.ps1 with commit message.
7. Owner sends screenshot to ChatGPT.
8. ChatGPT verifies GitHub commit and report.
9. ChatGPT gives next task or says stop.
```

## Current project status at this handoff

Latest research milestone: M10S.

Main finding:

```text
Counter=0x100 events are the primary cause of early crossed-book reconstruction.
--counter-mode ignore-book reduces crossed snapshots from 7890 to 907.
```

Still not done:

```text
L2 strategy-ready: NO
remaining crossed snapshots after counter-ignore-book: 907
```

Recommended next task:

```text
M10T_REMAINING_CROSSED_AFTER_COUNTER_IGNORE
```

Owner should not proceed to strategy/UI/live trading until ChatGPT or another reviewing AI confirms that the L2 data quality gate is clean or that invalid segments are explicitly excluded.
