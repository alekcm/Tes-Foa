# Аудит кода: чем проверена каждая функция

Правило: **ни одна функция не попадает в релиз, если для неё нет строки в этой таблице.**
Автоматическая проверка полноты: `python tools/verify_audit.py`
(падает, если в `native/src/*.cpp` появилась функция, которой здесь нет, или если в коде есть метка
`AS-n`, отсутствующая в [`ASSUMPTIONS.md`](ASSUMPTIONS.md)).

Что чем проверяется:

| Вид утверждения в коде | Чем проверяется |
|---|---|
| символы `RE::…` и имена вызываемых методов существуют в движке | `python tools/verify_api.py --methods` по закреплённым заголовкам (`48b17e1d`) |
| поведение движка (баш, блок, стаггер, попадание) | [`wiki/`](wiki/README.md) — скопированные страницы UESP/CK Wiki, [`block-bash-stagger.md`](block-bash-stagger.md) |
| числа движка (дистанции, шансы, стаггер) | [`game-settings.md`](game-settings.md), CK Wiki `GameSetting` |
| то, что нельзя узнать из документации | [`ASSUMPTIONS.md`](ASSUMPTIONS.md), метка `AS-n` в коде, проверка строкой лога |
| наша собственная практика | [`measured-in-game.md`](measured-in-game.md) |

## Функции `native/src/main.cpp`

| Функция | Откуда взято поведение | Допущения | Проверка в игре |
|---|---|---|---|
| `SKSE_PLUGIN_LOAD` | стандартная точка входа SKSE; версия рантайма логируется | — | `TGCombatPort v0-X-Y-Z` первой строкой лога **[A]** |
| `RegisterPapyrusFunctions` | `IVirtualMachine::RegisterFunction`, имена совпадают с `TGCP_Native.psc` | — | `Papyrus functions registered: …` **[A]** |
| `HasShield` | щит = `TESObjectARMO::IsShield()` в экипированной руке; UESP: блокировать можно щитом/оружием **[B]** | — | `diag: … shield=True/False` **[A]** |
| `GetHeavyBlunt` | `kOneHandMace`, `kTwoHandAxe` (`WEAPON_TYPE`); UESP: двуручный топор и молот — один тип **[B]** | — | `power window opened: actor=… weapon=Стальная булава` **[A]** |
| `GetBashEventNames` | `TESRace::attackDataMap` + флаг `kBashAttack`; имена атак из данных расы **[A]** | — | `race attack data: … bash-событий: 2`, `atkd: bashStart bash=true 0.50` **[A]** |
| `GetCombatStyle` | `TESNPC::GetCombatStyle()`; поля CSTY по CK Wiki **[B]** | — | `power window opened: … style=…` **[A]** |
| `CooldownPassed` | наша логика (дизайн: кулдаун 7 с на NPC) | — | окна ровно раз в 7 с **[A]** |
| `OrderBash` | `Actor::NotifyAnimationGraph` — только анимация; CK Wiki: решения AI не управляемы **[B]** | `AS-6` (вынутое оружие), `AS-3` (момент конца) | `bash ordered: … sent=true` **[A]**; попадания нет **[A]** |
| `OpenPowerWindow` | `CombatStyle::meleeData.powerAttackBlockingMult`; CK Wiki: «more likely … power attack … and thus, break a block» **[B]** | — | `power window opened/closed` **[A]** |
| `CloseExpiredBuffs` | возврат исходного значения множителя | — | `power window closed: … powAtkBlock -> 1.00` **[A]** |
| `CloseExpiredWatches` | снятие слушателя через 3 с | — | — |
| `ConsiderActor` | правила дизайна 22.11.2026 (щит/без щита игрока) | — | `skip: actor=… без щита` **[A]** |
| `LogDiagnostics` | самодиагностика раз в 3 с | — | `diag: blocking=… hostiles=…` **[A]** |
| `InitStaggerSpell` | `MagicItem::HasEffect(EffectArchetype::kStagger)`; CK Wiki: архетип Stagger **[B]** | — | `stagger spell: …`, `stagger: выбран спелл …` **[A]** |
| `ApplyStagger` | `MagicCaster::CastSpellImmediate` (VTABLE 01), magnitude override **[A]** | `AS-1`, `AS-2`, `AS-4` | `stagger cast: … mag=1.00` — **ждём лог** |
| `StaggerOnCooldown` | наша логика (4 с) | — | — |
| `ProcessBlockProbes` | `Actor::IsBlocking()`, `Actor::IsStaggering()` **[A]** | `AS-7` | `block probe: … blocking=… staggered=…` — **ждём лог** |
| `OnBashFinished` | UESP: стаггер прерывает текущее действие **[B]** | `AS-3`, `AS-8` | `bash finished: … → без эффекта` или `stagger cast` |
| `OnBashLanded` | лог 0.6.2: `bashStart` не приходит **[A]** | — | `bandit anim: bashStart …` (ожидается редко) |
| `OnPowerAttackStarted` | UESP: стаггер от силовой **[B]**; имена событий из списка animation variables **[C]** | `AS-5` | `power attack start: … event=…` |
| `RefreshPowerWatches` | подписка/снятие слушателя по дистанции `fCombatBashReach`+60 **[B]** | `AS-5` | `power watch: следим за силовыми атаками …` |
| `IsWatchedBash` | наша логика (только свои заказы) | — | — |
| `PapyrusIsBlocking` / `PapyrusIsBlockingWithShield` | `IsBlocking()` + щит **[A]** | — | вызываются скриптом каждый тик |
| `PapyrusTick` | точка входа из `TGCP_PlayerBlockWatcher` | — | `ticks=…` в `GetStats` **[A]** |
| `PapyrusSetEnabled` | переключатель без перезапуска | — | `enabled=true/false` |
| `PapyrusGetStats` | счётчики | — | `GetStats()` из консоли/скрипта |
| `BashWatchSink::ProcessEvent` | `BSAnimationGraphEvent.tag`; `holder` — `TESObjectREFR*` **[A]** | `AS-3`, `AS-5` | `bandit anim: bashStop`, `power attack start:` |

## Числа в коде и их источник

| Значение | Где | Источник |
|---|---|---|
| `kMeleeRadius = 200.f` | радиус внимания | дизайн (docs/skyrim_enemy_combat_design_spec.md) |
| `kCooldownSeconds = 7.f` | кулдаун баша | дизайн («bash cooldown 7 s») |
| `kPowerWindowSeconds = 2.5f`, `kPowerAttackBlockingBoost = 10.f` | окно множителя | дизайн; кратность 10 — подобрана, влияние проверено логом 0.2.1 (не сработало) **[A]** |
| `kStaggerReach = 140.f` | порог «баш достаёт» | `fCombatBashReach = 141` **[B]** |
| `kPowerReach = 160.f` | порог силовой | длина оружия + запас; проверяется дистанцией в логе **[D]** (в реестре не значится как допущение, т.к. влияет только на фильтр) |
| `kStaggerMagnitude = 1.0f` | сила стаггера | CK Wiki: 0..1 **[B]**; выбор 1.0 — `AS-2` |
| `kStaggerCooldown = 4.f` | пауза между стаггерами | дизайн (≈1 с стаггер + запас) |
| `0.4f` (пауза между заказами), `12` неудач | защита от спама графа | наша логика, проверено логом 0.5.0–0.6.2 **[A]** |
| `3.0f` (окно слушателя), `0.15/0.45/0.9/1.4` с (пробы) | замер стаггера | наша логика; интервалы выбраны под UESP `fStaggerMaxDuration = 3` и ~1 с стаггер **[C]** |
