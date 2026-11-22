# Справочник по механике Skyrim (для этого мода)

Назначение: чтобы не выдумывать. Любое утверждение о поведении игры берётся отсюда,
а не из памяти агента. Если нужного факта здесь нет — он помечается как **неизвестный**
и проверяется логом, а не догадкой.

## Уровни доверия

У каждой записи в этом справочнике стоит метка:

| Метка | Что значит | Можно ли на это опираться в решениях |
|---|---|---|
| **[A]** | измерено в этой игре, в наших логах (файл и строка указаны) | да, это факт об установке |
| **[B]** | официальная документация: UESP, CreationKit Wiki | да, но с оговоркой на версию игры |
| **[C]** | отчёты моддеров (форумы, Nexus, Reddit), покорпусные наблюдения | только как гипотеза для проверки |
| **[D]** | гипотеза агента, ничем не подтверждена | нельзя. Сначала проверить |

Правило: **если утверждение не имеет метки [A] или [B], его нельзя писать пользователю как факт.**
Формулировка «должно сработать» запрещена; вместо неё — «проверяется строкой лога …».

## Разделы

| Файл | О чём |
|---|---|
| [`block-bash-stagger.md`](block-bash-stagger.md) | блок, баш, стаггер, силовые атаки: что в ванили есть и чего нет |
| [`combat-ai.md`](combat-ai.md) | CombatStyle: какие поля управляют башем и силовой, и почему они не сработали у нас |
| [`game-settings.md`](game-settings.md) | `fBlock*`, `fStagger*`, `fCombatBash*`, `fStamina*` с заводскими значениями и что меняют оверхаулы |
| [`attacks-and-hits.md`](attacks-and-hits.md) | `BGSAttackData` и `HitData`: что такое настоящее попадание и почему анимация его не даёт |
| [`overhaul-techniques.md`](overhaul-techniques.md) | каталог способов сделать стаггер/сбивание блока, с источниками и ограничениями |
| [`animation-events.md`](animation-events.md) | имена animation events, что из них рассылается слушателям, чем событие отличается от атаки |
| [`magic-effect-stagger.md`](magic-effect-stagger.md) | архетип Stagger, magnitude, как наложить эффект из DLL |
| [`commonlib-api.md`](commonlib-api.md) | что реально есть в `RE::` (CommonLibSSE-NG), а чего нет |
| [`measured-in-game.md`](measured-in-game.md) | всё, что измерено в игре: логи по версиям, что работало и что нет |

## Источники

- UESP: <https://en.uesp.net/wiki/Skyrim:Block>, <https://en.uesp.net/wiki/Skyrim:Combat>
- CreationKit Wiki, Magic Effect: <https://ck.uesp.net/wiki/Magic_Effect>
- Blade and Blunt (описание стаггера у игрока и NPC): <https://www.nexusmods.com/skyrimspecialedition/mods/34549>
- Список animation events: <https://pastebin.com/Z5NwnMbQ>
- Список idle-анимаций (там же `bashStart` = idle `0001B417`): <https://elderscrolls.fandom.com/wiki/Console_Commands_(Skyrim)/Idle_Animations>
- CK Wiki: [Combat Style](https://skyrimck.uesp.net/wiki/Combat_Style),
  [Perk Entry Point](https://ck.uesp.net/wiki/Perk_Entry_Point),
  [GameSetting](https://ck.uesp.net/wiki/GameSetting),
  [KnockAreaEffect](https://ck.uesp.net/wiki/KnockAreaEffect_-_ObjectReference),
  [Magic Effect](https://ck.uesp.net/wiki/Magic_Effect)
- UESP: [Mod File Format/CSTY](https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/CSTY),
  [Mod File Format/PERK](https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/PERK)
- Документация оверхаулов: [Wildcat](https://www.nexusmods.com/skyrim/mods/76529?tab=docs),
  [Valravn](https://www.nexusmods.com/skyrimspecialedition/mods/53869?tab=docs),
  [Blade and Blunt](https://www.nexusmods.com/skyrimspecialedition/mods/34549)
- Заголовки CommonLibSSE-NG, пин `48b17e1dea3028850adf63209f6733dbf4dcdcd7` (16.10.0), ветка `ng`:
  <https://github.com/alandtse/CommonLibVR>

## Как добавлять запись

1. Взять факт из источника, указать ссылку и метку.
2. Если факт про нашу установку — указать лог и версию мода.
3. Если факт спорный (вики сама пишет «verification needed») — так и написать.
4. Не стирать опровергнутые записи: переносить в раздел «опровергнуто» соответствующего файла,
   чтобы не проверять одно и то же дважды.
