# Animation events: что они делают и чего не делают

## 1. Главное различие

| | Атака движка | Animation event |
|---|---|---|
| задаёт урон, хитбокс, attack data | да | **нет** |
| может попасть по цели | да | **нет** |
| проигрывает анимацию | да | да |
| можно заказать из DLL | нет | да, `Actor::NotifyAnimationGraph(name)` |

**[A]** для третьей строки и «да» в четвёртой: заказ проходит (`bash ordered … sent=true` в логах),
но попаданий нет — лог 0.6.2, `measured-in-game.md`.

`NotifyAnimationGraph` возвращает `true`, если граф **принял** событие. Это не значит «актёр сделал действие»:
в логе 0.6.2 за ним через 0,12–0,34 с приходил `bashStop`, то есть анимация начиналась и обрывалась. **[A]**

## 2. Имена событий (используемые в этом моде)

Из опубликованного списка animation variables: <https://pastebin.com/Z5NwnMbQ> **[C]**

- блок: `blockStart`, `blockStartOut`, `blockStop`, `blockStopOut`
- баш: `bashStart`, `bashStop`, `bashRelease`, `bashPowerStart`
- обычные атаки: `attackStart`, `attackStop`, `attackRelease`, `attackPowerStart`,
  `attackPowerStartInPlace`, `attackPowerStartLeft`, `attackPowerStartRight`,
  `attackPowerStartForward`, `attackPowerStartBackward`
- стаггер: `staggerStart`, `staggerStop`, `staggerIdleStart`

`bashStart` — это ещё и **idle-анимация** с ID `0001B417` (`playidle`-список):
<https://elderscrolls.fandom.com/wiki/Console_Commands_(Skyrim)/Idle_Animations> **[C]**
Это объясняет, почему его отправка даёт только анимацию: это не «атака башем», а проигрывание idle.

## 3. Что реально доходит до слушателя (измерено)

Слушатель — `RE::BSTEventSink<RE::BSAnimationGraphEvent>` (у нас `BashWatchSink`).

| Событие | Приходит слушателю? | Чем подтверждено |
|---|---|---|
| `bashStop` | **да** | каждый заказ баша в логах 0.5.0–0.6.2 дает строку `bandit anim: bashStop` **[A]** |
| `bashStart` | **нет** | 0.6.2: 5 заказов → 5 `bashStop`, `bashStart` не пришёл ни разу **[A]** |
| `attackStop` | да | лог 0.2.1 **[A]** |
| `blockStop`, `blockStartOut`, `preHitFrame`, `weaponSwing`, `weaponDraw`, `IdleStop` | да | Papyrus-пробник, `docs/papyrus_verification.md` **[A]** |
| `attackPowerStart*`, `bashPowerStart` | ожидается да (на них подписан 0.7.0) | **ещё не измерено** **[D]** |

Вывод, на котором построена 0.7.0: **триггер механики нельзя вешать на `bashStart`** — событие не рассылается.
Надёжный сигнал окончания замаха — `bashStop`. **[A]**

## 4. Список attack data расы — как узнать, какие имена являются «башем»

В `TESRace::attackDataMap` у каждой атаки есть флаг `AttackData::AttackFlag::kBashAttack`.
Плагин выгружает таблицу в лог (`race attack data:` + `atkd:`). Пример для Imperial (79684) из лога 0.4.0: **[A]**

```
atkd: bashStart              bash=true  chance=0.50
atkd: bashPowerStart         bash=true  chance=0.33
atkd: attackStart_MC         bash=false chance=1.00
```

Это единственный способ узнать имена атак для конкретной расы без Creation Kit.

## 5. Опровергнуто

| Гипотеза | Чем опровергнута |
|---|---|
| `Debug.SendAnimationEvent(target, "bashStart")` заставит NPC ударить | отклонено пользователем как механизм; и по факту это анимация без атаки **[A]** |
| «`bashStart` достаточно, попадание случится само» | 0.6.2: анимация есть, попаданий нет **[A]** |
| `blockStart` как признак «начал блокировать» | приходит не всегда; признак блока строится на `Actor::IsBlocking()` **[A]** |
