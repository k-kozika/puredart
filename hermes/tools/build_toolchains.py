#!/usr/bin/env python3
"""Build the four pinned official compilers and Android VMs, without source edits.

Run with uv run --group build. SDK/NDK and JDK are external prerequisites.
Downloaded archives and builds are cached under the pinned commit in build/.
"""

import argparse
import pathlib
import subprocess
import sys
import urllib.request
import zipfile

from toolchains import fetch_source, format_pins, toolchain

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from hooktools.sdk import find_ndk


def run(args):
    subprocess.run([str(x) for x in args], check=True)


def prepare_fbjni(work):
    root = work / "fbjni"
    root.mkdir(exist_ok=True)
    archive = root / "fbjni.aar"
    if not archive.exists():
        urllib.request.urlretrieve(
            "https://repo.maven.apache.org/maven2/com/facebook/fbjni/fbjni/0.7.0/fbjni-0.7.0.aar",
            archive,
        )
    with zipfile.ZipFile(archive) as z:
        z.extractall(root)
    return root


def build(v, work, ndk, cmake, jobs, fbjni, link_page_size):
    tc = toolchain(v, work)
    source = fetch_source(v, work)
    common = [
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DHERMES_ENABLE_INTL=OFF",
        "-DHERMES_ENABLE_TEST_SUITE=OFF",
        "-DHERMES_ENABLE_WERROR=OFF",
    ]
    run([cmake, "-S", source, "-B", tc / "host", *common, "-DCMAKE_BUILD_TYPE=Release"])
    run([cmake, "--build", tc / "host", "--target", "hermesc", "-j", jobs])
    imports = (
        tc
        / "host"
        / ("ImportHermesc.cmake" if v == 96 else "ImportHostCompilers.cmake")
    )
    options = [
        "-DCMAKE_TOOLCHAIN_FILE=" + str(ndk / "build/cmake/android.toolchain.cmake"),
        "-DANDROID_ABI=arm64-v8a",
        "-DANDROID_PLATFORM=android-26",
        "-DANDROID_STL=c++_shared",
        f"-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size={link_page_size}",
        "-DHERMES_UNICODE_LITE=ON",
        "-DHERMES_IS_MOBILE_BUILD=ON",
        "-DHERMES_ENABLE_TOOLS=OFF",
        "-DHERMES_ENABLE_DEBUGGER=OFF",
        "-DHERMESVM_ALLOW_JIT=0",
    ]
    options += (
        ["-DIMPORT_HERMESC=" + str(imports)]
        if v == 96
        else ["-DIMPORT_HOST_COMPILERS=" + str(imports)]
    )
    options += ["-DHERMES_IS_ANDROID=" + ("ON" if v == 99 else "OFF")]
    if v == 96:
        options += ["-DICU_FOUND=TRUE", "-DICU_LIBRARIES="]
    if v == 97:
        options += ["-DCMAKE_CXX_FLAGS=-DHERMES_PARSE_FLOW=1 -DHERMES_PARSE_TS=1"]
    if v == 99:
        options += [
            "-Dfbjni_DIR=" + str(ROOT / "hermes/cmake/fbjni"),
            "-DFBJNI_ROOT=" + str(fbjni),
        ]
    run([cmake, "-S", source, "-B", tc / "android", *common, *options])
    run(
        [
            cmake,
            "--build",
            tc / "android",
            "--target",
            "libhermes" if v == 96 else "hermesvm",
            "-j",
            jobs,
        ]
    )


if __name__ == "__main__":
    import shutil

    p = argparse.ArgumentParser()
    p.add_argument(
        "--versions",
        type=int,
        nargs="+",
        choices=list(format_pins()),
        default=list(format_pins()),
    )
    p.add_argument("--ndk", type=pathlib.Path)
    p.add_argument(
        "--fetch-only",
        action="store_true",
        help="fetch pinned sources without compiling",
    )
    p.add_argument("--cmake", default=shutil.which("cmake"))
    p.add_argument("--jobs", default="4")
    p.add_argument("--link-page-size", type=int, choices=[4096, 16384], default=16384)
    a = p.parse_args()
    work = ROOT / "build/hermes-toolchains"
    work.mkdir(parents=True, exist_ok=True)
    if a.fetch_only:
        for version in a.versions:
            print(fetch_source(version, work))
    else:
        ndk = find_ndk(a.ndk, version="27.1.12297006")
        fbjni = prepare_fbjni(work)
        for version in a.versions:
            build(version, work, ndk, a.cmake, a.jobs, fbjni, a.link_page_size)
