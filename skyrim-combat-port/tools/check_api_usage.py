#!/usr/bin/env python3
"""Сверка вызовов движка в native/src/main.cpp с заголовками CommonLibSSE.

Зачем: часть ошибок в плагине — не опечатки компилятора, а «уверенность», что в движке
есть такой тип, метод или константа. Компилятор ловит не всё, а «по памяти» ошибаться нельзя.

Что проверяется:
  1. Каждый `RE::Имя` (тип, класс, enum, namespace) объявлен в заголовках.
  2. Каждый `RE::Тип::Имя` (значение enum, вложенный тип, статический метод) встречается
     в заголовках рядом с этим типом.
  3. Печатается таблица: символ → файл:строка заголовка.

Чего НЕ делает: не проверяет сигнатуры (это компилятор) и не проверяет семантику.
Задача — не дать коду ссылаться на несуществующее.

Примеры:
    python tools/check_api_usage.py --headers /tmp/clib/include
    python tools/check_api_usage.py --headers lib/commonlibsse-ng/include --quiet
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "native" / "src" / "main.cpp"

QUALIFIED = re.compile(r"\bRE::([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)")

DECLARATION = re.compile(
    r"^[ \t]*(?:class|struct)[ \t]+(?:SKYRIM_REL_VR[ \t]+|SKYRIM_REL[ \t]+)?([A-Za-z_]\w*)"
    r"|^[ \t]*enum[ \t]+(?:class[ \t]+)?([A-Za-z_]\w*)"
    r"|^[ \t]*using[ \t]+([A-Za-z_]\w*)[ \t]*="
    r"|^[ \t]*typedef[^;]*?\b([A-Za-z_]\w*)[ \t]*;",
    re.MULTILINE,
)
NAMESPACE = re.compile(r"^[ \t]*namespace[ \t]+((?:RE::)?(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)", re.MULTILINE)


class Headers:
    """Индекс заголовков: где объявлен тип и где встречается имя."""

    def __init__(self, root: Path) -> None:
        self.root = root
        self.files: dict[Path, str] = {}
        for path in sorted(root.rglob("*.h")):
            try:
                self.files[path] = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
        self.types: dict[str, tuple[int, Path, int]] = {}
        for path, text in self.files.items():
            for match in DECLARATION.finditer(text):
                name = next((group for group in match.groups() if group), None)
                if not name:
                    continue
                line = text.count("\n", 0, match.start()) + 1
                rank = 0 if path.stem == name else 1
                current = self.types.get(name)
                candidate = (rank, path, line)
                if current is None or candidate[0] < current[0]:
                    self.types[name] = candidate
            for match in NAMESPACE.finditer(text):
                name = match.group(1).split("::")[-1]
                line = text.count("\n", 0, match.start()) + 1
                rank = 2
                current = self.types.get(name)
                candidate = (rank, path, line)
                if current is None or candidate[0] < current[0]:
                    self.types[name] = candidate

    def relative(self, path: Path) -> str:
        try:
            return str(path.relative_to(self.root.parent))
        except ValueError:
            return str(path)

    def locate(self, name: str) -> tuple[Path, int] | None:
        place = self.types.get(name)
        if place is None:
            return None
        _, path, line = place
        return path, line

    def locate_anywhere(self, name: str, prefer: Path | None = None) -> tuple[Path, int] | None:
        """Ищет имя как отдельное слово во всех заголовках; сначала — в «своём» файле."""
        pattern = re.compile(rf"\b{re.escape(name)}\b")
        order = ([prefer] if prefer else []) + [path for path in self.files if path != prefer]
        for path in order:
            text = self.files.get(path)
            if text is None:
                continue
            match = pattern.search(text)
            if match:
                return path, text.count("\n", 0, match.start()) + 1
        return None


def check(source: Path, headers: Headers) -> tuple[list[str], list[str]]:
    problems: list[str] = []
    rows: list[str] = []
    text = source.read_text(encoding="utf-8")

    seen: set[str] = set()
    for match in QUALIFIED.finditer(text):
        parts = match.group(1).split("::")
        head = parts[0]
        if head not in seen:
            seen.add(head)
            place = headers.locate(head)
            if place is None:
                problems.append(f"RE::{head} не объявлен ни в одном заголовке CommonLib")
                continue
            path, line = place
            rows.append(f"RE::{head} — {headers.relative(path)}:{line}")
        own = headers.locate(head)
        for member in parts[1:]:
            key = f"RE::{head}::{member}"
            if key in rows:
                continue
            place = headers.locate_anywhere(member, own[0] if own else None)
            if place is None:
                problems.append(f"{key} — такого имени нет ни в одном заголовке")
            else:
                path, line = place
                rows.append(f"{key} — {headers.relative(path)}:{line}")
    return problems, rows


def main() -> int:
    # Консоль Windows может быть в cp1252 — без этого печать кириллицы падает.
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass
    parser = argparse.ArgumentParser(description="Сверить вызовы движка в main.cpp с заголовками CommonLib.")
    parser.add_argument("--headers", required=True, help="путь к include/ из CommonLibSSE-NG")
    parser.add_argument("--source", default=str(SOURCE), help="файл плагина (по умолчанию main.cpp)")
    parser.add_argument("--quiet", action="store_true", help="не печатать таблицу, только ошибки")
    args = parser.parse_args()

    headers_root = Path(args.headers)
    if not headers_root.is_dir():
        print(f"нет папки заголовков: {headers_root}")
        return 2

    headers = Headers(headers_root)
    if not headers.files:
        print(f"в {headers_root} нет заголовков .h — путь к CommonLib неверный")
        return 2

    problems, rows = check(Path(args.source), headers)
    if problems:
        print("ПРОВЕРКА НЕ ПРОЙДЕНА:")
        for problem in problems:
            print(f"  - {problem}")
        return 1
    if not args.quiet:
        for row in rows:
            print(row)
    print(f"вызовы движка сверены с заголовками: {len(rows)} имён, ошибок нет")
    return 0


if __name__ == "__main__":
    sys.exit(main())
