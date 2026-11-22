#!/usr/bin/env python3
"""Смотрит внутрь собранного .esp: записи, флаг «Start Game Enabled», привязанные скрипты.

Зачем: проверить плагин можно только текстом — присылать .esp нельзя. Этот скрипт
читает записи сам и печатает короткий отчёт: какие записи есть, у квеста стоит ли
флаг Start Game Enabled, встречаются ли в записи квеста имена наших скриптов,
а по ключу --game-dir — лежат ли .pex в Data\\Scripts и включён ли плагин в plugins.txt.

Полноценным редактором плагинов не является: разбирает заголовок, группы и подзаписи,
а имена скриптов ищет в данных записи как строки. Точное место скрипта (алиас или
сам квест) по отчёту не различить — для этого есть запись разбора в docs/.

Примеры:
    python tools/inspect_records.py src/plugin/TGCombatPort.esp
    python tools/inspect_records.py src/plugin/TGCombatPort.esp --json
    python tools/inspect_records.py src/plugin/TGCombatPort.esp --game-dir "A:\\...\\Skyrim Special Edition"
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import zlib
from pathlib import Path
from typing import Iterator, Sequence

RECORD_HEADER = 24
GROUP_HEADER_SEEDS = (24, 20)
COMPRESSED_FLAG = 0x00040000
START_GAME_ENABLED = 0x01

GLOB_EDID = "TGCP_PlayerBlocking"
QUEST_EDID = "TGCP_MainQuest"
ALIAS_NAME = "PlayerAlias"
SCRIPT_NAMES = ("TGCP_PlayerBlockWatcher", "TGCP_AnimEventProbe")
PROPERTY_NAME = "PlayerBlocking"
PEX_NAMES = (
    "TGCP_AnimEventProbe",
    "TGCP_PackLeapState",
    "TGCP_PlayerBlockWatcher",
    "TGCP_WebSpiderAbility",
)


class FormatError(ValueError):
    """.esp не похож на плагин или обрезан."""


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_record(data: bytes, offset: int, end: int) -> tuple[str, int, bytes, int]:
    """Возвращает (тип, formID, распакованные данные, позиция после записи)."""
    if offset + RECORD_HEADER > end:
        raise FormatError(f"запись на позиции {offset} выходит за границу файла")
    record_type = data[offset : offset + 4].decode("latin-1")
    size = u32(data, offset + 4)
    flags = u32(data, offset + 8)
    form_id = u32(data, offset + 12)
    payload_start = offset + RECORD_HEADER
    payload_end = payload_start + size
    if payload_end > end:
        raise FormatError(f"запись {record_type} на позиции {offset} обрезана")
    payload = data[payload_start:payload_end]
    if flags & COMPRESSED_FLAG:
        try:
            payload = zlib.decompress(payload)
        except zlib.error as error:
            raise FormatError(f"не удалось распаковать {record_type}: {error}") from error
    return record_type, form_id, payload, payload_end


def subrecords(payload: bytes) -> list[tuple[str, bytes]]:
    """Подзаписи внутри записи; проверяет, что они покрывают данные ровно."""
    result: list[tuple[str, bytes]] = []
    offset = 0
    while offset < len(payload):
        if offset + 6 > len(payload):
            raise FormatError("обрезанный заголовок подзаписи")
        name = payload[offset : offset + 4].decode("latin-1")
        size = u16(payload, offset + 4)
        start = offset + 6
        stop = start + size
        if stop > len(payload):
            raise FormatError(f"подзапись {name} выходит за данные записи")
        result.append((name, payload[start:stop]))
        offset = stop
    return result


def editor_id(payload: bytes) -> str | None:
    for name, value in subrecords(payload):
        if name == "EDID":
            return value.split(b"\x00", 1)[0].decode("cp1252", errors="replace")
    return None


def walk(data: bytes) -> Iterator[dict[str, object]]:
    """Обходит все записи, включая вложенные в группы."""
    if len(data) < RECORD_HEADER or data[:4] != b"TES4":
        raise FormatError("файл не начинается с записи TES4 — это не плагин")
    _, _, header_payload, offset = read_record(data, 0, len(data))
    yield {"type": "TES4", "form_id": 0, "payload": header_payload, "depth": 0}
    yield from walk_sequence(data, offset, len(data), depth=0)


def walk_sequence(data: bytes, start: int, end: int, depth: int) -> Iterator[dict[str, object]]:
    offset = start
    while offset < end:
        if data[offset : offset + 4] == b"GRUP":
            size = u32(data, offset + 4)
            group_end = offset + size
            if size < GROUP_HEADER_SEEDS[-1] or group_end > end:
                raise FormatError(f"группа на позиции {offset} обрезана")
            children, used_header = parse_group_children(data, offset, group_end)
            for child in children:
                child["depth"] = depth + 1
                yield child
            offset = used_header
            continue
        record_type, form_id, payload, offset = read_record(data, offset, end)
        yield {"type": record_type, "form_id": form_id, "payload": payload, "depth": depth}


def parse_group_children(data: bytes, offset: int, group_end: int) -> tuple[list[dict[str, object]], int]:
    """Размер заголовка группы в SSE — 24 байта; на всякий случай пробуем 20."""
    last_error: Exception | None = None
    for header_size in GROUP_HEADER_SEEDS:
        try:
            children = list(walk_sequence(data, offset + header_size, group_end, depth=0))
        except FormatError as error:
            last_error = error
            continue
        return children, group_end
    raise FormatError(f"не удалось разобрать группу на позиции {offset}: {last_error}")


def parse(data: bytes) -> list[dict[str, object]]:
    nodes = list(walk(data))
    for node in nodes:
        if node["type"] == "TES4":
            node["edid"] = None
            continue
        try:
            node["edid"] = editor_id(node["payload"])  # type: ignore[arg-type]
        except FormatError:
            node["edid"] = None
    return nodes


def masters_of(data: bytes) -> list[str]:
    header = next(node for node in walk(data) if node["type"] == "TES4")
    masters: list[str] = []
    for name, value in subrecords(header["payload"]):  # type: ignore[arg-type]
        if name == "MAST":
            masters.append(value.split(b"\x00", 1)[0].decode("cp1252", errors="replace"))
    return masters


def quest_flags(payload: bytes) -> int | None:
    for name, value in subrecords(payload):
        if name == "DNAM" and value:
            return value[0]
    return None


def contains(payload: bytes, text: str) -> bool:
    return text.encode("ascii", errors="ignore") in payload


def check(data: bytes) -> list[tuple[bool, str]]:
    """Пункты проверки: (выполнено, текст)."""
    nodes = parse(data)
    glob = [n for n in nodes if n["type"] == "GLOB" and n["edid"] == GLOB_EDID]
    quest = [n for n in nodes if n["type"] == "QUST" and n["edid"] == QUEST_EDID]

    result: list[tuple[bool, str]] = []
    result.append((bool(glob), f"глобальная переменная {GLOB_EDID}"))
    result.append((bool(quest), f"квест {QUEST_EDID}"))

    if quest:
        payload = quest[0]["payload"]
        assert isinstance(payload, bytes)
        flags = quest_flags(payload)
        start = flags is not None and bool(flags & START_GAME_ENABLED)
        value = "нет данных" if flags is None else f"байт флагов 0x{flags:02x}"
        result.append((start, f"у квеста стоит Start Game Enabled ({value})"))
        result.append((contains(payload, ALIAS_NAME), f"в записи квеста есть имя алиаса {ALIAS_NAME}"))
        for script in SCRIPT_NAMES:
            result.append((contains(payload, script), f"в записи квеста есть скрипт {script}"))
        result.append(
            (contains(payload, PROPERTY_NAME), f"в записи квеста есть свойство {PROPERTY_NAME}")
        )
    else:
        result.append((False, "Start Game Enabled — проверить нельзя, квеста нет"))
        result.append((False, f"алиас {ALIAS_NAME} — проверить нельзя, квеста нет"))
        for script in SCRIPT_NAMES:
            result.append((False, f"скрипт {script} — проверить нельзя, квеста нет"))
        result.append((False, f"свойство {PROPERTY_NAME} — проверить нельзя, квеста нет"))
    return result


def check_files(game_dir: Path, plugin_name: str, plugins_txt: Path | None) -> list[tuple[bool, str]]:
    result: list[tuple[bool, str]] = []
    esp = game_dir / "Data" / plugin_name
    result.append((esp.is_file(), f"плагин {plugin_name} лежит в Data игры"))
    scripts = game_dir / "Data" / "Scripts"
    missing = [name for name in PEX_NAMES if not (scripts / f"{name}.pex").is_file()]
    result.append(
        (not missing, f"в Data\\Scripts лежат все 4 скрипта .pex" + (f" (нет: {', '.join(missing)})" if missing else ""))
    )
    if plugins_txt is not None:
        if not plugins_txt.is_file():
            result.append((False, f"файла plugins.txt нет: {plugins_txt}"))
        else:
            text = plugins_txt.read_text(encoding="utf-8", errors="replace")
            entries = [
                line.strip().lstrip("*").lower()
                for line in text.splitlines()
                if line.strip() and not line.strip().startswith("#")
            ]
            enabled = plugin_name.lower() in entries
            result.append((enabled, f"плагин включён в plugins.txt"))
    return result


def default_plugins_txt() -> Path | None:
    if sys.platform != "win32":
        return None
    local = os.environ.get("LOCALAPPDATA")
    if not local:
        return None
    return Path(local) / "Skyrim Special Edition" / "plugins.txt"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Показать записи .esp и проверить сборку мода.")
    parser.add_argument("plugin", help="путь к .esp")
    parser.add_argument("--game-dir", help="папка игры, чтобы проверить Data\\Scripts и plugins.txt")
    parser.add_argument("--plugins-txt", help="путь к plugins.txt (по умолчанию берётся из папки LOCALAPPDATA)")
    parser.add_argument("--json", action="store_true", help="вывод в JSON")
    args = parser.parse_args(argv)

    path = Path(args.plugin).expanduser()
    if not path.is_file():
        print(f"Файла нет: {path}")
        return 2

    data = path.read_bytes()
    try:
        nodes = parse(data)
    except FormatError as error:
        print(f"Не смог прочитать плагин: {error}")
        return 2

    report: dict[str, object] = {
        "plugin": str(path),
        "size": len(data),
        "masters": masters_of(data),
        "records": [
            {"type": node["type"], "form_id": f"{node['form_id']:08X}", "edid": node.get("edid")}
            for node in nodes
            if node["type"] != "TES4"
        ],
        "checks": [{"ok": ok, "text": text} for ok, text in check(data)],
    }
    if args.game_dir:
        game_dir = Path(args.game_dir).expanduser()
        plugins_txt = Path(args.plugins_txt).expanduser() if args.plugins_txt else default_plugins_txt()
        report["files"] = [
            {"ok": ok, "text": text}
            for ok, text in check_files(game_dir, path.name, plugins_txt)
        ]

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0

    print(f"Плагин: {path} ({len(data)} байт)")
    print("Мастер-файлы: " + (", ".join(report["masters"]) or "нет"))  # type: ignore[arg-type]
    print("Записи:")
    if not report["records"]:  # type: ignore[truthy-function]
        print("  (ни одной — плагин пустой)")
    for record in report["records"]:  # type: ignore[union-attr]
        edid = f" {record['edid']}" if record["edid"] else ""
        print(f"  {record['type']} {record['form_id']}{edid}")

    print("Проверка сборки мода:")
    failed = 0
    for ok, text in check(data):
        print(f"  [{'ok' if ok else 'нет'}] {text}")
        failed += 0 if ok else 1

    if args.game_dir:
        print(f"Проверка установки в {args.game_dir}:")
        for ok, text in report["files"]:  # type: ignore[union-attr]
            print(f"  [{'ok' if ok else 'нет'}] {text}")
            failed += 0 if ok else 1

    print()
    print("Итог: " + ("всё на месте" if failed == 0 else f"не хватает {failed} пункт(ов) — см. выше"))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
