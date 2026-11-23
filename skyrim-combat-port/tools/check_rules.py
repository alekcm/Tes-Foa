#!/usr/bin/env python3
"""Проверка файла правил: опечатка в ключе или в слове значения = правило молча выключится.

Плагин при запуске печатает «(ОТКЛЮЧЕНО: ошибка в файле)», но увидеть это можно только в игре
и только если дойти до боя. Поэтому те же имена проверяются здесь: списки ключей и значений
взяты из `native/src/main.cpp` (`ParseWeapon`, `ParseKind`, `ParseTri`, `ParseDo`).

Использование: python tools/check_rules.py [путь]  (по умолчанию native/rules/Rules.txt)
"""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT = ROOT / "native" / "rules" / "Rules.txt"

# Из native/src/main.cpp: kWeaponTable.
WEAPONS = {"fists", "sword", "dagger", "axe", "mace", "greatsword", "battleaxe", "warhammer", "bow", "staff", "crossbow"}
KINDS = {"melee", "ranged", "spell", "shout", "unarmed", "any"}
TRIGGERS = {"hit", "bash"}
TRI = {"yes", "no", "any"}
FLAGS = {"blocked", "power", "bash", "sneak"}
EFFECTS = {"unblock", "stagger", "bash", "chip"}
# Секция [settings] — не правило, а переключатели плагина.
SETTINGS = {"effects"}
SETTINGS_VALUES = {"effects": {"on", "off"}}
KEYS = {
    "trigger",
    "hit-flags",
    "hit-not-flags",
    "attack.kind",
    "attacker.race",
    "attacker.weapon",
    "attacker.shield",
    "hit.source",
    "hit.projectile",
    "player.blocking",
    "player.shield",
    "player.ward",
    "reach",
    "cooldown",
    "chip",
    "do",
}


def check_line(key: str, value: str) -> list[str]:
    problems: list[str] = []
    items = [item.strip() for item in value.split(",") if item.strip()]
    if key == "trigger":
        problems += [f"trigger: неизвестное значение «{i}»" for i in items if i not in TRIGGERS]
    elif key in ("hit-flags", "hit-not-flags"):
        problems += [f"{key}: неизвестный флаг «{i}»" for i in items if i not in FLAGS]
    elif key == "attack.kind":
        problems += [f"attack.kind: неизвестный вид «{i}»" for i in items if i not in KINDS]
    elif key == "attacker.weapon":
        problems += [
            f"attacker.weapon: неизвестное оружие «{i}» (есть: {', '.join(sorted(WEAPONS))})"
            for i in items
            if i != "any" and i not in WEAPONS
        ]
    elif key in ("attacker.shield", "player.blocking", "player.shield", "player.ward", "reach"):
        if value not in TRI:
            problems.append(f"{key}: нужно yes/no/any, написано «{value}»")
    elif key == "do":
        problems += [f"do: неизвестный эффект «{i}» (есть: {', '.join(sorted(EFFECTS))})" for i in items if i not in EFFECTS]
    elif key in ("cooldown", "chip"):
        try:
            float(value)
        except ValueError:
            problems.append(f"{key}: нужно число, написано «{value}»")
    return problems


def check_text(text: str) -> tuple[list[str], list[str]]:
    """Возвращает (ошибки, имена правил)."""
    problems: list[str] = []
    names: list[str] = []
    section = ""
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            if section == "settings":
                continue
            names.append(section)
            continue
        if "=" not in line:
            problems.append(f"строка {number}: не «ключ = значение» и не [секция]")
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if not section:
            problems.append(f"строка {number}: ключ «{key}» вне секции [имя]")
            continue
        if section == "settings":
            if key not in SETTINGS:
                problems.append(f"строка {number} [settings]: неизвестная настройка «{key}» (есть: {', '.join(sorted(SETTINGS))})")
            elif value not in SETTINGS_VALUES[key]:
                problems.append(f"строка {number} [settings]: «{key}» принимает {', '.join(sorted(SETTINGS_VALUES[key]))}, написано «{value}»")
            continue
        if key not in KEYS:
            problems.append(f"строка {number} [{section}]: неизвестный ключ «{key}»")
            continue
        for problem in check_line(key, value):
            problems.append(f"строка {number} [{section}]: {problem}")
    return problems, names


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Проверить файл правил TGCombatPort на опечатки.")
    parser.add_argument("rules", nargs="?", type=Path, default=DEFAULT, help="путь к Rules.txt")
    args = parser.parse_args(argv)
    path = args.rules
    if not path.is_file():
        print(f"нет файла правил: {path}")
        return 1
    problems, names = check_text(path.read_text(encoding="utf-8"))
    if problems:
        print(f"ПРАВИЛА С ОШИБКАМИ ({path}):")
        for problem in problems:
            print(f"  - {problem}")
        return 1
    print(f"правила в порядке: {path.name}, секций {len(names)}: {', '.join(names)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
