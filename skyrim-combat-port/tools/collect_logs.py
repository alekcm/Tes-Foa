#!/usr/bin/env python3
"""Достаёт из Papyrus-лога строки мода и ошибки в короткий отчёт.

Зачем: полный Papyrus.0.log бывает на тысячи строк, и присылать его целиком бессмысленно.
Скрипт оставляет только строки с меткой мода ([TGCP]) и строки с ошибками, добавляет
шапку с датой, путём к логу и количеством строк. Отчёт можно приложить в чат.

Примеры:
    python tools/collect_logs.py
    python tools/collect_logs.py --log "D:\\logs\\Papyrus.0.log"
    python tools/collect_logs.py --pattern "TGCP|error" --out logs/report.txt
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PATTERN = r"\[TGCP\]"
MAX_LINES = 400


def default_log_path() -> Path | None:
    if sys.platform != "win32":
        return None
    documents = Path(os.environ.get("USERPROFILE", str(Path.home()))) / "Documents"
    return documents / "My Games" / "Skyrim Special Edition" / "Logs" / "Script" / "Papyrus.0.log"


def collect(lines: list[str], pattern: str, limit: int = MAX_LINES) -> tuple[list[str], int]:
    """Возвращает выбранные строки с номерами и полное число совпадений."""
    regex = re.compile(pattern)
    error = re.compile(r"\berror\b", re.IGNORECASE)
    selected: list[str] = []
    total = 0
    for number, raw in enumerate(lines, start=1):
        line = raw.rstrip("\r\n")
        if regex.search(line) or error.search(line):
            total += 1
            if len(selected) < limit:
                selected.append(f"{number:>7}: {line}")
    return selected, total


def build_report(log_path: Path, pattern: str, selected: list[str], total: int, scanned: int) -> str:
    header = [
        "# Отчёт по Papyrus-логу",
        f"Дата: {dt.datetime.now().isoformat(timespec='seconds')}",
        f"Лог: {log_path}",
        f"Шаблон: {pattern} (плюс строки со словом error)",
        f"Строк прочитано: {scanned}",
        f"Совпадений: {total}" + (f", показаны первые {len(selected)}" if total > len(selected) else ""),
        "",
    ]
    return "\n".join(header + selected) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Собрать строки мода и ошибки из Papyrus-лога.")
    parser.add_argument("--log", help="путь к Papyrus.0.log (по умолчанию — стандартный путь Windows)")
    parser.add_argument("--pattern", default=DEFAULT_PATTERN, help=f"регулярное выражение, по умолчанию {DEFAULT_PATTERN}")
    parser.add_argument("--out", help="куда записать отчёт (по умолчанию logs/papyrus_report.txt)")
    parser.add_argument("--stdout", action="store_true", help="только напечатать, файл не создавать")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    log_path = Path(args.log) if args.log else default_log_path()
    if log_path is None:
        print("Не знаю, где лежит лог: укажи путь через --log.")
        print(r'Пример: python tools\collect_logs.py --log "C:\Users\ИМЯ\Documents\My Games\Skyrim Special Edition\Logs\Script\Papyrus.0.log"')
        return 2
    if not log_path.is_file():
        print(f"Файла нет: {log_path}")
        print("Проверь, что в Skyrim.ini включён лог: [Papyrus] bEnableLogging=1, bEnableTrace=1.")
        return 2

    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    selected, total = collect(lines, args.pattern)
    report = build_report(log_path, args.pattern, selected, total, len(lines))

    if args.stdout:
        print(report)
        return 0

    out = Path(args.out) if args.out else ROOT / "logs" / "papyrus_report.txt"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(report, encoding="utf-8")
    print(f"Прочитано строк: {len(lines)}; совпадений: {total}; в отчёт попало: {len(selected)}")
    print(f"Отчёт: {out}")
    print("Приложи этот файл в чат или вставь его текст. Полный лог присылать не нужно.")
    if total == 0:
        print("Совпадений нет: мод не загрузился, скрипты не запустились или логи выключены.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
