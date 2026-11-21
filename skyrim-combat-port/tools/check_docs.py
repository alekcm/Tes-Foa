#!/usr/bin/env python3
"""Проверка локальных ссылок в Markdown.

Ищет ссылки вида [текст](цель) и проверяет, что файл/папка по относительному пути
существуют. Внешние ссылки (http/https/mailto) не проверяются: без сети это шум.

Примеры:
    python tools/check_docs.py
    python tools/check_docs.py --quiet
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parent.parent
LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
SCHEME = re.compile(r"^(?:[a-z][a-z0-9+.-]*:|//)", re.IGNORECASE)


def markdown_files(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("*.md") if ".git" not in path.parts)


def check_file(path: Path) -> list[str]:
    problems: list[str] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    for line_number, line in enumerate(text.splitlines(), start=1):
        for raw_target in LINK.findall(line):
            target = raw_target.strip()
            if not target or target.startswith("#") or SCHEME.match(target):
                continue
            clean = unquote(target.split("#", 1)[0].split("?", 1)[0])
            if not clean:
                continue
            resolved = (path.parent / clean).resolve()
            try:
                resolved.relative_to(ROOT)
            except ValueError:
                problems.append(f"{path.relative_to(ROOT)}:{line_number}: ссылка выходит за пределы проекта: {target}")
                continue
            if not resolved.exists():
                problems.append(f"{path.relative_to(ROOT)}:{line_number}: нет файла по ссылке: {target}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description="Проверить локальные ссылки в Markdown.")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    files = markdown_files(ROOT)
    problems: list[str] = []
    for path in files:
        problems.extend(check_file(path))

    for problem in problems:
        print(f"[ошибка] {problem}")

    if problems:
        print(f"\nБитых ссылок: {len(problems)}")
        return 1
    if not args.quiet:
        print(f"Ссылки в порядке: проверено {len(files)} файлов Markdown")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
