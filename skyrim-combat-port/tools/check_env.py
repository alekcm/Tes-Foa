#!/usr/bin/env python3
"""Проверка локального окружения сборки Skyrim-мода.

Скрипт не устанавливает и не копирует ничего. Он читает .env (или переменные
окружения), ищет установленные инструменты и печатает отчёт "есть / нет".
Запускать на своей машине; ничего из найденного в Git не попадает.

Примеры:
    python tools/check_env.py
    python tools/check_env.py --tier papyrus
    python tools/check_env.py --json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parent.parent

DLC = ("Dawnguard.esm", "Hearthfire.esm", "Dragonborn.esm")
TIER_REQUIRES = {
    "plugin": ("skyrim", "data", "dlc"),
    "papyrus": ("skyrim", "data", "dlc", "editor", "papyrus_compiler", "papyrus_flags", "papyrus_sources"),
    "native": ("skyrim", "data", "dlc", "editor", "skse"),
}


def load_env() -> dict[str, str]:
    """Значения из .env, поверх них — реальные переменные окружения."""
    values: dict[str, str] = {}
    env_file = ROOT / ".env"
    if env_file.is_file():
        for raw in env_file.read_text(encoding="utf-8", errors="replace").splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            value = value.strip().strip('"').strip("'")
            values[key.strip()] = value
    for key in (
        "SKYRIM_DIR",
        "CK_DIR",
        "XEDIT_DIR",
        "SKSE_DIR",
        "PAPYRUS_COMPILER",
        "PAPYRUS_FLAGS",
        "PAPYRUS_SOURCE",
    ):
        override = os.environ.get(key, "").strip()
        if override:
            values[key] = override
    return values


def as_path(value: str) -> Path | None:
    if not value:
        return None
    return Path(os.path.expandvars(value)).expanduser()


def parse_library_folders(text: str) -> list[Path]:
    """Достаёт пути библиотек Steam из libraryfolders.vdf."""
    paths = []
    for raw in re.findall(r'"path"\s+"([^"]+)"', text):
        paths.append(Path(raw.replace("\\\\", "\\")))
    return paths


def steam_library_roots() -> list[Path]:
    """Корни библиотек Steam из реестра Windows и libraryfolders.vdf."""
    if sys.platform != "win32":
        return []
    roots: list[Path] = []
    try:
        import winreg

        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:
            steam_path = Path(winreg.QueryValueEx(key, "SteamPath")[0])
    except (OSError, ImportError):
        return roots
    roots.append(steam_path)
    for vdf in (steam_path / "steamapps" / "libraryfolders.vdf", steam_path / "config" / "libraryfolders.vdf"):
        if vdf.is_file():
            try:
                roots.extend(parse_library_folders(vdf.read_text(encoding="utf-8", errors="replace")))
            except OSError:
                pass
    return roots


def autodetect_game(roots: list[Path] | None = None) -> Path | None:
    """Ищет папку Skyrim SE в библиотеках Steam."""
    for root in roots if roots is not None else steam_library_roots():
        candidate = root / "steamapps" / "common" / "Skyrim Special Edition"
        if (candidate / "SkyrimSE.exe").is_file():
            return candidate
    return None


def find_in_dirs(names: Iterable[str], roots: Iterable[Path | None], max_depth: int = 3) -> Path | None:
    """Ищет файл по имени в корнях, не глубже max_depth."""
    for root in roots:
        if root is None or not root.is_dir():
            continue
        root = root.resolve()
        base_depth = len(root.parts)
        for current, dirs, files in os.walk(root):
            current_path = Path(current)
            if len(current_path.parts) - base_depth >= max_depth:
                dirs[:] = []
            for name in names:
                if name in files:
                    return current_path / name
    return None


def read_exe_version(exe: Path) -> str:
    if sys.platform != "win32":
        return "unknown (версия читается только на Windows)"
    script = (
        f"(Get-Item -LiteralPath '{exe}').VersionInfo | "
        "Select-Object -ExpandProperty FileVersion"
    )
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.SubprocessError):
        return "unknown"
    version = result.stdout.strip()
    return version or "unknown"


def build_report(env: dict[str, str]) -> list[dict[str, str]]:
    game_dir = as_path(env.get("SKYRIM_DIR", ""))
    ck_dir = as_path(env.get("CK_DIR", ""))
    xedit_dir = as_path(env.get("XEDIT_DIR", ""))
    skse_dir = as_path(env.get("SKSE_DIR", ""))

    rows: list[dict[str, str]] = []
    if game_dir is None or not (game_dir / "SkyrimSE.exe").is_file():
        detected = autodetect_game()
        if detected is not None:
            game_dir = detected
            rows.append(
                {
                    "key": "autodetect",
                    "title": "Папка игры найдена автоматически",
                    "status": "info",
                    "detail": f"{detected} — впиши этот путь в SKYRIM_DIR в .env",
                }
            )

    def add(key: str, title: str, path: Path | None, detail: str = "") -> None:
        if path is not None and path.exists():
            rows.append({"key": key, "title": title, "status": "ok", "detail": f"{path} {detail}".strip()})
        else:
            rows.append({"key": key, "title": title, "status": "missing", "detail": detail or "не найдено"})

    skyrim_exe = game_dir / "SkyrimSE.exe" if game_dir else None
    add("skyrim", "SkyrimSE.exe", skyrim_exe if skyrim_exe and skyrim_exe.is_file() else None)

    data_dir = game_dir / "Data" if game_dir else None
    add("data", "Data/Skyrim.esm", data_dir / "Skyrim.esm" if data_dir and (data_dir / "Skyrim.esm").is_file() else None)

    found_dlc = [name for name in DLC if data_dir and (data_dir / name).is_file()]
    missing_dlc = [name for name in DLC if name not in found_dlc]
    rows.append(
        {
            "key": "dlc",
            "title": "Dawnguard / Hearthfire / Dragonborn",
            "status": "ok" if not missing_dlc else "missing",
            "detail": f"найдено {len(found_dlc)}/3"
            + (f"; нет: {', '.join(missing_dlc)}" if missing_dlc else ""),
        }
    )

    ck_exe = find_in_dirs(("CreationKit.exe",), (ck_dir, game_dir), max_depth=2)
    xedit_exe = find_in_dirs(("SSEEdit.exe", "xEdit.exe"), (xedit_dir, game_dir), max_depth=2)
    if ck_exe:
        rows.append({"key": "editor", "title": "Creation Kit", "status": "ok", "detail": str(ck_exe)})
    elif xedit_exe:
        rows.append(
            {
                "key": "editor",
                "title": "Creation Kit / xEdit",
                "status": "ok",
                "detail": f"CK нет, но есть {xedit_exe} — record-only сборка возможна через xEdit",
            }
        )
    else:
        rows.append(
            {"key": "editor", "title": "Creation Kit / xEdit", "status": "missing", "detail": "ни CK, ни SSEEdit не найдены"}
        )

    compiler = as_path(env.get("PAPYRUS_COMPILER", ""))
    if not (compiler and compiler.is_file()):
        compiler = find_in_dirs(("PapyrusCompiler.exe",), (ck_dir, game_dir), max_depth=4)
    add("papyrus_compiler", "PapyrusCompiler.exe", compiler)

    flags = as_path(env.get("PAPYRUS_FLAGS", ""))
    if not (flags and flags.is_file()):
        flags = find_in_dirs(("TESV_Papyrus_Flags.flags",), (ck_dir, game_dir), max_depth=4)
    add("papyrus_flags", "TESV_Papyrus_Flags.flags", flags)

    source = as_path(env.get("PAPYRUS_SOURCE", ""))
    candidates = [source] if source else []
    if data_dir:
        candidates.extend(
            [
                data_dir / "Scripts" / "Source",
                data_dir / "Scripts",
                data_dir / "Source" / "Scripts",
                data_dir / "Scripts.zip",
            ]
        )
    found_source = None
    for candidate in candidates:
        if candidate and candidate.is_file():
            found_source = candidate
            break
        if candidate and candidate.is_dir() and any(candidate.glob("*.psc")):
            found_source = candidate
            break
    add("papyrus_sources", "Исходные скрипты игры", found_source, "нет Scripts.zip и нет *.psc в Data/Scripts")

    skse_loader = find_in_dirs(("skse64_loader.exe",), (skse_dir, game_dir), max_depth=2)
    add("skse", "skse64_loader.exe", skse_loader)

    if skyrim_exe and skyrim_exe.is_file():
        version = read_exe_version(skyrim_exe)
        rows.append({"key": "runtime", "title": "Версия SkyrimSE.exe", "status": "info", "detail": version})

    rows.append(
        {
            "key": "native_toolchain",
            "title": "MSVC / CMake (для SKSE DLL)",
            "status": "info",
            "detail": "не проверяется этим скриптом; нужен только для уровня native",
        }
    )
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description="Проверить локальное окружение сборки Skyrim-мода.")
    parser.add_argument("--tier", choices=sorted(TIER_REQUIRES), help="уровень сборки, для которого важен код возврата")
    parser.add_argument("--json", action="store_true", help="вывести отчёт в JSON")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    rows = build_report(load_env())

    if args.json:
        print(json.dumps({"tier": args.tier, "checks": rows}, ensure_ascii=False, indent=2))
    else:
        print("Проверка окружения сборки (локально, ничего не отправляется)")
        print(f"Корень проекта: {ROOT}")
        if not (ROOT / ".env").is_file():
            print("Файла .env нет: скопируй .env.example в .env и впиши пути (или задай переменные окружения).")
        print()
        width = max(len(row["title"]) for row in rows)
        marks = {"ok": "[ok]  ", "missing": "[нет] ", "info": "[i]   "}
        for row in rows:
            print(f"{marks[row['status']]}{row['title']:<{width}}  {row['detail']}")
        print()
        print("Что это значит: [нет] у уровня A — нужен CK или xEdit; [нет] у Papyrus — скрипты не собрать;")
        print("SKSE нужен только если прототип докажет необходимость C++ DLL.")
        print("Пути выше — личные; перед отправкой в чат замени их на <local>.")

    if not args.tier:
        return 0
    failed = [row["title"] for row in rows if row["key"] in TIER_REQUIRES[args.tier] and row["status"] != "ok"]
    if failed:
        print(f"\nУровень {args.tier}: не готово — {', '.join(failed)}")
        return 1
    print(f"\nУровень {args.tier}: все проверяемые компоненты на месте.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
