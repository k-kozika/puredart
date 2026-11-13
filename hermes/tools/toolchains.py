"""Official source pins and commit-keyed Hermes toolchain directories."""

import json
import pathlib
import tarfile
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
CACHE = ROOT / "build/hermes-toolchains"
LOCK = ROOT / "hermes/src/formats.lock.json"


def format_pins():
    return {int(version): pin for version, pin in json.loads(LOCK.read_text()).items()}


def toolchain(version, cache=CACHE):
    return cache / str(version) / format_pins()[version]["commit"]


def fetch_source(version, cache=CACHE):
    pin = format_pins()[version]
    directory = toolchain(version, cache)
    directory.mkdir(parents=True, exist_ok=True)
    archive = directory / "source.tar.gz"
    source = directory / "source"
    if not archive.exists():
        owner_repo = pin["repository"].removeprefix("https://github.com/")
        download = archive.with_suffix(".download")
        urllib.request.urlretrieve(
            f"https://codeload.github.com/{owner_repo}/tar.gz/{pin['commit']}", download
        )
        download.replace(archive)
    if not source.exists():
        with tempfile.TemporaryDirectory(dir=directory, prefix="extract-") as work:
            tree = pathlib.Path(work) / "source"
            tree.mkdir()
            with tarfile.open(archive) as package:
                for member in package.getmembers():
                    parts = member.name.split("/", 1)
                    if len(parts) == 2 and parts[1]:
                        member.name = parts[1]
                        package.extract(member, tree, filter="data")
            tree.rename(source)
    return source
