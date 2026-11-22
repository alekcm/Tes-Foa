# Tainted Grail → Skyrim AE Combat Port

## Что это за репозиторий сейчас

Это **передача дизайна и исследований**, а не готовый мод: здесь пока нет `.esp`, Papyrus-скриптов, анимаций, SKSE DLL, сборки или результата проверки в Skyrim.

Новый чат должен сначала прочитать [`AGENT_TASK.md`](AGENT_TASK.md), затем документы ниже. Он должен реализовывать согласованный дизайн, а не сочинять его заново. Если движок не позволяет реализовать элемент как написано, нужно указать конкретный технический блокер и предложить минимальную замену на утверждение, не менять дизайн молча.

## Первый раз? Начни здесь

Пошаговая инструкция для первого запуска, от скачивания ZIP до первого лога в игре:
[`docs/first_time.md`](docs/first_time.md).

## Быстрый старт

```powershell
python tools\check_env.py --tier papyrus   # что установлено
python tools\build.py --name TGCombatPort --version 0.1.0 --plugin build\TGCombatPort.esp
# без готового .esp: добавь --skip-package и соберутся только скрипты
```

Готовый архив появляется в `dist/` и ставится в MO2/Vortex как обычный мод. Подробности — `BUILD.md` и
`RELEASING.md`.

## Что уже добавлено в Git-папку

- [`docs/skyrim_enemy_combat_design_spec.md`](docs/skyrim_enemy_combat_design_spec.md) — конкретный дизайн боёвки врагов для 42 архетипов: мувсеты, телеграфы, реакция игрока и окна уязвимости.
- [`docs/tainted-grail-skyrim-enemy-port-plan.md`](docs/tainted-grail-skyrim-enemy-port-plan.md) — контекст врагов и что является источником/переносом/новым предложением.
- [`docs/tainted-grail-to-skyrim-ae-combat-port-design.md`](docs/tainted-grail-to-skyrim-ae-combat-port-design.md) — общий дизайн переноса боёвки.
- [`docs/tainted-grail-skyrim-combat-research.md`](docs/tainted-grail-skyrim-combat-research.md) — исследовательское досье и нерешённые расхождения.
- [`docs/vertical_slice_feasibility.md`](docs/vertical_slice_feasibility.md) — предкодовая оценка spider/wolf/bandit с техническими рисками и тестами.
- [`docs/vertical_slice_records.md`](docs/vertical_slice_records.md) — черновик: какие именно записи плагина и скрипты нужны для среза, с критериями проверки.
- [`docs/papyrus_api.md`](docs/papyrus_api.md) — что делают уже написанные скрипты, чего они не делают и что проверять первым.
- [`docs/papyrus_verification.md`](docs/papyrus_verification.md) — сверка каждой Papyrus-функции с документацией и запасные варианты.
- `src/papyrus/` — исходники скриптов: пробник animation events, наблюдатель блока, кулдаун паутины, состояние стаи.
- `tools/` — проверка окружения, линт Papyrus, сборка, упаковка, проверка заголовка плагина, сбор выжимки из Papyrus-лога, включение логов в Skyrim.ini, разбор записей собранного плагина и проверка ссылок; `tests/` — тесты этих инструментов.
- [`data/combat_tables_phases_1-2.xlsx`](data/combat_tables_phases_1-2.xlsx) и [`data/phases_1-2_coverage.json`](data/phases_1-2_coverage.json) — восстановленные исследовательские таблицы и coverage.
- [`docs/how_a_skyrim_mod_works.md`](docs/how_a_skyrim_mod_works.md) — простыми словами: из чего состоит мод, что скачивает игрок с Nexus, чем отличаются варианты реализации и что это значит для требований к игроку.
- [`BUILD.md`](BUILD.md) — что нужно для сборки на каждом уровне (records → Papyrus → SKSE), что не попадает в Git и как проверить окружение.
- [`RELEASING.md`](RELEASING.md) — путь от исходников до обычного zip: упаковка, проверка установки, GitHub Releases/Nexus, разбор проблем.
- [`TESTING.md`](TESTING.md) — план тестов среза по шагам и критериям pass/fail; пока не выполнялся.
- [`docs/ck_setup_guide.md`](docs/ck_setup_guide.md) — первый заход в Creation Kit: какие записи создать и где проверить.
- [`docs/first_time.md`](docs/first_time.md) — первый проход целиком: установка, компиляция, Creation Kit, включение плагина, первый лог.
- [`docs/workflow.md`](docs/workflow.md) — как идёт работа: что делаю я, что делаешь ты, что присылать и что коммитить.
- [`docs/faq.md`](docs/faq.md) — короткие ответы: что уже готово, что нужно загружать агенту, почему нет `.esp`.
- [`docs/decisions.md`](docs/decisions.md) — журнал принятых решений и открытых вопросов.
- [`SETUP.md`](SETUP.md) — заполняемая карточка твоей конкретной версии игры и среды разработки.
- [`DEPENDENCIES.md`](DEPENDENCIES.md) — здесь фиксируются только реально выбранные инструменты и версии.
- [`STATUS.md`](STATUS.md) — что спроектировано, реализовано, собрано и реально проверено.

## Цель и что ещё не зафиксировано

Пользователь сообщил, что играет в **Skyrim Special Edition (SE) со всеми дополнениями**. Для проекта это трактуется как полный набор официальных DLC (Dawnguard, Hearthfire, Dragonborn); точный runtime/exe, магазин/платформа, список плагинов, mod manager/load order и наличие инструментов пока не установлены. Старые design-документы и workbook называют целью Skyrim AE и включают отдельные Creation Club-данные; их наличие у пользователя не подтверждено, поэтому CC/AE-записи остаются optional.

`SETUP.md` — только карточка target-совместимости и тестового профиля: её заполнение **не** даёт агенту доступ к ПК пользователя. Игру/CK можно тестировать из sandbox только если они реально установлены в sandbox; иначе локальные шаги выполняет пользователь и возвращает результаты. Точный runtime нужен для решений, привязанных к версии, но не блокирует независимый анализ и подготовку исходников.

Выбрана база реализации: записи плагина и Papyrus; SKSE разрешён по конкретной необходимости. Поэтому `src/papyrus/` существует и содержит первый скрипт — пробник animation events, а не пустую заготовку. `src/native/`, Pandora, Nemesis и Spriggit по-прежнему не подключены и версии зависимостей не сочинены. Весь остальной design scope остаётся обязательным, если задача — портировать всю боёвку, а не только врагов.

## Что добавлять дальше, когда появится исходный код

Добавлять в репозиторий **свои исходники и конфигурацию сборки**, а не копию игры:

- `src/plugin/` — исходная/текстовая форма плагина либо выбранный способ его сборки.
- `src/papyrus/` — исходные `.psc` и скрипты сборки, если нужны Papyrus.
- `src/native/` — C++ и build-конфиги **только если** прототип доказал необходимость SKSE DLL.
- `assets/` — только авторские файлы или материалы с документированным разрешением на распространение.
- `tests/` и `TESTING.md` — конкретные проверки, шаги, результаты и известные сбои.
- Скрипты сборки/пакетирования и CI — только после того, как локальная сборка воспроизводится.

Не коммитить `SkyrimSE.exe`, игровые BSA/архивы, извлечённые игровые ресурсы, полную папку Data, бинарники CK/SKSE и чужие моды. Это не исходники проекта; зависимости записывать ссылками и версиями. Не включать в Git личные абсолютные пути пользователя.

## Статусы не смешивать

- **Designed** — есть согласованное описание механики.
- **Implemented** — изменение внесено в исходный плагин/скрипт/ассет.
- **Built** — артефакт успешно собрался.
- **Tested in game** — проверено в настоящей Skyrim нужного runtime; приложены шаги/результат.

`Built` не означает `Tested in game`.

## Источники по инструментам

- Bethesda Creation Kit в Steam: https://store.steampowered.com/app/1946180/Skyrim_Special_Edition_Creation_Kit/
- Официальный SKSE и runtime-specific загрузки: https://skse.silverlock.org/
- CK Wiki — community-maintained копия UESP, полезная, но не официальный актуальный контракт Bethesda: https://ck.uesp.net/wiki/Main_Page
- Spriggit — необязательный community tool для plugin ↔ YAML/JSON: https://github.com/Mutagen-Modding/Spriggit
- Pandora Behaviour Engine+ — community tool для работы с behavior/animation; пока не выбран как зависимость: https://github.com/Monitor221hz/Pandora-Behaviour-Engine-Plus

## Загрузить эту папку на GitHub

Точные команды и пошаговый порядок находятся в [`PUBLISH.md`](PUBLISH.md). Сначала создай пустой репозиторий на github.com, затем выполни команды из файла, подставив свой URL.
