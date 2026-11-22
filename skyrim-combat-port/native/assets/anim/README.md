# Анимация укола: как она сделана и как поставить

Здесь лежит готовая анимация укола для силовой атаки кинжалом и мечом. **Собственных HKX мы не
делали и не правили**: взяты ванильные клипы из твоей игры, которые уже являются уколом, и
переназначены на силовую атаку условием.

## Из чего собрано (проверено по файлам игры)

| Что | Откуда | Файл |
|---|---|---|
| Вид от первого лица | ванильный клип укола | `_1stperson\animations\1hm_attackstab.hkx` (9 200 байт) |
| Вид от третьего лица | ванильная силовая атака из-под sneak — это выпад/укол | `animations\1hm_sneakattackpowerforward.hkx` (17 904 байта) |
| Куда назначается | клип прямой силовой атаки одноручным | `1hm_attackpowerfwd.hkx` |

Оба исходных файла — настоящий Havok-пак (первые байты `57 E0 E0 57` = `hk_2010.2.0-r1`),
они скопированы как есть, поэтому риск «сломанной анимации» нулевой: ломать нечего.

Условие отбора — только кинжал и меч, остальные одноручные (топор, булава, молот) бьют как раньше.
Синтаксис условия проверен по открытой реализации DAR (`noxsidereum/dargh`, `src/DARProject.cpp`):
`IsEquippedRightType(N)`, где по документации DAR `1 = Swords`, `2 = Daggers`
(полный список: 0 Fists, 1 Swords, 2 Daggers, 3 War Axes, 4 Maces, 5 Greatswords, 6 Battleaxes,
7 Bows, 8 Staff, 9 Crossbows, 10 Warhammers, 11 Shields).

```
IsEquippedRightType(1) OR IsEquippedRightType(2)
```

## Контрольные суммы (чтобы сверить свою копию с той, что в релизе)

SHA-256 считаются командой `certutil -hashfile "<путь к файлу>" SHA256` в cmd:

| Файл | Размер | SHA-256 |
|---|---|---|
| `animations\DynamicAnimationReplacer\_CustomConditions\90001\1hm_attackpowerfwd.hkx` (3-е лицо) | 17 904 | `c33488ebb15113ba0178ff5bb46f8b4d84d1c58f04e72fc4314c00f0336daa2b` |
| `_1stperson\animations\DynamicAnimationReplacer\_CustomConditions\90001\1hm_attackpowerfwd.hkx` (1-е лицо) | 9 200 | `8b2f33dd9c28e7b83500167289ddfae1baa888b3ff3e01554af78a026c3812b1` |

Если хэш не совпал — файл другой (чаще всего это архив, скачанный не из последнего релиза, или
файл, который перезаписал другой анимационный мод).

## Что нужно установить

Анимацию назначает мод **Dynamic Animation Replacer** (сокращённо DAR) — бесплатный SKSE-плагин
с Nexus Mods. Без него файлы просто лежат и ничего не делают (вреда нет).

## Установка (cmd, Windows)

Архив `TGCombatPort-vX.Y.Z.zip` содержит папку `meshes` наравне с DLL, то есть анимация ставится
той же командой, что и сам плагин. Порядок: сначала проверить архив, потом распаковать.

```cmd
tar -tf "%USERPROFILE%\Downloads\TGCombatPort-v0.13.0.zip" | findstr /i "DynamicAnimationReplacer"
```

В списке должны быть **четыре** строки (два клипа и два файла условий: третье лицо и вид от первого
лица). Если строк нет — архив не тот, писать мне, а не распаковывать.

```cmd
tar -xf "%USERPROFILE%\Downloads\TGCombatPort-v0.13.0.zip" -C "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data"
dir /s /b "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\meshes\actors\character\animations\DynamicAnimationReplacer\_CustomConditions\90001"
```

Во второй команде должно быть два файла: `1hm_attackpowerfwd.hkx` (17 904 байта) и `_conditions.txt`.

Если DAR ещё не стоит: скачать с Nexus, поставить как обычный мод (файл `DynamicAnimationReplacer.dll`
в `Data\SKSE\Plugins\`), перезапустить игру.

## Как проверить, что укол работает

1. Взять кинжал в правую руку (или меч), подойти к бандиту.
2. Зажать атаку — это силовая атака. Должен быть выпад вперёд, а не боковой замах.
3. В логе плагина появится строка правила:

```
rule fired: [dagger-thrust-through-any-block] via=попадание actor=Бандит effects=chip
chip: 10.0 hp сквозь блок (dagger-thrust-through-any-block)
```

`chip` = урон прошёл, хотя блок остался. Это и есть укол.

## Как отключить

Удалить папку:

```cmd
rmdir /s /q "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\meshes\actors\character\animations\DynamicAnimationReplacer\_CustomConditions\90001"
rmdir /s /q "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\meshes\actors\character\_1stperson\animations\DynamicAnimationReplacer\_CustomConditions\90001"
```

Механика укола (`chip`) останется, вернётся только ванильная анимация силовой атаки.

## Почему анимация может не появиться, хотя файлы на месте

Три причины, все проверяются за минуту:

| Причина | Как проверить | Что делать |
|---|---|---|
| не установлен Dynamic Animation Replacer | в игре нет `Data\SKSE\Plugins\DynamicAnimationReplacer.dll` | поставить DAR (Nexus, SKSE-плагин), перезапустить игру |
| старый архив (до 0.13.0) | команда `tar -tf` по архиву не печатает папку `meshes` | скачать 0.13.0 и новее |
| удар не силовой | в логе плагина нет строки `hit: ... power=true` | зажать атаку, а не нажимать |

Важно: **правило укола (`chip`) работает и без анимации** — урон сквозь блок проходит, меняется
только внешний вид удара. То есть эти две вещи независимы, и «анимации нет» не значит «механики нет».

## Гарантия сборки (чтобы это не повторилось)

В `.github/workflows/native.yml` шаг упаковки теперь:

1. до упаковки падает, если исходных клипов нет в `native/assets/anim/meshes`;
2. после упаковки читает готовый zip и падает, если в нём нет хотя бы одного из четырёх файлов
   (`анимация укола не попала в версию`);
3. печатает в лог полный список файлов архива — его видно в интерфейсе Actions.

Плюс тест `tests/test_tools.py::ThrustAnimationAssetsTests` проверяет клипы и условия в репозитории
(размер, Havok-подпись `57 E0 E0 57`, наличие `IsEquippedRightType(1)` и `(2)`).

## Что осталось честно неизвестным

- Как выглядит в игре клип `1hm_sneakattackpowerforward` в роли обычной силовой атаки — по имени
  это выпад вперёд, но глазами я его не видел. Если тело приседает или движение кажется странным,
  скажи, и я заменю файл на другой ванильный клип из твоего набора (кандидаты:
  `1hm_sneakattackpower.hkx`, `1hm_sneakattackpowerleft/right.hkx`).
- Приоритет `90001` выбран высоким, чтобы перебивать другие анимационные моды. Если ты хочешь,
  чтобы какой-то мод был важнее, число в имени папки уменьшается.
