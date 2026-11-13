"""Verify device runners use local ADB and consistent device selection."""

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hermes/tools"))


def load_module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


runner = load_module("hermes_device_runner", "hermes/tools/run_device.py")
demo = load_module("hermes_demo_runner", "hermes/example/demo/run-demo.py")
loader = load_module("hermes_demo_loader", "hermes/example/demo/load.py")


class HermesRunnersTest(unittest.TestCase):
    def test_harness_defaults_to_local_adb_selection(self):
        args = runner.parse_args([])
        self.assertIsNone(args.adb)
        self.assertIsNone(args.serial)
        self.assertEqual(args.reports, ROOT / "build/hermes-reports")
        self.assertEqual(args.versions, [96, 97, 98, 99])

    def test_harness_accepts_explicit_device_configuration(self):
        args = runner.parse_args(
            [
                "99",
                "--serial",
                "device-42",
                "--adb",
                "/custom sdk/adb",
                "--reports",
                "/custom reports",
            ]
        )
        self.assertEqual(args.serial, "device-42")
        self.assertEqual(args.adb, "/custom sdk/adb")
        self.assertEqual(args.reports, pathlib.Path("/custom reports"))

    def test_frida_selection_is_usb_unless_explicitly_configured(self):
        frida = mock.Mock()
        with mock.patch.dict(sys.modules, frida=frida):
            self.assertIs(
                loader.select_device(None, None), frida.get_usb_device.return_value
            )
            frida.get_device_manager.assert_not_called()
            frida.reset_mock()
            self.assertIs(
                loader.select_device(None, "device-42"), frida.get_device.return_value
            )
            frida.get_device.assert_called_once_with("device-42", timeout=5)
            frida.get_usb_device.assert_not_called()
            frida.reset_mock()
            remote = "localhost:28000"
            device = loader.select_device(remote, "device-42")
            self.assertIs(
                device,
                frida.get_device_manager.return_value.add_remote_device.return_value,
            )
            frida.get_device_manager.return_value.add_remote_device.assert_called_once_with(
                remote
            )
            frida.get_usb_device.assert_not_called()
            frida.get_device.assert_not_called()

    def test_demo_passes_explicit_device_settings_without_port_forwarding(self):
        with tempfile.TemporaryDirectory(prefix="hermes-runner-") as directory:
            location = pathlib.Path(directory) / "demo"
            payload = location / "build/payload"
            payload.mkdir(parents=True)
            app = location.parent / "app"
            app.mkdir()
            (app / "app.json").write_text(
                json.dumps({"expo": {"android": {"package": "org.example.counter"}}})
            )
            for name in ("app-release.apk", "libpurehermes_demo.so"):
                (payload / name).write_bytes(b"fixture")
            cases = [
                ([], None, None, []),
                (
                    ["--serial", "device-42"],
                    "device-42",
                    None,
                    ["--serial", "device-42"],
                ),
                (
                    [
                        "--serial",
                        "device-42",
                        "--adb",
                        "/custom sdk/adb",
                        "--remote",
                        "localhost:28000",
                    ],
                    "device-42",
                    "/custom sdk/adb",
                    ["--remote", "localhost:28000"],
                ),
            ]
            for argv, serial, adb, selection in cases:
                with (
                    self.subTest(argv=argv),
                    mock.patch.object(demo, "DEMO", location),
                    mock.patch.object(demo, "Device") as device,
                    mock.patch.object(demo.subprocess, "run") as process,
                    mock.patch.dict(
                        "os.environ", {"ANDROID_SERIAL": "", "FRIDA_REMOTE": ""}
                    ),
                ):
                    device.return_value.run.side_effect = [
                        mock.Mock(stdout=""),
                        mock.Mock(stdout="package:/data/app/example/base.apk\n"),
                        mock.Mock(stdout="0\n"),
                        mock.Mock(stdout=""),
                    ]
                    demo.main(argv)
                    device.assert_called_once_with(serial=serial, adb=adb)
                    command = process.call_args.args[0]
                    self.assertEqual(command[5:], selection)
                    self.assertEqual(command[2], "/data/app/example/lib/arm64")
                    self.assertEqual(device.return_value.run.call_count, 4)
