# CK Wiki + UESP: Perk Entry Point (копия)

Источники: <https://ck.uesp.net/wiki/Perk_Entry_Point>, <https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/PERK>
(скопировано 2026-11-22). Лицензия: CK Wiki / UESP, CC BY-SA.

## Описания

> **Mod Incoming Stagger** — Modifies the amount of stagger sustained by the perk owner when hit.
> **Mod Target Stagger** — Adjusts the amount of stagger targets suffer when hit by this perk owner.

## Номера эффектов (PERK DATA)

> | 21 | Mod Incoming Stagger | Value Types | | Perk Owner, Attacker | 2 |
> | 22 | Mod Target Stagger | Value Types | | Perk Owner, Target | 2 |

## Наблюдения моддеров (не вики, [C])

- «Mod Target Stagger … It's unclear how this perk entry's value influences stagger. Higher values increase
  the chance of stagger, but stagger remains the same … This suggests that stagger is a chance trigger.»
  <https://fallout.wiki/wiki/User:Alegendv1>
- «the vanilla **Mod Target Stagger** perk entry does not affect the stagger magic magnitude in vanilla»
  <https://www.nexusmods.com/skyrimspecialedition/mods/57722>
- Попытки моддеров заставить этот entry point работать дают «ничего».
  <https://forums.nexusmods.com/topic/1533673-mod-target-stagger-arrrgh/>

**Вывод:** перками стаггер не настраивается надёжно; и они требуют записей `PERK` в esp.
