# Реестр допущений (Assumptions Register)

Правило проекта: **всё, что не подтверждено источником [A]/[B], живёт здесь с номером.**
В коде такие места помечены комментарием `AS-n`. Проверка автоматическая:
`python tools/verify_audit.py` падает, если в коде есть `AS-n`, которого нет в этой таблице,
или если у записи нет строки «Проверяется».

Статусы: `открыто` — не проверено в игре; `подтверждено` — есть лог; `опровергнуто` — есть лог.

| ID | Допущение | Почему принято | Проверяется (строка лога) | Статус |
|---|---|---|---|---|
| AS-1 | Стаггер, наложенный на игрока через спелл с архетипом Stagger, **снимает у него блок** | в движке нет сеттера блока; UESP: «Staggering interrupts whatever action was being taken by the target»; но UESP помечает поведение у игрока «verification needed» | лог 0.7.1 (`TGCombatPort-10.log`): `block probe … blocking=true staggered=true` — **не снимает** | опровергнуто |
| AS-2 | `magnitude = 1.0` даёт заметный стаггер (не «ничего» и не рагдолл) | шкала magnitude 0..1 по CK Wiki; моддеры сообщают, что «стоячие» значения на игрока не действуют | `staggered=true` в пробах + подтверждение пользователя, что стаггер чувствуется | подтверждено |
| AS-3 | `bashStop` — конец замаха баша, подходящий момент для эффекта | `bashStart` слушателям не приходит (лог 0.6.2), `bashStop` приходит | лог 0.7.1: `bash finished:` приходит в пределах окна, после него идёт `stagger cast` | подтверждено |
| AS-4 | `CastSpellImmediate` на `kInstant`-кастере NPC не требует анимации каста и не тормозит бой | сигнатура `MagicCaster::CastSpellImmediate` (VTABLE 01) | `stagger cast:` появляется без вылета и без пауз в бою | открыто |
| AS-5 | `TESHitEvent` приходит на каждое попадание по игроку и несёт флаги `kHitBlocked` / `kPowerAttack` / `kBashAttack` | перечисление `RE/T/TESHitEvent.h`; регистрация через `ScriptEventSourceHolder` | лог 0.7.1: `hit sink registered (TESHitEvent)` + `hit без эффекта: … blocked=true … power=true` | подтверждено |
| AS-6 | `ActorState::IsWeaponDrawn()` = «оружие и щит в руках»; при `false` баш проигрывается кулаком | метод есть в заголовках; семантика по имени | `bash finished: … shield=true` при вынутом оружии | открыто |
| AS-7 | `Actor::IsStaggering()` возвращает `true` именно в момент стаггера у игрока | метод есть в заголовках; поведение не документировано | лог 0.7.1: `staggered=true` в первых пробах, `false` позже | подтверждено |
| AS-8 | Конец заказа баша (`bashStop` при `dist ≤ fCombatBashReach`) визуально совпадает с моментом «щит достал игрока» | геометрия анимации не документирована | визуально + `bash finished: … dist=…` | открыто |
| AS-9 | `GameSettingCollection::GetSetting("fCombatBashReach")` отдаёт значение в рантайме | метод есть в заголовках; значение 141 документировано | лог 0.7.1: `fCombatBashReach = 128 (дистанция баша)` — значение пришло из игры (у пользователя его меняет другой мод) | подтверждено |
| AS-12 | вид атаки выводится из формы в `TESHitEvent::source`: оружие → `melee`/`ranged` по `WEAPON_TYPE`, заклинание/свиток → `spell`, крик → `shout`, снаряд → `ranged`, иначе `unarmed` | поля события документированы в `RE/T/TESHitEvent.h`, но какая именно форма попадает в `source` при ударе существа, заклинании и дыхании — не документировано | строка `hit: … kind=… source=… projectile=…` для удара медведя, заклинания и дыхания дракона | открыто |
| AS-13 | `TESForm::LookupByEditorID` находит формы по EditorID из загруженных плагинов, а `TESDataHandler::LookupFormID` — по записи «плагин + 0xID» | обе функции объявлены в заголовках (`T/TESForm.h`, `T/TESDataHandler.h`) | при ошибке в имени в логе будет `Rules.txt: форма «…» не найдена`; при верном — правило загрузится без пометки | открыто |
| AS-11 | файл правил лежит в `Data/SKSE/Plugins/TGCombatPort/Rules.txt` относительно **рабочего каталога** игры | своего API «где папка Data» в CommonLib нет; Skyrim запускается со своего корня, поэтому cwd = корень игры | `Rules.txt [<путь>]: правил 2` — путь существует; `Rules.txt не найден [...]` — нет | открыто |
| AS-10 | `NotifyAnimationGraph("blockStop")` на игроке во время стаггера **гасит блок** и он не возвращается, пока игрок стаггерится | своего API для блока в движке нет (`tools/verify_api.py`), а состояние блока живёт в графе анимаций; `blockStop` — событие того же графа | лог 0.7.2 (`TGCombatPort-11.log`): вне стаггера `sent=true` и блок слетает, но **в стаггере `sent=false`** — событие не принимается, блок возвращается | опровергнуто |
| AS-15 | «блок магией» читается как `GetActorValue(kWardPower) > 0`: вард-спелл (Малый Оберег и др.) выставляет эту величину, пока держится | в заголовках есть `kWardPower = 63` и `kWardDeflection = 67` (`RE/A/ActorValues.h`), а также звуки `kWardBreak/absorb/deflect` в `BGSDefaultObjectManager` **[A]**; существование вард-спеллов подтверждено пользователем и SKSE-плагинами `Dynamic-Wards`, `WardAnytime` **[A/B]** | лог: `hit: … вард=<число>` больше нуля при поднятом обереге и `0` без него | открыто |
| AS-16 | урон сквозь блок задаётся числом в правиле (`chip = N`), потому что `TESHitEvent` не сообщает величину урона | в `RE/T/TESHitEvent.h` нет поля урона **[A]**; `DamageActorValue` есть в `RE/A/ActorValueOwner.h` **[A]** | `chip: 10.0 hp сквозь блок (…)` + здоровье игрока уменьшилось при удержании блока | открыто |
| AS-20 | двуручный топор и двуручный молот различаются кейвордами `WeapTypeBattleaxe` и `WeapTypeWarhammer`, а не типом движка (у обоих `kTwoHandAxe = 6`) | `RE/T/TESObjectWEAP.h:44-57` — тип один на оба; `TESObjectWEAP::BGSKeywordForm` (`TESObjectWEAP.h:74`) с `HasKeywordID` (`BGSKeywordForm.h:36`); кейворды используют ванильные перки (Warmaster: `WeapTypeBattleaxe`/`WeapTypeGreatsword`/`WeapTypeWarhammer`) и USKP **[B]** | сравнение идёт через `BGSKeywordForm::HasKeywordString` в бою (0.14.1: `LookupByEditorID` на загрузке кейворд не нашёл, лог 0.14.0 — «не понял attacker.weapon = warhammer»); строка `hit: оружие … кейворды:` печатает реальные editor ID оружия; в бою по молоту `rule fired`, по топору — нет | открыто |
| AS-14 | `NotifyAnimationGraph("staggerStart")` переводит игрока в собственное состояние стаггера: прерывает блок, не даёт поднять его заново, не наносит урона и не рисует эффектов | это то состояние, которым движок сам прерывает действия; спелл-стаггер дал урон и разлом на земле (лог 0.7.2) | `stagger anim: staggerStart sent=true` + проба `blocking=false` без спелла в логе | открыто |

## Что НЕ является допущением (проверено источником)

| Утверждение | Источник |
|---|---|
| дистанция баша в движке 141 игровая единица | `fCombatBashReach = 141`, CK Wiki GameSetting **[B]** |
| стаггер прерывает текущее действие (в т.ч. блок) | UESP Skyrim:Combat, раздел Staggering **[B]** |
| блок против силовой атаки работает на 66 % и не исчезает | `fBlockPowerAttackMult = 0.66`, UESP Skyrim:Block **[B]** |
| архетип Stagger: magnitude 0..1, duration — это пауза до следующего стаггера | CK Wiki Magic Effect **[B]** |
| баш даёт лишь шанс стаггера, а не гарантированное снятие блока | UESP Skyrim:Block#Bashing **[B]** |
| «Power Attack Blocking Mult … and thus, break a block» — сбивание блока происходит через стаггер | CK Wiki Combat Style **[B]** |
| `bashStart` слушателям не приходит, `bashStop` приходит | лог 0.6.2, [`measured-in-game.md`](measured-in-game.md) **[A]** |
| анимационное событие не создаёт попадания (`HitData` не появляется) | лог 0.6.2 + [`attacks-and-hits.md`](attacks-and-hits.md) **[A]/[B]** |
| `Actor::SetStaggered` / `DoStagger` не существуют | `tools/verify_api.py` по заголовкам **[A]** |
| `KnockAreaEffect` есть в Papyrus, но не обёрнут в CommonLib | CK Wiki + `TESCondition` 685 **[B]/[A]** |
