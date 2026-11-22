# CK Wiki: Magic Effect — архетип Stagger (копия)

Источник: <https://ck.uesp.net/wiki/Magic_Effect> (скопировано 2026-11-22). Лицензия CK Wiki (UESP): CC BY-SA.

## Строка таблицы архетипов

> **Stagger** — Staggers the target. **Value between 0 and 1.** Duration determines how long before the
> target can be staggered again. Should not be «detrimental».

## Про magnitude / duration / area

> **No Magnitude**, **No Area** and **No Duration** do not actually affect the inner workings of the effect,
> checking them just makes it so that these parameters will be unavailable when you assign the effect to a
> Spell, Scroll, Potion, Shout or Enchantment.

**Вывод:** magnitude = сила стаггера (0..1), duration = пауза до следующего стаггера, а не длительность.

## Наблюдения моддеров (не вики, помечено [C])

- magnitude 0.1 — короткий стоячий стаггер, 0.5 — заметный стоячий, 1.0 — на колено;
  для крупных существ 0.5 может не сработать. <https://www.gamesas.com/staggerkneeldownknockdown-effect-t208765.html>
- «стоячие» значения на игрока могут не действовать — только «на колено».
- Blade and Blunt: тиры стаггера задаются magnitude 0.25/0.5/0.75/1.0, длительность 1.5–3 с;
  magnitude 0.01 ≈ 1.2 с. <https://www.nexusmods.com/skyrimspecialedition/mods/34549>
