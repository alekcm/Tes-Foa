#!/usr/bin/env python3
"""Разбор ESP/ESM/ESL: записи, имена, заклинания, эффекты, настройки.

Зачем: числа и списки берём из модов, а не из головы. Никаких догадок — только то, что
лежит в файле.

Формат (проверено по файлам Bethesda):
  * заголовок записи — 24 байта в Skyrim SE (signature, size, flags, formID, versionControl,
    formVersion u16, unknown u16) и 20 байт в старых файлах; парсер пробует оба и выбирает тот,
    при котором подзаписи читаются без мусора;
  * GRUP — контейнер: signature, size (включая весь 24-байтовый заголовок группы), дальше записи;
  * запись с флагом 0x00040000 сжата zlib, в начале 4 байта — размер распакованных данных.

Использование:
    python tools/esp_dump.py <файл.esp> [--json out.json] [--type SPEL] [--grep текст]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

RECORD_FLAG_COMPRESSED = 0x0004_0000
HEADER_SIZES = (24, 20)
# Заголовок GRUP в Skyrim: signature, size, label, groupType, stamp+unknown, version+unknown.
GRUP_HEADER_SIZE = 24


def decode_string(raw: bytes) -> str:
    """Строки в модах бывают cp1251 (русские переводы) и utf-8."""
    for encoding in ("utf-8", "cp1251"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace")


def split_subrecords(data: bytes) -> dict[str, list[bytes]]:
    """Подзаписи: 4 символа тип, uint16 длина, данные."""
    fields: dict[str, list[bytes]] = {}
    offset = 0
    while offset + 6 <= len(data):
        kind = data[offset : offset + 4].decode("ascii", errors="replace")
        (size,) = struct.unpack_from("<H", data, offset + 4)
        offset += 6
        if offset + size > len(data):
            raise ValueError("подзапись выходит за границу записи")
        fields.setdefault(kind, []).append(data[offset : offset + size])
        offset += size
    if offset != len(data):
        raise ValueError("хвост данных не разобран")
    return fields


class Record:
    def __init__(self, sig: str, form_id: int, flags: int, fields: dict[str, list[bytes]]):
        self.sig = sig
        self.form_id = form_id
        self.flags = flags
        self.fields = fields

    @property
    def editor_id(self) -> str:
        raw = self.fields.get("EDID")
        return decode_string(raw[0]).rstrip("\x00") if raw else ""

    @property
    def name(self) -> str:
        raw = self.fields.get("FULL")
        return decode_string(raw[0]).rstrip("\x00") if raw else ""

    def to_dict(self) -> dict:
        out = {
            "type": self.sig,
            "formid": f"{self.form_id:08X}",
            "edid": self.editor_id,
            "name": self.name,
        }
        if self.sig == "SPEL":
            out["effects"] = decode_spell_effects(self)
            out["spell_data"] = decode_spell_data(self)
        if self.sig == "MGEF":
            out["mgef"] = decode_mgef(self)
        if self.sig == "GMST":
            out["value"] = decode_gmst(self)
        if self.sig == "GLOB":
            out["value"] = decode_glob(self)
        if self.sig in ("NPC_", "RACE", "PERK", "KYWD", "QUST"):
            out["form_ids"] = sorted({f"{form_id:08X}" for form_id in referenced_form_ids(self.fields)})
        return out


def decode_gmst(record: Record) -> object:
    data = record.fields.get("DATA")
    if not data:
        return None
    raw = data[0]
    if len(raw) == 4:
        return {"float": struct.unpack("<f", raw)[0]}
    return {"string": decode_string(raw).rstrip("\x00")}


def decode_glob(record: Record) -> object:
    raw = record.fields.get("FLTV")
    if raw and len(raw[0]) >= 4:
        return {"float": round(struct.unpack_from("<f", raw[0], 0)[0], 3)}
    return None


def decode_spell_data(record: Record) -> dict:
    data = record.fields.get("SPIT")
    if not data:
        return {}
    raw = data[0]
    if len(raw) < 16:
        return {"raw": raw.hex()}
    spell_type, cost, level, flags = struct.unpack_from("<IIII", raw, 0)
    return {"spell_type": spell_type, "cost": cost, "level": level, "flags": f"{flags:08X}"}


def decode_spell_effects(record: Record) -> list[dict]:
    """В SPEL эффекты идут парами: EFID (FormID эффекта) + EFIT (числа)."""
    effects: list[dict] = []
    ids = record.fields.get("EFID", [])
    items = record.fields.get("EFIT", [])
    for index, form_id_raw in enumerate(ids):
        (form_id,) = struct.unpack("<I", form_id_raw[:4])
        effect: dict = {"mgef": f"{form_id:08X}"}
        if index < len(items) and len(items[index]) >= 12:
            magnitude, area, duration = struct.unpack_from("<fii", items[index], 0)
            effect.update({"magnitude": round(magnitude, 3), "area": area, "duration": duration})
        effects.append(effect)
    return effects


def decode_mgef(record: Record) -> dict:
    """MGEF: показываем сырые данные (полная расшифровка структуры не нужна) и связанные формы."""
    data = record.fields.get("DATA")
    out: dict = {}
    if data:
        out["data_hex"] = data[0].hex()
        if len(data[0]) >= 8:
            out["flags"] = f"{struct.unpack_from('<I', data[0], 0)[0]:08X}"
            out["base_cost"] = round(struct.unpack_from("<f", data[0], 4)[0], 3)
    return out


def referenced_form_ids(fields: dict[str, list[bytes]]) -> set[int]:
    found: set[int] = set()
    for kind, values in fields.items():
        # Поля, которые заведомо не FormID, чтобы не сыпать мусором.
        if kind in ("EDID", "FULL", "DESC", "CNAM", "SNAM", "NAM1", "TNAM", "BNAM", "DATA"):
            continue
        for raw in values:
            if len(raw) == 4:
                (value,) = struct.unpack("<I", raw)
                if 0 < value < 0xFF000000:
                    found.add(value)
    return found


def parse_records(data: bytes, start: int, end: int, header_size: int) -> list[Record]:
    records: list[Record] = []
    offset = start
    while offset + 8 <= end:
        sig = data[offset : offset + 4].decode("ascii", errors="replace")
        (size,) = struct.unpack_from("<I", data, offset + 4)
        if sig == "GRUP":
            if size < GRUP_HEADER_SIZE or offset + size > end:
                raise ValueError(
                    f"битый GRUP на смещении {offset}: size={size}, конец группы={end}"
                )
            records.extend(parse_records(data, offset + GRUP_HEADER_SIZE, offset + size, header_size))
            offset += size
            continue
        if offset + header_size + size > end:
            (fail_flags,) = struct.unpack_from("<I", data, offset + 8)
            (fail_id,) = struct.unpack_from("<I", data, offset + 12)
            raise ValueError(
                f"запись выходит за границу группы: смещение={offset}, sig={sig!r}, "
                f"size={size}, flags={fail_flags:08X}, formID={fail_id:08X}, "
                f"заголовок={header_size}, конец группы={end}"
            )
        (flags,) = struct.unpack_from("<I", data, offset + 8)
        (form_id,) = struct.unpack_from("<I", data, offset + 12)
        payload = data[offset + header_size : offset + header_size + size]
        if flags & RECORD_FLAG_COMPRESSED:
            (unpacked_size,) = struct.unpack_from("<I", payload, 0)
            payload = zlib.decompress(payload[4:])
            if len(payload) != unpacked_size:
                raise ValueError("размер распакованных данных не совпал")
        fields = split_subrecords(payload)
        records.append(Record(sig, form_id, flags, fields))
        offset += header_size + size
    return records


def parse_plugin(path: Path) -> list[Record]:
    data = path.read_bytes()
    if data[:4] != b"TES4":
        raise ValueError(f"{path}: это не плагин Bethesda (нет TES4)")
    last_error: Exception | None = None
    for header_size in HEADER_SIZES:
        try:
            return parse_records(data, 0, len(data), header_size)
        except (ValueError, struct.error, zlib.error) as error:  # пробуем другой размер заголовка
            last_error = error
    raise ValueError(f"{path}: не удалось разобрать ({last_error})")


def main() -> int:
    parser = argparse.ArgumentParser(description="Разбор ESP/ESM/ESL")
    parser.add_argument("plugin", type=Path)
    parser.add_argument("--json", type=Path, help="куда сохранить JSON")
    parser.add_argument("--type", dest="only_type", help="только записи этого типа, например SPEL")
    parser.add_argument("--grep", help="подстрока в EditorID или названии (без учёта регистра)")
    args = parser.parse_args()

    records = parse_plugin(args.plugin)
    selected = records
    if args.only_type:
        selected = [r for r in selected if r.sig == args.only_type.upper()]
    if args.grep:
        needle = args.grep.lower()
        selected = [r for r in selected if needle in r.editor_id.lower() or needle in r.name.lower()]

    by_type: dict[str, int] = {}
    for record in records:
        by_type[record.sig] = by_type.get(record.sig, 0) + 1

    print(f"файл: {args.plugin}")
    print(f"записей: {len(records)}")
    print("по типам: " + ", ".join(f"{k}={v}" for k, v in sorted(by_type.items(), key=lambda kv: -kv[1])[:15]))
    print(f"показано: {len(selected)}")

    for record in selected:
        print(f"  {record.sig} {record.form_id:08X}  {record.editor_id:<40} {record.name}")

    if args.json:
        payload = [record.to_dict() for record in selected if record.sig in ("SPEL", "MGEF", "GMST", "PERK", "NPC_", "KYWD")]
        args.json.write_text(json.dumps(payload, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"JSON: {args.json} ({len(payload)} записей)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
