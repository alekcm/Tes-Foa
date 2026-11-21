# План переноса боёвки Tainted Grail: The Fall of Avalon → Skyrim Anniversary Edition

**Статус:** проектное ТЗ/предлагаемый дизайн, не готовый мод и не внутриигровая проверка. Дата обзора публичных отзывов: 21 ноября 2026. Целевая версия Skyrim AE, SKSE runtime, платформа и зависимости пока должны быть закреплены до реализации.

## 1. Что показывают отзывы игроков — осторожный синтез

Отзывы не являются опросом и сильно зависят от патчей, билда игрока, сложности, первого акта/демо и установленного набора модов. У FoA встречаются и восторг, и резкая критика. Повторяющиеся «плюсы»: в сравнении с ванильным Skyrim — уклонение/рывок и timed parry поверх блокирования; более осознанный темп атак и расход выносливости; читаемые намерения врагов; более ощутимые попадания/стаггер; явная разница классов оружия; предметы с собственными эффектами и свобода гибридных билдов. Steam-обсуждения хвалят именно ощущение попадания, парирование, ритм «несколько ударов → защита/восстановление» и разнообразие оружия, но это отдельные мнения, не измеренная доля игроков ([Steam-положительный отзыв](https://steamcommunity.com/app/1466060/reviews/?browsefilter=toprated), [HowLongToBeat отзывы](https://howlongtobeat.com/game/93631/reviews/latest/1), [IGN](https://sea.ign.com/tainted-grail-the-fall-of-avalon/228541/review/tainted-grail-the-fall-of-avalon-review)).

Повторяющаяся критика FoA: неровная кривая сложности; ранние бои могут быть «губкой» по здоровью, а атаки врагов — чрезмерно болезненными; пустая выносливость иногда резко лишает игрока действий; поздние комбинации перков/оружия/магии могут ломать сложность; часть игроков считает анимации/контакт неотшлифованными, третье лицо и ranged особенно спорными. Даже в положительных обзорах есть предупреждения о балансе и jank. Поэтому переносим **намерение/ритм**, а не копируем числа и не копируем недостатки ([IGN](https://sea.ign.com/tainted-grail-the-fall-of-avalon/228541/review/tainted-grail-the-fall-of-avalon-review), [Steam-обсуждение баланса](https://steamcommunity.com/app/1466060/discussions/0/4416425017211918283/), [Steam-обсуждение melee](https://steamcommunity.com/app/1466060/discussions/0/591772149599011013/)).

Частые претензии к Skyrim combat в пользовательских обсуждениях: melee ощущается повторяющимся и «без веса», мало реакции на попадание и разнообразия действий; атака часто сводится к спаму/размену полосок здоровья; сложность может ощущаться как рост HP у врагов, а не новые решения; два оружия в руках дают урон, но лишают защиты/осмысленного ритма; магия на старте дорога, а позднее может хуже масштабироваться либо превращаться в спам; vanilla bow обычно признают рабочим/приятным, поэтому его не следует переделывать в чужой жанр. Это обобщение тем Reddit/форумов, а не консенсус всех игроков ([r/pcgaming](https://www.reddit.com/r/pcgaming/comments/5ai6eg/anyone_else_feel_like_skyrims_combat_kind_of/), [r/skyrim](https://www.reddit.com/r/skyrim/comments/1f1ityf/please_tell_me_i_am_not_the_only_who_thinks/), [r/patientgamers](https://www.reddit.com/r/patientgamers/comments/vx8ayb/skyrim_doesnt_click_with_me/), [r/skyrimmods](https://www.reddit.com/r/skyrimmods/comments/1bw0onh/nobody_plays_skyrim_for_the_combat_is_such_a/)).

**Вывод дизайна:** сделать Skyrim более тактическим и читаемым, но сохранить его первую-person открытость, простое управление, широкий билд/лут и работу с NPC. Не превращать весь Skyrim в Souls-клон; dodge и lock-on не навязывать как обязательное условие.

## 2. Рамки и состав переносимого каталога

Существующая книга содержит предметную основу: 107 одноручных, 90 двуручных, 8 fist weapons, 38 луков/великих луков, 28 стрел, 40 щитов и 56 записей заклинаний. Оружейных/боеприпасных записей всего 311; это не равно числу готовых, проверенных игровых предметов. Детальный построчный шаблон переноса лежит в книге на листах `Port_Weapon_Mapping` и `Port_Spell_Mapping`; исходные поля сохраняются, а неизвестное не угадывается.

В навыках исходный индекс содержит 191 запись Fextralife и дополнительно 47 отдельных wiki.gg WIP записей: всего 238 строк в `TG_All_Skills_Union`. Это не доказательство, что все 238 — игровые навыки одного базового набора. Для Sarras и прочих DLC — отдельный необязательный плагин после проверки права владения/версии. Спорные значения Fextralife/wiki.gg не сливать.

**Принцип данных:** у каждой записи — ID мода, тип, Skyrim keyword, исходный предмет/эффект, применённые величины, источник/патч, состояние проверки и fallback. Сырые FoA урон, SP, mana, armor rating и проценты в Skyrim не переносятся 1:1. Балансировать через TTK/DPS, цену выносливости/магии, сопротивления, лимиты Skyrim и тестовые уровни.

## 3. Боевая основа — что именно меняем

1. **Контакт оружия/удар:** точнее hit collision/reach, реакция NPC на удар, небольшой hit-stop/звук/вспышка и материал-зависимый recoil. Без тяжёлого постоянного screen-shake. При технической совместимости использовать Precision как необязательный/рекомендуемый SKSE слой, а не молча предполагать его наличие: он поддерживает Skyrim SE/AE, 1st/3rd person и физические коллизии/реакции ([Nexus: Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347)).
2. **Темп и commitment:** легкие атаки остаются отзывчивыми, силовые атаки имеют понятное окно восстановления. Редкие cancel/interrupt правила, без блокировки управления на долгие анимации. Отличать оружия скоростью, дальностью, stamina и poise damage, не только числом damage.
3. **Выносливость:** обычный удар не должен автоматически сделать персонажа беспомощным; power attack, блок тяжёлого удара, bash, удержание лука и dodge имеют цену. Успешный удар/kill/парирование могут дать ограниченное возмещение, но не полный бесконечный цикл. Регeneration начинает работать предсказуемо; «overexertion» — короткое, ясно показанное окно, не 10 секунд наказания. Ванильная экономика stamina иначе устроена, поэтому это новый слой.
4. **Блок и parry:** оставить обычный block; добавить узкое configurable timed-block окно, сниженное/нулевое урон-получение и ограниченный stagger/stamina у атакующего. Не дать повторно парировать bash, взрыв, драконьи широкие AOE и каждую непрерывную атаку. Показать звук/визуальный сигнал. Valhalla Combat уже имеет конфигурируемые timed/perfect blocks, projectile parry и stamina mechanics, но perfect-block с полной регенерацией может чрезмерно обесценить ресурсы — при использовании отключить/снизить возврат. Не запускать одновременно две системы stamina/poise ([Nexus: Valhalla Combat](https://www.nexusmods.com/skyrimspecialedition/mods/64741)).
5. **Dodge:** только отдельный optional компонент для PC/SKSE с выбранной и зафиксированной версией AE; не обещать vanilla/console поддержку. Лёгкий короткий evade с cooldown/ценой stamina, i-frames ограничены; большой длинный dash не становится обязательным способом играть. Без dodge-плагина остаются block, movement, timed block, spacing.
6. **Poise/stagger:** видимый/понятный игроку порог stagger для игрока и врага, stagger resistance у больших целей, diminishing returns на stun/knockdown. Тяжёлое оружие и удар в окно дают большую poise-силу. Нет бесконечного stagger-lock и гарантированного парирования любого босса.
7. **Враг/сложность:** телеграфирование вражеской силовой атаки и punish окна; большая угроза идёт от тайминга, дистанции и нескольких ролей противников, а не избыточных HP. Не переписывать AI широким конфликтным пакетом в MVP. Отдельно настраивать гуманоидов, зверей, драконов, магов, боссов.
8. **HUD/опции:** минимальная stamina/poise подсказка, настройка окна парирования, регенерации, stamina-cost, stagger и dodge; переключатели third-party hook. Не требовать обязательного lock-on.

## 4. Как меняются классы оружия и каждый предметный ряд

| Ряд каталога | Перенос в Skyrim | Целевое отличие/функция |
|---|---|---|
| **107 1H**: 39 swords, 14 axes, 20 blunt, 29 daggers, 5 polearms | Sword→sword; Axe→war axe; Blunt→mace; Dagger→отдельный keyword и быстрый moveset; Polearm→spear/halberd, отдельная reach/анимация. Если совместимая анимация отсутствует, spear deferred/опционален, не подменяется мечом. | Меч — средний баланс; war axe — кровотечение/броня только из подтвержденных эффектов; mace — poise/armor pressure; dagger — скорость, crit/backstab/status, но низкий stagger; spear — reach, узкий сектор/колющий профиль. Быстрый удар противнику не должен автоматически иметь бонус к скрытности без Sneak state.
| **90 2H**: 26 swords, 16 axes, 18 blunt, 30 polearms | Greatsword, battleaxe, warhammer; polearm как длинное оружие с отдельным типом/анимацией/радиусом. | Сильнее commitment, stamina и stagger; более медленные, читаемые силовые удары. AoE/cleave только для предметов/перков, где это оговорено. Для двухручного оружия block остаётся; parry может иметь меньшую ширину/более точное окно, но не исчезает.
| **8 fist weapons** | Предметы перчаток/кулаков + hidden weapon/keyword или enchantment; отдельные vanilla-friendly unarmed bonuses; атакующий профиль без видимого меча в руке. | Рабочий unarmed билд, комбо/стаггер/bleed/poison по источникам. Не заменять весь Skyrim hand-to-hand до отдельного тестирования; `Iron Fists` с неизвестным requirement заблокирован до сверки.
| **38 ranged**: 21 light bow, 3 medium, 1 heavy, 13 Greatbow | Все остаются луками; Greatbow — более тяжёлый archetype, не автоматически crossbow. Добавить keywords, draw speed/hold drain, дальность/weakspot и stagger. Новые анимации optional. | Light быстрый/низкая цена; medium базовый компромисс; heavy/Greatbow медленнее, больше stagger/damage/цена stamina и лучше на weakspot. Не переделывать Skyrim bow, который игроки часто считают сильной частью combat.
| **28 arrows** | Независимые ammo/projectile/effect records. Fire/frost/poison/bleed/wet/frailty/explosive/utility — собственные effects и сопротивления. | Крафт/лимиты/стоимость; не глобальный overwrite обычных стрел. `Death's Arrow` не убивает автоматически драконов/боссов/essential NPC: ограничить целями, сделать очень редкой или заменить на большой scripted/vanilla-safe execute effect. Сначала подтвердить данные источника.
| **40 shields** | Новые щиты или отдельные формы/keywords; armor/block rating под Skyrim; passives через enchant/ability. | Щит = защита и сигнатурный триггер (парирование, retaliation, ward, status) при наличии подтвержденного текста. Stamina price block; shield bash отдельная цена. Не переносить TG 95% armor cap: Skyrim имеет собственную механику физического mitigation/cap (обычно 80% в справке UESP); проект не должен незаметно ломать ванильные caps.
| **Wands / soul cubes** (отдельный item list нужно уточнять) | Wand/staff как spell delivery/focus, soul cube — отдельный артефакт/снаряжение/activation power при технической реализации. | Не делать wand визуально обычным заклинанием с бесконечной бесплатной атакой. Цена/каденс/блокирование off-hand должны быть ясны. Не завершать item coverage, пока конкретные исходные записи и эффекты не reconciled.

**Общий предметный шаблон:** Skyrim damage — по тестовому TTK, не по FoA damage; скорость/reach — отдельные значения; перки и weapon material keywords; базовое/силовое действие; stamina cost; тип урона/статус; заряд/активация; требования — Skyrim skill/perk gate, но без буквального импорта шести FoA атрибутов; rarity/loot; уникальный effect; resist/boss behavior; patch/DLC flag. Vanilla matching зависит от runtime и существующего combat/moveset стека.

### Имена из текущего оружейного каталога (инвентарь для поштучного маппинга)

**TG_1H (107):** Arcane Mace, Arcanist's Sword, Ashen Calamity, Bear's Claw, Bear's Paw, Bitter Sword, Blood Oath, Bloodseeker Blade, Bone Reaver, Breaker's Edge, Briar Star, Burning Axe, Chatterblade, Cleaveheart, Collateral Blade, Cooling Axe, Crackbone, Crimson Oracle Dagger, Crownbreaker, Culling Blade, Cursed Bloodblade, Dead Man's Vow, Doombringer's Hook, Drowner's Sword, Duelist's Blade, Duelist's Sword, Exploding Morgenstern, Eye of Tuathan, Flesh-Ripper, Frostblade, Frostfang Dagger, Frozen Sword, Glare, Guardian's Sword, Hand Knife, Honorary Blade, Honorary Blade (Shortsword), Ice Crusher, Inferno Blade, Iron Morningstar, Iron Tooth (Weapon), Keelgnawer, Kramer's Estoc, Leech Bite, Lover's Lament, Lucky Knife, Moonlit Vow, Odran's Trusty Hammer, Oldsteel Mace, Oldsteel Sword, Parrying Dagger, Parrying Sword, Poisoned Knight's Gladius, Quiet Death, Rage Blade, Ravencrest Executioner, Red Death-Infused Dagger, Remor Dagger, Riposte Sword, Ritual Blade, Royal Edict, Sanguine Blade, Sea Urchin, Shadow Blade, Shadowfang Reaver, Shard Of Fallen Sky, Silencer, Skull Knight's Blade Sword, Smuggler's Dagger, Smuggler's Knife, Soulsplitter, Spear of an Ancient Hero, Spine Splinter, Splitter Axe, Star Catcher, Steel Dagger, Sting, Stingray, Stormbringer, Sunfire Shortsword, Surgebite, Swift Dagger, Tainted Axe of Vengeance, Tainted Greataxe of the Four Afflictions, Tainted Staff of the Battle-Mage, The Final Cut, The Firm Hand, The Last Litany, The Mighty Herring, The Stag's Woe, The Thousand Stabs, Tidepiercer, Titanium Tonic, Twin-Tide Spear, Umbilical Scissors, Vanquisher Of Penny Buns, Verdant Adder, Void Sickle, Wavepiercer, Wayward Edge, Winding Dagger, Winged Battle Axe, Wolf's Fang, Wolf's Song, Cut-Off Hand, Giant's Grudge, Eclipse's Kiss.

**TG_2H (90):** Abyssal Trident, Anchor, Arondight, Azure Blade, Berserker's Axe, Big Bonk, Bloodsting, Bone Club, Broken Greatsword Of Living Ice, Butcher's Tool, Cascade Spear, Cranium Crusher, Crescent Cutter, Crescent Halberd, Crooked Scythe, Doombringer, Eclipse Shard, Excalibur, Executioner's Axe, Fields Of Misery, Frostmaw, Frozen Scythe, Galahad's Mace, Galahad's Mace (Act1), Galahad's Mace (Act3), Giant's Grudge (2H), Giant's Sword, Great Hammer, Great Hammer Of Stun, Greatsword Of Living Ice, Green Knight Axe, Halberd of the Depths, Hammer Of Stupefaction, Heartwrought, Hefty Spear (Bleed), Hefty Spear (Stun), Honorary Sword, Hullsplitter, Icetongue Spear, Isleward Spear, Kraken's Maw, Midnight Wolf Spear, Moonring Staff, Oath Of Famine, Ogre Bonk, Ogre's Grip, Oldsteel Warhammer, Parade Blade, Penance, Perceval's Left Blade, Perceval's Right Blade, Poisoned Knight's Greatsword, Processional Halberd, Reef Glaive, Scythe of the Archmage, Sower Of Plague, Spear Of The Unbroken, Spectral Sword, Spell Reaper, Spindle Spear, Spinebreaker, Starbearer, Starlight Serpent Lance, Stormcrow Oculus, Stormleap Axe, Surgecaller, Surgestopper, Sveinn Battleaxe, Tethered Spear, The Cold Sun, The Lightning, The Pale Justiciar, The Ram Of Nuada, The Unstoppable, Thornreach, Thornsword, Thrygve's Axe, Trial Of Death, Tristan's Pike, Turtar's Relics Staff, Twin Tempest, Ulfr Ceremonial Axe, Veinpiercer, Verdigris Blade, Widowmaker, Winged Spear, Wintersong Blade, Wither & Wane, Wolf's Howl, Wyrdshovel.

**TG_Fists (8):** Bitter Lesson, Brawler's Gloves, Charred Discipline, Patient Grasp, Still Touch, Unbroken Motion, Unburdened Hands, Iron Fists.

**TG_Ranged (38):** Anchor's Mark, Arcanist's Bow, Assasin's Bow, Burial at Sea, Cascade Deluge, Coralsting Bow, Deadeye Bow, Drevan's Bow, Forlorn Frostbitten Bow, Frostbloom Bow, Galahad Bow, Giant Bow, Gilded Adder, Harrowbend, Hexed Bow, Morrigan's Wings, Nightshade Bow, Orchard Warden, Perilous Dawn, Powerbow, Rime Bow, Robbie's Bow, Sanguine Sting, Siege Bow, Silent Ritual, Slack Tide, Spiteful Arch, Tainted Bow of Unraveling, The Dolorous Shot, The Green Chapel, The Pursuer, Thunderous Greatbow, Tidebreaker, Trailshot Bow, Triple Threat Bow, Volcanic Tree Bow, Whiteout, Wraithshot Bow.

**TG_Arrows (28):** Arrow of Freezing Death, Broken Arrow, Copper Arrow, Coral Arrow, Death's Arrow, Dousing Arrow, Explosive Magic Arrow, Fire Arrow, Frailty Arrow, Frost Arrow, Iron Arrow, Iron Fire Arrow, Iron Frost Arrow, Iron Poison Arrow, Iron Sharpened Arrow, Jump Arrow, Meteorite Arrow, Poison Arrow, Sharpened Arrow, Spectral Arrow, Titanium Arrow, Titanium Fire Arrow, Titanium Frost Arrow, Titanium Poison Arrow, Titanium Sharpened Arrow, Void Arrow, Wooden Arrow, Wyrdbonearrow.

**TG_Shields (40):** Aegis of Retribution, Balor's Bulwark, Block of Ice, Bloodtithe Shield, Bramble Shield, Brighid's Anvil, Brinewall, Crown of Thorns, Custodian's Shield, Duel Shield, Flameguard, Frost Aegis, Gilded Parade Shield, Howling Crest, Inlaid Shield, Leviathan Crest, Mirror Aegis, Ornate Tower Shield, Paragon's Guard, Radiant Shield, Riptide Ward, Scathach's Ward, Sellsword's Shield, Shield of Reflection, Shield of the White Wyrm, Steel Wall, Tainted Shield of Prophecy, The Ashen Vow, The Dancer's Crest, The Everfrost, The Iron Stag, The Smelter's Heart, The Stag's Defiance, The Waning Ward, The Worn Sentinel, The Wyrmguard, Tuathan Bulwark, Vanguard Shield, Waveguard, Wooden Shield.

Перечень в workbook содержит все строки и source data. Это **не утверждение**, что каждая строка готова к выпуску: поля `???`, malformed numeric text и source conflicts остаются блокирующими QA.

## 5. Как меняются заклинания

В `TG_Spells` сейчас 56 строк. FoA «Light cast» и «Heavy cast» — это две разные формы/профиля. В Skyrim не следует обещать перенос одного заклинания с тем же удержанием кнопки без отдельной input-системы: делаем два явных Skyrim spell records (быстрый/тяжёлый variant) либо только один вариант, если источник heavy отсутствует. Назначить разные тома/школы/стоимость; heavy выше по tier/цене/cast commitment и не просто множитель ×N. Переносить эффект, а не raw FoA damage/mana.

| FoA cast family | Skyrim реализация |
|---|---|
| Projectile | Destruction projectile; projectile speed/area/collision/elemental resist проверены. Heavy — отдельный более дорогой spell record. |
| Ray | Direct spell/ray или targeted debuff; отдельная концентрация, если именно channeled в исходнике. |
| Area of Effect | Радиус/направление/ground-target, cast-time и friendly-fire определены явно; не перекрывать всю комнату автоматически. |
| Channeled | Concentration only для реального drain/heal/beam; mana cost per second, interrupt и выход из каста тестируются. |
| Trap | Rune с arm-time, trigger radius и damage window. |
| Buff / Ward / Heal | Разделить Restoration / Alteration / Illusion по смыслу; duration, stack cap и dispel/resist указать явно. Одноразовый блок Arcane Aegis не должен стать бесконечным immunity. |
| Summon / order / salvage / explode | Conjuration summon с lifetime/actor level cap; target order / dismiss / salvage / explode — отдельные lesser powers/spells только если надежно работают на NPC и save/reload. Иначе оставить ванильный summon AI и убрать непереносимую микрокоманду. |
| Calm / control / polymorph / gravity | Illusion/Alteration; лимиты по level/resist/boss; «превратить врага в сыр» — уникальный high-risk эффект: отдельная форма/визуал или замена на временное обездвиживание/мощный debuff, если полное преобразование технически ненадёжно. |

**Баланс магии:** цена по шкале полезности, DPS/сек, duration, number of targets и ресурсу; ранги Destruction действительно должны усиливать урон по мере развития. Исключить двойное масштабирование от FoA effect + vanilla perk + enchantment. NPC получают ограниченный набор spell tiers по skill/role; новое заклинание игроку не выдаётся всем NPC без фильтра. SPID/SkyPatcher — необязательный distribute layer и отдельная compatibility матрица.

### Полный список 56 строк в исходном реестре: source cast → предлагаемое семейство Skyrim

| Spell | Light cast (исходник) | Heavy cast (исходник) | Перенос |
|---|---|---|---|
| Abhartach's Calling | Area of Effect: 30-35 Damage; 18 Mana | Channeled: 2 Damage; 6 Mana | Necromantic corpse-triggered area burst/bleed plus separate healing channel; corpse rules and friendly fire tested. |
| Absorption | Buff: -; 30 Mana | Channeled: 5 Mana; None | Alteration ward / Magicka-shield effect; timed and channeled modes are separate spells. |
| Arcane Aegis | Buff: 90-110 Damage; 150 Mana | -: -; - | Restoration/Alteration one-hit ward plus retaliation burst; single charge and explicit recharge/cooldown. |
| Banish | Ray: -; 20 Mana | Ray: -; 30 Mana | Illusion/Alteration control effect (targeted ray delivery); level/resist/boss limits, duration and summon exception are explicit. |
| Blazing Orb | Projectile: 8-10 Damage; 9 Mana | Projectile: ?? Damage; 18 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Blood Transfusion | Projectile: 3-4 Damage; 5 Mana | Channeled: 2 Health per second; 7 Mana per second | Projectile delivery with bleed + bounded life-drain/heal payload; school/effect split and exploit test. |
| Burning Ember | Projectile: 5 Damage; 5 Mana | Projectile: 18 Damage; 10 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Calm Animals | Buff: -; 30 Mana | Buff: -; 30 Mana | Illusion calm/friend effect with creature-level caps and faction/hostility checks. |
| Caustic Wrath | Projectile: 19-21 Damage; 36 Mana | Projectile: 69-77 Damage; 54 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Crippling Bolt | Ray: -; 25 Mana | Ray: -; 35 Mana Cost | Illusion/debuff ray: outgoing damage reduction or a separate crit-mark variant; do not code as raw damage. |
| Divine Grace | Buff: 50 Health; 100 Mana | Channeled: 8 Health; 24 Mana Cost | Restoration immediate healing; rebalance against vanilla healing and potion recovery. |
| Drain of Vigor | Projectile: 19-25 Damage; 30 Mana | Channeled: -; 27 Mana Cost | Targeted stamina drain / concentration variant; test NPC actor-value recovery and boss immunity. |
| Fire Rune | Trap: 66-72 Damage; 110 Mana | Trap: 198-216 Damage; 220 Mana Cost | Rune/ground trap spell; arming delay and trigger radius preserved, not a generic fire bolt. |
| Flame Torrent | Area of Effect: 18-20 Damage; 25 Mana | Channeled: 9 Damage; 13 Mana | Area delivery; choose school and resistances from payload. Preserve radius, charge-up and area/line targeting where feasible. |
| Flickerbolt | Ray: 5-11 Damage; 6 Mana | Ray: 7-16 Damage; 9 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Frost Needle | Projectile: 10-14 Damage; 14 Mana | Projectile: 36-51 Damage; 21 Mana Cost | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Ghost of Broc Meala | Ray: -; - | Summon: -; 75 Mana | Conjuration summon + command power; order command is separate from summoning. |
| Hailstone | Projectile: 4-6 Damage; 5 Mana | Projectile: 14-22 Damage; 10 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Hailstorm | Projectile: 17-23 Damage; 36 Mana | Projectile: 62-84 Damage; 54 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Impenetrable Shield | Buff: -; 25 Mana | -: -; - | Alteration / Restoration / Illusion according to semantics; duration, magnitude and stacking tagged separately. |
| Life Transfusion | Projectile: 7-9 Damage; 10 Health | Channeled: 6 Health per second; 15 Mana per second | Projectile delivery with bleed + bounded life-drain/heal payload; school/effect split and exploit test. |
| Merlin's Thorn | Projectile: 20-22 Damage; 21 Mana | Projectile: 63-69 Damage; 42 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Mother's Embrace | Buff: 30 Health; 50 Mana Cost | Channeled: 6 Health /s; 12 Mana Cost /s | Restoration instant and concentration heal variants; interrupt and heal-per-second caps. |
| Mystic Ward | Buff: 60-70 Damage; 100 Mana | None: None; None | Restoration/Alteration one-hit ward plus retaliation burst; single charge and explicit recharge/cooldown. |
| Nature's Caress | Buff: 10 Health; 20 Mana | Channeled: 2 Health/s; 5 Mana Cost/s | Restoration instant and concentration heal variants; interrupt and heal-per-second caps. |
| Poison Spray | Projectile: 18-20 Damage; 20 Mana | Channeled: 38 Damage/s; 13 Mana Cost/s | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Rumpolt's Revenge | Summon: -; 15 Health | Channeled: -; 8 Mana per second | Unique utility/transformation spell; if reliable model transformation is unavailable, replace with bounded polymorph/disable or omit, not a generic projectile. |
| Scorching Blaze | Projectile: 80-100 Damage; 90 Mana | Projectile: 96-120 Damage; 135 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Scorching Pathway | Area of Effect: 28 Damage; 56 Mana | Area of Effect: 42 Damage; 84 Mana | Area delivery; choose school and resistances from payload. Preserve radius, charge-up and area/line targeting where feasible. |
| Searing Cinder | Projectile: 20 Damage; 20 Mana | Projectile: 73 Damage; 54 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Severing Ray | Ray: 27-30 Damage; 40 Mana | Channeled: 54-60 Damage/s; 50 Mana Cost/s | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Skeleton Army | Ray: -; - | Summon: -; 30 Mana | Conjuration summon + command power; order command is separate from summoning. |
| Skeleton Knight | Ray: -; - | Summon: -; 60 Mana | Conjuration summon + command power; order command is separate from summoning. |
| Soul Salvage | Ray: -; - | Ray: -; 15 Mana | Conjuration dismissal/recall effect with capped resource return; no negative-cost exploit. |
| Stone Skin | Buff: -; 60 Mana | -: -; - | Alteration / Restoration / Illusion according to semantics; duration, magnitude and stacking tagged separately. |
| Stormpiercer | Ray: 28-40 Damage; 38 Mana | Ray: 42-60 Damage; 58 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Battlemage | Ray: -; - | Summon: -; 50 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Corpse Eater | Ray: -; - | Summon: -; 15 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Crystal Crawler | Ray: -; - | Summon: -; 40 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Grindylow | Ray: -; - | Summon: -; 20 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Kamelot Spearman | Ray: -; - | Summon: -; 60 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Keeper | Ray: -; - | Summon: -; 45 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Master Assassin | Ray: -; - | Summon: -; 50 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Redcap | Ray: -; - | Summon: -; 25 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Remor Warrior | Ray: -; - | Summon: -; 60 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Sir Galahad | Ray: -; - | Summon: -; 150 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Summon Sir Vast | Ray: -; - | Summon: -; 140 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Thunder Lash | Ray: 17-25 Damage; 19 Mana | Ray: 25-37 Damage; 29 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |
| Thunderstorm | Area of Effect: -; 45 Mana | Area of Effect: 45-52 Damage; - | Destruction ground-target multi-strike spell; telegraph, charge duration, target cap and mana cost tested. |
| Toxic Dart | Projectile: 11-13 Damage; 14 Mana | Projectile: 40-70 Damage; 21 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Venom Flick | Projectile: 5 Damage; 5 Mana | Projectile: 18 Damage; 10 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Vinelash | Area of Effect: 30-35 Damage; 45 Mana | Trap: 120-140 Damage; 135 Mana | Area delivery; choose school and resistances from payload. Preserve radius, charge-up and area/line targeting where feasible. |
| Volatile Summon | Ray: -; 15 Mana | Ray: -; 30 Mana | Conjuration summon-detonation effect; capped health-based burst, one use per summon. |
| Weightbound | Ray: -; 30 Mana | Channeled: -; 45+5 Mana | Alteration gravity/slow control with boss and actor-type limits. |
| Winter's Crone Wrath | Projectile: 15-20 Damage; 12 Mana | Projectile: 22-30 Damage; 18 Mana | Projectile is a delivery form, not a school assignment: choose Destruction/Restoration/Conjuration/etc. from payload. Light and heavy are distinct learned spell forms, not undocumented hold-button behavior. |
| Wolf's Call | Ray: -; - | Summon: -; 10 Mana | Ray is a delivery/targeting form, not a school assignment: damage, control, summon order and drain use the corresponding native school/effect. Concentration only where source says channeled. |

Все исходные эффекты/URLs и unresolved marker находятся в `Port_Spell_Mapping`. Наличие строки не означает, что оба режима/числа в игре подтверждены. В частности `Blazing Orb` heavy damage неизвестен; `Volatile Summon`, Rumpolt's Revenge, Banish, Weightbound, targeted summon commands и редкие utility требуют отдельного прототипа/решения.

## 6. Как меняются деревья и прогрессия

Skyrim уже имеет 18 native skill trees; UESP lists 180 unique perks / 251 ranked rows, а не 6 атрибутных деревьев FoA. Поэтому шесть атрибутов **не** импортируются как шесть новых полос базовых статов. Разбить FoA tree effects на игровые Skyrim perks, сохранить vanilla skills/skill books/trainers/level gates; вводить custom skill trees только для функций, которые нельзя выразить чисто существующими деревьями. Custom Skills Framework технически позволяет отдельные perk trees, но это отдельная SKSE/Address Library зависимость и должна быть опциональна или объявлена необходимой ([UESP Skills](https://en.uesp.net/wiki/Skyrim:Skills), [Custom Skills Framework](https://www.nexusmods.com/skyrimspecialedition/mods/41780)).

| FoA source stat/tree | Skyrim target | Переносимое ядро |
|---|---|---|
| Strength: One-/Two-Handed, Unarmed, General | One-Handed, Two-Handed, Block + optional Unarmed custom branch | archetype bonuses, heavy attack, sequential hits, paired weapons; unarmed отдельный путь. |
| Endurance: Shields, Stamina, Health | Block, Heavy/Light Armor + selected Restoration; optional resilience branch | shield stamina, timed-block rewards, capacity/recovery, kill/stagger regain; ограничить resource loops. |
| Dexterity: Parry, Attack Speed, Movement, Bows | Block, Archery, Sneak; optional Mobility/Parry | timed block; bow draw/weakspot/range; dodge remains optional framework. |
| Spirituality: Wands/Cubes, Summoning, Combat, buffs | Destruction, Conjuration, Restoration, Alteration, Illusion, Enchanting | spell schools, summons, wards, cost/power, mana-on-kill etc.; keep vanilla spell progression. |
| Perception: Stealth, Daggers, Critical Hits | Sneak, Archery, One-Handed + optional Critical/Dagger custom branch | backstab only under verified sneak state, weakspots, crits/status; guard against one-shot stacking. |
| Practicality: Healing, Crafting/Trading, Statuses, Armor | Alchemy, Smithing, Speech, Armor, Restoration | professions remain useful; status synergy and healing/crafting branches split. |
| Red Death | quest-gated late-game branch/custom tree | only after actual quest/story gate exists; not start-game free perk points. |
| Sarras Warrior/Rogue/Mystic | separate optional DLC patch/custom branches | 46 source-page entries recorded separately; WIP and DLC coverage must be verified before release. |

`Port_Progression_Map` gives full crosswalk. Design tree rank has 1–3 clear ranks, diminishing returns, stated prerequisites and no surprise irreversible trap. Respec policy: optional shrine/item or defined quest, not undocumented source-specific potion. Balance total available perk points against vanilla leveling so new perks do not force every build into maxed-out power.

## 7. Stats, statuses, gear and stealth (related systems)

- FoA's Strength/Dexterity/etc. soft-cap formula is research input only; Skyrim leveling uses Health/Magicka/Stamina increases and skill perks. Do not add six sliders by stealth.
- Bleed/burn/chill/poison/shock/wet/frailty/armor break become custom Magic Effects/keywords mapped to Skyrim damage types/resists. Define stacking, refresh, boss immune/shorter duration and player readability. Don't let 10 arrows stack uncapped DoT.
- Stamina/Health/Magicka-on-hit effects have per-trigger cooldowns/caps; kill resets and chain kills tested.
- Armor equipment requirements become optional perk/skill gates or simply reduce benefits while unmet; avoid invisible arbitrary penalties. Reuse Skyrim armor classes and its cap, do not import TG 95% armor cap as a new global rule.
- Stealth damage only applies while target has the correct unaware state. Once alerted, no persistent FoA stealth multiplier. Keep Vanilla bow feel and separate bow damage from sneak-only crit.
- Wands/cubes and armor relic set bonuses are separate future content if all source rows are reconciled; avoid bundling crafting/looting systems beyond combat MVP.

## 8. Proposed technical distribution and compatibility

**Recommended PC MVP:** plugin/ESL for new records, perks, spells, weapons/ammo/shields, recipes, loot; optionally SKSE layer for accurate collisions/poise/stamina input and MCM. Lock exact AE runtime and SKSE version per release. A custom dodge plugin/animation package is optional, not a silent dependency. Precision, Valhalla, custom skills framework, SPID and SkyPatcher each overlap certain features: we must either integrate with one chosen implementation or supply clean patch/toggle configurations, never run duplicate stamina/poise/dodge systems. Current Nexus information shows modern compatibility is runtime-specific and mods can need exact updated DLLs (examples: [Precision AE builds](https://www.nexusmods.com/skyrimspecialedition/mods/189664), [Valhalla Combat update](https://www.nexusmods.com/skyrimspecialedition/mods/191704)).

**Do not promise:** console/vanilla-game compatibility for SKSE dodge/hit collision; every AE runtime working; all NPC overhauls working without patches; controller/VR compatibility until tested; complete FoA art/animation replication. Use original/permission-cleared assets; do not redistribute FoA proprietary meshes, textures, audio or code. For external mod prerequisites, publish a clear alternative/patch matrix and avoid depending on unstable post-load DLL without known runtime support.

## 9. Phased implementation and acceptance tests

1. **Freeze target profile.** Record exact Steam runtime, SKSE, DLC/CC ownership, supported combat stack, first-person priority, controller and plugin-light budget.
2. **Feel prototype with vanilla weapons first.** Arena test 1H sword/axe/mace/dagger, 2H families, shield, dual-wield, unarmed, bow, magic; capture collision, stagger, stamina and input latency. No custom loot yet.
3. **Combat MVP:** timed block, reasonable stamina, telegraphs, clear hits/stagger, optional dodge, boss exceptions. Acceptance: no infinite parry, no forced dodge, no stun-lock, good 1st person; combat remains playable without optional dodge module.
4. **Weapon families.** Add dagger / fist / greatbow / spear distinct identity; map all records in `Port_Weapon_Mapping`. Each item receives unique effect only if source is verified. Test 1H vs 2H DPS, stamina/kill, NPC equipment, smithing and loot.
5. **Spells.** Add spell families and explicit cast variants; source fields survive. Test player and NPC costs, spells versus resistances, concentration interruption, summons saved/reloaded and dismissed, boss immunity, healing/exploit loops. Every spell needs tooltip, tome placement and rank gate.
6. **Perks.** Implement native-tree mapping and optional custom trees. Acceptance: each translated effect has exactly one implementation owner, caps, prerequisites, tooltip and no accidental multiplication through vanilla perk stacking.
7. **Loot/DLC/NPC.** Leveled lists and vendors; Sarras and Weapon Pack are clearly optional; NPC distribution disabled by default or carefully filtered. Patch compatibility with known overhauls.
8. **Balance/QA/release.** Test representative character tiers and difficulties; measure time-to-kill, incoming damage, stamina recovery, block consumption, spell cost, sneak damage, arrows; run save/reload, new game, upgrade, respec, uninstall tests. Publish unresolved items and source disagreements. Only then call a mechanic implemented.

Release gates: unresolved `???` fields and unverified Wiki.gg WIP rank sequences do not become mechanics; no mismatched source is silently selected; test results are tied to exact runtime and modlist. Add regression profiles: light dagger, shield/mace, two-hand, unarmed, archer, caster/summoner, stealth hybrid.

## 10. Open decisions that must be fixed before development

1. PC SKSE mod or vanilla-only plugin? Vanilla-only cannot safely promise the full responsive collision/dodge/parry redesign.
2. Exact AE runtime/Steam executable version and supported runtime list?
3. Which combat foundations are dependencies versus optional compat patches (Precision / Valhalla / dodge framework / animations)? Avoid double-owning mechanics.
4. First-person only for MVP, or first- and third-person parity requirement?
5. Keep Dragonborn/CC optional or mandatory? Is Sanctuary of Sarras included? Which source catalog rows actually belong to installed DLC?
6. Custom Skills Framework dependency acceptable, or fit translated skills into vanilla trees only?
7. Standalone new items and spell content or rebalance replacements for existing Skyrim items too? Current proposal favors additions and minimal vanilla edits.
8. How harsh should death/stamina economy be? Starting preset is fair: timed defense matters but weak players are not helpless after one mistake.

## Источники отзывов и механик

- [Steam FoA top-rated review page](https://steamcommunity.com/app/1466060/reviews/?browsefilter=toprated) — отзывы о dodge/parry, stamina, weapon build diversity; мнения разнятся.
- [IGN FoA review](https://sea.ign.com/tainted-grail-the-fall-of-avalon/228541/review/tainted-grail-the-fall-of-avalon-review) — сообщает об удачных first-person melee ощущениях вместе с проблемами balance/health-sponge.
- [Steam: combat balance issue](https://steamcommunity.com/app/1466060/discussions/0/4416425017211918283/) и [Steam: melee is clunky](https://steamcommunity.com/app/1466060/discussions/0/591772149599011013/) — противоположные/смешанные пользовательские оценки FoA.
- [Reddit: Skyrim melee/magic complaints](https://www.reddit.com/r/pcgaming/comments/5ai6eg/anyone_else_feel_like_skyrims_combat_kind_of/), [r/skyrim hit feedback discussion](https://www.reddit.com/r/skyrim/comments/1f1ityf/please_tell_me_i_am_not_the_only_who_thinks/), [r/patientgamers](https://www.reddit.com/r/patientgamers/comments/vx8ayb/skyrim_doesnt_click_with_me/) — повторяющиеся, но не репрезентативные отзывы.
- [FoA wiki.gg Skills](https://taintedgrail.wiki.gg/wiki/Skills) — полный массив деревьев; WIP, не game-file verification.
- [Fextralife FoA Weapons](https://taintedgrailfoa.wiki.fextralife.com/Weapons) — типы оружия и требования; содержит source placeholders.
- [UESP Skyrim Skills](https://en.uesp.net/wiki/Skyrim:Skills) — 18 vanilla skills/trees and perk structure.
- [Nexus Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347), [Nexus Valhalla Combat](https://www.nexusmods.com/skyrimspecialedition/mods/64741), [Nexus Custom Skills Framework](https://www.nexusmods.com/skyrimspecialedition/mods/41780) — ориентиры/возможные зависимости, не утверждение о совместимости с любым AE runtime.


## Дополнение: противники Skyrim

Отдельная матрица вдохновений FoA и предложений для 42 архетипов противников (включая bandits, guards, пауков, животных, undead, DLC/CC и dragons) находится в [tainted-grail-skyrim-enemy-port-plan.md](tainted-grail-skyrim-enemy-port-plan.md), а также в листах `FoA_Enemy_Inspirations` и `SK_Enemy_Port_Plan` книги. Это дизайн, не реализованные способности. Уточнение: Frostbite Spiders Skyrim уже прыгают и плюются ядом; новое предложение — более читаемая подготовка и sticky-web slow, а не дублирование существующих атак.


## Принцип player-perceived enemy distinction

Игрок уточнил критерий: существующее движение не считается механикой, если реальный бой всё равно ощущается как бой с волком/бандитом. Поэтому к 42 строкам `SK_Enemy_Port_Plan` добавлен отдельный forced-response столбец: что игрок должен сделать иначе. Обязательный blind-playtest gate и требования к tell, двум counter options, recovery и cooldown записаны в enemy design brief. Это всё ещё план, не реализованный мод.


Построчная enemy moveset/game-design спецификация v1.0: [skyrim_enemy_combat_design_spec.md](skyrim_enemy_combat_design_spec.md).
