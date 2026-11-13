"""Check immutable APK entry extraction through the shared command."""

import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile

TOOL = pathlib.Path(__file__).resolve().parents[1] / "android.py"


class ApkExtractionTest(unittest.TestCase):
    def test_extracts_original_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = pathlib.Path(directory) / "sample.apk"
            output = pathlib.Path(directory) / "bundle.hbc"
            with zipfile.ZipFile(apk, "w") as archive:
                archive.writestr("assets/index.android.bundle", b"original bytecode")
            original = apk.read_bytes()
            subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "extract",
                    str(apk),
                    "assets/index.android.bundle",
                    str(output),
                ],
                check=True,
            )
            self.assertEqual(output.read_bytes(), b"original bytecode")
            self.assertEqual(apk.read_bytes(), original)

    def test_refuses_to_overwrite_input(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = pathlib.Path(directory) / "sample.apk"
            with zipfile.ZipFile(apk, "w") as archive:
                archive.writestr("asset", b"original")
            original = apk.read_bytes()
            result = subprocess.run(
                [sys.executable, str(TOOL), "extract", str(apk), "asset", str(apk)],
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(apk.read_bytes(), original)
