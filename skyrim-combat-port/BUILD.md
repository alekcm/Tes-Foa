# Что нужно, чтобы собрать плагин

Плагин — это SKSE-DLL на C++ (`native/`). Записи `.esp`, Papyrus-скрипты и Creation Kit для сборки
**не нужны**: ESP уже лежит в репозитории и не пересобирается.

## Что должно быть на машине

| Что | Версия | Зачем |
|---|---|---|
| Windows x64 | 10/11 | MSVC-ABI, как у самой игры |
| Visual Studio 2022 (Build Tools, C++ workload) | 17.x | компилятор `cl.exe` |
| [xmake](https://xmake.io) | последняя | сборка (`native/xmake.lua`) |
| Git | любая | клон CommonLibSSE-NG |
| Сеть до github.com | — | библиотека тянется из репозитория |

CommonLibSSE-NG клонируется отдельно (в Git его нет):

```cmd
cd native
git clone -b ng https://github.com/alandtse/CommonLibVR.git lib/commonlibsse-ng
cd lib\commonlibsse-ng
git checkout 48b17e1dea3028850adf63209f6733dbf4dcdcd7
```

Коммит **закреплён**: на свежих версиях ломается поставка prebuilt-заголовков
(`openvr.h not found`). Тот же коммит использует CI.

## Сборка

```cmd
cd native
xmake f -y -m release
xmake build -y
```

Результат: `native\build\windows\x64\release\TGCombatPort.dll`.

## Что попадает в архив для игрока

CI собирает это сам (`.github/workflows/native.yml`), руками ничего складывать не нужно:

| Путь в архиве | Откуда |
|---|---|
| `SKSE/Plugins/TGCombatPort.dll` | результат сборки |
| `SKSE/Plugins/TGCombatPort/Rules.txt` | `native/rules/Rules.txt` |
| `meshes/actors/character/animations/.../_CustomConditions/90001/*` | `native/assets/anim/meshes` (укол кинжалом/мечом, нужен мод DAR) |

Архив распаковывается **в папку Data игры**: внутри нет папки `Data`, пути уже относительно неё.

## Чего в Git быть не должно

`SkyrimSE.exe`, `Skyrim.esm`, `Update.esm`, DLC и CC-плагины, `Data/`, BSA/BA2, распакованные ассеты
игры, `lib/commonlibsse-ng`, `build/`, `dist/`.

## Проверки без игры

```cmd
python -m unittest discover -s tests
python tools\check_rules.py
python tools\check_docs.py
python tools\verify_audit.py
python tools\check_api_usage.py --headers native\lib\commonlibsse-ng\include
```

Последняя сверяет каждое `RE::Имя` в `native/src/main.cpp` с заголовками и печатает файл:строку.
В CI она идёт **до** сборки: если имени нет в заголовках, сборка не начинается.
