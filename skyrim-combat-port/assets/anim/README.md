# Анимации для плагина: как получить файл и что с ним будет

Здесь лежат анимации (HKX), которые делает плагин. Пока папка пустая — нужен исходный ванильный
файл из игры.

## Почему нужен именно файл из игры

`docs/combat-block-matrix.md`, раздел 5: ванильного клипа укола нет — для одноручного оружия в игре
есть только `1hm_attackpowerfwd.hkx`, `1hm_attackpower.hkx`, `1hm_attackpowerleft/right/bwd.hkx`
(список 5586 ванильных путей: `Monitor221hz/Pandora-Behaviour-Engine-Plus`,
`vanilla_hkxpaths.txt`). Поэтому укол делается правкой существующего клипа.

В Skyrim SE анимации лежат не россыпью, а в архиве `Data\Skyrim - Animations.bsa`
(подтверждено списком базовых архивов SE в `Nexus-Mod-Manager`,
`Game Modes/Definitions/SkyrimSE/skyrimse_base.txt`).

## Шаги для пользователя (cmd, Windows)

### 1. Проверить, нет ли файла россыпью

Анимационные моды (DAR/OAR) иногда кладут HKX прямо в `Data`.

```cmd
dir /b "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\meshes\actors\character\animations\1hm_attackpower*.hkx"
```

- если файлы нашлись — нужен `1hm_attackpowerfwd.hkx` (без `_1stperson`), переходите к шагу 3;
- если `File Not Found` — шаг 2.

Заодно можно поискать любые распакованные анимации по всей папке Data:

```cmd
dir /s /b "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\*.hkx" | findstr /i "1hm_attackpower"
```

### 2. Достать файл из `Skyrim - Animations.bsa`

Нужен бесплатный распаковщик BSA. Подходит **BSA Browser** (автор AlexxEG) или **Bethesda Archive
Extractor (BAE)** — оба на Nexus Mods, регистрация бесплатная.

Порядок в BSA Browser:

1. Скачать и распаковать BSA Browser в любую папку, запустить `BSA Browser.exe`.
2. `File` → `Open`, открыть
   `A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\Skyrim - Animations.bsa`.
3. Дождаться списка файлов (архив большой, 5–20 секунд).
4. В поле фильтра ввести `1hm_attackpowerfwd`.
5. Выбрать строку `meshes\actors\character\animations\1hm_attackpowerfwd.hkx`.
6. Кнопка `Extract` → выбрать папку, например `%USERPROFILE%\Downloads\anim`.
7. Проверить, что файл появился:

```cmd
dir "%USERPROFILE%\Downloads\anim\1hm_attackpowerfwd.hkx"
```

Ожидаемый размер — десятки килобайт (не 0). Если 0 — файл не извлёкся, повторить шаг 6.

### 3. Прислать файл

Приложить `1hm_attackpowerfwd.hkx` в чат (скрепка или перетащить), как присылались логи.
Файл небольшой, вложение пройдёт.

## Что будет сделано с файлом

1. Разбор HKX в текстовый вид (TagXML) — правила укола: корпус подаётся вперёд, клинок идёт по
   прямой, без бокового замаха; кадры 0–20 при 30 fps.
2. Обратная упаковка в `1hm_attackpowerfwd.hkx` и проверка, что файл читается тем же инструментом.
3. Пакет для установки: папка под OAR/DAR с условием «тип оружия = кинжал/меч» + «силовая атака»,
   либо прямая замена файла в `Data\meshes\actors\character\animations\`.
4. Инструкция установки в `docs/` — командами cmd, как остальные.

## Если ставить BSA Browser не хочется

Альтернатива без распаковщика: любой бесплатный мод с анимациями одноручного оружия, который
ставит HKX **россыпью** (DAR/OAR-паки). Тогда файл найдётся командой из шага 1, и мы правим его.
