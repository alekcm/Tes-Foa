# Аудит кода: чем проверена каждая функция

Правило: **ни одна функция не попадает в релиз, если для неё нет строки в этой таблице.**
Автоматическая проверка полноты: `python tools/verify_audit.py`
(падает, если в `native/src/*.cpp` появилась функция, которой здесь нет, или если в коде есть метка
`AS-n`, отсутствующая в [`ASSUMPTIONS.md`](ASSUMPTIONS.md)).

Что чем проверяется:

| Вид утверждения в коде | Чем проверяется |
|---|---|
| символы `RE::…` и имена вызываемых методов существуют в движке | `python tools/verify_api.py --methods` по закреплённым заголовкам (`48b17e1d`) |
| поведение движка (баш, блок, стаггер, попадание) | [`wiki/`](wiki/README.md) — скопированные страницы UESP/CK Wiki |
| числа движка (дистанции, шансы, стаггер) | [`game-settings.md`](game-settings.md), CK Wiki `GameSetting` |
| то, что нельзя узнать из документации | [`ASSUMPTIONS.md`](ASSUMPTIONS.md), метка `AS-n` в коде, проверка строкой лога |
| наша собственная практика | [`measured-in-game.md`](measured-in-game.md) |

## Функции `native/src/main.cpp` (версия 0.7.1)

| Функция | Откуда взято поведение | Допущения | Проверка в игре |
|---|---|---|---|
| `SKSE_PLUGIN_LOAD` | стандартная точка входа SKSE; версия рантайма логируется | — | `TGCombatPort v0-7-1-0` первой строкой **[A]** |
| `RegisterPapyrusFunctions` | `IVirtualMachine::RegisterFunction`, имена совпадают с `TGCP_Native.psc` | — | `Papyrus functions registered: …` **[A]** |
| `HasShield` | щит = `TESObjectARMO::IsShield()` в экипированной руке; UESP: блокировать можно щитом/оружием **[B]** | — | `diag: … shield=True/False` **[A]** |
| `GetHeavyBlunt` | `kOneHandMace`, `kTwoHandAxe` (`WEAPON_TYPE`); двуручный топор отдельного типа не имеет **[B]** | — | `hit без эффекта: … оружие=Стальная булава` |
| `GetBashEventNames` | `TESRace::attackDataMap` + флаг `kBashAttack` (структура `AttackData`) **[A]** | — | `race attack data: … bash-событий: 2` **[A]** |
| `IsWatchedBash` | только свои заказы (иначе вмешивались бы в ванильный AI) | — | — |
| `InitStaggerSpell` | `MagicItem::HasEffect(EffectArchetype::kStagger)`; CK Wiki: архетип и смысл magnitude/duration **[B]** | — | `stagger spell: …`, `stagger: выбран спелл …` |
| `StaggerOnCooldown` | наша логика (4 с) | — | — |
| `ApplyStagger` | `MagicCaster::CastSpellImmediate` (VTABLE 01); `MagicSystem::CastingSource::kInstant` **[A]** | `AS-1`, `AS-2`, `AS-4` | `stagger cast: … mag=1.00` |
| `ProcessBlockProbes` | `Actor::IsBlocking()`, `Actor::IsStaggering()` **[A]** | `AS-7` | `block probe: … blocking=… staggered=…` |
| `ReadEngineSettings` | `GameSettingCollection::GetSetting`, `Setting::GetFloat` **[A]**; `fCombatBashReach = 141` **[B]** | `AS-9` | `fCombatBashReach = 141 (дистанция баша)` |
| `HitSink::ProcessEvent` | `BSTEventSink<TESHitEvent>` + `ScriptEventSourceHolder::AddEventSink` **[A]** | `AS-5` | `hit sink registered (TESHitEvent)` |
| `OnPlayerHit` | флаги `kHitBlocked`/`kPowerAttack`/`kBashAttack` из `RE/T/TESHitEvent.h`; правила 1–3 | `AS-5` | `stagger cast: … (попадание)` или `hit без эффекта: …` |
| `OnBashFinished` | UESP: стаггер прерывает действие **[B]**; лог 0.6.2: bashStop приходит **[A]** | `AS-3`, `AS-8` | `bash finished: … → без эффекта` |
| `OnBashLanded` | лог 0.6.2: `bashStart` не приходит **[A]** | — | `bandit anim: bashStart …` (ожидается редко) |
| `ProcessPendingStaggers` | отложенный стаггер на 0,25 с от замаха | `AS-3` | `stagger cast: баш щитоносца (заказ)` |
| `CloseExpiredWatches` | снятие слушателя через 3 с | — | — |
| `OrderBash` | `Actor::NotifyAnimationGraph` — только анимация **[B]** | `AS-6` | `bash ordered: … sent=true` **[A]** |
| `LogDiagnostics` | самодиагностика раз в 3 с | — | `diag: blocking=… hostile=…` **[A]** |
| `ConsiderActor` | правила дизайна 22.11.2026; при блоке щитом активных действий нет (ждём настоящее попадание) | — | `skip: actor=… без щита` **[A]** |
| `PapyrusIsBlocking` / `PapyrusIsBlockingWithShield` | `IsBlocking()` + щит **[A]** | — | вызываются скриптом каждый тик |
| `PapyrusTick` | точка входа из `TGCP_PlayerBlockWatcher` | — | `ticks=…` в `GetStats` **[A]** |
| `PapyrusSetEnabled` | переключатель без перезапуска | — | `enabled=true/false` |
| `PapyrusGetStats` | счётчики и выбранный спелл | — | `GetStats()` |
| `BashWatchSink::ProcessEvent` | `BSAnimationGraphEvent.tag`; `holder` — `TESObjectREFR*` **[A]** | `AS-3` | `bandit anim: bashStop` **[A]** |

## Убрано в 0.7.1 и почему

| Что убрано | Причина |
|---|---|
| `OpenPowerWindow` / `CloseExpiredBuffs` / `Buff` / `g_lastPower` (подъём `powerAttackBlockingMult`) | по логу 0.2.1 множители ни разу не заставили AI ударить башем; правка ванильных форм — лишний риск без наблюдаемой пользы (**[A]**) |
| `RefreshPowerWatches` / `OnPowerAttackStarted` / `PowerWatch` | триггер «силовая по анимации» заменён на настоящее попадание (`TESHitEvent`): анимация не говорит ни о попадании, ни о блоке (**[A]**) |
| `CooldownPassed` | использовался только окном множителей |

## Числа в коде и их источник

| Значение | Где | Источник |
|---|---|---|
| `kMeleeRadius = 200.f` | радиус внимания | дизайн (`docs/skyrim_enemy_combat_design_spec.md`) |
| `kCooldownSeconds = 7.f` | кулдаун баша | дизайн («bash cooldown 7 s») |
| `g_bashReach` (по умолчанию 141) | порог дистанции баша | `fCombatBashReach = 141` **[B]**, читается в рантайме (`AS-9`) |
| `kStaggerMagnitude = 1.0f` | сила стаггера | CK Wiki: 0..1 **[B]**; выбор 1.0 — `AS-2` |
| `kStaggerCooldown = 4.f` | пауза между стаггерами | дизайн (стаггер ≈1 с + запас) |
| `kStaggerDelaySeconds = 0.25f` | задержка от замаха до эффекта | `AS-3`/`AS-8` |
| `0.4f` (пауза между заказами), `12` неудач, `3.0f` (окно слушателя) | защита от спама графа | наша логика, проверено логами 0.5.0–0.6.2 **[A]** |
| `0.15 / 0.45 / 0.9 / 1.4` с | моменты проб блока | подобраны под `fStaggerMaxDuration = 3` **[B]** и ~1 с стаггер **[C]** |
