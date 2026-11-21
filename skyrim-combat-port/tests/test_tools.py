"""Тесты инструментов сборки и упаковки. Запуск: python -m unittest discover -s tests"""

from __future__ import annotations

import hashlib
import json
import struct
import sys
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import build  # noqa: E402
import check_docs  # noqa: E402
import check_env  # noqa: E402
import inspect_plugin  # noqa: E402
import package  # noqa: E402
import papyrus_lint  # noqa: E402


def touch(path: Path, content: bytes = b"") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


class PackageValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.stage = Path(self._tmp.name) / "stage"
        self.stage.mkdir()

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def check(self, allow_dll: bool = False, allow_psc: bool = False) -> tuple[list[str], list[str]]:
        files = package.collect(self.stage)
        return package.validate(self.stage, files, "TGCombatPort", allow_dll, allow_psc)

    def test_valid_stage_has_no_errors(self) -> None:
        touch(self.stage / "TGCombatPort.esp")
        touch(self.stage / "Scripts" / "Test.pex")
        errors, _ = self.check()
        self.assertEqual(errors, [])

    def test_missing_plugin_is_error(self) -> None:
        touch(self.stage / "Scripts" / "Test.pex")
        errors, _ = self.check()
        self.assertTrue(any("плагина" in error for error in errors))

    def test_master_files_are_rejected(self) -> None:
        touch(self.stage / "TGCombatPort.esp")
        touch(self.stage / "Skyrim.esm")
        touch(self.stage / "Data" / "Update.esm")
        errors, _ = self.check()
        self.assertTrue(any("мастер-файл" in error for error in errors))

    def test_nested_data_folder_is_rejected(self) -> None:
        touch(self.stage / "TGCombatPort.esp")
        touch(self.stage / "Data" / "Something.txt")
        errors, _ = self.check()
        self.assertTrue(any("папка Data" in error for error in errors))

    def test_dll_requires_flag(self) -> None:
        touch(self.stage / "TGCombatPort.esp")
        touch(self.stage / "TGCP.dll")
        errors, _ = self.check()
        self.assertTrue(any("DLL" in error for error in errors))
        allowed_errors, _ = self.check(allow_dll=True)
        self.assertEqual(allowed_errors, [])

    def test_psc_requires_flag(self) -> None:
        touch(self.stage / "TGCombatPort.esp")
        touch(self.stage / "Scripts" / "Old.psc")
        errors, _ = self.check()
        self.assertTrue(any(".psc" in error for error in errors))

    def test_executable_is_rejected(self) -> None:
        touch(self.stage / "TGCombatPort.esp")
        touch(self.stage / "tool.exe")
        errors, _ = self.check()
        self.assertTrue(any("исполняемый" in error for error in errors))


class PackageZipTests(unittest.TestCase):
    def test_zip_layout_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            stage = Path(tmp) / "stage"
            touch(stage / "TGCombatPort.esp", b"fake plugin")
            touch(stage / "Scripts" / "Test.pex", b"fake pex")
            files = package.collect(stage)
            out = Path(tmp) / "dist" / "TGCombatPort-0.1.0.zip"
            entries = package.write_zip(stage, files, out, "TGCombatPort", "0.1.0")

            with zipfile.ZipFile(out) as archive:
                self.assertEqual(
                    sorted(archive.namelist()),
                    ["Scripts/Test.pex", "TGCombatPort.esp"],
                )
                self.assertEqual(archive.read("TGCombatPort.esp"), b"fake plugin")

            manifest = json.loads(out.with_suffix(".zip.manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["version"], "0.1.0")
            self.assertEqual(len(manifest["files"]), 2)
            self.assertEqual(len(entries), 2)

            plugin_entry = next(item for item in manifest["files"] if item["path"] == "TGCombatPort.esp")
            self.assertEqual(plugin_entry["sha256"], hashlib.sha256(b"fake plugin").hexdigest())


def make_plugin(masters: list[str], esl: bool = False, compressed: bool = False) -> bytes:
    subrecords = b"".join(
        b"MAST" + struct.pack("<H", len(master.encode()) + 1) + master.encode() + b"\x00"
        for master in masters
    )
    flags = 0
    payload = subrecords
    if compressed:
        flags |= inspect_plugin.COMPRESSED_FLAG
        payload = zlib.compress(subrecords)
    if esl:
        flags |= inspect_plugin.ESL_FLAG
    header = struct.pack("<4sIII", b"TES4", len(payload), flags, 0) + struct.pack("<HHHH", 0, 0, 44, 0)
    return header + payload


class InspectPluginTests(unittest.TestCase):
    def test_reads_vanilla_masters(self) -> None:
        report = inspect_plugin.describe(
            make_plugin(["Skyrim.esm", "Update.esm", "Dawnguard.esm", "Hearthfire.esm", "Dragonborn.esm"])
        )
        self.assertEqual(len(report["masters"]), 5)
        self.assertEqual(report["unexpected_masters"], [])
        self.assertFalse(report["esl"])

    def test_reports_foreign_master_and_esl(self) -> None:
        report = inspect_plugin.describe(make_plugin(["Skyrim.esm", "SomeOtherMod.esp"], esl=True))
        self.assertIn("SomeOtherMod.esp", report["unexpected_masters"])
        self.assertTrue(report["esl"])

    def test_reads_compressed_header(self) -> None:
        report = inspect_plugin.describe(make_plugin(["Skyrim.esm"], compressed=True))
        self.assertEqual(report["masters"], ["Skyrim.esm"])
        self.assertTrue(report["compressed"])

    def test_rejects_non_plugin(self) -> None:
        with self.assertRaises(ValueError):
            inspect_plugin.describe(b"not a plugin at all")

    def test_reads_plugin_from_zip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            archive_path = Path(tmp) / "mod.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("TGCombatPort.esp", make_plugin(["Skyrim.esm"]))
            found = inspect_plugin.candidates(archive_path)
            self.assertEqual(len(found), 1)
            self.assertEqual(found[0][0], "TGCombatPort.esp")


class CheckDocsTests(unittest.TestCase):
    def test_project_docs_have_no_broken_links(self) -> None:
        problems: list[str] = []
        for path in check_docs.markdown_files(check_docs.ROOT):
            problems.extend(check_docs.check_file(path))
        self.assertEqual(problems, [])

    def test_detects_missing_link(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            document = root / "a.md"
            document.write_text("[файл](missing.md)\n", encoding="utf-8")
            with mock.patch.object(check_docs, "ROOT", root):
                problems = check_docs.check_file(document)
        self.assertEqual(len(problems), 1)
        self.assertIn("missing.md", problems[0])


class CheckEnvTests(unittest.TestCase):
    def test_reports_installed_game(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            touch(game / "SkyrimSE.exe")
            for name in ("Skyrim.esm", "Dawnguard.esm", "Hearthfire.esm", "Dragonborn.esm"):
                touch(game / "Data" / name)
            rows = {row["key"]: row for row in check_env.build_report({"SKYRIM_DIR": str(game)})}

            self.assertEqual(rows["skyrim"]["status"], "ok")
            self.assertEqual(rows["data"]["status"], "ok")
            self.assertEqual(rows["dlc"]["status"], "ok")
            self.assertIn("3/3", rows["dlc"]["detail"])
            self.assertEqual(rows["editor"]["status"], "missing")

    def test_missing_dlc_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            touch(game / "SkyrimSE.exe")
            touch(game / "Data" / "Skyrim.esm")
            rows = {row["key"]: row for row in check_env.build_report({"SKYRIM_DIR": str(game)})}
            self.assertEqual(rows["dlc"]["status"], "missing")
            self.assertIn("0/3", rows["dlc"]["detail"])


class PapyrusLintTests(unittest.TestCase):
    def lint(self, name: str, text: str) -> tuple[list[str], list[str]]:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / name
            path.write_text(text, encoding="utf-8")
            return papyrus_lint.lint_file(path)

    def test_valid_script_passes(self) -> None:
        errors, _ = self.lint(
            "TestScript.psc",
            "ScriptName TestScript extends Quest\n\n"
            "Bool Function Check(Actor akTarget)\n"
            "    If akTarget == None\n"
            "        Return False\n"
            "    EndIf\n"
            "    Return True\n"
            "EndFunction\n",
        )
        self.assertEqual(errors, [])

    def test_script_name_must_match_file(self) -> None:
        errors, _ = self.lint("Wrong.psc", "ScriptName Right extends Quest\n")
        self.assertTrue(any("не совпадает" in error for error in errors))

    def test_unclosed_block_is_reported(self) -> None:
        errors, _ = self.lint(
            "Broken.psc",
            "ScriptName Broken extends Quest\n\nEvent OnInit()\n    If True\n        Debug.Trace(\"x\")\nEndEvent\n",
        )
        self.assertTrue(any("не закрыт" in error for error in errors))

    def test_stray_end_is_reported(self) -> None:
        errors, _ = self.lint("Stray.psc", "ScriptName Stray extends Quest\nEndFunction\n")
        self.assertTrue(any("без открывающего" in error for error in errors))

    def test_tabs_are_reported(self) -> None:
        errors, _ = self.lint("Tabs.psc", "ScriptName Tabs extends Quest\n\tEvent OnInit()\n\tEndEvent\n")
        self.assertTrue(any("таб" in error for error in errors))

    def test_project_scripts_are_clean(self) -> None:
        errors, _ = papyrus_lint.lint_paths([ROOT / "src" / "papyrus"])
        self.assertEqual(errors, [])


class BuildTests(unittest.TestCase):
    def test_locate_without_paths_returns_nothing(self) -> None:
        compiler, flags, sources = build.locate({})
        self.assertIsNone(compiler)
        self.assertIsNone(flags)
        self.assertIsNone(sources)

    def test_locate_finds_compiler_in_ck_folder(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            ck = Path(tmp) / "ck"
            touch(ck / "CreationKit.exe")
            touch(ck / "Papyrus Compiler" / "PapyrusCompiler.exe")
            touch(ck / "Papyrus Compiler" / "TESV_Papyrus_Flags.flags")
            compiler, flags, _ = build.locate({"CK_DIR": str(ck)})
            self.assertIsNotNone(compiler)
            self.assertEqual(compiler.name, "PapyrusCompiler.exe")
            self.assertIsNotNone(flags)

    @unittest.skipIf(sys.platform == "win32", "заглушка компилятора — POSIX-скрипт")
    def test_end_to_end_build_with_stub_compiler(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            src = work / "src"
            src.mkdir()
            (src / "Demo.psc").write_text(
                "ScriptName Demo extends Quest\n\nEvent OnInit()\n    Debug.Trace(\"demo\")\nEndEvent\n",
                encoding="utf-8",
            )

            compiler = work / "PapyrusCompiler.exe"
            compiler.write_text(
                "#!/usr/bin/env bash\n"
                "out=\"\"\n"
                "src=\"\"\n"
                "for a in \"$@\"; do\n"
                "  case \"$a\" in\n"
                "    -o=*) out=\"${a#-o=}\" ;;\n"
                "    *.psc) src=\"$a\" ;;\n"
                "  esac\n"
                "done\n"
                "mkdir -p \"$out\" || exit 3\n"
                "test -n \"$src\" || exit 4\n"
                "printf 'pex' > \"$out/$(basename \"$src\" .psc).pex\"\n",
                encoding="utf-8",
            )
            compiler.chmod(0o755)
            flags = work / "TESV_Papyrus_Flags.flags"
            flags.write_text("", encoding="utf-8")

            plugin = work / "TGCombatPort.esp"
            plugin.write_bytes(make_plugin(["Skyrim.esm"]))

            stage = work / "stage"
            dist = work / "dist"
            argv = [
                "build.py",
                "--name",
                "TGCombatPort",
                "--version",
                "0.1.0",
                "--plugin",
                str(plugin),
                "--out-dir",
                str(dist),
            ]

            with mock.patch.object(build, "SRC", src), mock.patch.object(build, "STAGE", stage), mock.patch.object(
                build, "locate", return_value=(compiler, flags, None)
            ), mock.patch.object(sys, "argv", argv), mock.patch("sys.stdout"):
                exit_code = build.main()

            self.assertEqual(exit_code, 0)
            archive = dist / "TGCombatPort-0.1.0.zip"
            self.assertTrue(archive.is_file())
            with zipfile.ZipFile(archive) as packed:
                self.assertEqual(sorted(packed.namelist()), ["Scripts/Demo.pex", "TGCombatPort.esp"])

    def test_missing_compiler_exits_with_code_2(self) -> None:
        with mock.patch.object(build, "locate", return_value=(None, None, None)), mock.patch.object(
            sys, "argv", ["build.py", "--name", "TGCombatPort", "--version", "0.1.0"]
        ), mock.patch("sys.stdout"):
            exit_code = build.main()
        self.assertEqual(exit_code, 2)


if __name__ == "__main__":
    unittest.main()
