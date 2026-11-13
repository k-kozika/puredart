#!/usr/bin/env python3
"""Build four release APK harnesses using existing pinned VM toolchains.

Requires Android SDK/NDK, Java, CMake 3.x, Python pyelftools. Tests real VMs;
no cross-version bytecode conversion and no target bytecode mutation.
"""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import urllib.request
import zipfile

from toolchains import toolchain

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from hooktools.sdk import find_ndk, find_ndk_prebuilt, find_sdk


def run(args, **kwargs):
    subprocess.run([str(x) for x in args], check=True, **kwargs)


def build(version, sdk, ndk, cmake, python):
    prebuilt = find_ndk_prebuilt(ndk)
    tc = toolchain(version)
    work = tc / "device"
    work.mkdir(exist_ok=True)
    tools = sdk / "build-tools/37.0.0"
    android = sdk / "platforms/android-35/android.jar"
    vm = tc / (
        "android/API/hermes/libhermes.so"
        if version == 96
        else "android/lib/libhermesvm.so"
    )
    run(
        [
            python,
            ROOT / "hermes/tools/profile.py",
            vm,
            version,
            tc / "profile.json",
            "--llvm",
            prebuilt / "bin",
        ]
    )
    run(
        [
            tc / "host/bin/hermesc",
            "-O",
            "-emit-binary",
            "-out",
            tc / "fixture.hbc",
            ROOT / "hermes/tests/fixture.js",
        ]
    )
    run(
        [
            cmake,
            "-S",
            ROOT / "hermes/tests/android",
            "-B",
            work / "native-build",
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_TOOLCHAIN_FILE="
            + str(ndk / "build/cmake/android.toolchain.cmake"),
            "-DANDROID_ABI=arm64-v8a",
            "-DANDROID_PLATFORM=android-26",
            "-DANDROID_STL=c++_shared",
            "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384",
            "-DREPOSITORY=" + str(ROOT),
            "-DTOOLCHAIN=" + str(tc),
            "-DVM_LIBRARY=" + str(vm),
        ]
    )
    run([cmake, "--build", work / "native-build", "-j", "4"])
    classes = work / "classes"
    classes.mkdir(exist_ok=True)
    dex = work / "dex"
    dex.mkdir(exist_ok=True)
    native_loader = ROOT / "build/hermes-toolchains/fbjni/nativeloader.jar"
    if not native_loader.exists():
        urllib.request.urlretrieve(
            "https://repo.maven.apache.org/maven2/com/facebook/soloader/nativeloader/0.12.1/nativeloader-0.12.1.jar",
            native_loader,
        )
    run(
        [
            "javac",
            "--release",
            "8",
            "-cp",
            str(android) + os.pathsep + str(native_loader),
            "-d",
            classes,
            ROOT / "hermes/tests/android/MainActivity.java",
        ]
    )
    run(
        [
            tools / "d8",
            "--lib",
            android,
            "--output",
            dex,
            classes / "org/purehermes/test/MainActivity.class",
            ROOT / "build/hermes-toolchains/fbjni/classes.jar",
            native_loader,
        ]
    )
    apk = work / "unsigned.apk"
    run(
        [
            tools / "aapt2",
            "link",
            "-I",
            android,
            "--manifest",
            ROOT / "hermes/tests/android/AndroidManifest.xml",
            "--version-code",
            version,
            "-o",
            apk,
        ]
    )
    cxx = prebuilt / "sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
    fbjni = (
        ROOT
        / "build/hermes-toolchains/fbjni/prefab/modules/fbjni/libs/android.arm64-v8a/libfbjni.so"
    )
    with zipfile.ZipFile(apk, "a", compression=zipfile.ZIP_DEFLATED) as z:
        z.write(dex / "classes.dex", "classes.dex")
        z.write(tc / "fixture.hbc", "assets/fixture.hbc")
        for src in [vm, work / "native-build/libtest.so", cxx, fbjni]:
            z.write(src, "lib/arm64-v8a/" + src.name)
    run([tools / "zipalign", "-f", "4", apk, work / "aligned.apk"])
    keystore = ROOT / "build/hermes-toolchains/test.keystore"
    if not keystore.exists():
        run(
            [
                "keytool",
                "-genkeypair",
                "-keystore",
                keystore,
                "-storepass",
                "android",
                "-keypass",
                "android",
                "-alias",
                "test",
                "-dname",
                "CN=Purehermes Test",
                "-keyalg",
                "RSA",
                "-validity",
                "3650",
            ]
        )
    run(
        [
            tools / "apksigner",
            "sign",
            "--ks",
            keystore,
            "--ks-pass",
            "pass:android",
            "--key-pass",
            "pass:android",
            "--out",
            work / "test.apk",
            work / "aligned.apk",
        ]
    )
    return work / "test.apk"


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--versions", type=int, nargs="+", default=[96, 97, 98, 99])
    p.add_argument("--sdk", type=pathlib.Path)
    p.add_argument("--ndk", type=pathlib.Path)
    p.add_argument("--cmake", default=shutil.which("cmake"))
    a = p.parse_args()
    sdk = find_sdk(a.sdk)
    ndk = find_ndk(a.ndk, sdk=sdk, version="27.1.12297006")
    for v in a.versions:
        print(
            build(v, sdk, ndk, a.cmake, sys.executable),
            flush=True,
        )
