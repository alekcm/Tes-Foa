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

## 4. Кому что давать: решение

Ниже — по одной строке на класс врага. Формат — будущий вид таблицы данных
(`native/rules/spellbook.txt`), которую будет читать плагин. **Код чтения пока не написан** —
это решение по составу, а не работающая механика.

### 4.1 Маг-разрушитель (бандит-маг, форсверн-маг, колдун)

| Уровень врага | Список |
|---|---|
| 1–9 (новичок) | `WB_D000_Crackle_Spell_NPC`, `WB_D000_Hailstone_Spell_NPC` |
| 10–19 | + `WB_D025_RiftBolt_Spell_NPC`, `WB_D025_Fracture_Spell_NPC` |
| 20–29 | + `WB_D050_Bolide_Spell_NPC`, `WB_D050_IceShiv_Spell_NPC` |
| 30+ | + `WB_D075_SleetStorm_Spell_NPC`, `WB_D075_Bombardment_Spell_NPC` |

Почему так: три стихии на всех уровнях — игрок не может подобрать один резист и стоять.
У этих версий уже снята перковая часть, значит урон предсказуем.

### 4.2 Некромант

| Что | Список |
|---|---|
| призыв (по уровню) | `WB_C025_ConjureDremoraChurl_Spell_NPC` → `WB_C050_ConjureXivilaiSorcerer_Spell_NPC` → `WB_C075_ConjureDremoraChampion_Spell_NPC` |
| порча | `WB_R025_Necroplague_Spell_PC`, `WB_R050_FingerOfDeath_Spell_PC` |
| мастер | `WB_C100_ConjureNetherLich_Spell_PC` |

Некромант опасен не уроном, а тем, что бой становится «двое на одного». Ванильные скелеты
рядом с ним — уже привычный опыт игрока; лич-призыв — шаг вверх.

### 4.3 Нежить-колдун (драугр-«жрец», «Смертный слуга»)

| Что | Список |
|---|---|
| порча | `WB_R000_BoneSpirit_Spell_PC`, `WB_R025_Necroplague_Spell_PC` |
| 20+ | + `WB_R050_FingerOfDeath_Spell_PC`, `WB_R075_Decompose_Spell_PC` |

Тема: медленная смерть, а не взрыв. Работает с нашей механикой ран: игрок уже ранен,
нежить добивает.

### 4.4 Лич (мастер нежити)

| Что | Список |
|---|---|
| мастер-разрушение | `WB_D100Z_ForbiddenSun_Spell_NPC`, `WB_D100Z_StaticDome_Spell_NPC` |
| призыв | `WB_C100_ConjureNetherLich_Spell_PC` |
| порча | `WB_R100_WormShroud_Spell_PC` |
| контроль | `WB_I100_MagickaVoid_Spell_PC` |

Лич бьёт тем, чего у игрока нет: сжигает магию (`Magicka Void`), закрывает пространство
(`Static Dome`). Это босс, поэтому мастер-тир оправдан.

### 4.5 Природный дух (спригган, дух леса)

| Что | Список |
|---|---|
| лечение себя | `WB_R000_WildHealing_Spell_PC`, `WB_R050_TreeRings_Spell_PC` |
| природа | `WB_R025_LeechSeed_Spell_PC`, `WB_R075_HealingBlossom_Spell_PC` |
| 30+ | + `WB_R075_NaturesBalance_Spell_PC` |

Спригган, который лечится, — совсем другой бой: игрок обязан давить, а не обмениваться
ударами. Ровно то, что нужно для проверки нашей механики ран.

### 4.6 Босс-маг (драконий жрец, главный колдун)

| Что | Список |
|---|---|
| мастер | `WB_D100_ApocalypseNew_Spell_PC`, `WB_D100_FrozenOrb_Spell_PC`, `WB_D100_Twister_Spell_PC` |
| добивание | `WB_R100_DustToDust_Spell_PC`, `WB_R100_MeridiasWrath_Spell_PC` |
| мобильность | `WB_A100_WindRunning_Spell_PC` |

## 5. Как это будет выдаваться в игре (и что не проверено)

API проверено по заголовкам CommonLibSSE-NG (`tools/verify_api.py`):

| Что нужно | Что есть |
|---|---|
| найти заклинание по EditorID | `RE::TESForm::LookupByEditorID(std::string_view)` — `RE/T/TESForm.h:231` |
| или по FormID + имя плагина | `RE::TESDataHandler::LookupForm(FormID, std::string_view)` — `RE/T/TESDataHandler.h:62` |
| выдать актёру | `RE::Actor::AddSpell(SpellItem*)` — `RE/A/Actor.h:531` |
| событие начала боя | `RE::TESCombatEvent` (есть в `RE/T/TESCombatEvent.h`) |

Открытые вопросы, которые решает только игра. В реестр допущений `AS-*` они попадут вместе с кодом
(сейчас кода нет, поэтому записи в реестре были бы «мёртвыми» — `verify_audit.py` такое не пропускает):

| № | Вопрос | Что покажет лог |
|---|---|---|
| 1 | начнёт ли NPC применять заклинание, выданное через `AddSpell` в бою | строка `spellbook: <актёр> +<EditorID>` и дальше — каст в игре или нет |
| 2 | не выдаём ли заклинания дважды (у NPC уже есть свой список) | строка `spellbook: пропуск <актёр> (уже знает N заклинаний)` |
| 3 | тянет ли NPC мастер-тир по магии (хватит ли маг`ики и навыка) | в логе — `spellbook: магия <актёра> = N`, в игре видно, кастует или стоит |

Правила безопасности (обязательны, иначе вылеты):

1. Мод может отсутствовать → `LookupByEditorID` вернёт `nullptr`, таблица пропускается,
   в лог одна строка `spellbook: Apocalypse не загружен, пропуск`. Никаких падений.
2. Не выдавать заклинания игроку и его спутникам.
3. Выданное заклинание не удалять в бою (удаление в бою — известный источник вылетов).
4. Не выдавать больше 4–6 заклинаний на актёра: AI всё равно выберет 1–2, остальное — вес.

## 6. Что это даёт нашей механике

| Наша механика | Как с ней стыкуется Apocalypse |
|---|---|
| 4 состояния защиты (щит/оружие/вард/без блока) | вард игрока теперь имеет смысл: маг-разрушитель бьёт, игрок держит вард, `chip`/`dispel` проверяются на живом враге |
| раны по частям тела | «рука ранена → нельзя кастовать» становится осмысленным ограничением против мага |
| стаггер без урона | мастер-спелл врага — повод для стаггера игрока, а не для скрытого урона |
| кровотечение (план) | `Necroplague`, `Welling Blood`, `Hemorrhage` — готовые источники «болезни» поверх нашей раны |
