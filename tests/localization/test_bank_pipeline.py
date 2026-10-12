import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts" / "localization"
sys.path.insert(0, str(SCRIPTS))
from mainus_bank import MainUSBank


def synthetic_bank(identifiers, indexed_count):
    bank = bytearray(16 + indexed_count * 4)
    struct.pack_into("<H", bank, 0, indexed_count)
    struct.pack_into("<I", bank, 8, len(identifiers))
    struct.pack_into("<I", bank, 12, 16)
    for index, identifier in enumerate(identifiers):
        if index < indexed_count:
            struct.pack_into("<I", bank, 16 + index * 4, len(bank))
        bank.extend(struct.pack("<I", identifier) + f"source {index}".encode() + b"\0")
        if index + 1 < len(identifiers):
            while len(bank) % 4:
                bank.append(0)
    return bytes(bank)


class BankPipelineTests(unittest.TestCase):
    def bootstrap(self, installed, fallback, digest):
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell is required for the Windows bootstrap helper")
        env = dict(os.environ, BLACK_TEST_INSTALLED=str(installed),
                   BLACK_TEST_FALLBACK=str(fallback), BLACK_TEST_DIGEST=digest,
                   BLACK_TEST_BOOTSTRAP=str(SCRIPTS / "prepare_english_bank.ps1"))
        return subprocess.run(
            [powershell, "-NoProfile", "-Command",
             "$ErrorActionPreference = 'Stop'; . $env:BLACK_TEST_BOOTSTRAP; "
             "Initialize-EnglishLanguageBank -InstalledBank $env:BLACK_TEST_INSTALLED "
             "-EnglishBank $env:BLACK_TEST_FALLBACK -ExpectedSha256 $env:BLACK_TEST_DIGEST"],
            env=env, capture_output=True, text=True)

    def test_clean_setup_preserves_english_and_builds_all_draft_banks(self):
        rows = [json.loads(line) for line in
                (ROOT / "localization" / "game" / "es.jsonl").read_text(encoding="utf-8").splitlines()]
        source = synthetic_bank([int(row["id"], 16) for row in rows], 2)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            installed = root / "game" / "language" / "strings" / "MainUS.bin"
            fallback = root / "local" / "language-banks" / "en-US" / "MainUS.bin"
            installed.parent.mkdir(parents=True)
            installed.write_bytes(source)
            timestamp = installed.stat().st_mtime_ns
            result = self.bootstrap(installed, fallback, hashlib.sha256(source).hexdigest())
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(fallback.read_bytes(), source)
            for locale in ("es", "fr", "de", "it"):
                catalog = ROOT / "localization" / "game" / f"{locale}.jsonl"
                output = fallback.parent.parent / locale / "MainUS.bin"
                result = subprocess.run([sys.executable, str(SCRIPTS / "build_mainus.py"),
                                         str(fallback), str(catalog), str(output)],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                translated = [json.loads(line) for line in catalog.read_text(encoding="utf-8").splitlines()]
                bank = MainUSBank.parse(output.read_bytes())
                self.assertEqual([record.text for record in bank.records], [row["text"] for row in translated])
            self.assertEqual(installed.read_bytes(), source)
            self.assertEqual(installed.stat().st_mtime_ns, timestamp)
            self.assertEqual(list(root.rglob("*.launcher-tmp-*")), [])

    def test_unknown_installed_bank_is_rejected_without_publishing_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            installed, fallback = root / "installed.bin", root / "fallback" / "MainUS.bin"
            installed.write_bytes(b"previously translated bank")
            result = self.bootstrap(installed, fallback, hashlib.sha256(b"English").hexdigest())
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("original English", result.stderr)
            self.assertFalse(fallback.exists())
            self.assertEqual(installed.read_bytes(), b"previously translated bank")
            self.assertEqual(list(root.rglob("*.launcher-tmp-*")), [])

    def test_existing_fallback_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            installed, fallback = root / "installed.bin", root / "MainUS.bin"
            installed.write_bytes(b"translated")
            fallback.write_bytes(b"preserved English")
            result = self.bootstrap(installed, fallback, hashlib.sha256(b"English").hexdigest())
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(fallback.read_bytes(), b"preserved English")

    def test_all_indexed_round_trip_does_not_duplicate_final_padding(self):
        source = synthetic_bank([1, 2], 2) + b"\0\0"
        bank = MainUSBank.parse(source)
        values = {record.record_id: record.text for record in bank.records}
        self.assertEqual(bank.rebuild(values), source)

    def test_unchanged_launcher_resource_keeps_build_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "catalog.json"
            command = [sys.executable, str(SCRIPTS / "build_launcher_catalog.py"),
                       str(ROOT / "localization" / "launcher" / "en-pt-BR.jsonl"),
                       str(ROOT / "localization" / "launcher"), str(output)]
            subprocess.run(command, check=True, capture_output=True)
            os.utime(output, (1000000000, 1000000000))
            timestamp = output.stat().st_mtime_ns
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(output.stat().st_mtime_ns, timestamp)


if __name__ == "__main__":
    unittest.main()
