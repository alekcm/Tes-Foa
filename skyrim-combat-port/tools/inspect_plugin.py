#!/usr/bin/env python3
"""Проверка заголовка плагина: мастер-файлы, ESL-флаг, версия.

Зачем: перед релизом убедиться, что мод зависит только от игры и официальных DLC,
а не от случайно подключённого чужого мода. Именно из-за такого мастера у игрока
мод «не запускается», хотя архив скачан правильно.

Читает только заголовок TES4 (24 байта) и его подзаписи MAST. Полноценным
редактором плагинов не является.

Примеры:
    python tools/inspect_plugin.py build/TGCombatPort.esp
    python tools/inspect_plugin.py dist/TGCombatPort-0.1.0.zip --strict
    python tools/inspect_plugin.py build/TGCombatPort.esp --json
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zipfile
import zlib
from pathlib import Path

HEADER_SIZE = 24
COMPRESSED_FLAG = 0x00040000
ESL_FLAG = 0x00000200

VANILLA_MASTERS = {
    "skyrim.esm",
    "update.esm",
    "dawnguard.esm",
    "hearthfire.esm",
    "dragonborn.esm",
}


def read_header(data: bytes) -> tuple[str, int, int, int]:
    if len(data) < HEADER_SIZE:
        raise ValueError("файл короче 24 байт — это не плагин")
    record_type, size, flags, form_id = struct.unpack_from("<4sIII", data, 0)
    return record_type.decode("latin-1"), size, flags, form_id


def read_masters(data: bytes) -> list[str]:
    record_type, size, flags, _ = read_header(data)
    if record_type != "TES4":
        raise ValueError(f"первая запись не TES4, а {record_type!r}")

    payload = data[HEADER_SIZE : HEADER_SIZE + size]
    if len(payload) < size:
        raise ValueError("файл обрезан: заголовок TES4 короче заявленного размера")
    if flags & COMPRESSED_FLAG:
        try:
            payload = zlib.decompress(payload)
        except zlib.error as error:
            raise ValueError(f"не удалось распаковать заголовок: {error}") from error

    masters: list[str] = []
    offset = 0
    while offset + 6 <= len(payload):
        sub_type = payload[offset : offset + 4].decode("latin-1")
        sub_size = struct.unpack_from("<H", payload, offset + 4)[0]
        start = offset + 6
        end = start + sub_size
        if end > len(payload):
            break
        if sub_type == "MAST":
            masters.append(payload[start:end].split(b"\x00", 1)[0].decode("cp1252", errors="replace"))
        offset = end
    return masters


def describe(data: bytes) -> dict[str, object]:
    record_type, size, flags, form_id = read_header(data)
    masters = read_masters(data)
    return {
        "record_type": record_type,
        "size": size,
        "form_id": f"{form_id:08X}",
        "compressed": bool(flags & COMPRESSED_FLAG),
        "esl": bool(flags & ESL_FLAG),
        "masters": masters,
        "unexpected_masters": [name for name in masters if name.lower() not in VANILLA_MASTERS],
    }


def candidates(path: Path) -> list[tuple[str, bytes]]:
    if path.suffix.lower() == ".zip":
        found: list[tuple[str, bytes]] = []
        with zipfile.ZipFile(path) as archive:
            for name in archive.namelist():
                if name.lower().endswith((".esp", ".esm", ".esl")):
                    found.append((name, archive.read(name)))
        return found
    return [(path.name, path.read_bytes())]


def main() -> int:
    parser = argparse.ArgumentParser(description="Показать мастер-файлы и флаги плагина.")
    parser.add_argument("path", help=".esp/.esl/.esm или zip с ними")
    parser.add_argument("--strict", action="store_true", help="код 1, если есть не-ванильные мастер-файлы")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    path = Path(args.path)
    if not path.is_file():
        print(f"Ошибка: файла {path} нет")
        return 2

    reports = []
    try:
        for name, data in candidates(path):
            reports.append({"file": name, **describe(data)})
    except (ValueError, struct.error, zipfile.BadZipFile) as error:
        print(f"Ошибка: {error}")
        return 2

    if not reports:
        print("В архиве не найдено ни одного плагина.")
        return 2

    if args.json:
        print(json.dumps(reports, ensure_ascii=False, indent=2))
    else:
        for report in reports:
            print(f"Плагин: {report['file']}")
            print(f"  ESL-флаг: {'да' if report['esl'] else 'нет'}")
            print(f"  Мастер-файлы ({len(report['masters'])}):")
            for master in report["masters"]:
                print(f"    - {master}")
            unexpected = report["unexpected_masters"]
            if unexpected:
                print(f"  [!] Не из базовой игры и DLC: {', '.join(unexpected)}")
                print("      Такой мод не запустится без этих плагинов у игрока.")
            else:
                print("  Зависимости только от игры и официальных DLC.")

    if args.strict and any(report["unexpected_masters"] for report in reports):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
