#!/usr/bin/env python3
"""Locate the Android SDK/NDK, extract APK entries or run local ADB."""

import argparse
import pathlib
import subprocess
import sys
import zipfile

from hooktools.adb import Device
from hooktools.sdk import find_ndk, find_sdk


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", "-s")
    parser.add_argument("--adb", help="local ADB executable; supports leading ~/")
    parser.add_argument("--sdk")
    parser.add_argument("--ndk")
    parser.add_argument("--ndk-version")
    parser.add_argument("command", choices=("sdk", "ndk", "adb", "extract"))
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.command == "sdk":
            print(find_sdk(args.sdk))
        elif args.command == "ndk":
            print(find_ndk(args.ndk, sdk=args.sdk, version=args.ndk_version))
        elif args.command == "extract":
            if len(args.arguments) != 3:
                parser.error("extract requires APK ENTRY OUTPUT")
            apk, entry, output = args.arguments
            if pathlib.Path(apk).resolve() == pathlib.Path(output).resolve():
                raise ValueError("cannot overwrite the input APK")
            with zipfile.ZipFile(apk) as archive:
                data = archive.read(entry)
            pathlib.Path(output).write_bytes(data)
        else:
            command = args.arguments
            if not command:
                parser.error("adb requires a command")
            device = Device(serial=args.serial, adb=args.adb, sdk=args.sdk)
            if command[0] == "install" and len(command) >= 2:
                device.install(command[-1], options=command[1:-1], capture=False)
            elif command[0] in ("push", "pull") and len(command) == 3:
                getattr(device, command[0])(command[1], command[2], capture=False)
            else:
                device.run(*command, capture=False)
    except (
        OSError,
        ValueError,
        KeyError,
        zipfile.BadZipFile,
        subprocess.CalledProcessError,
    ) as error:
        print(f"android tools: {error}", file=sys.stderr)
        return (
            error.returncode if isinstance(error, subprocess.CalledProcessError) else 1
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
