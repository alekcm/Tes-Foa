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
| `Game.FindClosestActorFromRef(ObjectReference, float)` | `Game` script: «Finds the closest Actor within a given radius of a ObjectReference» | находит ли врага, а не самого игрока | `Game.FindClosestActor(x,y,z,r)` или перебор через алиасы |
| `Debug.SendAnimationEvent(ObjectReference, string)` | `Debug` script: `void SendAnimationEvent(ObjectReference arRef, string asEventName)`; CK Wiki: событие реально проигрывает действие («AttackStart» заставляет актёра атаковать). Предупреждение там же: отправка события актёру может рассинхронизировать его внутреннее состояние анимации | работает ли `bashStart` на NPC и не ломает ли его | не отправлять NPC, а только проигрывать эффект/звук; либо SKSE-DLL |
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

### Второй прогон (22.11.2026, 05:21:26–05:22:51, пробник v2 на 24 имени)

Зарегистрировано 24 имени (`probe: registered 24 candidate events`), ошибок нет. Пришли:

| Событие | Раз | Что это даёт |
|---|---|---|
| `attackStop` | 18 | конец атаки игрока |
| `preHitFrame` | 8 | кадр попадания атаки — годится как «момент удара» |
| `weaponSwing` | 7 | замах оружием |
| `blockStop` | 3 | конец блока |
| `blockStartOut` | 3 | **начало блока** |
| `weaponDraw` | 1 | достал оружие |
| `bashStop` | 1 | конец bash (толчка щитом/оружием) |

Тайминги из того же лога: `blockStartOut` в 05:22:25 → `blockStop` в 05:22:29 (блок держали ~4 секунды);
в 05:22:40 `blockStartOut` → в 05:22:41 `blockStop` и `bashStop` (bash прервал блок). То есть `blockStartOut`
приходит **в начале** блока, а не в конце.

Не пришли ни разу: `blockStart`, `blockHitStart`, `blockHitStop`, `blockAnticipateStart`,
`blockAnticipateStop`, `blockStopOut`, `blockStopInstant`, `bashStart`, `bashRelease`, `attackStart`,
`attackStart_Attack1`, `AttackStart`, `attackRelease`, `staggerStart`, `staggerStop`, `recoilStart`, `IdleStop`.

Вывод, на котором построен `TGCP_PlayerBlockWatcher` v2: началом блока считаем `blockStart` **или**
`blockStartOut`, концом — `blockStop`. Имя сработавшего события пишется в лог, поэтому следующий прогон
покажет, какое именно работает.

### Третий прогон (22.11.2026, 05:25:45–05:25:57, watcher v2)

Подтверждено в игре, что признак блока работает:

```
05:25:51  [TGCP] player block start (blockStartOut)
05:25:54  [TGCP] player block stop (blockStop)
05:25:55  [TGCP] player block start (blockStartOut)
05:25:56  [TGCP] player block stop (blockStop)
05:25:56  [TGCP] player block start (blockStartOut)   ← bash прервал блок, новый блок начался сразу
05:25:57  [TGCP] player block stop (blockStop)
```

То есть `TGCP_PlayerBlocking` меняется 1/0 по блоку игрока. Технический вопрос «можно ли узнать, что игрок
блокирует» закрыт: можно, без SKSE, по `blockStartOut` + `blockStop`.

Открытый вопрос: почему `attackStart`/`bashStart`/`staggerStart` не приходят, хотя `*Stop` и `blockStartOut`
приходят. Тест проводился **не в бою** (игрок махал в воздух). Возможная причина — другая ветка графа
анимаций в бою; это проверяется только боем с NPC.

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
