#!/usr/bin/env python3
"""Record ADB arguments and transfer only files in the test's fake device root."""

import json
import os
import pathlib
import shutil
import sys

args = sys.argv[1:]
if os.environ.get("FAKE_ADB_TRACE"):
    with open(os.environ["FAKE_ADB_TRACE"], "a") as trace:
        trace.write(json.dumps(args) + "\n")
command = args[2:] if args[:1] == ["-s"] else args
if command[0] == "shell" and command[1].startswith("pm path "):
    print("package:/data/app/test/base.apk")
else:
    print(json.dumps(args))
if os.environ.get("FAKE_ADB_FAIL"):
    sys.exit(7)
if command[0] in ("push", "pull"):
    device_path = pathlib.Path(
        command[2] if command[0] == "push" else command[1]
    ).resolve()
    if device_path.is_relative_to(os.environ["FAKE_DEVICE_ROOT"]):
        shutil.copyfile(command[1], command[2])
