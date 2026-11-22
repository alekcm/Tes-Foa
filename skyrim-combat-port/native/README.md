# native/ — SKSE-плагин (DLL)

Здесь лежит C++ часть мода. Она нужна только для того, чего не умеет Papyrus:
точное чтение состояния анимации и (в перспективе) управление действиями NPC.

## Что уже есть (0.1.1)

- `TGCombatPort.dll` — собирается из `src/main.cpp`.
- `TGCP_Native.IsBlocking(Actor)` — `true`, если актёр блокирует. Читает `RE::Actor::IsBlocking()`,
  то есть состояние самого движка, а не событие анимации.
- `TGCP_Native.SetEnabled(bool)` — включить/выключить механику бандита без перезапуска игры (для тестов A/B).
- `TGCP_Native.GetStats()` — строка вида `calls=… bash=… boosts=… enabled=…` для скриптового теста.
- **Механика бандита со щитом.** Плагин перехватывает `Actor::GetAttackChance(target, attackData)` —
  функцию, через которую боевой AI решает, какую атаку выбрать, — и поднимает шанс bash-атаки
  (`AttackData::kBashAttack`), когда одновременно:
  - игрок держит блок,
  - бандит со щитом в руке, в бою и враждебен игроку,
  - дистанция не больше 240 игровых единиц,
  - с последнего подъёма прошло 7 с (кулдаун по дизайну).
  Анимацию, попадание и урон при этом считает сама игра: мы не отправляем animation events и не
  ломаем состояние анимации NPC. Адрес функции в runtime берётся по ID из CommonLibSSE:
  49748 (SE) / 50675 (AE). Если хук не поставится, в логе будет строка `hook failed`.
- В лог `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log` пишутся: версия, факт
  установки хука, статистика (`hook stats`), первые bash-кандидаты (`bash candidate`) и каждое
  срабатывание (`bash boost`).

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

- https://github.com/alekcm/Tes-Foa/releases/download/v0.1.1/TGCombatPort-v0.1.1.zip —
  архив с `SKSE/Plugins/TGCombatPort.dll`, распаковывается в папку `Data` игры
  или ставится как обычный мод в Vortex;
- https://github.com/alekcm/Tes-Foa/releases/download/v0.1.1/TGCombatPort.dll — сам файл.

Почему не через `git pull`: в git лежат исходники; `.dll` — собранный Windows-бинарник, он
собирается на сервере GitHub и выкладывается в Releases. Папка проекта на рабочем столе и
папка игры — вообще разные места, поэтому файл всё равно надо положить в `Data`.

## Установка (для проверки)

1. Скачать `TGCombatPort-v0.1.1.zip` по ссылке выше.
2. Распаковать в папку `Data` игры (cmd):

```cmd
tar -xf "%USERPROFILE%\Downloads\TGCombatPort-v0.1.1.zip" -C "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data"
```

3. Поставить **Address Library v13** (Nexus, мод 32444, файл для 1.6+) — без него DLL не загрузится.
4. Запускать игру через `skse64_loader.exe`.
5. Смотреть `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`
   (если файла нет — `skse64.log` в той же папке).

Требования: SKSE64 2.3.1 (для runtime 1.7.104) и Address Library v13.
