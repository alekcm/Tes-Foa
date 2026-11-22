# Реестр допущений (Assumptions Register)

Правило проекта: **всё, что не подтверждено источником [A]/[B], живёт здесь с номером.**
В коде такие места помечены комментарием `AS-n`. Проверка автоматическая:
`python tools/verify_audit.py` падает, если в коде есть `AS-n`, которого нет в этой таблице,
или если у записи нет строки «Проверяется».

Статусы: `открыто` — не проверено в игре; `подтверждено` — есть лог; `опровергнуто` — есть лог.

| ID | Допущение | Почему принято | Проверяется (строка лога) | Статус |
|---|---|---|---|---|
| AS-1 | Стаггер, наложенный на игрока через спелл с архетипом Stagger, **снимает у него блок** | в движке нет сеттера блока; UESP: «Staggering interrupts whatever action was being taken by the target»; но UESP помечает поведение у игрока «verification needed» | `block probe: … blocking=false` — подтверждено; `blocking=true` — опровергнуто | открыто |
| AS-2 | `magnitude = 1.0` даёт заметный стаггер (не «ничего» и не рагдолл) | шкала magnitude 0..1 по CK Wiki; моддеры сообщают, что «стоячие» значения на игрока не действуют | `block probe: … staggered=true` + визуально | открыто |
| AS-3 | `bashStop` — конец замаха баша, подходящий момент для эффекта | `bashStart` слушателям не приходит (лог 0.6.2), `bashStop` приходит | интервал `bash ordered` → `bash finished` = 0,1–0,6 с | открыто |
| AS-4 | `CastSpellImmediate` на `kInstant`-кастере NPC не требует анимации каста и не тормозит бой | сигнатура `MagicCaster::CastSpellImmediate` (VTABLE 01) | `stagger cast:` появляется без вылета и без пауз в бою | открыто |
| AS-5 | `TESHitEvent` приходит на каждое попадание по игроку и несёт флаги `kHitBlocked` / `kPowerAttack` / `kBashAttack` | перечисление `RE/T/TESHitEvent.h`; регистрация через `ScriptEventSourceHolder` | `hit без эффекта: … blocked=…` и `stagger cast: … (попадание)` | открыто |
| AS-6 | `ActorState::IsWeaponDrawn()` = «оружие и щит в руках»; при `false` баш проигрывается кулаком | метод есть в заголовках; семантика по имени | `bash finished: … shield=true` при вынутом оружии | открыто |
| AS-7 | `Actor::IsStaggering()` возвращает `true` именно в момент стаггера у игрока | метод есть в заголовках; поведение не документировано | `block probe: … staggered=true` | открыто |
| AS-8 | Конец заказа баша (`bashStop` при `dist ≤ fCombatBashReach`) визуально совпадает с моментом «щит достал игрока» | геометрия анимации не документирована | визуально + `bash finished: … dist=…` | открыто |
| AS-9 | `GameSettingCollection::GetSetting("fCombatBashReach")` отдаёт значение в рантайме | метод есть в заголовках; значение 141 документировано | строка `fCombatBashReach = 141 (дистанция баша)` | открыто |

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
