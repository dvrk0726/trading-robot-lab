# Owner / ChatGPT / MiMo Practical Workflow

Дата обновления: 2026-07-10  
Репозиторий: `dvrk0726/trading-robot-lab`  
Статус: актуальный практический процесс

## Цель

Владелец формулирует желаемый результат и проверяет готовый результат. Работа с ветками, сборкой, тестами, commit, push и Pull Request выполняется MiMo. Архитектурный review и управление задачами выполняет ChatGPT.

## Нормальный цикл

```text
Owner формулирует результат
-> ChatGPT готовит Issue и спецификацию
-> Issue получает READY_FOR_MIMO
-> Owner запускает MiMo одной командой
-> MiMo создаёт branch, реализует, тестирует и создаёт PR
-> MiMo переводит Issue в READY_FOR_REVIEW и останавливается
-> ChatGPT проверяет diff, CI, тесты и архитектуру
-> при замечаниях Issue получает CHANGES_REQUIRED
-> после технического принятия Issue получает OWNER_REVIEW
-> Owner проверяет готовый интерфейс/результат
-> OWNER_APPROVED
-> проверяемый merge в main
-> PROJECT_STATE/ROADMAP обновляются
-> DONE
```

## Что делает владелец

В обычной задаче владельцу нужно только:

1. Описать, какой результат нужен.
2. После сообщения ChatGPT запустить MiMo.
3. После технического review открыть готовый интерфейс или результат по Owner Review Package.
4. Написать замечания или подтвердить принятие.

Владелец не должен вручную выбирать файлы для commit, придумывать branch или выполнять merge вслепую.

## Единственная обычная команда MiMo

В Developer PowerShell из папки репозитория:

```powershell
mimo --model xiaomi/mimo-v2.5-pro --prompt "Возьми следующую задачу READY_FOR_MIMO, выполни её, создай Pull Request и остановись"
```

Команда запускается только после того, как ChatGPT подтвердил статус `READY_FOR_MIMO`.

## Что MiMo делает автоматически

MiMo обязан:

```text
проверить отсутствие незавершённой предыдущей задачи;
найти следующий READY_FOR_MIMO Issue;
прочитать контекст и спецификацию;
обновить локальный main через fast-forward;
создать ветку mimo/issue-<N>-<slug>;
перевести Issue в IN_PROGRESS;
выполнить только эту задачу;
запустить сборку, тесты и hygiene check;
создать commit и push ветки;
создать Pull Request в main;
добавить отчёт;
перевести Issue в READY_FOR_REVIEW;
остановиться.
```

MiMo запрещено выполнять merge или начинать следующую задачу.

## Что проверяет ChatGPT

После появления Pull Request ChatGPT проверяет:

```text
связь PR с правильным Issue;
changed files и полный diff;
соответствие спецификации;
результаты GitHub Actions;
локальные команды и отчёт MiMo;
отсутствие secrets/raw data/binaries;
отсутствие незапрошенного network/FIX/TWIME/order code;
сохранение QSH-семантики и strategy_ready gating;
архитектурные границы;
Owner Review Package для интерфейсной задачи.
```

Без этого review задача не переходит к владельцу.

## Как владелец проверяет интерфейс

Для UI-задачи MiMo готовит пакет по:

```text
docs/engineering/OWNER_REVIEW_PACKAGE.md
```

В пакете должны быть:

```text
что изменилось;
как запустить;
START_DEMO.cmd;
STOP_DEMO.cmd;
адрес интерфейса;
короткий сценарий проверки;
скриншоты;
известные ограничения.
```

Владелец запускает `START_DEMO.cmd`, открывает указанный адрес, проходит короткий сценарий и затем запускает `STOP_DEMO.cmd`.

## Если владелец нашёл проблему

Владелец описывает проблему обычными словами или прикладывает скриншот. ChatGPT превращает замечание в точный review comment или компактную задачу.

Для исправлений текущего результата используется тот же Pull Request:

```text
READY_FOR_REVIEW
-> CHANGES_REQUIRED
-> MiMo исправляет в той же ветке
-> повторяет tests/CI
-> READY_FOR_REVIEW
```

Новая задача создаётся только для отдельного scope.

## Что больше не используется

Старый процесс считается отменённым:

```text
MiMo работает прямо в main;
MiMo самостоятельно push в main;
Owner вручную запускает mimo_save.ps1 на main;
Owner передаёт только скриншот git log вместо Pull Request;
следующая задача запускается до review предыдущей.
```

`tools/mimo_save.ps1` сохраняется только как безопасный helper для текущей feature-ветки и обязан отказать на `main`/`master`.

## Когда нужно остановиться

Не продолжать работу и сообщить ChatGPT, если:

```text
нет задачи READY_FOR_MIMO;
предыдущий PR ожидает review;
CI упал;
тесты упали;
MiMo находится в main с изменениями;
появились data/raw, QSH, pcap, databases, binaries или secrets;
MiMo расширил scope;
задача требует реальных доступов или production/live решения.
```

## Merge

Автоматический merge запрещён. MiMo merge не выполняет.

Перед merge должны быть выполнены:

```text
CI passed;
Architecture/Review Agent accepted;
все review comments закрыты;
Owner approval получен, если задача пользовательская или owner-gated;
нет секретов и запрещённых файлов.
```

После merge ChatGPT обновляет состояние проекта и только затем разрешает следующую задачу.
