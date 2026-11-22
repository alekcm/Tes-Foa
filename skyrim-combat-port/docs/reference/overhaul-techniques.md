# Какими способами вообще делают «сбивание блока» и стаггер (каталог с источниками)

Каждый способ помечен: работает ли без своих записей в esp, проверен ли сообществом,
и наш опыт. Это меню, из которого выбираем, а не список обещаний.

## 1. GameSettings (`fStagger*`, `fBlockPowerAttackMult`, `fCombatBash*`)

- **Как:** поменять числа движка; стаггер считается из них при каждом настоящем попадании. **[B]/[C]**
- **Без esp:** да, из DLL через `GameSettingCollection`.
- **Проверено сообществом:** да — Wildcat, Smilodon, Valravn, Blade and Blunt правят ровно эти числа.
  [`game-settings.md`](game-settings.md)
- **Ограничение:** не создаёт попадания. Наш «анимационный» баш под это не попадает.
- **Наш опыт:** не применяли.

## 2. Спелл с архетипом Stagger (то, что в 0.7.0)

- **Как:** `MagicCaster::CastSpellImmediate` ванильного спелла, magnitude 0..1. **[B]/[A]**
- **Без esp:** да, если брать загруженный игрой спелл (у нас 129 кандидатов).
- **Проверено сообществом:** да — так устроены тиры стаггера в Blade and Blunt (magnitude 0.25/0.5/0.75/1.0,
  длительность 1.5–3 с; magnitude 0.01 ≈ 1.2 с). **[C]**
- **Ограничение:** magnitude шкалируется нелинейно; по отзывам «стоячие» значения на игрока могут не
  действовать. Поэтому у нас magnitude 1.0 и пробы `block probe:`.

## 3. `ObjectReference.KnockAreaEffect(magnitude 0..1, radius)`

- **Как:** нативная функция Papyrus «Executes a knock effect on actors in an area». **[B]**
  <https://ck.uesp.net/wiki/KnockAreaEffect_-_ObjectReference>
- **Трюк сообщества:** вызвать её на пустом активаторе, поставленном у ног цели, и сразу удалить объект —
  это даёт **настраиваемый стаггер** без новых магических записей. **[C]**
  <https://www.reddit.com/r/skyrimmods/comments/7uu26b/how_to_apply_stagger_to_melee_attackers/>
- **Без esp:** да (только Papyrus-скрипт).
- **Ограничение:** в CommonLib **нет** обёртки над этой функцией (она есть лишь как функция условий,
  `TESCondition` 685), поэтому вызвать её можно из Papyrus, а из DLL — только косвенно. **[A]**
- **Наш опыт:** не применяли. Это **запасной вариант**, если пробы 0.7.0 покажут, что стаггер от спелла
  не снимает блок у игрока.

## 4. Перки с entry point `Mod Incoming Stagger` / `Mod Target Stagger`

- **Есть в игре:** entry points 21 и 22, «Modifies the amount of stagger sustained by the perk owner
  when hit» / «Adjusts the amount of stagger targets suffer when hit by this perk owner». **[B]**
  <https://ck.uesp.net/wiki/Perk_Entry_Point>, <https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/PERK>
- **Без esp:** нет — перк это запись `PERK`; из DLL его не создать.
- **Надёжность:** низкая: моддеры сообщают, что значение влияет на **шанс**, а не на величину стаггера,
  и что entry point ведёт себя непредсказуемо («does not affect the stagger magic magnitude in vanilla»). **[C]**
  <https://forums.nexusmods.com/topic/1533673-mod-target-stagger-arrrgh/>
- **Вывод:** не наш путь.

## 5. Правка поведения (behavior) и stagger-lock моды

- **Как:** подмена hkx-поведения, новые анимации стаггера, блокировка управления.
  Примеры: Modern Stagger Lock, «Cancelable large stagger». **[C]**
- **Без esp:** нет (behavior + анимации).
- **Когда нужно:** если требуется «красивый» стаггер с собственными анимациями и таймингом. Это
  следующий большой этап, отдельно от текущего каркаса.

## 6. Хуки в код игры

- **Как:** перехват функций движка (`write_branch`, по-настоящему мощно).
- **Наш опыт: вылет.** 0.1.1 патчила `Actor::GetAttackChance`, игра падала при загрузке сейва. **[A]**
- **Вывод:** не используем.

## 7. Чего не существует (проверено по заголовкам и вики)

| Хочется | Реальность |
|---|---|
| «застаггерить актёра на N мс» | сеттеров нет, только чтение `IsStaggering()` **[A]** |
| «приказ NPC атаковать/башить» | только `NotifyAnimationGraph` (анимация) либо вероятность через `CombatStyle` **[A]/[B]** |
| «снять блок напрямую» | нет ни в API, ни в настройках; только через стаггер **[A]/[B]** |
