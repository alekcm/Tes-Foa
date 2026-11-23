# Что есть в `RE::` (CommonLibSSE-NG), а чего нет

Источник: заголовки CommonLibSSE-NG, пин `48b17e1dea3028850adf63209f6733dbf4dcdcd7` (релиз 16.10.0),
ветка `ng`: <https://github.com/alandtse/CommonLibVR>. Всё ниже проверено `grep` по `include/RE`
и меткой **[A]**.

## Есть и используется в моде

| Что | Где | Зачем |
|---|---|---|
| `Actor::IsBlocking()` | `RE/A/Actor.h:644` | признак блока игрока |
| `Actor::IsStaggering()` | `RE/A/Actor.h:677` | проверка, что стаггер наложился |
| `Actor::IsAttacking()`, `IsInCombat()`, `IsHostileToActor()` | `RE/A/Actor.h` | фильтры |
| `Actor::GetEquippedObject(bool a_leftHand)` | `TESObjectREFR` | что в руках (щит/оружие) |
| `TESObjectARMO::IsShield()` | `RE/T/TESObjectARMO.h` | щит в руке |
| `TESObjectWEAP::GetWeaponType()` | `RE/T/TESObjectWEAP.h` | булава `kOneHandMace`, двуручное `kTwoHandAxe` |
| `Actor::AsActorState()->IsWeaponDrawn()` | `RE/A/ActorState.h:206` | оружие/щит вынуты (иначе баш = кулак) |
| `ActorState::IsStaggered()` | `RE/A/ActorState.h:201` | альтернативная проверка стаггера |
| `Actor::NotifyAnimationGraph(BSFixedString)` | `RE/A/Actor.h` | заказ анимации (не атаки!) |
| `Actor::AddAnimationGraphEventSink / Remove...` | `RE/A/Actor.h` | слушать `BSAnimationGraphEvent` |
| `Actor::GetMagicCaster(MagicSystem::CastingSource)` | `RE/A/Actor.h:324` | каст спелла |
| `MagicCaster::CastSpellImmediate(...)` | `RE/M/MagicCaster.h:62` | наложить стаггер |
| `MagicItem::HasEffect(EffectArchetype)` | `RE/M/MagicItem.h:121` | поиск спеллов со стаггером |
| `MagicItem::effects` (`BSTArray<Effect*>`) | `RE/M/MagicItem.h` | обход эффектов |
| `TESDataHandler::GetFormArray<T>()` | `RE/T/TESDataHandler.h` | перебор форм |
| `TESCombatStyle::meleeData.*` | `RE/T/TESCombatStyle.h` | множители баша/силовой |
| `TESRace::attackDataMap` | `RE/T/TESRace.h` | имена атак и флаг `kBashAttack` |
| `Actor::HandleHealthDamage(Actor*, float)` | `RE/A/Actor.h:488` | нанести урон в обход атаки (пока не используется) |

## Чего НЕТ (не пытаться)

| Чего искали | Что в заголовках |
|---|---|
| «застаггерить актёра» (`SetStaggered`, `DoStagger`, `Stagger(float)`) | только чтение: `IsStaggering()`, `ActorState::IsStaggered()` |
| «включить/выключить блок» | только `IsBlocking()`; сеттера нет |
| «заставить NPC атаковать/башить» | только `NotifyAnimationGraph` (анимация) и AI (решение принимает игра) |
| «пробить блок» | нет ни в API, ни в GameSettings; только перемножение урона блока |

## Грабли, на которые уже наступали

| Ошибка компиляции/вылет | Причина и решение |
|---|---|
| `write_branch<5>` в `Actor::GetAttackChance` | вылет при загрузке сейва (0.1.1). Патчинг кода игры не используем |
| `error C2662` `GetCombatStyle` на `const TESNPC` | вызывать у `GetActorBase()` (не const) |
| `error C2440` `GetEquippedObject` → `const TESForm*` | приводить к `const TESObjectWEAP*`/`TESObjectARMO*` |
| `error C2039 'IsWeaponDrawn' is not a member of AttemptState` | локальная переменная `state` уже занята; называть иначе |
| `Cannot open include file: 'openvr.h'` | в CommonLibSSE-NG 16.11.0 сломан prebuilt; пин на `48b17e1d` (16.10.0) |
| `BSAnimationGraphEvent::holder` | это `TESObjectREFR*`, не `NiPointer` (`.get()` не нужен) |
| `EffectSetting::HasArchetype` | тип аргумента — `RE::EffectArchetypes::ArchetypeID` (`RE::EffectArchetype` — его alias) |

## Про Papyrus и движок анимаций

- `Actor.RegisterForAnimationEvent` принимает имена, но не все события рассылаются: `bashStart` —
  не рассылается, `bashStop` — рассылается. **[A]** (наш лог 0.6.2)
- В Papyrus нет функции «наложить стаггер», «заставить атаковать», «сбить блок». Всё это — только DLL.
