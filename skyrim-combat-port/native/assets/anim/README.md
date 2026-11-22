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

## Что нужно установить

Анимацию назначает мод **Dynamic Animation Replacer** (сокращённо DAR) — бесплатный SKSE-плагин
с Nexus Mods. Без него файлы просто лежат и ничего не делают (вреда нет).

## Установка (cmd, Windows)

Архив `TGCombatPort-vX.Y.Z.zip` уже содержит нужную папку, поэтому установка — та же команда, что и
раньше, плюс проверка:

```cmd
tar -xf "%USERPROFILE%\Downloads\TGCombatPort-v0.12.0.zip" -C "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data"
dir /s /b "A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\meshes\actors\character\animations\DynamicAnimationReplacer\_CustomConditions\90001"
```

Во второй команде должно быть два файла: `1hm_attackpowerfwd.hkx` и `_conditions.txt`.

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

## Что осталось честно неизвестным

- Как выглядит в игре клип `1hm_sneakattackpowerforward` в роли обычной силовой атаки — по имени
  это выпад вперёд, но глазами я его не видел. Если тело приседает или движение кажется странным,
  скажи, и я заменю файл на другой ванильный клип из твоего набора (кандидаты:
  `1hm_sneakattackpower.hkx`, `1hm_sneakattackpowerleft/right.hkx`).
- Приоритет `90001` выбран высоким, чтобы перебивать другие анимационные моды. Если ты хочешь,
  чтобы какой-то мод был важнее, число в имени папки уменьшается.
