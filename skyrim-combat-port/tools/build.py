#!/usr/bin/env python3
"""Одна команда сборки мода: Papyrus -> stage -> готовый zip.

Что делает:
  1. Находит PapyrusCompiler.exe и TESV_Papyrus_Flags.flags (по .env или поиском).
  2. Компилирует все src/papyrus/*.psc в build/stage/Scripts.
  3. Копирует плагин (--plugin) и ресурсы (assets/) в stage.
  4. Вызывает tools/package.py и получает dist/<мод>-<версия>.zip.

Чего не делает: не создаёт .esp и не запускает игру. Плагин должен быть собран
в Creation Kit или xEdit заранее.

Примеры:
    python tools/build.py --name TGCombatPort --version 0.1.0 --plugin build/TGCombatPort.esp
    python tools/build.py --name TGCombatPort --version 0.1.0 --skip-package
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import inspect_plugin
import papyrus_lint
from check_env import load_env, as_path, find_in_dirs

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "papyrus"
STAGE = ROOT / "build" / "stage"


def locate(env: dict[str, str]) -> tuple[Path | None, Path | None, Path | None]:
    game_dir = as_path(env.get("SKYRIM_DIR", ""))
    ck_dir = as_path(env.get("CK_DIR", ""))

    compiler = as_path(env.get("PAPYRUS_COMPILER", ""))
    if not (compiler and compiler.is_file()):
        compiler = find_in_dirs(("PapyrusCompiler.exe",), (ck_dir, game_dir), max_depth=4)

    flags = as_path(env.get("PAPYRUS_FLAGS", ""))
    if not (flags and flags.is_file()):
        flags = find_in_dirs(("TESV_Papyrus_Flags.flags",), (ck_dir, game_dir), max_depth=4)

    sources = as_path(env.get("PAPYRUS_SOURCE", ""))
    if not (sources and sources.is_dir()):
        sources = None
        if game_dir:
            candidate = game_dir / "Data" / "Scripts"
            if candidate.is_dir() and any(candidate.glob("*.psc")):
                sources = candidate
    return compiler, flags, sources


def compile_scripts(compiler: Path, flags: Path, sources: Path | None, out_dir: Path) -> list[str]:
    scripts = sorted(SRC.glob("*.psc"))
    if not scripts:
        return []
    out_dir.mkdir(parents=True, exist_ok=True)
    for stale in out_dir.glob("*.pex"):
        stale.unlink()

    include = [SRC]
    if sources:
        include.append(sources)

    compiled: list[str] = []
    for script in scripts:
        command = [str(compiler), str(script), f"-f={flags}"]
        for path in include:
            command.append(f"-i={path}")
        command.append(f"-o={out_dir}")
        result = subprocess.run(command, capture_output=True, text=True)
        output = (result.stdout or "") + (result.stderr or "")
        if result.returncode != 0 or not (out_dir / f"{script.stem}.pex").exists():
            print(f"[ошибка] {script.name} не скомпилирован (код {result.returncode})")
            print(output.strip() or "компилятор ничего не вывел")
            raise SystemExit(1)
        compiled.append(f"{script.stem}.pex")
        for line in output.strip().splitlines():
            print(f"  {line}")
    return compiled


def main() -> int:
    parser = argparse.ArgumentParser(description="Собрать скрипты и упаковать мод.")
    parser.add_argument("--name", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--plugin", help="готовый .esp/.esl, который положить в корень архива")
    parser.add_argument("--skip-package", action="store_true", help="только собрать скрипты, zip не делать")
    parser.add_argument("--out-dir", help="куда положить zip (по умолчанию dist/)")
    parser.add_argument("--allow-dll", action="store_true", help="передать в package.py (SKSE-плагин)")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
    except (AttributeError, ValueError):
        pass

    lint_errors, lint_warnings = papyrus_lint.lint_paths([SRC])
    for warning in lint_warnings:
        print(f"[внимание] {warning}")
    if lint_errors:
        for error in lint_errors:
            print(f"[ошибка]   {error}")
        print("\nПроверка исходников не пройдена — компиляция отменена.")
        return 1

    env = load_env()
    compiler, flags, sources = locate(env)

    if not compiler or not flags:
        print("Не найден компилятор Papyrus или файл флагов.")
        print("Заполни .env (см. .env.example) или проверь окружение: python tools/check_env.py --tier papyrus")
        print(f"  compiler: {compiler or 'нет'}")
        print(f"  flags:    {flags or 'нет'}")
        return 2

    print(f"Компилятор: {compiler}")
    print(f"Флаги:      {flags}")
    print(f"Vanilla scripts: {sources or 'не найдены (скомпилируется только то, что не импортирует ванильные скрипты)'}")

    compiled = compile_scripts(compiler, flags, sources, STAGE / "Scripts")
    print(f"Скомпилировано скриптов: {len(compiled)}")
    for name in compiled:
        print(f"  Scripts/{name}")

    if args.plugin:
        plugin = Path(args.plugin)
        if not plugin.is_file():
            print(f"[ошибка] плагин {plugin} не найден — собери его в CK/xEdit")
            return 1
        STAGE.mkdir(parents=True, exist_ok=True)
        shutil.copy2(plugin, STAGE / plugin.name)
        print(f"Плагин: {plugin.name}")

        try:
            report = inspect_plugin.describe(plugin.read_bytes())
        except (ValueError, OSError) as error:
            print(f"[внимание] не удалось прочитать заголовок плагина: {error}")
        else:
            print(f"  мастер-файлы: {', '.join(report['masters']) or 'нет'}")
            if report["unexpected_masters"]:
                print(f"[внимание] плагин зависит не только от игры и DLC: {', '.join(report['unexpected_masters'])}")
                print("           такой мод не запустится у игрока без этих плагинов")

    assets = ROOT / "assets"
    if assets.is_dir():
        shutil.copytree(assets, STAGE, dirs_exist_ok=True)
        print("Ресурсы из assets/ скопированы")

    if args.skip_package:
        print(f"\nStage готов: {STAGE} (zip не собирался)")
        return 0

    command = [
        sys.executable,
        str(ROOT / "tools" / "package.py"),
        "--stage",
        str(STAGE),
        "--name",
        args.name,
        "--version",
        args.version,
    ]
    if args.out_dir:
        command.extend(["--out-dir", args.out_dir])
    if args.allow_dll:
        command.append("--allow-dll")
    return subprocess.run(command).returncode


if __name__ == "__main__":
    raise SystemExit(main())
