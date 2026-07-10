# GitHub Write Limits and AI Workflow

Дата обновления: 2026-07-10  
Статус: обязательное правило

## Назначение

Документ определяет, какие изменения допустимо делать через GitHub connector, а какие выполняются MiMo локально через обычный Git.

## Два типа ограничений

### GitHub

Важные практические ограничения:

```text
Git блокирует отдельные файлы больше 100 MiB.
Большие repositories ухудшают clone, CI and review.
Contents API заменяет файл целиком.
Raw data, binaries and generated build artifacts не подходят для normal Git.
```

### AI connector

Connector может отклонить запись раньше GitHub из-за:

```text
large JSON payload;
long replacement content;
context/tool safety checks;
timeout;
multiple sequential writes to one path.
```

Размер connector-запроса не считается стабильным GitHub limit.

## Внутренние пределы проекта

Для connector writes:

```text
новый Markdown-файл: целевой размер до 12 KB;
один write request: целевой размер до 20 KB;
Issue body: желательно до 8 KB;
одна запись = один логический результат;
не выполнять параллельные update/delete одного path.
```

Если длинный replacement отклонён, документ сокращается или разделяется по смыслу. Не повторять одинаковый большой запрос.

## Что делает ChatGPT через connector

Подходит для:

```text
Issue creation/status/handoff;
небольших task specs;
коротких architecture/process docs;
точечных правок;
review comments;
создания отдельной branch;
небольшой логической серии commits;
открытия Pull Request.
```

Даже connector не должен писать implementation/process changes прямо в `main`. Для набора связанных изменений сначала создаётся branch, затем Pull Request.

## Что делает MiMo локально

MiMo используется для:

```text
implementation code;
large or multi-file changes;
C++/Python build work;
fixtures and tests;
локального запуска full regression suite;
Owner Review Package;
branch, commit, push and Pull Request.
```

Полный процесс описан в `docs/mimo_developer_workflow.md`.

## Branch-only rule

```text
main is read-only for implementation agents.
Every code/process change uses a dedicated branch.
Every branch is reviewed through a Pull Request.
MiMo never merges and never enables auto-merge.
```

Рекомендуемая ветка:

```text
mimo/issue-<NUMBER>-<short-slug>
```

Architecture/Review Agent использует `chore/`, `docs/` или `fix/` для собственных небольших изменений.

## Большие задачи

Не помещать полную спецификацию в Issue.

```text
Issue:
- objective;
- status/dependencies;
- constraints;
- acceptance;
- links.

Task package:
- tasks/<task-id>/00_OVERVIEW.md
- tasks/<task-id>/01_REQUIREMENTS.md
- tasks/<task-id>/02_TEST_PLAN.md
- tasks/<task-id>/03_ACCEPTANCE.md
```

## Перед commit

```powershell
git status --short
git diff --check
git diff --stat
git diff
python tools/check_repository_hygiene.py
```

Затем запускаются task-specific проверки и baseline CI commands.

## Baseline checks

```powershell
python -m pytest -q
python shared/schemas/validate_examples.py

cmake -S cpp/qsh_ingest -B build/qsh_ingest
cmake --build build/qsh_ingest --config Release
ctest --test-dir build/qsh_ingest -C Release --output-on-failure
```

Для C++ implementation существующие 20 QSH/M10X tests обязательны, если Issue явно не доказывает неприменимость.

## Хранение больших и private данных

Не помещать в normal Git:

```text
QSH;
pcap/pcapng;
raw FAST packets;
Parquet archives;
databases;
EXE/DLL;
build directories;
official private XML;
credentials/private connection parameters;
large generated reports.
```

Использовать локальное хранилище, approved object storage или другой отдельно согласованный механизм.

## update_file safety

Перед `update_file` обязательно получить текущий SHA. После чужого commit SHA получить повторно.

Не превращать:

```text
AI_CONTEXT.md в длинную историю;
PROJECT_STATE.md в журнал каждого commit;
ROADMAP.md в task dump;
один shared document в параллельный chat нескольких агентов.
```

## Pull Request handoff

После работы агент сообщает:

```text
Issue;
branch;
commit SHA;
Pull Request;
changed files;
implemented and omitted scope;
commands and results;
CI status;
known limitations;
Owner Review Package path when applicable.
```

После создания PR MiMo останавливается. Следующая задача не начинается до review предыдущей.

## Если connector write отклонён

```text
1. Не повторять тот же payload.
2. Проверить, была ли запись фактически создана.
3. Уменьшить replacement.
4. Разделить по логическим файлам.
5. Передать большой implementation MiMo/local Git.
6. Повторно получить SHA перед следующей записью.
```

## Главный принцип

```text
GitHub хранит проверенное состояние.
Issue хранит компактную задачу.
Branch хранит незавершённое изменение.
Pull Request хранит review и evidence.
Main получает только проверенный результат.
Raw data, binaries and secrets не попадают в repository.
```
