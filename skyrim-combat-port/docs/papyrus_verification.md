# Проверка Papyrus-функций, которые мы используем

Скрипты написаны без компилятора, поэтому каждая функция сверена с доступной документацией. Это не заменяет
компиляцию: подтверждено существование функции и её примерная сигнатура, но не поведение в конкретной игре.
Таблица нужна, чтобы при первой ошибке компиляции было понятно, что менять.

| Что используем | Где подтверждено | Что осталось проверить | Запасной вариант |
|---|---|---|---|
| `RegisterForAnimationEvent(ObjectReference, string)` | это функция `Form`, есть у `ReferenceAlias` и `ActiveMagicEffect`; в CK Wiki — «RegisterForAnimationEvent — Form» | приходят ли конкретные имена событий (blockStart и т. п.) | перебрать имена по логу пробника; при отсутствии — SKSE |
| `OnAnimationEvent(ObjectReference akSource, string asEventName)` | событие `Form`; получает только тот alias/effect/form, который регистрировался | срабатывает ли в 1-м и 3-м лице, на игроке и NPC | логировать все имена, не фильтровать заранее |
| `OnAliasInit()` | событие `ReferenceAlias`; community-практика: `OnInit` на алиасе ненадёжен | срабатывает ли до заполнения алиаса | повторная регистрация по таймеру/событию квеста |
| `RegisterForSingleUpdate(float)` + `OnUpdate()` у `ActiveMagicEffect` | объявлены в `ActiveMagicEffect.psc` | не плодится ли обновление при повторном надевании способности | ставить/снимать обновление в `OnEffectStart`/`OnEffectFinish` |
| `Actor.HasLOS(ObjectReference)` | в списке Papyrus-функций указана как функция `Actor` | корректность для игрока за укрытием | убрать проверку LOS на первом тесте, затем вернуть |
| `Actor.DoCombatSpellApply(Spell, ObjectReference)` | есть в `Actor.psc` Skyrim: «Apply a spell to a target in combat» | появится ли видимый снаряд у missile-заклинания | `WebSpell.Cast(spider, victim)` |
| `Spell.Cast(ObjectReference akSource, ObjectReference akTarget)` | CK Wiki: `Function Cast(ObjectReference akSource, ObjectReference akTarget = None)` | проигрывается ли анимация каста | оставить как fallback, не как основной путь |
| `Actor.GetCombatTarget()`, `GetDistance()`, `IsDead()` | стандартные функции `Actor`/`ObjectReference` | — | — |
| `GlobalVariable.SetValue(float)` | стандартная функция `GlobalVariable` | — | — |
| `Utility.GetCurrentRealTime()` | стандартная функция `Utility`, секунды реального времени | переживает ли сохранение/загрузку как ожидается | хранить время в глобале, если нужно пережить перезапуск |

## Что показал первый запуск в игре (22.11.2026)

Лог `Papyrus.0.log` (05:10:51–05:13:05), старый сейв, Vortex, runtime 1.7.104.0.

Работает:
- квест `TGCP_MainQuest` стартует на загруженном сохранении (не только на новой игре);
- алиас заполнился игроком: `[TGCP] block watcher registered on [Actor < (00000014)>]`;
- `RegisterForAnimationEvent` принимает все 12 имён без ошибок: `probe: registered 12 candidate events`;
- `OnAnimationEvent` приходит и несёт имя события: `anim event: IdleStop | source=[Actor < (00000014)>]`;
- `GlobalVariable.SetValue` работает: по `blockStop` в лог ушло `player block stop`.

Пришли события (40 строк `[TGCP]` за 2 минуты):
| Событие | Сколько раз | Вывод |
|---|---|---|
| `IdleStop` | 29 | приходит постоянно, в реальной логике не использовать — шум |
| `attackStop` | 4 | приходит |
| `blockStop` | 2 | приходит |
| `bashStop` | 1 | приходит |

НЕ пришли ни разу: `blockStart`, `attackStart`, `attackRelease`, `bashStart`, `staggerStart`, `staggerStop`,
`JumpUp`, `JumpDown`.

Что это значит для дизайна: признак «игрок блокирует» в том виде, как задумано (по `blockStart`), пока не
подтверждён — в логе только завершение блока. Прежде чем менять механику, список имён расширен
(`TGCP_AnimEventProbe` v2: `blockHitStart`, `blockAnticipateStart`, `blockStartOut`, `preHitFrame`,
`weaponSwing`, `attackStart_Attack1` и др.) — нужен повторный прогон.

Запасные варианты, если `*Start` не придут:
1. Проверять состояние блока по факту попадания в блок (`blockHitStart`/`blockHitStop`) — без SKSE.
2. `GetAnimationVariableBool("Isblocking")` — только с SKSE (в Papyrus-функции `IsBlocking` нет;
   условие `IsBlocking` в CK внутри вызывает именно эту проверку графа).
3. Условная способность (ability) с условием `IsBlocking == 1` — без SKSE, но проверяется на событиях,
   а не мгновенно; требует отдельного теста.

## Чего мы сознательно не используем

- **`DoCombatSpellCast`** — в найденной документации для Skyrim не подтверждён (в списках фигурирует
  `DoCombatSpellApply`). Не добавлять «по памяти».
- **Прямая отмена атаки NPC** — в Papyrus такой функции нет; для дизайна «один leap на стаю» это и есть
  главный риск, см. `vertical_slice_feasibility.md`.
- **Принудительный выбор атаки NPC** (`PlayIdle` и подобные) — не даёт нужного результата и ломает анимации;
  не использовать как обход.

## Как обновлять таблицу

1. Скомпилировали скрипт — отметить дату и версию компилятора в `STATUS.md`.
2. Проверили в игре — заменить «осталось проверить» на результат теста с его ID из `TESTING.md`.
3. Если функцию пришлось заменить — записать причину здесь и в `docs/papyrus_api.md`.

Ссылки: [Form](https://papyrus.bellcube.dev/skyrimse/script/form/),
[ActiveMagicEffect](https://papyrus.bellcube.dev/skyrimse/script/activemagiceffect/),
[Spell.Cast](https://ck.uesp.net/wiki/Cast_-_Spell),
[List of Papyrus Functions](https://ck.uesp.net/wiki/List_of_Papyrus_Functions).
