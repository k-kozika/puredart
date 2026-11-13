#!/usr/bin/env python3
"""Embed reviewed RuntimeProfile JSON without runtime JSON dependencies."""

import argparse
import json
import pathlib

from profile_fields import PROFILE_FIELDS


def generate(directory, output):
    lines = [
        "// Generated from profiles/*.json. Select exact build IDs only.",
        "static std::vector<RuntimeProfile> builtin_profiles() { return {",
    ]
    for path in sorted(directory.glob("*.json")):
        profile = json.loads(path.read_text())
        lines.append(
            "{"
            + ",".join(
                json.dumps(value) if isinstance(value, (str, bool)) else str(value)
                for value in (profile[key] for key in PROFILE_FIELDS)
            )
            + "},"
        )
    lines.append("}; }")
    output.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("directory", type=pathlib.Path)
    p.add_argument("output", type=pathlib.Path)
    a = p.parse_args()
    generate(a.directory, a.output)
