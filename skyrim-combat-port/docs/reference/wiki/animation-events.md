# Animation events — копия списков

## Список имён (публичный список сообщества)

Источник: <https://pastebin.com/Z5NwnMbQ> (скопировано 2026-11-22). Это не вики, а собранный моддерами список. **[C]**

Нужные нам имена (в исходном списке идут нумерованными строками):

- `blockStart`, `blockStop`, `blockStartOut`, `blockStopOut`
- `bashStart`, `bashStop`, `bashRelease`, `bashPowerStart`
- `attackStart`, `attackStop`, `attackRelease`
- `attackPowerStartBackward`, `attackPowerStartForward`, `attackPowerStartLeft`,
  `attackPowerStartRight`, `attackPowerStartInPlace`
- `staggerStart`, `staggerStop`, `staggerIdleStart`
- прочие: `moveStart`, `moveStop`, `shoutStart`, `shoutStop`, `syncPoint`, `IdleStop`, `weaponDraw`

## `bashStart` — это idle-анимация

Список idle-анимаций (`playidle`): <https://elderscrolls.fandom.com/wiki/Console_Commands_(Skyrim)/Idle_Animations> **[C]**

> | 0001B417 | bashStart |

То есть `bashStart` существует как **idle**-запись и проигрывается как анимация. Это согласуется
с нашим измерением: анимация идёт, попадания нет.

## Что измерено у нас (не вики)

См. [`../measured-in-game.md`](../measured-in-game.md): `bashStop` доходит до слушателя,
`bashStart` — нет; попаданий от заказа нет.
