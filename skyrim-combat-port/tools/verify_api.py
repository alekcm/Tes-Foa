"""Проверка: каждый символ RE::, который использует наш C++ код, существует в заголовках CommonLibSSE-NG.

Зачем: чтобы в коде не появлялось «на глаз» несуществующих функций движка
(например, `Actor::SetStaggered`). Проверка механическая, без интернета, если заголовки уже скачаны.

Использование:
    python tools/verify_api.py                     # берёт .cache/commonlib/include
    python tools/verify_api.py --headers /tmp/clib/include
    python tools/verify_api.py --clone             # скачивает закреплённый коммит CommonLibVR в .cache

Код возврата: 0 — все символы найдены, 1 — есть отсутствующие.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NATIVE_SRC = ROOT / "native" / "src"
CACHE = ROOT / ".cache" / "commonlib"

# Закреплённый коммит — тот же, что в .github/workflows/native.yml и DEPENDENCIES.md.
PIN = "48b17e1dea3028850adf63209f6733dbf4dcdcd7"
REPO = "https://github.com/alandtse/CommonLibVR.git"

RE_SYMBOL = re.compile(r"\bRE::([A-Za-z_][A-Za-z0-9_]*)")
# Вызовы методов вида `obj->Метод(` и `obj.Метод(` — ловят выдуманные функции вроде SetStaggered.
METHOD_CALL = re.compile(r"(?:->|\.)([A-Z][A-Za-z0-9_]*)\s*\(")
SKIP_SEGMENTS = {"std", "literals", "memory_order_relaxed"}


def display_path(path: Path) -> str:
    """Путь для отчёта: относительно корня репозитория, иначе просто имя (для тестовых каталогов)."""
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return path.name


def ensure_headers(clone: bool, headers: Path | None) -> Path:
    if headers is not None:
        return headers
    default = CACHE / "include"
    if default.is_dir():
        return default
    if not clone:
        raise SystemExit(
            "Заголовки CommonLibSSE-NG не найдены.\n"
            f"  ожидались в {default}\n"
            "  запусти: python tools/verify_api.py --clone"
        )
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    if CACHE.is_dir():
        shutil.rmtree(CACHE)
    print(f"клонирую CommonLibVR (пин {PIN[:8]}) в {CACHE} …")
    subprocess.run(["git", "clone", "--quiet", REPO, str(CACHE)], check=True)
    subprocess.run(["git", "-C", str(CACHE), "fetch", "--quiet", "--depth", "1", "origin", PIN], check=True)
    subprocess.run(["git", "-C", str(CACHE), "checkout", "--quiet", "FETCH_HEAD"], check=True)
    return default


def collect_methods(paths: list[Path]) -> dict[str, list[str]]:
    """Имена методов, которые вызывает наш код (эвристика по `->Имя(` и `.Имя(`)."""
    found: dict[str, list[str]] = {}
    for path in paths:
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in METHOD_CALL.finditer(text):
            name = match.group(1)
            found.setdefault(name, [])
            rel = display_path(path)
            if rel not in found[name]:
                found[name].append(rel)
    return found


def collect_symbols(paths: list[Path]) -> dict[str, list[str]]:
    """Возвращает {символ: [файлы, где использован]}."""
    found: dict[str, list[str]] = {}
    for path in paths:
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in RE_SYMBOL.finditer(text):
            name = match.group(1)
            if name in SKIP_SEGMENTS:
                continue
            found.setdefault(name, [])
            rel = display_path(path)
            if rel not in found[name]:
                found[name].append(rel)
    return found


# Объявления: class/struct/union, enum (с class/struct), namespace, using-алиасы, typedef.
DECLARATION_PATTERNS = (
    r"\b(?:class|struct|union)\s+([A-Za-z_][A-Za-z0-9_]*)",
    r"\benum\s+(?:class\s+|struct\s+)?([A-Za-z_][A-Za-z0-9_]*)",
    r"\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)",
    r"\busing\s+([A-Za-z_][A-Za-z0-9_]*)\s*=",
    r"\btypedef[^;{}]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*;",
)


def index_headers(headers: Path) -> tuple[dict[str, list[str]], set[str]]:
    """Возвращает (объявления: имя -> файлы, просто упомянутые имена)."""
    declared: dict[str, list[str]] = {}
    mentioned: set[str] = set()
    for path in headers.rglob("*.h"):
        text = path.read_text(encoding="utf-8", errors="replace")
        mentioned.update(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\b", text))
        for pattern in DECLARATION_PATTERNS:
            for name in set(re.findall(pattern, text)):
                declared.setdefault(name, [])
                if path.name not in declared[name]:
                    declared[name].append(path.name)
    return declared, mentioned


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Сверить символы RE:: из кода с заголовками CommonLibSSE-NG.")
    parser.add_argument("--headers", help="каталог include с заголовками CommonLibSSE-NG")
    parser.add_argument("--clone", action="store_true", help="скачать закреплённый коммит в .cache/commonlib")
    parser.add_argument("--methods", action="store_true", help="дополнительно проверить имена вызываемых методов")
    args = parser.parse_args(argv)

    headers = ensure_headers(args.clone, Path(args.headers) if args.headers else None)
    if not headers.is_dir():
        print(f"нет каталога заголовков: {headers}")
        return 1

    sources = sorted(NATIVE_SRC.glob("*.cpp")) + sorted(NATIVE_SRC.glob("*.h"))
    if not sources:
        print("нет исходников C++ для проверки")
        return 1

    used = collect_symbols(sources)
    declared, mentioned = index_headers(headers)

    missing = {name: files for name, files in used.items() if name not in declared}
    referenced_only = {name: files for name, files in used.items() if name not in declared and name in mentioned}

    print(f"заголовки: {headers}")
    print(f"исходники: {len(sources)}, символов RE:: в коде: {len(used)}")
    if args.methods:
        header_text = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in headers.rglob("*.h"))
        calls = collect_methods(sources)
        def is_known(name: str) -> bool:
            if re.search(r"\b" + re.escape(name) + r"\s*\(", header_text):
                return True
            # RUNTIME_CAST_ACCESSOR_VERSIONED(ActorState, AsActorState, …) — макрос генерирует метод
            return any(
                "RUNTIME_CAST_ACCESSOR" in line and re.search(r"\b" + re.escape(name) + r"\b", line)
                for line in header_text.splitlines()
            )

        unknown = {name: files for name, files in calls.items() if not is_known(name)}
        print(f"методов в вызовах: {len(calls)}")
        if unknown:
            print("методы, которых нет в заголовках:")
            for name in sorted(unknown):
                print(f"  {name}  (используется: {', '.join(unknown[name])})")
            return 1
        print("все вызываемые методы найдены в заголовках")

    if not missing and not referenced_only:
        print("все символы найдены в заголовках")
        return 0

    for name in sorted(referenced_only):
        print(f"  ? RE::{name} — упоминается в заголовках, но объявления не найдено "
              f"(используется: {', '.join(missing.pop(name))})")
    if missing:
        print("НЕ НАЙДЕНЫ в заголовках:")
        for name in sorted(missing):
            print(f"  RE::{name}  (используется: {', '.join(missing[name])})")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
