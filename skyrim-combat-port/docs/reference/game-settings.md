# GameSettings: числа движка, которые управляют блоком, стаггером и башем

Все значения — заводские (Vanilla) для Skyrim SE, из CK Wiki `GameSetting` и UESP `Skyrim:Block`. **[B]**
<https://ck.uesp.net/wiki/GameSetting>, <https://en.uesp.net/wiki/Skyrim:Block>

## Блок

| Настройка | Vanilla | Смысл |
|---|---|---|
| `fBlockPowerAttackMult` | 0.66 | «блок против силовой атаки работает на 66 %»; это и есть «пробивание блока» в ванили |
| `fBlockMax` | 0.7 | потолок блокируемого урона (у оверхаулов 0.8–0.95) |
| `fBlockSkillBase` / `fBlockSkillMult` | 0 / 2.0 | вклад навыка Block |
| `fBlockWeaponBase` / `fBlockWeaponScaling` | 0.3 / 0.2 | блок оружием |
| `fShieldBaseFactor` / `fShieldScalingFactor` | 0.45 / 0.2 | блок щитом |
| `fStaminaBlockBase` / `fStaminaBlockDmgMult` | 0 / 0.25 | расход выносливости на блок |
| `fStaminaBlockStaggerMult` | — | выносливость при блоке против стаггера |

## Стаггер

| Настройка | Vanilla | Смысл (по названию и данным оверхаулов) |
|---|---|---|
| `fStaggerAttackBase` / `fStaggerAttackMult` | 0.5 / 0.01 | базовый стаггер от удара и вклад силы удара |
| `fStaggerBlockBase` / `fStaggerBlockMult` | 0.001 / 0.005 | стаггер, когда удар пришёлся в блок |
| `fStaggerBlockAttackBase` / `Mult` | 1.0 / −0.0001 | стаггер от атаки по блоку |
| `fStaggerBlockAttackShieldBase` / `Mult` | 0.01 / 0.001 | то же, если блок щитом |
| `fStaggerBlockingMult` | 0 | множитель стаггера для блокирующего |
| `fStaggerMin` | 0 | минимальный стаггер (обход «стаггер не сработал») |
| `fStaggerMaxDuration` | 3.0 | потолок длительности стаггера (секунды) |
| `fStaggerShieldMult` | 1.0 | стаггер по тому, у кого щит |
| `fStaggerRecoilingMult`, `fStaggerRunningMult` | 1.5 / 1.5 | по отскочившему / бегущему |
| `fStaggerMassMult`, `fStaggerMassOffsetBase/Mult` | 1.0 / 2.1 / 0.15 | влияние массы цели |
| `fStaggerPlayerMassMult` | 2.5 | отдельно для игрока |
| `iPerkBlockStaggerChance` | — | шанс стаггера от перков блока |

Ключевой факт: **стаггер при попадании считается из этих настроек** — значит его силу можно менять
глобально, без записей и без скриптов. Именно так делают оверхаулы. **[C]**

## Баш

| Настройка | Vanilla | Смысл |
|---|---|---|
| `fCombatBashReach` | 141 | дистанция баша |
| `fCombatBashChanceMin` / `Max` | 0.025 / 0.2 | рамки шанса баша |
| `fStaminaBashBase` / `fStaminaPowerBashBase` | 35 / 55 | стоимость баша / силового баша |

## Что меняют реальные оверхаулы (примеры, чтобы видеть, какие рычаги вообще работают)

| Мод | Изменения (фрагмент) | Источник |
|---|---|---|
| Wildcat | `fBlockPowerAttackMult 0.66→0.75`, `fStaggerMin 0→0.15`, `fStaggerMaxDuration 3→2.5`, `fStaggerAttackBase 0.5→0.55`, `fCombatBashChanceMin 0.025→0.25`, `fCombatBashChanceMax 0.2→0.5`, `fCombatBashReach 141→75` | [Wildcat docs](https://www.nexusmods.com/skyrim/mods/76529?tab=docs) **[C]** |
| Smilodon | `fBlockPowerAttackMult 0.66→0.5`, `fStaggerShieldMult 1→0.75`, `fStaggerRecoilingMult 1.5→2`, `fStaminaBashBase 35→15` | [Smilodon docs](https://www.nexusmods.com/skyrimspecialedition/mods/2824?tab=docs) **[C]** |
| Valravn | `fStaggerMin 0→0.15`, `fStaggerBlockMult 0.005→0`, `fStaggerPlayerMassMult 2.5→1.5` | [Valravn docs](https://www.nexusmods.com/skyrimspecialedition/mods/53869?tab=docs) **[C]** |

Мораль: **`fStaggerMin` и `fStagger*` — рабочий, проверенный сообществом рычаг «стаггер будет всегда»**.
Он влияет только на настоящие попадания; наше «анимационное» башение попаданий не создаёт (см.
[`attacks-and-hits.md`](attacks-and-hits.md)).

## Как читать и менять из DLL

```cpp
auto* settings = RE::GameSettingCollection::GetSingleton();
if (auto* setting = settings->GetSetting("fStaggerMin"); setting && setting->GetType() == RE::Setting::Type::kFloat) {
    float old = setting->GetFloat();
    setting->data.f = 0.15f;  // запись в живое значение
}
```

**[A]** для существования `GameSettingCollection` в CommonLib; **запись** в настройки из плагина
у нас ещё **не проверялась в игре** **[D]** — если решим использовать, менять только при загрузке
плагина и логировать старое/новое значение, чтобы это было видно в логе.
