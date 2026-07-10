# Owner Review Package Standard

Дата: 2026-07-10  
Статус: обязательный стандарт для задач с пользовательским интерфейсом

## Назначение

Owner Review Package позволяет владельцу проверить готовый интерфейс или локальный demo-результат без ручной сборки, поиска команд и чтения исходного кода.

Пакет обязателен, если Pull Request добавляет или существенно меняет:

```text
web UI;
dashboard;
локальную desktop/CLI demo-сцену, которую должен увидеть владелец;
визуализацию;
интерактивный отчёт;
пользовательский workflow.
```

Для чисто внутренней библиотеки, parser или тестовой инфраструктуры пакет может быть `not applicable`, но PR и MiMo report должны объяснить, какой результат доступен владельцу.

## Расположение

```text
owner_review_packages/issue-<NUMBER>/
```

Минимальная структура:

```text
owner_review_packages/issue-<NUMBER>/
  OWNER_REVIEW.md
  START_DEMO.cmd
  STOP_DEMO.cmd
  screenshots/
    01-main-view.png
```

Дополнительные безопасные файлы допускаются только при необходимости:

```text
README.txt
expected_output.txt
sample_config.example.json
```

Не помещать в пакет базы данных, raw market data, credentials, build artifacts или executable binaries.

## OWNER_REVIEW.md

Документ должен содержать следующие разделы.

### 1. Что изменилось

Коротко, на языке владельца:

```text
какой экран, функция или сценарий добавлен;
что владелец должен увидеть;
какое прежнее поведение изменилось.
```

### 2. Предварительные условия

Только необходимые условия:

```text
поддерживаемая ОС;
версия Python/другого runtime;
нужна ли предварительная сборка;
нужны ли локальные synthetic/demo данные.
```

Реальные MOEX/broker credentials не должны быть нужны для demo review.

### 3. Как запустить

```text
1. Открыть папку owner_review_packages/issue-<NUMBER>.
2. Запустить START_DEMO.cmd.
3. Дождаться сообщения READY.
4. Открыть указанный адрес интерфейса.
```

### 4. Адрес интерфейса

Пример:

```text
http://127.0.0.1:8000
```

По умолчанию demo слушает только loopback (`127.0.0.1`), а не `0.0.0.0`.

### 5. Сценарий проверки

От трёх до семи коротких шагов:

```text
1. Открыть раздел Data Quality.
2. Убедиться, что статус LIVE DISABLED виден постоянно.
3. Выбрать synthetic sample.
4. Проверить таблицу ошибок и tooltip.
5. Сравнить результат со screenshot 01-main-view.png.
```

Сценарий должен проверять именно acceptance criteria Issue, а не просто факт открытия страницы.

### 6. Как остановить

```text
Запустить STOP_DEMO.cmd.
```

### 7. Известные ограничения

Честно перечислить:

```text
что является demo;
какие данные synthetic;
что не реализовано;
какие платформы не проверены;
какие ошибки или ограничения отложены.
```

### 8. Связи

```text
Issue;
Pull Request;
MiMo report;
commit SHA;
```

## START_DEMO.cmd

Разрешённый script должен быть коротким, читаемым и безопасным.

Обязательные свойства:

```text
@echo off;
setlocal;
определение repository root относительно расположения script;
проверка необходимых команд/файлов;
никаких hard-coded credentials;
никакого скачивания или установки неизвестных binaries;
никакого broker/MOEX connection;
никакой отправки заявок;
только loopback binding для web demo;
понятный вывод READY и адреса;
ненулевой exit code при ошибке.
```

START_DEMO.cmd не должен:

```text
изменять system-wide настройки;
запрашивать administrator privileges;
открывать firewall;
писать в Program Files/registry;
запускать live/runtime order path;
вызывать PowerShell с ExecutionPolicy Bypass;
загружать код из сети;
содержать токены, логины, IP подключения MOEX или персональные данные.
```

Если demo требует процесса в фоне, script должен сохранить его PID в локальный временный файл внутри ignored runtime/output directory или использовать безопасный project-specific stop mechanism.

## STOP_DEMO.cmd

Обязательные свойства:

```text
останавливает только процесс данного demo;
не использует taskkill по общему имени вроде python.exe;
предпочитает сохранённый PID или project-specific port/process marker;
безопасно повторяется, если demo уже остановлен;
удаляет только собственный временный PID/marker;
возвращает понятный результат.
```

Запрещено массово завершать процессы пользователя.

## Скриншоты

Скриншоты должны:

```text
соответствовать текущему commit;
не содержать персональные данные, credentials, реальные account IDs или приватные адреса;
показывать ключевой экран и состояние;
иметь последовательные имена 01-, 02-, 03-;
быть разумного размера.
```

Минимум один screenshot для UI-задачи. Для сложного сценария — по одному screenshot на ключевой шаг.

## Review checklist

MiMo отмечает в PR:

```text
[ ] OWNER_REVIEW.md заполнен
[ ] START_DEMO.cmd проверен на чистом локальном запуске
[ ] STOP_DEMO.cmd останавливает только собственный процесс
[ ] адрес использует 127.0.0.1
[ ] сценарий проверяет acceptance criteria
[ ] screenshots соответствуют текущему commit
[ ] нет secrets/raw data/binaries
[ ] известные ограничения перечислены
```

Architecture/Review Agent читает scripts как код и проверяет их до передачи владельцу.

## Безопасность

Исключение для `.cmd` узкое:

```text
owner_review_packages/**/START_DEMO.cmd
owner_review_packages/**/STOP_DEMO.cmd
```

Другие `.cmd` по-прежнему запрещены по умолчанию и требуют отдельного решения. Разрешение хранить review scripts не является разрешением хранить executable binaries или live launch scripts.
