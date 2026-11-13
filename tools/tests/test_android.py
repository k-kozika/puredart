"""Exercise host-tool selection and local ADB through fake executables."""

import json
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from hooktools.adb import Device
from hooktools.sdk import find_adb, find_ndk, find_ndk_prebuilt, find_sdk


FIXTURES = pathlib.Path(__file__).with_name("fixtures")


class AndroidToolsTest(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix="hooktools tests ")
        self.addCleanup(self.work.cleanup)
        self.root = pathlib.Path(self.work.name)
        self.sdk = self.root / "sdk"
        (self.sdk / "platform-tools").mkdir(parents=True)
        self.adb = self.sdk / "platform-tools/adb"
        self.install_fixture(self.adb, "adb.py")
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.env = mock.patch.dict(
            os.environ,
            {
                "ANDROID_HOME": "",
                "ANDROID_SDK_ROOT": "",
                "ANDROID_NDK": "",
                "ADB_BIN": "",
                "ANDROID_SERIAL": "",
                "FAKE_ADB_FAIL": "",
                "FRIDA_REMOTE": "",
                "FAKE_ADB_TRACE": "",
                "FAKE_DEVICE_ROOT": str(self.root),
                "PATH": str(self.bin) + os.pathsep + os.environ.get("PATH", ""),
            },
        )
        self.env.start()
        self.addCleanup(self.env.stop)

    @staticmethod
    def install_fixture(path, filename):
        shutil.copyfile(FIXTURES / filename, path)
        path.chmod(0o755)

    def device(self, **kwargs):
        return Device(adb=str(self.adb), **kwargs)

    def test_sdk_selection_and_pinned_ndk(self):
        os.environ["ANDROID_SDK_ROOT"] = str(self.sdk)
        self.assertEqual(find_sdk(), self.sdk)
        ndk = self.sdk / "ndk/27.1.12297006"
        ndk.mkdir(parents=True)
        self.assertEqual(find_ndk(version="27.1.12297006"), ndk)
        with self.assertRaises(ValueError):
            find_ndk(version="missing")
        with self.assertRaises(ValueError):
            find_sdk(self.root / "missing")

    def test_explicit_tools_override_environment(self):
        os.environ["ANDROID_HOME"] = str(self.root / "invalid")
        os.environ["ADB_BIN"] = "/missing/adb"
        self.assertEqual(find_sdk(self.sdk), self.sdk)
        self.assertEqual(find_adb(self.adb), str(self.adb))
        with self.assertRaises(ValueError):
            find_adb()

    def test_adb_on_path_needs_no_sdk(self):
        adb = self.bin / "adb"
        self.install_fixture(adb, "adb.py")
        with mock.patch("pathlib.Path.home", return_value=self.root / "empty home"):
            self.assertEqual(find_adb(), str(adb))
            self.assertEqual(json.loads(Device().run("version").stdout), ["version"])

    def test_ndk_prebuilt_uses_installed_host_tag(self):
        ndk = self.root / "ndk"
        ndk.mkdir()
        with self.assertRaises(ValueError):
            find_ndk_prebuilt(ndk)
        prebuilt = ndk / "toolchains/llvm/prebuilt/host-toolchain"
        for name in ("bin", "sysroot"):
            (prebuilt / name).mkdir(parents=True)
        self.assertEqual(find_ndk_prebuilt(ndk), prebuilt)
        for name in ("bin", "sysroot"):
            (prebuilt.parent / "another-host" / name).mkdir(parents=True)
        with self.assertRaises(ValueError):
            find_ndk_prebuilt(ndk)

    def test_serial_and_structured_shell_keep_literal_arguments(self):
        command = ("printf", "%s", "literal space; $(uname)")
        args = json.loads(
            self.device(serial="serial space").run("shell", *command).stdout
        )
        self.assertEqual(args[:3], ["-s", "serial space", "shell"])
        self.assertEqual(shlex.split(args[3]), list(command))

    def test_raw_shell_script_is_preserved(self):
        script = "echo one | sed 's/one/two/'"
        self.assertEqual(
            json.loads(self.device().run("shell", script).stdout), ["shell", script]
        )

    def test_install_keeps_literal_apk_path(self):
        apk = self.root / "counter $(literal); space.apk"
        apk.write_bytes(b"test payload")
        local = json.loads(self.device().install(apk).stdout)
        self.assertEqual(local, ["install", "-r", str(apk)])

    def test_failed_install_preserves_local_apk(self):
        apk = self.root / "broken.apk"
        apk.write_bytes(b"test payload")
        os.environ["FAKE_ADB_FAIL"] = "1"
        with self.assertRaises(subprocess.CalledProcessError) as caught:
            self.device().install(apk)
        self.assertEqual(caught.exception.returncode, 7)
        self.assertEqual(json.loads(caught.exception.stdout)[-1], str(apk))
        self.assertEqual(apk.read_bytes(), b"test payload")

    def test_push_pull_round_trip(self):
        source = self.root / "source file.bin"
        source.write_bytes(bytes(range(256)))
        remote = self.root / "device file.bin"
        target = self.root / "downloaded file.bin"
        device = self.device()
        device.push(source, remote)
        device.pull(remote, target)
        self.assertEqual(target.read_bytes(), source.read_bytes())

    def test_dart_demo_uses_shared_adb_without_device_side_effects(self):
        repository = pathlib.Path(__file__).resolve().parents[2]
        checkout = self.root / "demo checkout"
        demo = checkout / "dart/example/demo"
        payload = demo / "build/payload"
        payload.mkdir(parents=True)
        shutil.copytree(repository / "tools/hooktools", checkout / "tools/hooktools")
        shutil.copy2(repository / "tools/android.py", checkout / "tools/android.py")
        shutil.copy2(repository / "dart/example/demo/run-demo.sh", demo / "run-demo.sh")
        for filename in ("app-release.apk", "libpuredart_demo.so"):
            (payload / filename).write_bytes(b"fixture")
        shutil.copyfile(repository / "dart/example/demo/load.js", payload / "load.js")
        frida = self.bin / "frida"
        self.install_fixture(frida, "frida.py")
        for name in ("pyproject.toml", "uv.lock", ".python-version"):
            shutil.copy2(repository / name, checkout / name)
        os.environ["UV_PROJECT_ENVIRONMENT"] = os.environ.get(
            "UV_PROJECT_ENVIRONMENT", str(repository / ".venv")
        )
        trace = self.root / "adb trace.jsonl"
        os.environ.update(
            ADB_BIN=str(self.adb), FRIDA_BIN=str(frida), FAKE_ADB_TRACE=str(trace)
        )
        result = subprocess.check_output(["bash", str(demo / "run-demo.sh")], text=True)
        command = json.loads(result.splitlines()[-1])
        self.assertEqual(command[:3], ["-U", "-f", "com.example.hookme"])
        self.assertEqual(
            json.loads(command[command.index("-P") + 1]),
            {"nativeDirectory": "/data/app/test/lib/arm64"},
        )
        self.assertEqual(
            (payload / "load.js").read_bytes(),
            (repository / "dart/example/demo/load.js").read_bytes(),
        )
        self.assertFalse((payload / "load-runtime.js").exists())
        calls = [json.loads(line) for line in trace.read_text().splitlines()]
        root_command = next(
            shlex.split(call[1])
            for call in calls
            if call[0] == "shell" and call[1].startswith("su ")
        )
        self.assertEqual(root_command[:2], ["su", "-c"])
        self.assertIn("chcon u:object_r:apk_data_file:s0", root_command[2])
        for remote, expected in (
            ("", ["-D", "device-42"]),
            ("localhost:28000", ["-H", "localhost:28000"]),
        ):
            with self.subTest(remote=remote):
                os.environ.update(ANDROID_SERIAL="device-42", FRIDA_REMOTE=remote)
                result = subprocess.check_output(
                    ["bash", str(demo / "run-demo.sh")], text=True
                )
                command = json.loads(result.splitlines()[-1])
                self.assertEqual(command[:2], expected)
                calls = [json.loads(line) for line in trace.read_text().splitlines()]
                self.assertEqual(calls[-1][:2], ["-s", "device-42"])


if __name__ == "__main__":
    unittest.main()
