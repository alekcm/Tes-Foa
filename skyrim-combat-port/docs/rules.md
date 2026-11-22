# Правила механики: `Data\SKSE\Plugins\TGCombatPort\Rules.txt`

Вся боевая логика плагина — **данные**, а не код. Плагин при запуске читает этот файл,
печатает разобранные правила в лог и дальше только сверяет с ними события.

- «Хочу, чтобы такая-то атака такого-то врага делала то же самое» → **новая секция** в файле.
- «Хочу, чтобы в этот момент у меня ещё что-то происходило» → **эффект в строке `do`**.
- Если эффекта такого ещё нет — добавляется **одна запись** в `kEffectTable` в коде и одна функция.

DLL и файл правил лежат рядом, поэтому правка правил **не требует пересборки**.

## Формат

```
[имя-правила]         # имя попадает в лог: rule fired: [имя] via=попадание
trigger         = hit, bash
hit-flags       = blocked
hit-not-flags   = power
attack.kind     = melee, ranged, spell, shout, unarmed | any
attacker.race   = BearRace, Skyrim.esm|0x131E9, 0x000131E9
attacker.weapon = fists, sword, dagger, axe, mace, greatsword, battleaxe, bow, staff, crossbow | any
attacker.shield = yes | no | any
hit.source      = <форма чем бьют: оружие / заклинание / крик>
hit.projectile  = <форма снаряда: болт / стрела / файрбол>
player.blocking = yes | no | any
player.shield   = yes | no | any
reach           = yes | no
cooldown        = 4
do              = unblock, stagger
```

| Ключ | Смысл |
|---|---|
| `trigger` | `hit` — настоящее попадание (`RE::TESHitEvent`), `bash` — закончился наш заказанный баш (попадания он не создаёт) |
| `hit-flags` | какие флаги попадания **обязаны** быть: `blocked` (`kHitBlocked`), `power` (`kPowerAttack`), `bash` (`kBashAttack`), `sneak` (`kSneakAttack`) |
| `hit-not-flags` | каких флагов быть **не должно** |
| `attacker.shield` | щит у того, кто бьёт |
| `attack.kind` | вид атаки: `melee`, `ranged`, `spell`, `shout`, `unarmed` — выводится из формы в `TESHitEvent::source` (`AS-12`) |
| `attacker.race` | раса бьющего: `TESRace` (`Actor::GetRace`) |
| `attacker.weapon` | тип оружия в руках бьющего (`RE::WEAPON_TYPE`); список = «любой из» |
| `hit.source` | форма, которой нанесён удар: оружие, заклинание, крик (`TESHitEvent::source`) |
| `hit.projectile` | форма снаряда: болт, стрела, огненный шар (`TESHitEvent::projectile`) |
| `player.blocking` | игрок блокирует прямо сейчас (`Actor::IsBlocking()`) |
| `player.shield` | игрок блокирует **щитом** (щит в левой руке) |
| `player.ward` | у игрока поднят **вард** (`kWardPower > 0` — Малый/Устойчивый/Великий Оберег) |
| `chip` | сколько здоровья снимает эффект `chip` (урон сквозь блок) |
| `reach` | проверять дистанцию `fCombatBashReach` (читается из настроек движка) |
| `cooldown` | секунды между срабатываниями этого правила |
| `do` | эффекты по порядку |

Пропущенные ключи получают значения по умолчанию: `trigger` пуст (правило не сработает никогда —
так видно, что его забыли), `player.blocking = yes`, `reach = yes`, `cooldown = 4`.

## Эффекты

| Эффект | Что делает | Чем проверено |
|---|---|---|
| `unblock` | снимает блок событием `blockStop` и повторяет попытки всё окно дезориентации (1,2 с) | `AS-10`, проба `blocking=false` |
| `stagger` | переводит игрока в состояние стаггера (`staggerStart`): прерывает действие, без урона и визуальных эффектов; спелл — только запасной путь | `AS-14`, лог 0.7.2 |
| `bash` | заказывает бьющему баш через граф | `AS-6`, лог 0.5.0 |
| `chip` | снимает N здоровья **сквозь блок** (блок остаётся, оглушения нет) — укол кинжала/меча | `AS-16` |

Порядок важен: `do = unblock, stagger` — сначала блок снимается, затем накладывается стаггер
(если наоборот, пробы успевают показать, что блок ещё держится).

## Текущие правила (они же встроенные, если файла нет)

```
[shield-bash-staggers-unblocked]   # правило 1: щит дезориентирует только БЕЗ блока
trigger = hit, bash
player.blocking = no
do = stagger
cooldown = 3

[onehand-mace-power-breaks-shield-block]   # правило 2: щит пробивает силовая булава
trigger = hit
hit-flags = blocked, power
attacker.weapon = mace
player.blocking = yes
player.shield = yes
do = unblock, stagger
```

Обрати внимание: `battleaxe` здесь больше нет — это весь двуручный тип движка (`kTwoHandAxe`),
то есть и топор, и молот; по решению 22.11.2026 обычное двуручное щит не пробивает.

## Примеры правок

**«Хочу, чтобы щит пробивал ещё и двуручный молот»** — но сперва нужен код: движок отдаёт
двуручный топор и молот одним типом, поэтому сначала в коде появляется признак по кейворду
`WeapTypeWarhammer`, и только потом в правиле можно написать `attacker.weapon = mace, warhammer`.

**«Хочу, чтобы топор снова выбивал из блока»** (отменённое решение — пример правки) — новая секция:

```
[axe-breaks-weapon-block]
trigger = hit, bash
hit-flags = blocked
player.blocking = yes
player.shield = no
attacker.weapon = axe, battleaxe
do = unblock, stagger
cooldown = 4
```

Сейчас этого правила нет: 22.11.2026 решено, что топор не оглушает и блок не пробивает.

**«Удар медведя выбивает из равновесия»** — новая секция (у существа нет оружия, поэтому
фильтр по оружию не ставится; `reach = no`, потому что `fCombatBashReach` — про баш, а не про лапы):

```
[bear-paw-breaks-block]
trigger = hit
hit-flags = blocked
attacker.race = BearRace
player.blocking = yes
do = unblock, stagger
reach = no
cooldown = 4
```

**«Особый разрывной болт»** — фильтр по снаряду (значение из лога `projectile=`):

```
[explosive-bolt-breaks-block]
trigger = hit
hit-flags = blocked
hit.projectile = 0x00000000
player.blocking = yes
do = unblock, stagger
```

**«Особое заклинание»** — фильтр по виду атаки и форме:

```
[special-spell-breaks-block]
trigger = hit
hit-flags = blocked
attack.kind = spell
hit.source = 0x00000000
player.blocking = yes
do = unblock, stagger
```

**«Дыхание дракона»** — приходит как крик:

```
[dragon-breath-breaks-block]
trigger = hit
hit-flags = blocked
attack.kind = shout
attacker.race = DragonRace
player.blocking = yes
do = unblock, stagger
```

**«Хочу, чтобы в этот момент меня ещё и отбрасывало»** — пока такого эффекта нет; добавляется так:
одна запись в `kEffectTable` (`{ "knockback", EffectId::kKnockback }`), `case` в `RunEffect`
и функция, которая это делает. После этого — просто `do = unblock, stagger, knockback` в правилах,
для любого врага и любой атаки, без правки логики.

## Формы: как записывать

| Запись | Что это |
|---|---|
| `BearRace` | EditorID из Creation Kit — самый простой способ |
| `Skyrim.esm|0x131E9` | плагин + локальный FormID, не зависит от порядка загрузки |
| `0x000131E9` | полный FormID, как он есть в этой игре |

Откуда брать значения: строка лога на каждое попадание

```
hit: actor=Медведь kind=unarmed flags=08 race=BearRace (000131E9) source=00000000 projectile=00000000 игрок.блок=true игрок.щит=false враж.щит=false
```

Если форма не нашлась при загрузке, правило помечается `(ОТКЛЮЧЕНО: ошибка в файле)` — молча
работать «на всё подряд» оно не будет.

## Чего в условиях пока нет

Ограничения «именно этот враг» (раса, фракция, конкретный NPC) в правилах нет — сейчас врага
задают оружие и щит. Если понадобится «только бандиты» или «только этот NPC» — это новое условие
(`attacker.race`, `attacker.faction`), одна ветка в `ApplyKey` и одна проверка в `RuleMatches`.

## Как убедиться, что правила загрузились

Первые строки лога после запуска:

```
Rules.txt [A:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\TGCombatPort\Rules.txt]: правил 2, строк с ошибками 0
правил загружено: 2
  [bandit-bash-breaks-weapon-block] trigger=hit,bash hit=08 not=00 attacker.shield=yes attacker.weapon=any player.blocking=yes player.shield=no reach=true do=unblock,stagger
```

`строк с ошибками` больше нуля — рядом будут строки `Rules.txt:<номер>: не понял «ключ = значение»`;
такая строка игнорируется, остальные правила работают.
