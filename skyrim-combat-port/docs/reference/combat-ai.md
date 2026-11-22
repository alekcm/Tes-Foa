# Combat AI: чем реально управляются решения NPC

## 1. `CombatStyle` — единственный «официальный» рычаг, и он вероятностный

CreationKit Wiki, [Combat Style](https://skyrimck.uesp.net/wiki/Combat_Style): **[B]**

- «A Combat Style represents an Actor's overall approach to combat … **It does not allow you to specify
  specific combat tactics** … the specific decisions made by the Combat AI cannot be controlled through
  the Creation Kit.»
- General: `Offensive Mult` — чаще атаки и чаще силовые; `Defensive Mult` — чаще блок и **баш**;
  `Group Offensive Mult` — то же в группе.
- Melee → Attack:
  - `Attack Staggered Mult` — чаще атаковать оглушённого;
  - `Power Attack Staggered Mult` — чаще делать силовую по оглушённому;
  - **`Power Attack Blocking Mult` — «more likely an NPC will power attack a player in a blocking state
    (and thus, break a block)»**;
- Melee → Bash:
  - **`Bash Mult` — «more likely an NPC will bash the player … This can result in the player being
    staggered, thus interrupting animations and temporarily disorienting & disabling actions»**;
  - `Bash Recoiled Mult` — баш по тому, кто отскочил после блокированного удара;
  - `Bash Attack Mult` — баш по атакующему;
  - `Bash Power Attack Mult` — баш по делающему силовую.

Важно для нас: **сама вики описывает «сбивание блока» как следствие стаггера от попадания**, а не как
отдельную механику. Наш дизайн (баш выбивает из блока) совпадает с замыслом игры, но реализуется
через стаггер, а не через «снятие блока».

Значения по умолчанию (`CSME`): все множители 1.0, `Special Attack Mult` 0.1. **[B]**
<https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/CSTY>

Papyrus/SKSE дают те же поля сеттерами: `CombatStyle.SetMeleeBashMult`, `SetMeleePowerAttackBlockingMult`,
`SetMeleeBashAttackMult`, `SetMeleeBashRecoiledMult`, `SetMeleeBashPowerAttackMult`. **[B]**
<https://skyrimck.uesp.net/wiki/CombatStyle_Script>
(в DLL — прямая запись `style->meleeData.*`, у нас так и сделано)

## 2. Наш опыт с этими множителями (отрицательный)

| Версия | Что делали | Результат |
|---|---|---|
| 0.2.0 | `bashMult` 1.70 → 10.0 на 1,5 с у ближайшего щитоносца | окон 7 с, баша нет **[A]** |
| 0.2.1 | все пять множителей (bash, bashRecoil, bashAttack, bashPowerAttack, powerAttackBlocking) → 10.0, окно 2,5 с | **ни одного `bashStart`** в логе, только `attackStop` **[A]** |

Вывод: множители — вероятность, а не команда; при их поднятии AI всё равно ни разу не выбрал баш
за 2 минуты боя. **Почему именно — неизвестно [D]**, есть две версии для проверки:
(1) баш требует, чтобы NPC сам держал блок в этот момент; (2) у используемого combat style
баш-ветка недостижима из-за других полей. Проверять — правкой `fCombatBash*` GameSettings
(см. [`game-settings.md`](game-settings.md)), а не новыми множителями.

## 3. GameSettings, которые прямо влияют на баш и стаггер

Из CK Wiki (`GameSetting`) и документации боевых оверхаулов: **[B]/[C]**

| Настройка | Значение | Смысл |
|---|---|---|
| `fCombatBashReach` | 141 | **дистанция, с которой движок считает баш достающим** — наше пороговое 140 в коде совпадает с этим |
| `fCombatBashChanceMin` / `Max` | 0.025 / 0.2 | нижняя/верхняя граница шанса баша в бою |
| `fCombatBlockChanceMin` | 0 | минимальный шанс блока |
| `fCombatAttackChanceBlockingMultMin` / `Max` | 0.2 / 0.67 | как падает шанс атаки, если цель блокирует |
| `fPowerAttackCooldownTime` | 0 | пауза между силовыми |
| `fStaminaBashBase` / `fStaminaPowerBashBase` | 35 / 55 | стоимость баша и силового баша |

Wildcat, Smilodon и Valravn — реальные боевые оверхаулы — правят именно эти числа, а не скрипты
баша: **[C]** <https://www.nexusmods.com/skyrim/mods/76529?tab=docs>,
<https://www.nexusmods.com/skyrimspecialedition/mods/53869?tab=docs>
(например `fCombatBashChanceMin 0.025 → 0.2`, `fCombatBashReach 141 → 134`).

## 4. Чего управление AI не даёт

- Нельзя «приказать» конкретному NPC ударить башем именно сейчас: только поднять вероятность
  и ждать. **[B]** (см. цитату выше)
- Нельзя заставить NPC выбрать силовую именно по блокирующему — только `Power Attack Blocking Mult`. **[B]**
- Значит для гарантированного результата нужен собственный эффект, а не уговоры AI. Это и есть
  причина, по которой 0.7.0 накладывает стаггер сам.
