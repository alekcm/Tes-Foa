# UESP: Skyrim:Block — копия нужных фрагментов

Источник: <https://en.uesp.net/wiki/Skyrim:Block> (скопировано 2026-11-22). Лицензия UESP: CC BY-SA.

## Что такое блок

> Block allows you to reduce damage from melee and ranged combat, using a shield, a torch, a
> one-handed weapon, or a two-handed weapon.

## Формулы блокируемого урона (сокращено)

Щит:

> Blocked Damage % = (45 + 0.2 x BaseShieldArmorRating x (1 + BlockSkill x 1.5 / 100))
> x (1 + Shield Wall perk) x (1 + Enchantment…) x (1 + Potion) x (fBlockPowerAttackMult if power attack)

Оружие:

> Blocked Damage % = (30 + 0.2 x EnemyWeaponBaseDamage x (1 + BlockSkill x 1.5 / 100))
> x (1 + Shield Wall perk) x (1 + Enchantment…) x (1 + Potion) x (fBlockPowerAttackMult if power attack)

> fShieldBaseFactor = 45 is the base value of blocking with a shield.
> fShieldScalingFactor = 0.2 is the scaling factor of blocking with a shield.

> Hide shield, 100 skill, all perks, no enchantments, no potions, power attack
> (45 + 0.2 x 15 x (1 + 100 x 1.5 / 100)) x (1 + 0.5) x (1) x (1) X 0.66 = 51.98%

**Вывод:** силовая атака не убирает блок, она умножает его эффективность на 0.66.

## Баш

> Tapping the attack button while blocking will bash your opponent with a shield or weapon.
> Bashing is an attack to an opponent's health; it carries a chance of staggering an opponent and of
> interrupting an opponent's power attack. This applies to dragons as well, whose «power attacks» are shouts.
> … You can bash (with or without benefits from perks) with two-handed weapons as well as shields;
> but if equipped with a bow, you can perform only a regular bash. Bashing with a torch will set opponents on fire.

**Вывод:** баш в ванили — шанс стаггера и прерывание силовой. Гарантированного снятия блока нет.
