"""Проверка «ни строки без источника»: аудит-таблица и реестр допущений должны совпадать с кодом.

Что проверяется:
1. каждая функция из native/src/*.cpp упомянута в docs/reference/CODE_AUDIT.md;
2. каждая метка `AS-n` в коде есть в docs/reference/ASSUMPTIONS.md и у неё заполнен статус;
3. каждая запись реестра имеет строку «Проверяется» (чем именно подтверждается в логе).

Использование:
    python tools/verify_audit.py
Код возврата: 0 — всё сходится, 1 — есть расхождения.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NATIVE_SRC = ROOT / "native" / "src"
AUDIT = ROOT / "docs" / "reference" / "CODE_AUDIT.md"
ASSUMPTIONS = ROOT / "docs" / "reference" / "ASSUMPTIONS.md"

STATUSES = ("открыто", "подтверждено", "опровергнуто")

FUNC_DEF = re.compile(r"^\t(?:[A-Za-z_][\w:<>,\*& ]*?)\b([A-Za-z_]\w*)\s*\(")
AS_TAG = re.compile(r"\bAS-(\d+)\b")


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return path.name


def collect_functions(paths: list[Path]) -> dict[str, str]:
    found: dict[str, str] = {}
    for path in paths:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line.startswith("\t") or line.startswith("\t\t"):
                continue
            if line.rstrip().endswith((";", ",", "(")) or ";" in line.split("(")[0]:
                continue
            match = FUNC_DEF.match(line)
            if match:
                found.setdefault(match.group(1), display_path(path))
    # Вложенные классы/структуры и лямбды не функции верхнего уровня — отфильтруем по ключевым словам C++.
    for junk in ("if", "for", "while", "switch", "return", "class", "struct", "namespace", "case"):
        found.pop(junk, None)
    return found


def parse_assumptions(path: Path) -> dict[str, dict[str, str]]:
    rows: dict[str, dict[str, str]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| AS-"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 5:
            continue
        rows[cells[0]] = {
            "claim": cells[1],
            "checked_by": cells[3],
            "status": cells[4],
        }
    return rows


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Сверить код с аудит-таблицей и реестром допущений.")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    problems: list[str] = []

    sources = sorted(NATIVE_SRC.glob("*.cpp")) + sorted(NATIVE_SRC.glob("*.h"))
    if not sources:
        print("нет исходников C++")
        return 1

    audit_text = AUDIT.read_text(encoding="utf-8")
    assumptions = parse_assumptions(ASSUMPTIONS)

    functions = collect_functions(sources)
    missing_funcs = [name for name in functions if name not in audit_text]
    if missing_funcs:
        for name in sorted(missing_funcs):
            problems.append(f"функция {name} ({functions[name]}) не описана в CODE_AUDIT.md")

    code_tags: dict[str, list[str]] = {}
    for path in sources:
        for match in AS_TAG.finditer(path.read_text(encoding="utf-8", errors="replace")):
            code_tags.setdefault(f"AS-{match.group(1)}", []).append(display_path(path))

    for tag in sorted(code_tags):
        if tag not in assumptions:
            problems.append(f"в коде есть {tag} ({', '.join(sorted(set(code_tags[tag])))}) — нет в ASSUMPTIONS.md")

    for tag, row in assumptions.items():
        if row["status"] not in STATUSES:
            problems.append(f"{tag}: неизвестный статус «{row['status']}», допустимы {', '.join(STATUSES)}")
        if len(row["checked_by"]) < 3:
            problems.append(f"{tag}: не указано, чем проверяется")
        if tag not in code_tags:
            problems.append(f"{tag}: запись в реестре есть, но в коде метки нет (мёртвое допущение)")

    if problems:
        print("АУДИТ НЕ ПРОЙДЕН:")
        for problem in problems:
            print(f"  - {problem}")
        return 1

    if not args.quiet:
        print(f"функций в коде: {len(functions)}, все описаны в CODE_AUDIT.md")
        print(f"допущений в реестре: {len(assumptions)}, все имеют статус и способ проверки")
        print("все метки AS-n из кода присутствуют в реестре")
    return 0


if __name__ == "__main__":
    sys.exit(main())
