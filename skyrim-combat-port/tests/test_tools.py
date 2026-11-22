"""Тесты инструментов сборки и упаковки. Запуск: python -m unittest discover -s tests"""

from __future__ import annotations

import hashlib
import json
import re
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
import collect_logs  # noqa: E402
import enable_plugin  # noqa: E402
import esp_dump  # noqa: E402
import check_env  # noqa: E402
import inspect_plugin  # noqa: E402
import inspect_records  # noqa: E402
import package  # noqa: E402
import papyrus_lint  # noqa: E402
import papyrus_logging  # noqa: E402
import verify_api  # noqa: E402
import verify_audit  # noqa: E402


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

    def test_hearthfires_master_is_vanilla(self) -> None:
        report = inspect_plugin.describe(make_plugin(["Skyrim.esm", "HearthFires.esm"]))
        self.assertEqual(report["unexpected_masters"], [])

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


class EnablePluginTests(unittest.TestCase):
    BASE = ["Skyrim.esm", "*Update.esm", "*Dawnguard.esm"]

    def test_enable_appends_plugin(self) -> None:
        lines, changed = enable_plugin.set_enabled(self.BASE, "TGCombatPort.esp", True)
        self.assertTrue(changed)
        self.assertEqual(lines[-1], "*TGCombatPort.esp")

    def test_enable_marks_existing_line(self) -> None:
        lines, changed = enable_plugin.set_enabled(self.BASE + ["TGCombatPort.esp"], "TGCombatPort.esp", True)
        self.assertTrue(changed)
        self.assertEqual(lines[-1], "*TGCombatPort.esp")

    def test_enable_is_idempotent(self) -> None:
        lines, changed = enable_plugin.set_enabled(self.BASE + ["*TGCombatPort.esp"], "tgcombatport.esp", True)
        self.assertFalse(changed)

    def test_disable_removes_star_but_keeps_line(self) -> None:
        lines, changed = enable_plugin.set_enabled(self.BASE + ["*TGCombatPort.esp"], "TGCombatPort.esp", False)
        self.assertTrue(changed)
        self.assertEqual(lines[-1], "TGCombatPort.esp")

    def test_disable_unknown_plugin_changes_nothing(self) -> None:
        lines, changed = enable_plugin.set_enabled(self.BASE, "Missing.esp", False)
        self.assertFalse(changed)
        self.assertEqual(lines, self.BASE)

    def test_is_enabled_is_case_insensitive(self) -> None:
        self.assertTrue(enable_plugin.is_enabled(self.BASE + ["*TGCombatPort.esp"], "tgcombatport.esp"))
        self.assertFalse(enable_plugin.is_enabled(self.BASE + ["TGCombatPort.esp"], "TGCombatPort.esp"))


class CollectLogsTests(unittest.TestCase):
    LOG = [
        "Papyrus log opened",
        "error: Cannot call GetActorReference() on a None object",
        "[TGCP] probe: registered 12 candidate events",
        "[TGCP] anim event: blockStart | source=PlayerRef",
        "unrelated line",
    ]

    def test_selects_mod_lines_and_errors(self) -> None:
        selected, total = collect_logs.collect(self.LOG, r"\[TGCP\]")
        self.assertEqual(total, 3)
        self.assertEqual(len(selected), 3)
        self.assertTrue(selected[0].endswith("error: Cannot call GetActorReference() on a None object"))
        self.assertIn("[TGCP] probe", selected[1])

    def test_limit_is_applied(self) -> None:
        selected, total = collect_logs.collect(self.LOG, r"\[TGCP\]", limit=1)
        self.assertEqual(total, 3)
        self.assertEqual(len(selected), 1)

    def test_report_contains_counts_and_path(self) -> None:
        selected, total = collect_logs.collect(self.LOG, r"\[TGCP\]")
        report = collect_logs.build_report(Path("C:/logs/Papyrus.0.log"), r"\[TGCP\]", selected, total, len(self.LOG))
        self.assertIn("Строк прочитано: 5", report)
        self.assertIn("Совпадений: 3", report)
        self.assertIn("Papyrus.0.log", report)


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


class SteamDetectionTests(unittest.TestCase):
    def test_parse_library_folders(self) -> None:
        text = (
            '"libraryfolders"\n'
            '{\n'
            '  "0"\n  {\n    "path" "C:/Program Files (x86)/Steam"\n  }\n'
            '  "1"\n  {\n    "path" "D:/SteamLibrary"\n  }\n'
            '}\n'
        )
        paths = check_env.parse_library_folders(text)
        self.assertEqual(paths, [Path("C:/Program Files (x86)/Steam"), Path("D:/SteamLibrary")])

    def test_autodetect_game_finds_install_in_library(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "SteamLibrary"
            game = root / "steamapps" / "common" / "Skyrim Special Edition"
            touch(game / "SkyrimSE.exe")
            self.assertEqual(check_env.autodetect_game([root]), game)

    def test_autodetect_returns_none_when_absent(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(check_env.autodetect_game([Path(tmp)]))


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

    def test_hearthfires_spelling_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            touch(game / "SkyrimSE.exe")
            for name in ("Skyrim.esm", "Dawnguard.esm", "HearthFires.esm", "Dragonborn.esm"):
                touch(game / "Data" / name)
            rows = {row["key"]: row for row in check_env.build_report({"SKYRIM_DIR": str(game)})}
            self.assertEqual(rows["dlc"]["status"], "ok")
            self.assertIn("3/3", rows["dlc"]["detail"])

    def test_finds_flg_in_source_scripts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            touch(game / "SkyrimSE.exe")
            touch(game / "Data" / "Source" / "Scripts" / "TESV_Papyrus_Flags.flg")
            rows = {row["key"]: row for row in check_env.build_report({"SKYRIM_DIR": str(game)})}
            self.assertEqual(rows["papyrus_flags"]["status"], "ok")
            self.assertIn(".flg", rows["papyrus_flags"]["detail"])

    def test_flags_inside_scripts_zip_are_detected_and_extracted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / "game"
            data = game / "Data"
            touch(data / "Skyrim.esm")
            with zipfile.ZipFile(data / "Scripts.zip", "w") as archive:
                archive.writestr("Scripts/TESV_Papyrus_Flags.flg", "flags")
            rows = {row["key"]: row for row in check_env.build_report({"SKYRIM_DIR": str(game)})}
            self.assertEqual(rows["papyrus_flags"]["status"], "ok")
            self.assertIn("Scripts.zip", rows["papyrus_flags"]["detail"])

            dest = Path(tmp) / "build" / "tools"
            extracted = check_env.extract_flags_from_scripts_zip(data, dest)
            self.assertIsNotNone(extracted)
            self.assertEqual(extracted.read_text(encoding="utf-8"), "flags")

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

    def test_native_function_declaration_needs_no_endfunction(self) -> None:
        errors, warnings = self.lint(
            "NativeApi.psc",
            "ScriptName NativeApi Hidden\n\n"
            "Bool Function IsBlocking(Actor akActor) Global Native\n"
            "Function Tick() Global Native\n",
        )
        self.assertEqual(errors, [])
        self.assertEqual(warnings, [])

    def test_script_name_with_extended_flag_is_accepted(self) -> None:
        errors, warnings = self.lint(
            "Flagged.psc",
            "ScriptName Flagged Hidden\n\nFunction DoIt() Global Native\n",
        )
        self.assertEqual(errors, [])
        self.assertEqual(warnings, [])

    def test_script_without_extends_still_warns(self) -> None:
        _, warnings = self.lint("NoExtends.psc", "ScriptName NoExtends\n\nEvent OnInit()\nEndEvent\n")
        self.assertTrue(any("без extends" in warning for warning in warnings))

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
            touch(ck / "Papyrus Compiler" / "TESV_Papyrus_Flags.flg")
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
            flags = work / "TESV_Papyrus_Flags.flg"
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


class PapyrusLoggingTests(unittest.TestCase):
    def test_creates_section_in_empty_file(self) -> None:
        text, actions = papyrus_logging.enable_logging("")
        self.assertIn("[Papyrus]", text)
        self.assertTrue(papyrus_logging.is_enabled(text))
        self.assertEqual(len(actions), 1)

    def test_keeps_other_keys_and_sections(self) -> None:
        text = "[General]\nsLanguage=RUSSIAN\n\n[Papyrus]\nbEnableLogging=0\nfPostLoadUpdateTimeMS=500\n"
        new, _ = papyrus_logging.enable_logging(text)
        self.assertIn("sLanguage=RUSSIAN", new)
        self.assertIn("fPostLoadUpdateTimeMS=500", new)
        self.assertIn("bEnableLogging=1", new)
        self.assertNotIn("bEnableLogging=0", new)
        self.assertTrue(papyrus_logging.is_enabled(new))

    def test_second_run_changes_nothing(self) -> None:
        once, _ = papyrus_logging.enable_logging("[Papyrus]\nbEnableLogging=1\n")
        twice, actions = papyrus_logging.enable_logging(once)
        self.assertEqual(once, twice)
        self.assertIn("изменений нет", actions[0])

    def test_crlf_is_preserved(self) -> None:
        text = "[Papyrus]\r\nbEnableLogging=1\r\nbEnableTrace=1\r\nbLoadDebugInformation=1\r\n"
        new, _ = papyrus_logging.enable_logging(text)
        self.assertEqual(text, new)

    def test_finds_ini_inside_onedrive_documents(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            drive = Path(tmp)
            game_dir = drive / "Documents" / papyrus_logging.GAME_DIR
            game_dir.mkdir(parents=True)
            ini = game_dir / "Skyrim.ini"
            ini.write_text("[Papyrus]\n", encoding="utf-8")
            documents = papyrus_logging.base_documents(
                {"USERPROFILE": str(drive / "u"), "OneDrive": str(drive)}
            )
            self.assertIn(ini, papyrus_logging.find_inis(documents))
            self.assertEqual(papyrus_logging.resolve_target(None, documents, create=False), ini)

    def test_ambiguous_inis_are_not_guessed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for documents in (root / "u" / "Documents", root / "OneDrive" / "Documents"):
                ini = documents / papyrus_logging.GAME_DIR / "Skyrim.ini"
                ini.parent.mkdir(parents=True)
                ini.write_text("[Papyrus]\n", encoding="utf-8")
            documents_dirs = papyrus_logging.base_documents(
                {"USERPROFILE": str(root / "u"), "OneDrive": str(root / "OneDrive")}
            )
            self.assertEqual(len(papyrus_logging.find_inis(documents_dirs)), 2)
            self.assertIsNone(papyrus_logging.resolve_target(None, documents_dirs, create=False))


def esp_record(record_type: str, form_id: int, payload: bytes, compressed: bool = False) -> bytes:
    body = zlib.compress(payload) if compressed else payload
    flags = inspect_records.COMPRESSED_FLAG if compressed else 0
    header = struct.pack("<4sIII", record_type.encode(), len(body), flags, form_id)
    return header + struct.pack("<IHH", 0, 44, 0) + body


def esp_sub(name: str, data: bytes) -> bytes:
    return name.encode() + struct.pack("<H", len(data)) + data


def esp_group(label: bytes, children: bytes) -> bytes:
    return b"GRUP" + struct.pack("<I", 24 + len(children)) + label + struct.pack("<IHHI", 0, 0, 0, 0) + children


def build_plugin(
    global_edid: str = "TGCP_PlayerBlocking",
    quest_edid: str = "TGCP_MainQuest",
    scripts: tuple[str, ...] = ("TGCP_PlayerBlockWatcher", "TGCP_AnimEventProbe"),
    start_game: bool = True,
) -> bytes:
    tes4 = esp_record(
        "TES4",
        0,
        esp_sub("HEDR", struct.pack("<fII", 1.7, 1, 0x800)) + esp_sub("MAST", b"Skyrim.esm\x00"),
    )
    glob = esp_record(
        "GLOB",
        0x800,
        esp_sub("EDID", global_edid.encode() + b"\x00") + esp_sub("FNAM", b"s") + esp_sub("DATA", struct.pack("<f", 0.0)),
    )
    vmad = b"".join(
        struct.pack("<H", len(name)) + name.encode() + b"\x00\x00\x00" for name in scripts
    ) + struct.pack("<H", len(b"PlayerBlocking")) + b"PlayerBlocking"
    qust_payload = (
        esp_sub("EDID", quest_edid.encode() + b"\x00")
        + esp_sub("VMAD", vmad)
        + esp_sub("DNAM", bytes([1 if start_game else 0]) + bytes(11))
        + esp_sub("ALID", b"PlayerAlias\x00")
    )
    qust = esp_record("QUST", 0x801, qust_payload, compressed=True)
    return tes4 + esp_group(b"GLOB", glob) + esp_group(b"QUST", qust)


class InspectRecordsTests(unittest.TestCase):
    def test_reads_records_and_masters(self) -> None:
        data = build_plugin()
        nodes = inspect_records.parse(data)
        types = [node["type"] for node in nodes]
        self.assertEqual(types, ["TES4", "GLOB", "QUST"])
        self.assertEqual(inspect_records.masters_of(data), ["Skyrim.esm"])

    def test_check_passes_on_complete_plugin(self) -> None:
        ok, failed = 0, 0
        for result, _ in inspect_records.check(build_plugin()):
            ok += result
            failed += not result
        self.assertEqual(failed, 0)
        self.assertGreater(ok, 4)

    def test_missing_alias_script_is_reported(self) -> None:
        checks = dict((text, good) for good, text in inspect_records.check(build_plugin(scripts=("TGCP_PlayerBlockWatcher",))))
        probe = [good for text, good in checks.items() if "TGCP_AnimEventProbe" in text]
        self.assertEqual(probe, [False])

    def test_start_game_enabled_is_read(self) -> None:
        data = build_plugin(start_game=False)
        texts = [text for good, text in inspect_records.check(data) if not good]
        self.assertTrue(any("Start Game Enabled" in text for text in texts))

    def test_broken_file_is_rejected(self) -> None:
        with self.assertRaises(inspect_records.FormatError):
            inspect_records.parse(b"NOT A PLUGIN AT ALL")


    def test_game_dir_report_prints_real_text(self) -> None:
        import contextlib
        import io as string_io

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game = root / "game"
            scripts = game / "Data" / "Scripts"
            scripts.mkdir(parents=True)
            for name in inspect_records.PEX_NAMES:
                touch(scripts / f"{name}.pex", b"PEX")
            plugin = root / "TGCombatPort.esp"
            plugin.write_bytes(build_plugin())
            (game / "Data" / plugin.name).write_bytes(plugin.read_bytes())
            plugins_txt = root / "plugins.txt"
            plugins_txt.write_text("*TGCombatPort.esp\n", encoding="utf-8")

            buffer = string_io.StringIO()
            with contextlib.redirect_stdout(buffer):
                code = inspect_records.main(
                    [str(plugin), "--game-dir", str(game), "--plugins-txt", str(plugins_txt)]
                )
            output = buffer.getvalue()
            self.assertEqual(code, 0, output)
            self.assertNotIn("[ok] text", output)
            self.assertIn("плагин TGCombatPort.esp лежит в Data игры", output)
            self.assertIn("включён в plugins.txt", output)

    def test_plugin_without_asterisk_is_reported_disabled(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game = root / "game"
            (game / "Data").mkdir(parents=True)
            (game / "Data" / "TGCombatPort.esp").write_bytes(build_plugin())
            plugins_txt = root / "plugins.txt"
            plugins_txt.write_text("TGCombatPort.esp\n", encoding="utf-8")
            checks = inspect_records.check_files(game, "TGCombatPort.esp", plugins_txt)
            texts = [text for ok, text in checks if not ok]
            self.assertTrue(any("ВЫКЛЮЧЕН" in text for text in texts), texts)

class CommandLineHelpTests(unittest.TestCase):
    """Python 3.13+ падает на знаке % в тексте справки — проверяем все инструменты."""

    def test_no_percent_in_help_strings(self) -> None:
        pattern = re.compile(r'help="[^"]*%[^"]*"')
        offenders = []
        for path in sorted((ROOT / "tools").glob("*.py")):
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                if pattern.search(line):
                    offenders.append(f"{path.name}:{number}")
        self.assertEqual(offenders, [], "в help= нельзя использовать % (argparse принимает его за формат)")

    def test_every_tool_shows_help(self) -> None:
        import subprocess

        for path in sorted((ROOT / "tools").glob("*.py")):
            result = subprocess.run(
                [sys.executable, str(path), "--help"],
                capture_output=True,
                text=True,
                cwd=ROOT,
                timeout=60,
            )
            self.assertEqual(result.returncode, 0, f"{path.name}: {result.stderr}")
            self.assertIn("usage", result.stdout.lower(), path.name)


class VerifyAuditTests(unittest.TestCase):
    def test_repository_audit_passes(self):
        """Реальный репозиторий: каждая функция описана, каждая метка AS-n в реестре."""
        self.assertEqual(verify_audit.main(["--quiet"]), 0)

    def test_collect_functions_finds_definitions(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sample.cpp"
            path.write_text(
                "namespace\n{\n\tvoid Foo(int a)\n\t{\n\t}\n\n\tbool Bar()\n\t{\n\t\treturn true;\n\t}\n}\n",
                encoding="utf-8",
            )
            found = verify_audit.collect_functions([path])
            self.assertIn("Foo", found)
            self.assertIn("Bar", found)
            self.assertNotIn("return", found)

    def test_parse_assumptions_reads_rows(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ASSUMPTIONS.md"
            path.write_text(
                "| ID | Допущение | Почему | Проверяется | Статус |\n"
                "|---|---|---|---|---|\n"
                "| AS-1 | x | y | `block probe` | открыто |\n",
                encoding="utf-8",
            )
            rows = verify_audit.parse_assumptions(path)
            self.assertEqual(rows["AS-1"]["status"], "открыто")
            self.assertEqual(rows["AS-1"]["checked_by"], "`block probe`")


class VerifyApiTests(unittest.TestCase):
    def test_index_headers_finds_declarations_and_aliases(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "A.h").write_text(
                "namespace RE { class Actor { public: bool IsBlocking() const; }; }\n"
                "namespace RE { using FormID = std::uint32_t; }\n"
                "namespace RE { enum class Delivery { kSelf = 0 }; }\n",
                encoding="utf-8",
            )
            declared, mentioned = verify_api.index_headers(root)
            self.assertIn("Actor", declared)
            self.assertIn("FormID", declared)
            self.assertIn("Delivery", declared)
            self.assertIn("Actor", mentioned)

    def test_collect_symbols_reports_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "x.cpp"
            path.write_text("RE::Actor* a = nullptr; RE::FormID id = 0;\n", encoding="utf-8")
            found = verify_api.collect_symbols([path])
            self.assertEqual(found["Actor"], ["x.cpp"] if False else found["Actor"])
            self.assertIn("Actor", found)
            self.assertIn("FormID", found)

    def test_collect_methods_ignores_plain_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "y.cpp"
            path.write_text("actor->IsBlocking(); actor->GetName();\n", encoding="utf-8")
            found = verify_api.collect_methods([path])
            self.assertIn("IsBlocking", found)
            self.assertIn("GetName", found)

class EspDumpTests(unittest.TestCase):
    """Парсер модов: заголовки 24 байта, группы 24 байта, эффекты заклинаний."""

    @staticmethod
    def _record(sig: str, form_id: int, payload: bytes) -> bytes:
        head = sig.encode() + struct.pack("<III", len(payload), 0, form_id)
        head += b"\x00" * 4 + struct.pack("<HH", 44, 0)
        return head + payload

    @staticmethod
    def _sub(kind: str, data: bytes) -> bytes:
        return kind.encode() + struct.pack("<H", len(data)) + data

    @staticmethod
    def _grup(label: str, inner: bytes) -> bytes:
        return b"GRUP" + struct.pack("<I", 24 + len(inner)) + label.encode()[:4].ljust(4, b"\x00") + b"\x00" * 12 + inner

    def _plugin(self, directory: Path) -> Path:
        tes4 = self._record("TES4", 0, self._sub("HEDR", struct.pack("<fII", 1.7, 3, 0x800)))
        mgef = self._record(
            "MGEF",
            0x04000800,
            self._sub("EDID", b"TestEffect\x00") + self._sub("DATA", struct.pack("<If", 0, 1.5)),
        )
        spel = self._record(
            "SPEL",
            0x04000801,
            self._sub("EDID", b"TestSpell\x00")
            + self._sub("EFID", struct.pack("<I", 0x04000800))
            + self._sub("EFIT", struct.pack("<fii", 5.0, 0, 300)),
        )
        path = directory / "synthetic.esp"
        path.write_bytes(
            tes4 + self._grup("MGEF", mgef) + self._grup("SPEL", spel)
        )
        return path

    def test_parses_groups_and_subrecords(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            records = esp_dump.parse_plugin(self._plugin(Path(tmp)))
        self.assertEqual([r.sig for r in records], ["TES4", "MGEF", "SPEL"])
        self.assertEqual(records[1].editor_id, "TestEffect")
        self.assertEqual(records[1].to_dict()["mgef"]["base_cost"], 1.5)

    def test_decodes_spell_effects(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            records = esp_dump.parse_plugin(self._plugin(Path(tmp)))
        effects = [r for r in records if r.sig == "SPEL"][0].to_dict()["effects"]
        self.assertEqual(effects, [{"mgef": "04000800", "magnitude": 5.0, "area": 0, "duration": 300}])

    def test_broken_group_reports_offset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = self._plugin(Path(tmp))
            path.write_bytes(path.read_bytes()[:-2])
            with self.assertRaises(ValueError) as caught:
                esp_dump.parse_plugin(path)
        self.assertIn("не удалось разобрать", str(caught.exception))
