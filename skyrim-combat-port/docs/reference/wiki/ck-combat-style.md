# CK Wiki: Combat Style — копия нужных фрагментов

Источник: <https://skyrimck.uesp.net/wiki/Combat_Style> (скопировано 2026-11-22). Лицензия CK Wiki (UESP): CC BY-SA.

## Общее

> A Combat Style represents an Actor's overall approach to combat: whether, for example, they will prefer
> melee to ranged combat, or how frequently they will choose to block. **It does not allow you to specify
> specific combat tactics** … the specific decisions made by the Combat AI cannot be controlled through the
> Creation Kit.

## General

> **Offensive Mult** — the higher the number, the more likely a character will attack, the more often they
> will attack, and the more often they will power attack.
> **Defensive Mult** — the higher the number, a character will block more often, hold their block up longer,
> and bash more often if they're capable to do so.

## Melee → Attack

> **Attack Staggered Mult** — The higher the number, the more likely an NPC will attack a player in a stagger state.
> **Power Attack Staggered** — … more likely an NPC will power attack a player in a stagger state.
> **Power Attack Blocking** — The higher the number, the more likely an NPC will power attack a player in a
> blocking state **(and thus, break a block)**.
> **Special Attack** — Not used, or use unknown to author.

## Melee → Bash

> **Bash Mult** — The higher the number, the more likely an NPC will bash the player (either with a shield or a
> special attack flagged as a 'bash.' **This can result in the player being staggered, thus interrupting
> animations and temporarily disorienting & disabling actions**.)
> **Bash Recoiled** — … more likely an NPC will bash a player who is in a recoiled state. A recoiled state is
> when the player (or npc) gets his regular attack blocked, thus recoiling him.
> **Bash Attack Mult** — … more likely an NPC will bash a player who's actively attacking, or in an attack state.
> **Bash Power Attack Mult** — … more likely an NPC will bash a player who's actively power attacking …

## Значения по умолчанию (UESP: Mod File Format/CSTY)

<https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/CSTY>

> CSME (melee): Attack Staggered Mult (default 1.0), Power Attack Staggered Mult (default 1.0),
> Power Attack Blocking Mult (default 1.0), Bash Mult (default 1.0), Bash Recoiled Mult (default 1.0),
> Bash Attack Mult (default 1.0), Bash Power Attack Mult (default 1.0), Special Attack Mult (default 0.1)

**Вывод:** «сбивание блока» в ванильном замысле = стаггер от силовой атаки; баш = стаггер по шансу.
