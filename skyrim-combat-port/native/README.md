# native/ — SKSE-плагин (DLL)

Здесь лежит C++ часть мода. Она нужна только для того, чего не умеет Papyrus:
точное чтение состояния анимации и (в перспективе) управление действиями NPC.

## Что уже есть

- `TGCombatPort.dll` — собирается из `src/main.cpp`.
- Papyrus-функция `TGCP_Native.IsBlocking(Actor)` — возвращает `true`, если актёр сейчас блокирует
  (читает переменную графа анимаций `Isblocking`, ту же, что использует условие `IsBlocking` в CK).
- При загрузке DLL пишет строку в `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`.

## Сборка

Собирается в GitHub Actions (`.github/workflows/native.yml`) на Windows-раннере:
туда же клонируется CommonLibSSE-NG. Локально нужен xmake 3.x и MSVC:

```cmd
cd skyrim-combat-port\native
git clone --depth 1 -b ng https://github.com/alandtse/CommonLibVR.git lib\commonlibsse-ng
xmake build
```

Готовый файл: `build\windows\x64\release\TGCombatPort.dll`.

## Установка (для проверки)

1. `TGCombatPort.dll` → `Data\SKSE\Plugins\`.
2. Запускать игру через `skse64_loader.exe`.
3. Смотреть `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`.

Требования: SKSE64 2.3.1 (для runtime 1.7.104) и Address Library v13 — без Address Library
плагин не загрузится.
