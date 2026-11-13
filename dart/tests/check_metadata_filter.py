"""Integration check using the release APK built from dart/example/app.

uv run --locked --group blutter python dart/tests/check_metadata_filter.py --blutter <rebuilt-native-executable> \
    --libapp <libapp.so-extracted-from-demo-apk>
"""

import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blutter", type=Path, required=True)
    parser.add_argument("--libapp", type=Path, required=True)
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    with tempfile.TemporaryDirectory(prefix="puredart-metadata-filter-") as directory:
        work = Path(directory)
        consumer = root / "tests/fixtures/metadata_consumer.cpp"

        def generate(label, selectors=(), fail=False):
            out = work / label
            out.mkdir(exist_ok=True)
            command = [
                str(args.blutter.resolve()),
                "-i",
                str(args.libapp.resolve()),
                "-o",
                str(out),
            ]
            for selector in selectors:
                command += ["--metadata-class", selector]
            result = subprocess.run(command, capture_output=True, text=True)
            if fail:
                assert result.returncode != 0, result.stdout
                assert "selector matched no classes" in result.stderr, result.stderr
                return out
            assert result.returncode == 0, result.stderr
            executable = out / "consumer"
            subprocess.run(
                [
                    args.cxx,
                    "-std=c++17",
                    "-I",
                    str(root / "include"),
                    "-I",
                    str(root / "third_party/blutter/blutter/src"),
                    str(consumer),
                    str(root / "src/meta.cpp"),
                    str(out / "blutter_meta.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            records = {"C": set(), "S": set(), "K": set()}
            for line in subprocess.check_output(
                [str(executable)], text=True
            ).splitlines():
                kind, number, *values = line.split("\t")
                records[kind].add(
                    (int(number), *(bytes.fromhex(value) for value in values))
                )
            return records

        full = generate("full")
        single = generate("single", ["MyHomePage"])
        assert single["C"] == {c for c in full["C"] if c[1] == b"MyHomePage"}
        assert len(single["C"]) == 1
        assert single["S"] == {s for s in full["S"] if b"MyHomePage::" in s[1]}
        assert single["S"]
        assert single["K"] == {c for c in full["K"] if c[1] == b"MyHomePage"}
        assert single["K"]  # The demo's title constant.

        # Resolve library URLs and class IDs from the assembly dump, including
        # duplicate names and generic classes, without hard-coding their IDs.
        libraries = {}
        for path in (work / "full/asm").rglob("*.dart"):
            source = path.read_text()
            url = source.splitlines()[0].split("url: ", 1)[1]
            for cid in re.findall(r"^// class id: (\d+),", source, re.MULTILINE):
                libraries[int(cid)] = url
        cid = next(iter(single["C"]))[0]
        qualified = generate("qualified", [libraries[cid] + "::MyHomePage"])
        assert qualified == single
        assert generate("repeat", ["MyHomePage", "MyHomePage"]) == single

        state = generate("state", ["_MyHomePageState"])
        assert state["S"] and not state["K"]
        combined = generate("combined", ["MyHomePage", "_MyHomePageState"])
        assert combined == {key: single[key] | state[key] for key in full}
        assert len(combined["C"]) == 2  # No parents/children added implicitly.
        assert all(b"::" in symbol[1] for symbol in combined["S"])

        counts = Counter(name for _, name in full["C"] if name)
        # Metadata can contain classes omitted from the assembly dump. Qualified
        # selectors need a library URL for every class sharing the chosen name.
        duplicates = sorted(
            name
            for name, count in counts.items()
            if count > 1
            and all(cid in libraries for cid, cls in full["C"] if cls == name)
        )
        if duplicates:
            name = duplicates[0]
            bare = generate("duplicate-name", [name.decode()])
            assert bare["C"] == {c for c in full["C"] if c[1] == name}
            cid = next(iter(bare["C"]))[0]
            url = libraries[cid]
            specific = generate("duplicate-qualified", [url + "::" + name.decode()])
            assert specific["C"] == {c for c in bare["C"] if libraries[c[0]] == url}
        else:
            print("SKIP: demo has no duplicate class names with complete assembly data")

        # Choose a class with neither methods nor constants to exercise empty tables.
        empty_name = next(
            name
            for _, name in sorted(full["C"])
            if name
            and counts[name] == 1
            and not any(name + b"::" in s[1] for s in full["S"])
            and not any(c[1] == name for c in full["K"])
        )
        empty = generate("empty-tables", [empty_name.decode()])
        assert len(empty["C"]) == 1 and not empty["S"] and not empty["K"]
        missing = generate("missing", ["MyHomePage", "MissingMetadataClass"], fail=True)
        assert not (missing / "blutter_meta.cpp").exists()
        assert not (missing / "blutter_meta.h").exists()
        # Existing files survive failed selection without truncation.
        target = work / "single"
        before = {
            name: (target / name).read_bytes()
            for name in ("blutter_meta.cpp", "blutter_meta.h")
        }
        generate("single", ["MissingMetadataClass"], fail=True)
        assert all(
            (target / name).read_bytes() == data for name, data in before.items()
        )
        for selector in ("", "myhomepage", "package:missing.dart::MyHomePage"):
            generate("invalid", [selector], fail=True)
        print("metadata filter integration checks passed")


if __name__ == "__main__":
    main()
