#!/usr/bin/env python3
"""Собирает устанавливаемый архив мода из подготовленной папки.

Папка-источник (stage) должна повторять содержимое Data игры: плагин в корне,
`Scripts/*.pex`, `Meshes/`, `Sound/` и т. п. Внутри архива эти файлы лежат так же,
поэтому менеджер модов (MO2/Vortex) ставит его как обычный мод.

Скрипт ничего не компилирует и не создаёт плагин: он только проверяет состав
и упаковывает. Проверяет, что в архив не попали мастер-файлы игры, exe/dll
(без явного разрешения) и исходники .psc.

Примеры:
    python tools/package.py --stage build/stage --name TGCombatPort --version 0.1.0
    python tools/package.py --stage build/stage --name TGCombatPort --version 0.1.0 --dry-run
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MASTERS = {"skyrim.esm", "update.esm", "dawnguard.esm", "hearthfire.esm", "dragonborn.esm"}
PLUGIN_SUFFIXES = {".esp", ".esm", ".esl"}


def collect(stage: Path) -> list[Path]:
    return sorted(p for p in stage.rglob("*") if p.is_file())


def validate(stage: Path, files: list[Path], name: str, allow_dll: bool, allow_psc: bool) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []

    if not files:
        errors.append(f"папка {stage} пуста — упаковывать нечего")
        return errors, warnings

    rel = [p.relative_to(stage) for p in files]
    plugins = [p for p in rel if p.suffix.lower() in PLUGIN_SUFFIXES and p.name.lower() not in MASTERS]
    if not plugins:
        errors.append("в корне архива нет плагина (.esp/.esm/.esl) — такой мод игра не загрузит")
    for plugin in plugins:
        if plugin.parent != Path("."):
            warnings.append(f"плагин лежит не в корне архива: {plugin} (менеджер модов ждёт его в корне)")
        if name and not plugin.stem.lower().startswith(name.lower()):
            warnings.append(f"имя плагина {plugin.name} не начинается с {name} — проверь, что это нужный файл")

    for part in rel:
        lower = part.name.lower()
        if lower in MASTERS:
            errors.append(f"мастер-файл игры внутри архива: {part} — его нельзя распространять")
        if any(seg.lower() == "data" for seg in part.parts[:-1]):
            errors.append(f"внутри stage есть папка Data: {part}; файлы должны быть уже относительно Data")
        if part.suffix.lower() == ".exe":
            errors.append(f"исполняемый файл в архиве: {part}")
        if part.suffix.lower() == ".dll" and not allow_dll:
            errors.append(f"DLL в архиве: {part}; если это намеренно (SKSE-плагин) — добавь --allow-dll")
        if part.suffix.lower() == ".psc" and not allow_psc:
            errors.append(f"исходник .psc в архиве: {part}; игроку нужен только .pex (--allow-psc, если так задумано)")
        if part.suffix.lower() == ".bsa":
            warnings.append(f"BSA-архив внутри мода: {part} — убедись, что он собран из своих ресурсов")

    if not any(p.suffix.lower() == ".pex" for p in rel) and any(p.suffix.lower() in PLUGIN_SUFFIXES for p in rel):
        warnings.append("нет ни одного .pex — это нормально, только если в моде нет Papyrus-скриптов")

    return errors, warnings


def write_zip(stage: Path, files: list[Path], out: Path, name: str, version: str) -> list[dict[str, str]]:
    out.parent.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, str]] = []
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            arcname = path.relative_to(stage).as_posix()
            archive.write(path, arcname)
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            entries.append({"path": arcname, "sha256": digest, "size": str(path.stat().st_size)})
    manifest = {
        "name": name,
        "version": version,
        "archive": out.name,
        "layout": "Data-relative",
        "files": entries,
    }
    out.with_suffix(out.suffix + ".manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description="Упаковать мод в устанавливаемый zip.")
    parser.add_argument("--stage", required=True, help="папка с содержимым Data (плагин в корне)")
    parser.add_argument("--name", required=True, help="имя мода, оно же префикс плагина")
    parser.add_argument("--version", required=True, help="версия для имени файла")
    parser.add_argument("--out-dir", default=str(ROOT / "dist"), help="куда положить zip (по умолчанию dist/)")
    parser.add_argument("--allow-dll", action="store_true", help="разрешить DLL (SKSE-плагин)")
    parser.add_argument("--allow-psc", action="store_true", help="разрешить исходники .psc в архиве")
    parser.add_argument("--dry-run", action="store_true", help="только проверить, ничего не записывать")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    stage = Path(args.stage)
    if not stage.is_dir():
        print(f"Ошибка: папки {stage} нет. Сначала собери артефакты в build/stage.")
        return 2

    files = collect(stage)
    errors, warnings = validate(stage, files, args.name, args.allow_dll, args.allow_psc)

    print(f"Stage: {stage}")
    for path in files:
        print(f"  {path.relative_to(stage).as_posix()}")
    for warning in warnings:
        print(f"[внимание] {warning}")
    for error in errors:
        print(f"[ошибка]   {error}")

    if errors:
        print("\nАрхив не собран: сначала исправь ошибки выше.")
        return 1
    if args.dry_run:
        print("\nПроверка пройдена (--dry-run): архив не записан.")
        return 0

    out = Path(args.out_dir) / f"{args.name}-{args.version}.zip"
    entries = write_zip(stage, files, out, args.name, args.version)
    print(f"\nГотово: {out} ({len(entries)} файлов)")
    print("Рядом лежит .manifest.json со списком файлов и sha256.")
    print("Проверь установку в отдельном профиле менеджера модов, а не в основном.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
