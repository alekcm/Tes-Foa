# Зависимости

Только то, что реально используется. Версии проверены на машине пользователя или в CI.

## Для игры (то, что нужно игроку)

| Компонент | Версия | Зачем | Проверено |
|---|---|---|---|
| Skyrim Special Edition | runtime **1.7.104.0** | база | да, из свойств `SkyrimSE.exe` |
| [SKSE64](https://skse.silverlock.org/) | **2.3.1** (для 1.7.104) | загрузка DLL | да, `skse64.log` на машине пользователя |
| [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) | v13 | коды версий для плагинов | требуется CommonLibSSE-NG |
| [Dynamic Animation Replacer](https://www.nexusmods.com/skyrimspecialedition/mods/33746) | 1.x | **не обязателен**: только для анимации укола | ставится по желанию |

Плагин `TGCombatPort.esp` входит в архив мода, отдельно качать нечего. USSEP, CC-контент и другие
моды не требуются, но и не конфликтуют: правила работают на любом враге.

## Для сборки

| Компонент | Версия | Зачем |
|---|---|---|
| Visual Studio 2022 (C++ Build Tools) | 17.x | `cl.exe`, MSVC-ABI |
| [xmake](https://xmake.io) | последняя | сборка `native/xmake.lua` |
| [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR) | ветка `ng`, коммит **48b17e1dea3028850adf63209f6733dbf4dcdcd7** | заголовки и API движка |
| Python | 3.10+ | инструменты `tools/`, тесты |

Коммит CommonLib закреплён намеренно: на более новых ломается поставка prebuilt-заголовков
(`openvr.h not found`). Тот же коммит использует CI.

## Чего НЕ требуется

- **Creation Kit** — `.esp` уже собран и лежит в репозитории; CK нужен, только если менять записи.
- **Papyrus-компилятор** — скриптов мода в сборке нет.
- **xEdit** — только для просмотра чужих плагинов.
- **Vortex/MO2** — установка архивом вручную тоже работает.
- **Nemesis/Pandora/FNIS** — новых анимаций поведения у нас нет.

## Ограничения по runtime

Плагин собран под `1.7.104` (SKSE 2.3.1). На другом runtime SKSE не загрузит DLL — это не «может
не работать», а «не загрузится». Версия игры видна в первой строке лога
`Документы\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`.
