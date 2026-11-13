#!/usr/bin/env python3
"""Rebuild parser goldens using the four pinned official compilers."""

import argparse
import pathlib
import shutil
import subprocess
import tempfile

from toolchains import format_pins, toolchain

ROOT = pathlib.Path(__file__).resolve().parents[2]
if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--output", type=pathlib.Path, default=ROOT / "build/fixtures")
    p.add_argument(
        "--versions",
        type=int,
        nargs="+",
        choices=list(format_pins()),
        default=list(format_pins()),
    )
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    # Hermes records source paths in HBC. Keep the historical relative names
    # while copying checked-in source, so goldens remain byte-for-byte stable.
    sources = [
        ("hermes/tests/fixture.js", "hermes/tests/fixture.js", "v"),
        ("hermes/tests/fixtures/large.js", "build/large.js", "large"),
    ]
    with tempfile.TemporaryDirectory(prefix="purehermes-fixtures-") as directory:
        work = pathlib.Path(directory)
        for source, relative, _ in sources:
            destination = work / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / source, destination)
        for v in a.versions:
            compiler = toolchain(v) / "host/bin/hermesc"
            for _, relative, prefix in sources:
                subprocess.run(
                    [
                        str(compiler),
                        "-O",
                        "-emit-binary",
                        "-out",
                        str((a.output / f"{prefix}{v}.hbc").resolve()),
                        relative,
                    ],
                    cwd=work,
                    check=True,
                )
