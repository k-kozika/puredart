#!/usr/bin/env python3
"""Inspect a native ELF's Build ID, symbols, relative pointers and DWARF members."""

import argparse
import json
import pathlib
import subprocess
import sys

from hooktools.elf import ElfImage, LlvmTools


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=pathlib.Path)
    parser.add_argument("--symbol", action="append", default=[])
    parser.add_argument(
        "--pointer", type=lambda value: int(value, 0), action="append", default=[]
    )
    parser.add_argument("--member", action="append", default=[])
    parser.add_argument(
        "--source", help="source-file substring to disambiguate DWARF members"
    )
    parser.add_argument("--llvm", type=pathlib.Path)
    parser.add_argument(
        "--address", type=lambda value: int(value, 0), help="disassemble this RVA"
    )
    parser.add_argument("--size", type=int, default=192)
    args = parser.parse_args()
    try:
        with ElfImage(args.elf) as image:
            result = {
                "path": str(args.elf),
                "machine": image.elf["e_machine"],
                "bits": image.elf.elfclass,
                "build_id": image.build_id,
                "symbols": {name: image.symbol(name) for name in args.symbol},
                "pointers": {
                    hex(address): image.relative_pointer(address)
                    for address in args.pointer
                },
            }
        llvm = LlvmTools(args.llvm)
        if args.member:
            result["members"] = {
                name: llvm.member_offset(args.elf, name, source=args.source)
                for name in args.member
            }
        if args.address is not None:
            result["disassembly"] = llvm.disassemble(args.elf, args.address, args.size)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"ELF tools: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
