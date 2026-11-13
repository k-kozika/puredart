"""Run local ADB and quote structured Android shell arguments."""

import os
import pathlib
import shlex
import subprocess

from .sdk import find_adb


class Device:
    def __init__(self, *, serial=None, adb=None, sdk=None):
        self.serial = serial or os.environ.get("ANDROID_SERIAL")
        self.adb = find_adb(adb, sdk=sdk)

    def run(self, *args, capture=True):
        arguments = list(map(str, args))
        if len(arguments) > 2 and arguments[0] == "shell":
            arguments = ["shell", shlex.join(arguments[1:])]
        arguments = (["-s", self.serial] if self.serial else []) + arguments
        return subprocess.run(
            [self.adb, *arguments],
            check=True,
            text=True,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE if capture else None,
        )

    def install(self, apk, *, options=("-r",), capture=True):
        path = pathlib.Path(apk).resolve(strict=True)
        return self.run("install", *options, path, capture=capture)

    def push(self, local, remote, *, capture=True):
        path = pathlib.Path(local).resolve(strict=True)
        return self.run("push", path, remote, capture=capture)

    def pull(self, remote, local, *, capture=True):
        return self.run("pull", remote, local, capture=capture)

    def environment(self):
        def output(*args):
            return self.run(*args).stdout.strip()

        return {
            "serial": self.serial,
            "abi": output("shell", "getprop", "ro.product.cpu.abi"),
            "sdk": output("shell", "getprop", "ro.build.version.sdk"),
            "page_size": int(output("shell", "getconf", "PAGESIZE")),
            "build_fingerprint": output("shell", "getprop", "ro.build.fingerprint"),
        }
