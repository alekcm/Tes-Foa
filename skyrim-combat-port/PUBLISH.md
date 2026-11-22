# Как загрузить папку проекта в GitHub

## Вариант A — через GitHub Desktop (без команд)

1. На github.com нажми **New repository**. Название, например `tainted-grail-skyrim-combat`. Выбери Private, если не хочешь публиковать дизайн/файлы. **Не** добавляй на сайте README, `.gitignore` или лицензию: файлы уже есть локально.
2. Установи GitHub Desktop и войди в аккаунт.
3. **File → Add local repository…** → выбери папку `skyrim-combat-port`. Если Git ещё не инициализирован, нажми предложенную кнопку **create a repository**.
4. В поле Summary напиши `Add combat design and handoff`; нажми **Commit to main**.
5. Нажми **Publish repository**, выбери созданный репозиторий/имя и нажми Publish.
6. Открой GitHub и проверь, что видны `README.md`, `AGENT_TASK.md`, `SETUP.md`, `docs/`, `data/`.

## Вариант B — терминал Git

Сначала создай на GitHub пустой репозиторий: **New repository**, выбери имя, Private/Public, **не** добавляй автоматически README/лицензию/.gitignore. Скопируй его HTTPS URL из GitHub. Потом выполни:

```bash
# Сначала перейди в распакованную папку проекта.
# Windows (cmd), если распаковал в Downloads:
cd /d "%USERPROFILE%\Downloads\skyrim-combat-port"
# На macOS/Linux укажи свой путь, например:
# cd "$HOME/Downloads/skyrim-combat-port"

git init -b main
git add README.md AGENT_TASK.md SETUP.md DEPENDENCIES.md STATUS.md PUBLISH.md .gitignore .gitattributes docs data
git status --short
# Проверь, что в выводе есть оба файла data/:
# data/combat_tables_phases_1-2.xlsx и data/phases_1-2_coverage.json
git commit -m "Add combat design and implementation handoff"
git remote add origin https://github.com/ТВОЙ-АККАУНТ/ИМЯ-РЕПО.git
git push -u origin main
```

Замени последний URL на скопированный URL своего репозитория. Если Git просит указать автора коммита, задай своё имя/email локально:

```bash
git config user.name "Твоё имя"
git config user.email "ТВОЙ_EMAIL"
```

Команду `git remote add origin ...` выполнять один раз. Для последующих отправок изменений:

```bash
git add -A
git status --short
git commit -m "Коротко: что изменил"
git push
```

Перед `git push` обязательно просмотреть `git status --short`, чтобы не отправить случайные локальные файлы. Не добавляй папку целой игры/Mod Organizer/Downloads.

## Передать репозиторий новому чату Arena Agent Mode

Официальный workflow Arena: подключить GitHub в Agent Mode, выбрать репозиторий и ветку; Arena делает отдельную sandbox-копию репозитория, агент меняет файлы и может отправить commit/PR. Для этого:

1. Создай репозиторий на GitHub и отправь туда начальный commit (см. команды выше).
2. Открой **Agent Mode** на Arena.
3. В строке рядом с полем ввода выбери **GitHub** и пройди OAuth-подключение.
4. В **Manage repositories** разреши доступ к этому конкретному репозиторию.
5. Выбери репозиторий и ветку для coding-сессии; включи GitHub connector для этого задания.
6. Дай задачу, например: «Прочитай `AGENT_TASK.md`, затем README и связанные документы. Работай в этой ветке. Сначала проверь `SETUP.md` и сообщи, какие тесты реально доступны».
7. Смотри правки в **Diff**. Если агент создал PR, проверь его перед merge. В справке Arena сказано, что сессия поддерживает один PR; после merge/закрытия PR эта сессия больше не может пушить новые изменения.

Ссылки на официальную инструкцию Arena: https://help.arena.ai/articles/1655691990-how-to-use-coding-in-agent-mode и https://arena.ai/blog/coding-in-agent-mode.

## Чего это подключение НЕ даёт

Подключение репозитория даёт агенту файлы Git-проекта в sandbox, **не доступ к твоему рабочему ПК**. Arena описывает запуск кода в отдельной облачной среде, а не управление твоим Skyrim/Creation Kit локально. Документация не обещает, что Creation Kit или установленная Skyrim уже есть в sandbox. Поэтому считать CK/game тест доступным можно только если агент проверил инструменты в своей среде и реально выполнил запуск; иначе собранные/проверенные пользователем результаты нужно передать обратно.

Если в новом чате GitHub connector не отображается, не делай вывод, что он отсутствует вообще: проверь Agent Mode и настройки/доступность коннекторов в интерфейсе, либо передай Markdown/JSON-файлы отдельными поддерживаемыми вложениями. Arena публикует поддерживаемые типы вложений в Help Center; `.xlsx` не указан в списке, так что для передачи workbook надёжнее подключить GitHub или экспортировать нужные листы в CSV.
