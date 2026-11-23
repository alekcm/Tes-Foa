# TGCombatPort — боёвка Skyrim SE: блок, пробития, стаггер

**Что это.** SKSE-плагин (`TGCombatPort.dll`) + файл правил `Rules.txt`, которые вместе меняют
физику боя: что пробивает блок оружием, что пробивает щит, когда срабатывает стаггер, как устроены
пробития уколом, стрелами и магией. Механика задаётся **данными** (`Rules.txt`), код только исполняет
её и пишет лог.

**Состояние (честно).** Последняя версия, на которой бой отработал в игре полностью — **0.9.0**:
в логе 40 попаданий, 7 сработавших правил, 24 снятия блока, 7 стаггеров. Версии после неё в игре
падают в момент удара; причина не найдена, из кода убраны все вызовы движка на пути удара, у которых
нет подтверждения логом. Сборки при этом собираются и выкладываются.

## Где что искать

| Вопрос | Файл |
|---|---|
| что какое оружие пробивает, какие эффекты, числа движка | [`docs/combat-audit.md`](docs/combat-audit.md) |
| матрица «атака × состояние защиты», все враги по семействам | [`docs/combat-block-matrix.md`](docs/combat-block-matrix.md) |
| оружие по типам: кейворды, `chip`, кто чем отвечает | [`docs/weapons-and-effects.md`](docs/weapons-and-effects.md) |
| заклинания врагов: 13 штук, телеграф, два ответа, потолок урона | [`docs/enemy-spells.md`](docs/enemy-spells.md) |
| синтаксис `Rules.txt` и действующие правила | [`docs/rules.md`](docs/rules.md), [`native/rules/Rules.txt`](native/rules/Rules.txt) |
| как быстро выдать врагу оружие и проверить правило | [`TESTING.md`](TESTING.md), [`docs/console_spawn.md`](docs/console_spawn.md) |
| факты о механике игры с метками доверия и цитатами | [`docs/reference/README.md`](docs/reference/README.md) |
| числа `fBlock*`, `fStagger*`, `fCombatBash*` и что меняют оверхаулы | [`docs/reference/game-settings.md`](docs/reference/game-settings.md) |
| что измерено в этой игре и каким логом | [`docs/reference/measured-in-game.md`](docs/reference/measured-in-game.md) |
| карта кода: функция → вызов движка → строка лога | [`docs/reference/CODE_AUDIT.md`](docs/reference/CODE_AUDIT.md) |
| допущения `AS-n` и чем каждое проверяется | [`docs/reference/ASSUMPTIONS.md`](docs/reference/ASSUMPTIONS.md) |
| дизайн врагов: мувсеты, телеграфы, контр-игра (42 архетипа) | [`docs/skyrim_enemy_combat_design_spec.md`](docs/skyrim_enemy_combat_design_spec.md) |

## Сборка и выпуск

```cmd
cd native
xmake f -y -m release
xmake build -y
```

CI (`.github/workflows/native.yml`) на каждый пуш: сверка вызовов движка с заголовками CommonLib,
сборка DLL, упаковка `TGCombatPort-<ref>.zip`, проверка содержимого архива. Тег `vX.Y.Z` публикует
релиз. Подробности — [`BUILD.md`](BUILD.md) и [`RELEASING.md`](RELEASING.md).

## Проверки, которые можно запустить без игры

| Команда | Что проверяет |
|---|---|
| `python -m unittest discover -s tests` | 85 тестов инструментов, парсера правил и упаковки |
| `python tools/check_rules.py` | `Rules.txt`: ключи, значения, оружие, эффекты, `[settings]` |
| `python tools/check_docs.py` | ссылки между документами |
| `python tools/verify_audit.py` | каждая функция кода описана, каждая метка `AS-n` есть в реестре |
| `python tools/check_api_usage.py --headers <CommonLib include>` | каждое `RE::Имя` в коде существует в заголовках |

## Железные правила проекта

- **Хуки в код движка не ставим.** В 0.1.1 перехват `Actor::GetAttackChance` (`write_branch`) уронил
  игру при загрузке сейва.
- **Ваншотов нет никогда**: урон за каст ≤ 25 % максимального здоровья, за тик ≤ 9 %.
- **Контроль ≤ 0,8 с**: единственное, что отнимает управление, — стаггер как наказание за ошибку.
- **Заклинание берём, только если оно даёт решение**, которого без него не было.
- **Ничего не выдумывать**: факт без метки `[A]`/`[B]` — гипотеза, её сначала проверяют логом.
