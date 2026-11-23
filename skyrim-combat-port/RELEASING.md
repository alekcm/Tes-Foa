# Выпуск версии

Всё делает CI: сборка DLL, упаковка, проверка архива, публикация релиза.

## Как выпустить

```cmd
git tag v0.16.0
git push origin v0.16.0
```

Тег `v*` запускает `.github/workflows/native.yml` → шаг **Publish release** создаёт релиз
`TGCombatPort v0.16.0` с одним файлом `TGCombatPort-v0.16.0.zip`.

Перед тегом стоит прогнать локальные проверки (они же в CI):

```cmd
python -m unittest discover -s tests
python tools\check_rules.py
python tools\check_docs.py
python tools\verify_audit.py
```

## Что CI проверяет до публикации

| Шаг | Что проверяет | Что будет, если не так |
|---|---|---|
| Clone CommonLibSSE-NG | берёт закреплённый коммит `48b17e1` | без него сборка не пойдёт |
| Check engine API usage | каждое `RE::Имя` в `native/src/main.cpp` есть в заголовках | ошибки печатаются аннотациями, сборка не начинается |
| Build | `xmake build` под runtime 1.7.104 | последние 15 строк ошибок уходят в аннотации |
| Upload DLL | `build/windows/x64/release/*.dll` | артефакт без DLL не создаётся |
| Package for install | собирает zip и **читает его обратно** | падает, если нет DLL, `Rules.txt` или файлов анимации |

## Что лежит в архиве

| Путь | Откуда |
|---|---|
| `SKSE/Plugins/TGCombatPort.dll` | сборка |
| `SKSE/Plugins/TGCombatPort/Rules.txt` | `native/rules/Rules.txt` |
| `meshes/actors/character/animations/DynamicAnimationReplacer/_CustomConditions/90001/…` | `native/assets/anim/meshes` |
| `meshes/actors/character/_1stperson/animations/…/90001/…` | там же |

Архив распаковывается в папку **Data** игры (внутри нет папки `Data`).

## Сборка руками, без CI

```cmd
cd native
xmake f -y -m release
xmake build -y
```

Дальше собрать каталог в структуре Data (DLL + `Rules.txt` + `meshes`), сжать в zip и поставить
в игру. Скрипт упаковки — `tools/package.py` (проверяет, что внутри нет папки `Data`, и считает SHA-256).

## Признак «версия работает»

Не «собралось», а строки в логе после боя: `правNл 8, строк с ошибками 0`, `hit: …`, `rule fired: …`.
Методика — [`TESTING.md`](TESTING.md).
