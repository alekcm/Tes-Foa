# Архетип Magic Effect «Stagger» и как наложить его из DLL

## 1. Что говорит документация

- Архетип называется **Stagger**, его номер в `RE::EffectArchetypes::ArchetypeID::kStagger = 33`. **[A]** (заголовки)
- CK Wiki: «Staggers the target. **Value between 0 and 1.** Duration determines how long before the target
  can be staggered again. Should not be "detrimental"». **[B]**
  <https://ck.uesp.net/wiki/Magic_Effect>
- Значит: **magnitude** — это сила стаггера (0..1), **duration** — не длительность стаггера, а кулдаун
  до следующего возможного стаггера. **[B]**
- Наблюдения моддеров по шкале magnitude: 0.1 — короткий стоячий стаггер, 0.5 — заметный стоячий,
  1.0 — на колено; для некоторых существ мелкие значения не срабатывают вовсе. **[C]**
  <https://www.gamesas.com/staggerkneeldownknockdown-effect-t208765.html>
- Там же есть жалоба, что «стоячие» стаггеры (0.1–0.5) на **игрока** не действуют, а срабатывают только
  «на колено». **[C]** → поэтому в 0.7.0 выбрано `magnitude = 1.0`, и результат измеряется пробами.

## 2. Как наложить эффект из плагина

Своего `.esp` с записями `MGEF`/`SPEL` у мода нет, поэтому используется уже загруженный игрой спелл
с эффектом архетипа Stagger. **[A]**

```cpp
// поиск: перебор всех SpellItem с эффектом kStagger
for (auto* spell : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::SpellItem>()) { ... }
// проверка: MagicItem::HasEffect(RE::EffectArchetype::kStagger)

// наложение: caster — NPC, target — игрок, magnitude переопределяем
auto* caster = npc->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
caster->CastSpellImmediate(spell, true, player, 1.0f, false, 1.0f /*magnitude*/, npc);
```

- `CastSpellImmediate(MagicItem*, bool a_noHitEffectArt, TESObjectREFR* a_target, float a_effectiveness,
  bool a_hostileEffectivenessOnly, float a_magnitudeOverride, Actor* a_blameActor)` — **[A]** (заголовок
  `include/RE/M/MagicCaster.h`, vtable 01)
- Переопределение magnitude позволяет использовать чужой спелл как «свой» стаггер с нужной силой. **[A]** (сигнатура)
- Кастером выбран **NPC**, а не игрок: враждебный спелл, наложенный игроком на самого себя, может быть
  отфильтрован движком; NPC — естественный источник. **[D]** — гипотеза, проверяется строкой
  `stagger cast: … игрок блокирует=true` и пробами `block probe:`.

## 3. Что показал поиск спеллов в реальной игре (лог 0.6.2)

Всего кандидатов с эффектом kStagger: **129**. **[A]** Первые строки таблицы (`stagger spell:`):

| Спелл | FormID | Effects | Delivery | Cost |
|---|---|---|---|---|
| Испепеление | 0010FD5F | 3 | 2 (aimed) | 298 |
| Грозовой разряд | 0010F7F8 | 3 | 2 | 198 |
| Огненный шар | 0010815C | 2 | 2 | 115 |
| Warmaster Knockdown | 00106254 | 1 | 1 (touch) | 2 |
| Порыв Ветра — заклинание удара плашмя | 00105A04 | 1 | 1 | 2 |
| Топот великана | 00104D43 | 1 | 1 | 0 |
| Безжалостная сила — Фус | 00103963 | 1 | 2 | 2 (type=11, крик) |

Выбор 0.6.2 — `Warmaster Knockdown (00106254)` (минимум эффектов, `kFireAndForget`). **[A]**
Но в том же логе `stagger applied` не появился: триггер не срабатывал вообще (см. `animation-events.md`).

## 4. Если ванильный спелл не даст нужного эффекта

Тогда единственный надёжный путь — **своя запись** `MGEF` (архетип Stagger, magnitude-независимая,
duration 0) + `SPEL` в esp мода, и кастовать её. Это требует одной сборки в Creation Kit; решение
принимается только по результату проб блоков (`block probe:`), а не заранее.

## 5. Полезные факты про магию в движке

- `MagicItem::effects` — `BSTArray<Effect*>`; каждый `Effect` содержит `effectItem.magnitude`,
  `effectItem.duration`, `baseEffect` (`EffectSetting`). **[A]**
- `EffectSetting::HasArchetype(EffectArchetype)` — проверка архетипа. **[A]**
- `SpellItem::data` (`SPIT`) содержит `spellType`, `castingType`, `delivery`, `costOverride`. **[A]**
- Значения `MagicSystem::Delivery`: 0 self, 1 touch, 2 aimed, 3 target actor, 4 target location. **[A]**
- Значения `MagicSystem::CastingType`: 0 constant effect, 1 fire and forget, 2 concentration. **[A]**
