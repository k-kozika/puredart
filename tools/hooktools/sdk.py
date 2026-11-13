"""Find Android host tools without selecting a VM-specific SDK/NDK version."""

import os
import pathlib
import shutil


def _directory(value, label):
    path = pathlib.Path(value).expanduser().resolve()
    if not path.is_dir():
        raise ValueError(f"{label} directory does not exist: {path}")
    return path


def find_sdk(explicit=None):
    configured = (
        explicit or os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    )
    if configured:
        return _directory(configured, "Android SDK")
    for path in (
        pathlib.Path.home() / "Android/Sdk",
        pathlib.Path.home() / "Library/Android/sdk",
    ):
        if path.is_dir():
            return path.resolve()
    raise ValueError("set ANDROID_HOME/ANDROID_SDK_ROOT or pass an SDK path")


def find_ndk(explicit=None, *, sdk=None, version=None):
    configured = explicit or os.environ.get("ANDROID_NDK")
    if configured:
        return _directory(configured, "Android NDK")
    if version is None:
        raise ValueError("set ANDROID_NDK, pass an NDK path, or specify an NDK version")
    return _directory(find_sdk(sdk) / "ndk" / version, "Android NDK")


def find_ndk_prebuilt(ndk):
    """Find the installed NDK host toolchain without hard-coding its host tag."""
    root = _directory(ndk, "Android NDK") / "toolchains/llvm/prebuilt"
    candidates = [
        path
        for path in root.glob("*")
        if (path / "bin").is_dir() and (path / "sysroot").is_dir()
    ]
    if len(candidates) != 1:
        raise ValueError(f"expected one NDK LLVM host toolchain under {root}")
    return candidates[0]


def find_adb(explicit=None, *, sdk=None):
    configured = explicit or os.environ.get("ADB_BIN")
    if configured:
        executable = shutil.which(str(pathlib.Path(configured).expanduser()))
        if executable:
            return executable
        raise ValueError(f"ADB executable not found: {configured}")
    try:
        candidate = find_sdk(sdk) / "platform-tools/adb"
    except ValueError:
        candidate = None
    if candidate and candidate.is_file() and os.access(candidate, os.X_OK):
        return str(candidate)
    executable = shutil.which("adb")
    if executable:
        return executable
    raise ValueError("ADB not found; set ADB_BIN or ANDROID_HOME")
