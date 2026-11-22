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
attacker.shield = yes | no | any
attacker.weapon = mace, battleaxe, sword, axe, dagger, greatsword, fists, bow, staff, crossbow | any
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
| `attacker.weapon` | тип оружия в руках бьющего (`RE::WEAPON_TYPE`); список = «любой из» |
| `player.blocking` | игрок блокирует прямо сейчас (`Actor::IsBlocking()`) |
| `player.shield` | игрок блокирует **щитом** (щит в левой руке) |
| `reach` | проверять дистанцию `fCombatBashReach` (читается из настроек движка) |
| `cooldown` | секунды между срабатываниями этого правила |
| `do` | эффекты по порядку |

Пропущенные ключи получают значения по умолчанию: `trigger` пуст (правило не сработает никогда —
так видно, что его забыли), `player.blocking = yes`, `reach = yes`, `cooldown = 4`.

## Эффекты

| Эффект | Что делает | Чем проверено |
|---|---|---|
| `unblock` | снимает блок у игрока событием графа `blockStop` и повторяет, пока он стаггерится | `AS-10`, проба `blocking=false` |
| `stagger` | стаггер спеллом от имени бьющего (дезориентация ~1 с) | `AS-2`, `AS-4`, лог 0.7.1 |
| `bash` | заказывает бьющему баш через граф | `AS-6`, лог 0.5.0 |

Порядок важен: `do = unblock, stagger` — сначала блок снимается, затем накладывается стаггер
(если наоборот, пробы успевают показать, что блок ещё держится).

## Текущие правила (они же встроенные, если файла нет)

```
[bandit-bash-breaks-weapon-block]   # правило 1: щитоносец выбивает блок оружием/кулаками
trigger = hit, bash
hit-flags = blocked
attacker.shield = yes
player.blocking = yes
player.shield = no
do = unblock, stagger

[heavy-blunt-power-breaks-shield-block]   # правило 2: щит пробивает силовая булава/двуручник
trigger = hit
hit-flags = blocked, power
attacker.weapon = mace, battleaxe
player.blocking = yes
player.shield = yes
do = unblock, stagger
```

## Примеры правок

**«Хочу, чтобы щит пробивал ещё и двуручный меч»** — одна строка:

```
attacker.weapon = mace, battleaxe, greatsword
```

**«Хочу, чтобы любой враг с топором тоже выбивал меня из блока оружием»** — новая секция:

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

**«Хочу, чтобы в этот момент меня ещё и отбрасывало»** — пока такого эффекта нет; добавляется так:
одна запись в `kEffectTable` (`{ "knockback", EffectId::kKnockback }`), `case` в `RunEffect`
и функция, которая это делает. После этого — просто `do = unblock, stagger, knockback` в правилах,
для любого врага и любой атаки, без правки логики.

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
