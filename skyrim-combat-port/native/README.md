# native/ — SKSE-плагин (DLL)

Здесь лежит C++ часть мода. Она нужна только для того, чего не умеет Papyrus:
точное чтение состояния анимации и (в перспективе) управление действиями NPC.

## Что уже есть (0.3.0)

Хуков (патчинга кода игры) нет: версия 0.1.1 патчила `Actor::GetAttackChance` и вызывала вылет, способ убран.

Papyrus-функции скрипта `TGCP_Native`:

| Функция | Что делает |
|---|---|
| `IsBlocking(Actor)` | `true`, если актёр блокирует (`RE::Actor::IsBlocking()`) |
| `IsBlockingWithShield(Actor)` | `true`, если блокирует именно щитом |
| `Tick()` | один шаг механики; зовётся из `TGCP_PlayerBlockWatcher` каждые 0.25 с, пока игрок блокирует |
| `SetEnabled(bool)` | включить/выключить механику без перезапуска |
| `GetStats()` | `ticks=… bashes=… powerWindows=… buffs=… enabled=…` |

### Правила механики (поправка дизайна 22.11.2026)

1. Игрок блокирует **не щитом** → щитоносец-бандит в ближнем бою (≤ 200) получает заказ на баш;
   DLL вызывает `actor->NotifyAnimationGraph("bashStart")`. Кулдаун 7 с, начатую атаку не перебиваем.
2. Игрок блокирует **щитом** → баша нет. Бандит с булавой (`kOneHandMace`) или двуручным
   молотом/топором (`kTwoHandAxe`) получает на 2,5 с `powerAttackBlockingMult = 10.0`
   («вероятность силовой атаки по блокирующему») — силовую атаку выбирает сам AI.
3. Игрок не блокирует → плагин не вмешивается.

Честно о способе из пункта 1: в движке нет функции «прикажи NPC ударить щитом». Прямой заказ возможен
только через animation event — это тот же механизм, что `Debug.SendAnimationEvent`, но вызванный из DLL,
с нашими проверками (враждебен, в бою, дистанция, не атакует, кулдаун). Риск рассинхрона анимации NPC
у этого механизма документирован. Пункт 2 ничего не подделывает: меняются только числа `CombatStyle`.

### Что показали тесты в игре

- 0.2.0/0.2.1: окна открывались, но AI **ни разу не выбрал баш** даже при всех множителях 10.0
  (в логе только `attackStop`, ни одного `bashStart`). Причина ванильная: NPC бьёт щитом, когда игрок
  атакует или получил recoil, а на пассивный блок движок отвечает иначе. Поэтому правила переписаны
  под решение пользователя (см. выше) и баш отдаётся прямым заказом.

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

- https://github.com/alekcm/Tes-Foa/releases/download/v0.3.0/TGCombatPort-v0.3.0.zip —
  архив с `SKSE/Plugins/TGCombatPort.dll`, распаковывается в папку `Data` игры
  или ставится как обычный мод в Vortex;
- https://github.com/alekcm/Tes-Foa/releases/download/v0.3.0/TGCombatPort.dll — сам файл.

Почему не через `git pull`: в git лежат исходники; `.dll` — собранный Windows-бинарник, он
собирается на сервере GitHub и выкладывается в Releases. Папка проекта на рабочем столе и
папка игры — вообще разные места, поэтому файл всё равно надо положить в `Data`.

## Проверка, что установилась нужная версия

Первая строка лога `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log` содержит версию
плагина, например `TGCombatPort v0-3-0-0`. Если там старая версия — игра грузит старый файл:

- браузер мог сохранить новый файл как `TGCombatPort (1).dll`; проверьте оба пути
  (`dir "%USERPROFILE%\Downloads\TGCombatPort*.dll"`) и скопируйте нужный с переименованием;
- если мод стоит в **Vortex**, он раскладывает файлы в `Data` сам при развёртывании и может вернуть
  старую версию поверх ручной копии. В этом случае ставить надо архив
  (`TGCombatPort-v0.3.0.zip`) через Vortex → Install From File, а не копированием.

## Установка (для проверки)

1. Скачать `TGCombatPort-v0.3.0.zip` по ссылке выше (внутри DLL **и** обновлённые `.pex`).
2. Распаковать в папку `Data` игры (cmd):

```cmd
tar -xf "%USERPROFILE%\Downloads\TGCombatPort-v0.3.0.zip" -C "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data"
```

3. Поставить **Address Library v13** (Nexus, мод 32444, файл для 1.6+) — без него DLL не загрузится.
4. Запускать игру через `skse64_loader.exe`.
5. Смотреть `Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`
   (если файла нет — `skse64.log` в той же папке).

Требования: SKSE64 2.3.1 (для runtime 1.7.104) и Address Library v13.
