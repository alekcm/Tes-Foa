# native/ — SKSE-плагин (DLL)

Здесь лежит C++ часть мода. Она нужна только для того, чего не умеет Papyrus:
точное чтение состояния анимации и (в перспективе) управление действиями NPC.

## Что уже есть (0.2.0)

- `TGCombatPort.dll` — собирается из `src/main.cpp`. **Хуков (перехвата функций игры) больше нет**:
  версия 0.1.1 патчила `Actor::GetAttackChance` и вызывала вылет, этот способ убран целиком.
- `TGCP_Native.IsBlocking(Actor)` — `true`, если актёр блокирует (`RE::Actor::IsBlocking()`, состояние движка).
- `TGCP_Native.Tick()` — один шаг механики; зовётся из `TGCP_PlayerBlockWatcher` каждые 0.25 с, пока игрок блокирует.
- `TGCP_Native.SetEnabled(bool)` — выключить/включить механику без перезапуска (для тестов A/B).
- `TGCP_Native.GetStats()` — строка `ticks=… candidates=… windows=… open=… enabled=…`.

### Механика бандита со щитом

«Bash только против блока»: пока игрок держит блок, плагин ищет бандитов со щитом, которые враждебны
игроку и стоят в ближнем бою (≤ 240), и на **1,5 с** поднимает в их `CombatStyle` множитель
`meleeData.bashMult` (`0.5 → 10.0`). Это те самые числа, по которым AI выбирает атаку, поэтому бандит
бьёт щитом сам; анимация, урон, блок и попадание остаются ванильными. Через 1,5 с исходное значение
возвращается, повторно для того же бандита — не раньше **7 с**.

Честно о побочном эффекте: `CombatStyle` — общая запись, поэтому подъём на 1,5 с действует и на других
NPC с тем же стилем. Окно короткое и открывается только пока игрок блокирует.

Строки в логе: `bash window opened: actor=… bashMult 0.50 -> 10.00`, `bash window closed: …`.

## Сборка

Собирается в GitHub Actions (`.github/workflows/native.yml`) на Windows-раннере:
туда же клонируется CommonLibSSE-NG. Локально нужен xmake 3.x и MSVC:

```cmd
cd skyrim-combat-port\native
git clone --depth 1 -b ng https://github.com/alandtse/CommonLibVR.git lib\commonlibsse-ng
xmake build
```

Готовый файл: `build\windows\x64\release\TGCombatPort.dll`.

## Готовый файл (без сборки)

Собранный плагин публикуется как GitHub Release по тегу `v*`:

- https://github.com/alekcm/Tes-Foa/releases/download/v0.2.0/TGCombatPort-v0.2.0.zip —
  архив с `SKSE/Plugins/TGCombatPort.dll`, распаковывается в папку `Data` игры
  или ставится как обычный мод в Vortex;
- https://github.com/alekcm/Tes-Foa/releases/download/v0.2.0/TGCombatPort.dll — сам файл.

Почему не через `git pull`: в git лежат исходники; `.dll` — собранный Windows-бинарник, он
собирается на сервере GitHub и выкладывается в Releases. Папка проекта на рабочем столе и
папка игры — вообще разные места, поэтому файл всё равно надо положить в `Data`.

## Установка (для проверки)

1. Скачать `TGCombatPort-v0.2.0.zip` по ссылке выше (внутри DLL **и** обновлённые `.pex`).
2. Распаковать в папку `Data` игры (cmd):

```cmd
tar -xf "%USERPROFILE%\Downloads\TGCombatPort-v0.2.0.zip" -C "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data"
```

3. Поставить **Address Library v13** (Nexus, мод 32444, файл для 1.6+) — без него DLL не загрузится.
4. Запускать игру через `skse64_loader.exe`.
5. Смотреть `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`
   (если файла нет — `skse64.log` в той же папке).

Требования: SKSE64 2.3.1 (для runtime 1.7.104) и Address Library v13.
