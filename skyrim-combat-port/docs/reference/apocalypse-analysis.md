# Apocalypse v9.45: что в моде лежит и что из этого брать врагам

Мод положен в репозиторий одним файлом: `Apocalypse v945-16225-9-45.zip` (63 478 463 байта),
внутри — `Apocalypse - The Spell Package.esp` (1 499 051 байт) и `.bsa` (152 759 188 байт).

Разбор парсером:

```cmd
python skyrim-combat-port\tools\esp_dump.py "Apocalypse - The Spell Package.esp"
python skyrim-combat-port\tools\esp_dump.py "Apocalypse - The Spell Package.esp" --type SPEL
```

## 1. Что внутри `.esp` (числа из файла)

| Тип | Количество |
|---|---|
| `MGEF` — магические эффекты | 717 |
| `SPEL` — заклинания | **404** |
| `SCRL` — свитки | 137 |
| `BOOK` — книги (в т.ч. тома заклинаний) | 191 |
| `KYWD` — ключевые слова | 150 |
| `FLST` — списки форм | 133 |
| `PERK`/`PROJ`/`EXPL`/`ARTO`/`EFSH` | 148 / 139 / 195 / 165 |
| `GLOB`, `IMAD`, `REFR`, `SNDR`, `MESG` | 98 / 93 / 255 / 257 / 154 |
| всего записей | 4218 |

## 2. Как определить школу, тип и уровень заклинания (проверено)

- **Школа** — не в `SPEL`, а в эффекте: `MGEF/DATA` смещение `0x0C` = навык
  (`18` Alteration, `19` Conjuration, `20` Destruction, `21` Illusion, `22` Restoration).
  Проверка: `WB_C100_ConjureNetherLich_Effect` → 19. Смещение `0x10` — сопротивление
  (там `-1`), его брать нельзя: в файле FormID-ы 4-байтовые, поэтому структура «сдвинута»
  на 4 байта относительно рантайм-структуры `EffectSettingData`.
- **Тип/доставка/время** — `SPEL/SPIT` = 36 байт:
  `costOverride i32`, `flags u32`, `spellType i32`, `chargeTime f32`,
  `castingType i32` (`0` constant effect, `1` fire and forget, `2` concentration,
  `3` scroll, `4` staff), `delivery i32` (`0` self, `1` touch, `2` aimed,
  `3` target actor, `4` target location), `castDuration f32`, `range f32`, `perk FormID`.
  Проверка на знакомых записях Wildcat: «Injured» → тип `Spell`, `fire and forget`, `self`;
  «Combat Ability» → `Ability` + `constant effect` — совпадает с тем, как эти спеллы используются.
- **Уровень (0/25/50/75/100)** — из имён книг-томов: `WB_<школа><уровень>_<Имя>_Book`,
  в `BOOK/DATA` по смещению `0x04` лежит FormID преподаваемого заклинания. Так получено
  **124** заклинания с точным уровнем. Остальные (силы, абилки, проки) уровня не имеют.

## 3. Главная находка: у мода есть готовые версии заклинаний для NPC

В `esp` есть 32 заклинания с суффиксом `_Spell_NPC` — отдельные версии, сделанные автором
именно для врагов (у них другая стоимость и отсутствуют перки игрока):

| Школа | NPC-версий |
|---|---|
| Destruction | **20** |
| Conjuration | **12** |
| Illusion / Restoration / Alteration | 0 |

Это готовая, сбалансированная автором лестница для врагов-магов — её и берём, не выдумывая
собственные числа.

### Destruction, версии для NPC (по уровням)

| Уровень | Заклинания (стоимость) |
|---|---|
| 0 (новичок) | Crackle 31, Hailstone 31, Unbound Fire 31 |
| 25 (ученик) | Creeping Cold 19, Dragon's Teeth 50, Fracture 54, Rift Bolt 56, Thundercrack 43 |
| 50 (адепт) | Bolide 134, Electrosphere 131, Ice Shiv 140, Incendiary Flow 134, Scattershock 36 |
| 75 (эксперт) | Bombardment 125, Scorching Hands 142, Sleet Storm 117 |
| 100 (мастер) | Forbidden Sun 610, Howling Blast 88, Static Dome 689 (`WB_D100Z_*`) |

Стихия следует из названия (огонь — Crackle/Unbound Fire/Dragon's Teeth/Bolide/Incendiary Flow/
Bombardment/Scorching Hands/Forbidden Sun; холод — Hailstone/Creeping Cold/Ice Shiv/Sleet Storm/
Howling Blast; молния — Rift Bolt/Thundercrack/Electrosphere/Scattershock/Static Dome).
Это вывод из названий, не проверено в игре.

### Conjuration, версии для NPC

| Уровень | Заклинания |
|---|---|
| 25 | Conjure Dremora Churl 157, Conjure Bound Churl 50 |
| 50 | Conjure Dremora Pit Fighter 229, Conjure Xivilai Sorcerer 229 |
| 75 | Conjure Dremora Champion 366, Conjure Dremora Honor Guard 380, Conjure Herne 394, Conjure Xivilai Lord 344 |
| 100 | Conjure Dremora Assassin 1201, Conjure Weeping Daedra 584, Conjure Lord of Bindings 1261, Conjure Nether Lich 1441 |

## 4. Почему заклинания мода врагам не выдаются

Решение: врагам даются **свои** заклинания ([`../../enemy-spells.md`](../enemy-spells.md)), а не
записи Apocalypse. Причины — по критерию «меняет ли бой»: у мода большинство заклинаний меняет только
числа (`Crackle`, `Hailstone`, `Bolide`, `Sleet Storm`, `Volcano`, `Apocalypse`), часть нечитаема
(`Fracture`, `Corpse Explosion`, `Slay Living`), часть — чистое лечение, часть отнимает управление
надолго (`Malviser's Gauntlet`, `Entomb`). Разбор по каждому — [`apocalypse-spells.md`](apocalypse-spells.md),
итоговый список отклонённого — [`../../enemy-spells.md`](../enemy-spells.md).

Полезным из мода остаётся готовый набор **NPC-версий** заклинаний (§3): если понадобится быстро дать
врагу чужой эффект, брать их, а не игровые версии.
