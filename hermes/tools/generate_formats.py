#!/usr/bin/env python3
"""Generate immutable C++ descriptors from pinned official Hermes sources.

Uses the C preprocessor for opcode macros; reads field declarations from the
official format header at the commits in formats.lock.json. No third-party tables.
"""

import argparse
import json
import pathlib
import re
import subprocess

from toolchains import format_pins, toolchain


def generate(source_root, output):
    lines = [
        "// Generated from Meta Hermes (MIT). See formats.lock.json.",
        "#pragma once",
        "namespace purehermes::detail {",
    ]
    lock = {}
    pins = format_pins()
    for version, pin in pins.items():
        root = toolchain(version, source_root) / "source"
        bc = root / "include/hermes/BCGen/HBC"
        text = (bc / "BytecodeFileFormat.h").read_text()
        assert re.search(
            r"BYTECODE_VERSION\s*=\s*" + str(version) + r"\b",
            (bc / "BytecodeVersion.h").read_text(),
        )
        preprocessed = subprocess.check_output(
            [
                "clang",
                "-E",
                "-P",
                "-I",
                str(bc),
                str(pathlib.Path(__file__).with_name("opcodes.c")),
            ],
            text=True,
        )
        string_masks = {}
        for name, n in re.findall(r"PH_STRING\(([^,]+),\s*(\d+)\)", preprocessed):
            name = name.strip()
            string_masks[name] = string_masks.get(name, 0) | (1 << (int(n) - 1))
        opcodes = []
        widths = {
            "Reg8": 1,
            "Reg32": 4,
            "UInt8": 1,
            "UInt16": 2,
            "UInt32": 4,
            "Addr8": 1,
            "Addr32": 4,
            "Imm32": 4,
            "Double": 8,
        }
        for match in re.finditer(r"PH_OP\(([^()]*)\)", preprocessed):
            parts = [x.strip() for x in match[1].split(",") if x.strip()]
            types = parts[1:]
            assert all(t in widths for t in types), parts
            opcodes.append((parts[0], types))
        assert 100 < len(opcodes) <= 256
        lines.append(f"static constexpr Op ops_{version}[] = {{")
        for name, types in opcodes:
            ws = [
                (-widths[t] if t.startswith("Addr") or t == "Imm32" else widths[t])
                for t in types
            ]
            lines.append(
                '{"'
                + name
                + '",'
                + str(string_masks.get(name, 0))
                + ","
                + str(len(types))
                + ",{"
                + ",".join(map(str, ws))
                + "}},"
            )
        lines.append("};")
        # Packed header words and small-function bitfields are explicitly decoded.
        header = text.split("struct BytecodeFileHeader {", 1)[1].split(
            "BytecodeOptions options;", 1
        )[0]
        fields = [
            f for f in re.findall(r"uint32_t\s+(\w+)\s*;", header) if f != "version"
        ]
        lines.append(
            f"static constexpr const char* header_{version}[] = {{"
            + ",".join(json.dumps(f) for f in fields)
            + "};"
        )
        block = text.split("#define FUNC_HEADER_FIELDS", 1)[1].split("/**", 1)[0]
        rawfields = []
        for call in re.findall(r"\b[VFN]\(([^()]*)\)", block):
            a = [s.strip() for s in call.split(",")]
            if len(a) == 4:
                name, bits = a[2], int(a[3])
            elif len(a) == 5:
                name, bits = a[3], int(a[4])
            elif len(a) == 6:
                name, bits = a[4], int(a[5])
            else:
                continue
            rawfields.append(
                (
                    name[0].lower() + name[1:],
                    bits,
                    1 if (a[0] if len(a) == 4 else a[2]) == "uint8_t" else 4,
                )
            )
        assert rawfields and rawfields[0][0] == "offset"
        lines.append(
            f"static constexpr Field fields_{version}[] = {{"
            + ",".join(
                "{" + json.dumps(n) + "," + str(b) + "," + str(w) + "}"
                for n, b, w in rawfields
            )
            + "};"
        )
        lock[str(version)] = {
            "repository": pin["repository"],
            "commit": pin["commit"],
            "ret_opcode": next(i for i, (n, _) in enumerate(opcodes) if n == "Ret"),
        }
    lines.append("inline Format format(uint32_t v) { switch(v) {")
    for v in pins:
        lines.append(
            f"case {v}: return {{ops_{v},sizeof(ops_{v})/sizeof(Op),header_{v},sizeof(header_{v})/sizeof(const char*),fields_{v},sizeof(fields_{v})/sizeof(Field),{16 if v == 96 else 12}}};"
        )
    lines.append('default: throw std::runtime_error("unsupported HBC version"); }}\n}')
    output.write_text("\n".join(lines) + "\n")
    output.with_name("formats.lock.json").write_text(json.dumps(lock, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source_root", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    generate(args.source_root, args.output)
