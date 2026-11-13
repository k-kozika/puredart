#!/usr/bin/env python3
"""Install release harnesses on a selected Android ARM64 device and require PASS."""

import argparse
import json
import pathlib
import sys
import time

from toolchains import toolchain

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from hooktools.adb import Device


def run(version, device, reports):
    apk = toolchain(version) / "device/test.apk"

    def adb(*args):
        return device.run(*args).stdout

    if (
        "package:org.purehermes.test"
        in adb("shell", "pm", "list", "packages", "org.purehermes.test").splitlines()
    ):
        adb("uninstall", "org.purehermes.test")
    device.install(apk)
    adb("shell", "am", "force-stop", "org.purehermes.test")
    adb("shell", "am", "start", "-n", "org.purehermes.test/.MainActivity")
    pid = adb("shell", "pidof", "org.purehermes.test").strip().split()[-1]
    for attempt in range(40):
        log = adb("logcat", "-d", f"--pid={pid}")
        if (
            "PurehermesTest:" in log
            or "FATAL EXCEPTION" in log
            or "Fatal signal" in log
        ):
            (reports / f"device-{version}.txt").write_text(log)
            result = {
                "version": version,
                "pid": int(pid),
                "pass": f"PASS HBC {version}" in log,
            }
            print(json.dumps(result), flush=True)
            return result
        time.sleep(0.25)
    raise RuntimeError(f"HBC {version} timed out")


def parse_args(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "versions",
        type=int,
        nargs="*",
        default=[96, 97, 98, 99],
        metavar="HBC",
    )
    p.add_argument(
        "--serial", help="ADB serial; otherwise ANDROID_SERIAL or ADB selection"
    )
    p.add_argument("--adb", help="local ADB executable")
    p.add_argument(
        "--reports",
        type=pathlib.Path,
        default=ROOT / "build/hermes-reports",
        help="output directory (default: build/hermes-reports)",
    )
    a = p.parse_args(argv)
    if any(version not in (96, 97, 98, 99) for version in a.versions):
        p.error("supported HBC versions are 96, 97, 98 and 99")
    return a


def main(argv=None):
    a = parse_args(argv)
    a.reports.mkdir(parents=True, exist_ok=True)
    device = Device(serial=a.serial, adb=a.adb)
    environment = {"versions": a.versions, **device.environment()}
    (a.reports / "environment.json").write_text(
        json.dumps(environment, indent=2) + "\n"
    )
    vm_builds = {
        str(v): json.loads((toolchain(v) / "profile.json").read_text())["build_id"]
        for v in a.versions
    }
    (a.reports / "vm-builds.json").write_text(json.dumps(vm_builds, indent=2) + "\n")
    results = []
    matrix = a.reports / "device-results.json"
    matrix.write_text("[]\n")
    for v in a.versions:
        results.append(run(v, device, a.reports))
        matrix.write_text(json.dumps(results, indent=2) + "\n")
    if not all(r["pass"] for r in results):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
