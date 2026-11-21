#!/usr/bin/env python3
"""Включает или выключает плагин в plugins.txt (прямая установка, без MO2/Vortex).

Нужно для первого теста: игра видит только те плагины, которые отмечены в plugins.txt.
Перед изменением файла делается резервная копия.

Примеры:
    python tools/enable_plugin.py --status
    python tools/enable_plugin.py --enable TGCombatPort.esp
    python tools/enable_plugin.py --disable TGCombatPort.esp
    python tools/enable_plugin.py --enable TGCombatPort.esp --dry-run
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
import shutil
import sys
from pathlib import Path


def default_plugins_txt() -> Path | None:
    if sys.platform != "win32":
        return None
    local = os.environ.get("LOCALAPPDATA")
    if not local:
        return None
    return Path(local) / "Skyrim Special Edition" / "plugins.txt"


def read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def is_enabled(lines: list[str], name: str) -> bool:
    target = name.lower()
    return any(line.strip().lstrip("*").lower() == target and line.strip().startswith("*") for line in lines)


def set_enabled(lines: list[str], name: str, enabled: bool) -> tuple[list[str], bool]:
    """Возвращает новые строки и признак, изменился ли файл."""
    target = name.lower()
    result: list[str] = []
    found = False
    changed = False
    for line in lines:
        stripped = line.strip()
        if stripped.lstrip("*").lower() == target:
            found = True
            new_line = f"*{stripped.lstrip('*')}" if enabled else stripped.lstrip("*")
            if new_line != line:
                changed = True
            result.append(new_line)
        else:
            result.append(line)
    if not found:
        if not enabled:
            return result, False
        result.append(f"*{name}")
        changed = True
    return result, changed


def main() -> int:
    parser = argparse.ArgumentParser(description="Включить/выключить плагин в plugins.txt.")
    parser.add_argument("--plugins-txt", help="путь к plugins.txt (по умолчанию — стандартный путь Windows)")
    parser.add_argument("--enable", metavar="PLUGIN", help="включить плагин, например TGCombatPort.esp")
    parser.add_argument("--disable", metavar="PLUGIN", help="выключить плагин")
    parser.add_argument("--status", nargs="?", const="", metavar="PLUGIN", help="показать состояние плагина или весь список")
    parser.add_argument("--dry-run", action="store_true", help="только показать, что изменится")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    path = Path(args.plugins_txt) if args.plugins_txt else default_plugins_txt()
    if path is None:
        print("Не знаю путь к plugins.txt: укажи его через --plugins-txt.")
        print(r"Обычно: %LOCALAPPDATA%\Skyrim Special Edition\plugins.txt")
        return 2
    if not path.is_file():
        print(f"Файла нет: {path}")
        print("Запусти игру один раз до главного меню, чтобы он создался, или укажи путь через --plugins-txt.")
        return 2

    lines = read_lines(path)
    name = args.enable or args.disable or args.status

    if args.status is not None:
        if not name:
            print(f"Файл: {path}")
            print(f"Плагинов в списке: {len([line for line in lines if line.strip()])}")
            for line in lines:
                if line.strip():
                    mark = "вкл" if line.strip().startswith("*") else "выкл"
                    print(f"  [{mark}] {line.strip().lstrip('*')}")
            return 0
        state = "включён" if is_enabled(lines, name) else "выключен"
        print(f"{name}: {state}")
        return 0

    if not name:
        parser.error("нужно указать --enable, --disable или --status")
        return 2

    new_lines, changed = set_enabled(lines, name, enabled=bool(args.enable))
    if not changed:
        print(f"Без изменений: {name} уже в нужном состоянии.")
        return 0

    if args.dry_run:
        print(f"Изменилось бы: {path}")
        print(f"  {len(lines)} строк → {len(new_lines)} строк")
        return 0

    backup = path.with_suffix(path.suffix + f".bak-{dt.datetime.now():%Y%m%d-%H%M%S}")
    shutil.copy2(path, backup)
    path.write_text("\n".join(new_lines) + "\n", encoding="utf-8")
    print(f"{'Включён' if args.enable else 'Выключен'}: {name}")
    print(f"Файл: {path}")
    print(f"Резервная копия: {backup.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
