# UESP: Skyrim:Combat — копия нужных фрагментов

Источник: <https://en.uesp.net/wiki/Skyrim:Combat> (скопировано 2026-11-22). Лицензия UESP: CC BY-SA.

## Стаггер

> When the player, a creature, or an NPC is hit by certain attacks, there is a chance that the attack's
> target will be **staggered**. Staggering interrupts whatever action was being taken by the target and
> temporarily stuns them, preventing any further attacks until they have recovered. When a creature or NPC
> is staggered, it also stops them from moving; however, you will still be able to move if you are staggered.
> *[verification needed — see talk page]*

> There are numerous ways to cause staggering, but power attacks are the simplest method and the one most
> used by NPCs. The Impact, Power Shot, and Power Bash perks will add staggering effects to some normal attacks …

**Ключевое для нас:** стаггер прерывает действие, то есть и блок. Это единственный «сбивающий блок» механизм.

## Силовые атаки

> Power attacks can be used to deal extra damage or break through an opponent's block. They are performed
> by holding down, instead of tapping, the attack button. A power attack has a chance of staggering its target …

> power attack stamina cost = (40 + weapon weight * 2) * attack cost multiplier * (1 - perk effect)

Таблица (сама UESP помечает «verification needed»):

| Attack Type | Damage | Stagger* | Stamina Use |
|---|---|---|---|
| Single Weapon | ×2 | ×1 | ×1 |
| Dual-Wield (×3 attacks) | ×1.5 | ×1 | ×0.5 / attack |
| Bash | ×0.5 | ×0.5 | ×1 |
| Power Bash | ×1.5 | ×1.5 | ×1 |
| Unarmed | ×2 | ×0.5 | ×1 |

**Вывод:** «break through an opponent's block» относится к множителю урона блока, а не к снятию блока.
