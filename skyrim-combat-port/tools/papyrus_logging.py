#!/usr/bin/env python3
"""Находит Skyrim.ini и включает запись Papyrus-лога.

Зачем: путь к Skyrim.ini зависит от того, куда Windows положила папку Documents
(часто это OneDrive), а сам файл появляется только после первого запуска игры.
Руками такой файл легко не найти, а правка не того .ini (например, SkyrimPrefs.ini)
ничего не даёт: игра читает настройки Papyrus только из Skyrim.ini.

Скрипт ничего не скачивает. Он показывает, какие .ini нашлись, и по ключу --enable
дописывает в секцию [Papyrus] три строки, делая резервную копию файла.

Примеры:
    python tools/papyrus_logging.py                 # показать, что найдено
    python tools/papyrus_logging.py --enable        # включить логи
    python tools/papyrus_logging.py --status        # только состояние ключей
    python tools/papyrus_logging.py --ini "D:\\ini\\Skyrim.ini" --enable
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path
from typing import Iterable, Sequence

GAME_DIR = Path("My Games") / "Skyrim Special Edition"
INI_NAME = "Skyrim.ini"
KEYS = ("bEnableLogging", "bEnableTrace", "bLoadDebugInformation")
LOG_HINT = Path("Logs") / "Script" / "Papyrus.0.log"


def base_documents(env: dict[str, str]) -> list[Path]:
    """Папки Documents, где Windows может держать настройки игры."""
    dirs: list[Path] = []
    home = env.get("USERPROFILE") or str(Path.home())
    for key in ("OneDrive", "OneDriveConsumer", "OneDriveCommercial"):
        value = env.get(key)
        if value:
            dirs.append(Path(value) / "Documents")
    dirs.append(Path(home) / "OneDrive" / "Documents")
    dirs.append(Path(home) / "Documents")
    unique: list[Path] = []
    for path in dirs:
        if path not in unique:
            unique.append(path)
    return unique


def game_dirs(documents: Iterable[Path]) -> list[Path]:
    """Существующие папки «My Games/Skyrim Special Edition»."""
    return [path / GAME_DIR for path in documents if (path / GAME_DIR).is_dir()]


def find_inis(documents: Iterable[Path]) -> list[Path]:
    return [path / INI_NAME for path in game_dirs(documents) if (path / INI_NAME).is_file()]


def read_ini(path: Path) -> str:
    """Читает .ini, не падая на не-UTF8 кодировке Windows."""
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "cp1251", "cp1252", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def _newline(text: str) -> str:
    return "\r\n" if "\r\n" in text else "\n"


def papyrus_values(text: str) -> dict[str, str]:
    """Значения ключей из секции [Papyrus] (имена ключей без регистра)."""
    values: dict[str, str] = {}
    in_section = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("[") and line.endswith("]"):
            in_section = line[1:-1].strip().lower() == "papyrus"
            continue
        if not in_section or "=" not in line or line.startswith(";"):
            continue
        key, _, value = line.partition("=")
        values[key.strip().lower()] = value.strip()
    return values


def is_enabled(text: str) -> bool:
    values = papyrus_values(text)
    return all(values.get(key.lower()) == "1" for key in KEYS)


def enable_logging(text: str) -> tuple[str, list[str]]:
    """Возвращает новый текст .ini и список выполненных действий."""
    newline = _newline(text)
    lines = text.splitlines()
    actions: list[str] = []
    found_keys: set[str] = set()

    start = None
    end = None
    for index, raw in enumerate(lines):
        line = raw.strip()
        if line.startswith("[") and line.endswith("]"):
            if start is not None and end is None:
                end = index
            if line[1:-1].strip().lower() == "papyrus":
                start = index
                end = None
    if start is not None and end is None:
        end = len(lines)

    if start is None:
        if lines and lines[-1].strip():
            lines.append("")
        lines.append("[Papyrus]")
        lines.extend(f"{key}=1" for key in KEYS)
        actions.append("создана секция [Papyrus]")
        return newline.join(lines) + newline, actions

    section = lines[start + 1 : end]
    updated: list[str] = []
    for raw in section:
        stripped = raw.strip()
        if "=" in stripped and not stripped.startswith(";"):
            key = stripped.split("=", 1)[0].strip()
            if key.lower() in {item.lower() for item in KEYS}:
                canonical = next(item for item in KEYS if item.lower() == key.lower())
                if stripped.split("=", 1)[1].strip() != "1":
                    actions.append(f"{canonical}: было {stripped.split('=', 1)[1].strip()!r}, стало 1")
                found_keys.add(canonical)
                updated.append(f"{canonical}=1")
                continue
        updated.append(raw)

    missing = [key for key in KEYS if key not in found_keys]
    if missing:
        while updated and not updated[-1].strip():
            updated.pop()
        updated.extend(f"{key}=1" for key in missing)
        actions.append("добавлены ключи: " + ", ".join(missing))

    if not actions:
        actions.append("уже включено, изменений нет")

    lines[start + 1 : end] = updated
    result = newline.join(lines)
    if text.endswith(("\n", "\r")):
        result += newline
    return result, actions


def describe(path: Path) -> dict[str, object]:
    text = read_ini(path)
    values = papyrus_values(text)
    return {
        "ini": str(path),
        "exists": True,
        "has_section": any(
            line.strip().lower() == "[papyrus]" for line in text.splitlines()
        ),
        "values": {key: values.get(key.lower()) for key in KEYS},
        "enabled": is_enabled(text),
        "log_path": str(path.parent / LOG_HINT),
    }


def resolve_target(explicit: str | None, documents: Sequence[Path], create: bool) -> Path | None:
    if explicit:
        return Path(explicit).expanduser()
    inis = find_inis(documents)
    if len(inis) == 1:
        return inis[0]
    if len(inis) > 1:
        return None
    if create:
        dirs = game_dirs(documents)
        if len(dirs) == 1:
            return dirs[0] / INI_NAME
    return None


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Найти Skyrim.ini и включить логи Papyrus.")
    parser.add_argument("--ini", help="путь к Skyrim.ini, если автопоиск не сработал")
    parser.add_argument("--enable", action="store_true", help="включить логи Papyrus")
    parser.add_argument("--status", action="store_true", help="только показать текущее состояние")
    parser.add_argument("--dry-run", action="store_true", help="ничего не записывать")
    parser.add_argument("--json", action="store_true", help="вывод в JSON")
    args = parser.parse_args(argv)

    env = dict(os.environ)
    documents = base_documents(env)
    inis = find_inis(documents)
    dirs = game_dirs(documents)

    if args.enable:
        target = resolve_target(args.ini, documents, create=True)
    else:
        target = resolve_target(args.ini, documents, create=False)

    report: dict[str, object] = {
        "documents": [str(path) for path in documents],
        "game_dirs": [str(path) for path in dirs],
        "found_inis": [str(path) for path in inis],
        "target": str(target) if target else None,
    }

    if args.json:
        if target and target.is_file():
            report["state"] = describe(target)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if target else 1

    print("Искал Skyrim.ini в папках:")
    for path in documents:
        print(f"  {path}")
    if dirs:
        print("Найдены папки игры:")
        for path in dirs:
            print(f"  {path}")
    else:
        print("Папки «My Games\\Skyrim Special Edition» нет ни в одной из них.")
        print("Значит игра ещё ни разу не запускалась на этом пользователе: запусти её до главного меню и повтори.")
        return 1

    if inis:
        print("Найдены Skyrim.ini:")
        for path in inis:
            print(f"  {path}")
    else:
        print("Файла Skyrim.ini нет — он создастся при включении логов (--enable).")

    if not target:
        print("Не смог выбрать один файл сам: запусти с ключом --ini \"полный путь к Skyrim.ini\".")
        return 1

    print(f"Работаю с: {target}")

    if args.enable:
        if target.is_file():
            backup = target.with_suffix(target.suffix + ".bak-py")
            if not args.dry_run:
                shutil.copy2(target, backup)
                print(f"Резервная копия: {backup.name}")
            text = read_ini(target)
        else:
            text = ""
            print("Файл будет создан с нуля (секция [Papyrus] и три ключа).")
        new_text, actions = enable_logging(text)
        for action in actions:
            print(f"  - {action}")
        if args.dry_run:
            print("--dry-run: файл не изменён.")
        elif new_text != text or not target.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(new_text, encoding="utf-8", newline="")
            print(f"Записано: {target}")
        state = describe(target) if target.is_file() else {}
        if state:
            print(f"Логи включены: {'да' if state.get('enabled') else 'нет'}")
            print(f"Лог появится здесь: {state.get('log_path')}")
        return 0

    state = describe(target) if target.is_file() else {}
    if state:
        print(f"Логи включены: {'да' if state.get('enabled') else 'нет'}")
        for key in KEYS:
            value = state["values"][key]  # type: ignore[index]
            print(f"  {key} = {value if value is not None else '(нет строки)'}")
        print(f"Лог: {state.get('log_path')}")
    if not args.status:
        print("Чтобы включить: python tools/papyrus_logging.py --enable")
    return 0


if __name__ == "__main__":
    sys.exit(main())
