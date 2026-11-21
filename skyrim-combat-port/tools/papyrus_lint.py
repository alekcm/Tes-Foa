#!/usr/bin/env python3
"""Минимальная проверка Papyrus-исходников без компилятора.

Проверяет только то, что можно проверить текстом:
  - имя файла совпадает с ScriptName;
  - парность блоков Event/Function/If/While/State/Group;
  - отсутствие табов (в репозитории пробелы).

Это не замена PapyrusCompiler: типы, свойства и вызовы функций проверяет только он.
Задача линта — поймать очевидные опечатки до запуска компилятора.

Примеры:
    python tools/papyrus_lint.py src/papyrus
    python tools/papyrus_lint.py src/papyrus/TGCP_WebSpiderAbility.psc
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

OPENERS = {
    "Event": "EndEvent",
    "Function": "EndFunction",
    "If": "EndIf",
    "While": "EndWhile",
    "State": "EndState",
    "Group": "EndGroup",
}
CLOSERS = {closer: opener for opener, closer in OPENERS.items()}
SCRIPT_NAME = re.compile(r"^\s*ScriptName\s+(\w+)(?:\s+extends\s+(\w+))?\s*$", re.IGNORECASE)
WORD = re.compile(r"^\s*(\w+)")


def strip_comment(line: str) -> str:
    in_string = False
    for index, char in enumerate(line):
        if char == '"':
            in_string = not in_string
        elif char == ";" and not in_string:
            return line[:index]
    return line


def lint_file(path: Path) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    stack: list[tuple[str, int]] = []
    script_name: str | None = None
    extends: str | None = None

    for number, raw in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
        if "\t" in raw:
            errors.append(f"{path.name}:{number}: таб вместо пробелов")
        line = strip_comment(raw)
        if not line.strip():
            continue

        match = SCRIPT_NAME.match(line)
        if match:
            script_name = match.group(1)
            extends = match.group(2)
            continue

        word_match = WORD.match(line)
        if not word_match:
            continue
        word = word_match.group(1)

        # Функция может быть объявлена с типом возврата: "Bool Function Foo()".
        tokens = re.findall(r"[A-Za-z_]\w*", line)
        if len(tokens) > 1 and tokens[1] == "Function":
            word = "Function"

        if word in OPENERS:
            stack.append((word, number))
        elif word in CLOSERS:
            expected_opener = CLOSERS[word]
            if not stack:
                errors.append(f"{path.name}:{number}: {word} без открывающего {expected_opener}")
                continue
            opener, opener_line = stack.pop()
            if opener != expected_opener:
                errors.append(
                    f"{path.name}:{number}: {word} закрывает {opener} (строка {opener_line}), а не {expected_opener}"
                )

    for opener, line_number in stack:
        errors.append(f"{path.name}:{line_number}: {opener} не закрыт ({OPENERS[opener]})")

    if script_name is None:
        errors.append(f"{path.name}: нет строки ScriptName")
    else:
        if script_name != path.stem:
            errors.append(f"{path.name}: ScriptName {script_name} не совпадает с именем файла")
        if extends is None:
            warnings.append(f"{path.name}: ScriptName без extends")

    return errors, warnings


def lint_paths(paths: list[Path]) -> tuple[list[str], list[str]]:
    targets: list[Path] = []
    for path in paths:
        if path.is_dir():
            targets.extend(sorted(path.glob("*.psc")))
        elif path.is_file():
            targets.append(path)
        else:
            raise SystemExit(f"нет такого файла или папки: {path}")

    errors: list[str] = []
    warnings: list[str] = []
    for target in targets:
        file_errors, file_warnings = lint_file(target)
        errors.extend(file_errors)
        warnings.extend(file_warnings)
    return errors, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description="Проверить Papyrus-исходники без компилятора.")
    parser.add_argument("paths", nargs="+", help="файлы .psc или папки")
    parser.add_argument("--quiet", action="store_true", help="не печатать предупреждения")
    args = parser.parse_args()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

    errors, warnings = lint_paths([Path(item) for item in args.paths])
    if not args.quiet:
        for warning in warnings:
            print(f"[внимание] {warning}")
    for error in errors:
        print(f"[ошибка]   {error}")

    if errors:
        print(f"\nНайдено ошибок: {len(errors)}")
        return 1
    print(f"Проверка пройдена: {len(warnings)} предупреждений, 0 ошибок")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
