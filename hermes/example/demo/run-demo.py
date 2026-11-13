#!/usr/bin/env python3
"""Install hookme and inject its separately built native Hermes payload."""

import argparse
import json
import os
import pathlib
import shlex
import subprocess
import sys

DEMO = pathlib.Path(__file__).resolve().parent
ROOT = DEMO.parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from hooktools.adb import Device


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", default=os.environ.get("ANDROID_SERIAL") or None)
    parser.add_argument("--adb")
    parser.add_argument(
        "--remote",
        default=os.environ.get("FRIDA_REMOTE") or None,
        help="Frida server address; otherwise use the selected USB device",
    )
    args = parser.parse_args(argv)
    package = json.loads((DEMO.parent / "app/app.json").read_text())["expo"]["android"][
        "package"
    ]
    payload = DEMO / "build/payload"
    apk = payload / "app-release.apk"
    library = payload / "libpurehermes_demo.so"
    if not apk.is_file() or not library.is_file():
        parser.error("run build-demo.sh first")
    device = Device(serial=args.serial, adb=args.adb)
    device.install(apk, capture=False)
    device.run("shell", "am", "force-stop", package)
    apk_path = next(
        line.removeprefix("package:")
        for line in device.run("shell", "pm", "path", package).stdout.splitlines()
        if line.endswith("/base.apk")
    )
    native_dir = apk_path.removesuffix("/base.apk") + "/lib/arm64"
    upload = "/data/local/tmp/purehermes-demo.so.upload"
    device.push(library, upload, capture=False)
    destination = native_dir + "/libpurehermes_demo.so"
    script = "\n".join(
        shlex.join(command)
        for command in (
            ["mkdir", "-p", native_dir],
            ["cp", upload, destination],
            ["chown", "system:system", destination],
            ["chmod", "755", destination],
            ["chcon", "u:object_r:apk_data_file:s0", destination],
            ["rm", upload],
        )
    )
    if device.run("shell", "id", "-u").stdout.strip() == "0":
        device.run("shell", script)
    else:
        device.run("shell", "su", "-c", script)
    command = [sys.executable, str(DEMO / "load.py"), native_dir, "--package", package]
    if args.remote:
        command += ["--remote", args.remote]
    elif args.serial:
        command += ["--serial", args.serial]
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
