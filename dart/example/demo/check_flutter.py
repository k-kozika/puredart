#!/usr/bin/env python3
"""Validate the Flutter machine-version output used by the release demo."""

import json
import sys

version = json.load(sys.stdin)
if (
    version.get("frameworkVersion") != "3.41.2"
    or version.get("dartSdkVersion", "").split(" ")[0] != "3.11.0"
):
    sys.exit(
        "puredart demo: requires Flutter 3.41.2 / Dart 3.11.0; use dart/example/app/mise.toml or FLUTTER_BIN"
    )
