#!/usr/bin/env python3
"""Extract an explicit Android ARM64 interpreter profile from unstripped ELF.

Requires pyelftools and NDK llvm tools. Refuses ambiguous register locations.
The output is a candidate until the device suite validates the exact build ID.
"""

import argparse
import json
import pathlib
import re
import sys

from profile_fields import PROFILE_FIELDS

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from hooktools.elf import ElfImage, LlvmTools


def extract(path, version, llvm):
    llvm = LlvmTools(llvm)
    with ElfImage(path) as image:
        if image.elf["e_machine"] != "EM_AARCH64":
            raise ValueError("Hermes runtime profile requires an ARM64 ELF")
        symbol = image.symbol
        word = image.word
        build_id = image.build_id
        ctor = symbol("createCodeBlockE")
        mapping = symbol("createSymbolFromStringIDMayAllocateE")
        lazy = symbol("registerLazyIdentifierImplIcE")
        handle = symbol("15getSymbolHandleERNS0_7RuntimeEN4llvh8ArrayRefIcEEj")
        destructor = (
            symbol("RuntimeModuleD2Ev")
            if image.has_symbol("RuntimeModuleD2Ev")
            else symbol("RuntimeModuleD1Ev")
        )
        table = symbol("opcodeDispatch")
        fmt = ROOT / "hermes/src/formats.lock.json"
        ret = json.loads(fmt.read_text())[str(version)]["ret_opcode"]
        slot = table + ret * 8
        handler = image.relative_pointer(slot)
        first = {
            name: word(value)
            for name, value in [
                ("constructor", ctor),
                ("map", mapping),
                ("destructor", destructor),
                ("lazy", lazy),
                ("handle", handle),
            ]
        }
        lazy_has_runtime = any(
            "7Runtime" in s.name
            for s in image.symbols
            if s["st_value"] == lazy and "registerLazyIdentifierImplIcE" in s.name
        )
    dis = llvm.disassemble(path, handler)
    load = re.search(r"([0-9a-f]+):.*ldrb\s+w(\d+), \[x(\d+), #0x1\]", dis)
    if not load:
        raise ValueError("Ret operand load not recognized:\n" + dis)
    index = load[2]
    ip = int(load[3])
    frame = re.search(
        r"([0-9a-f]+):.*ldr\s+x\d+, \[x(\d+), x" + index + r", lsl #3\]", dis
    )
    if not frame:
        raise ValueError("Ret register load not recognized:\n" + dis)
    # curCodeBlock can be dead at Ret; use the callee slot, which also handles
    # compiler-deduplicated bytecode bodies correctly.
    codeblock_offset = llvm.member_offset(path, "codeBlock_")
    key_offset = 0
    if version <= 97:

        def member(name, source=None):
            return llvm.member_offset(path, name, source=source)

        module_runtime = member("runtime_", "/RuntimeModule.h")
        key_offset = member("heapStorage_") + member("pointerEncryptionKey_") + 8
    else:
        module_runtime = 0
    result = {
        "build_id": build_id,
        "hbc_version": version,
        "factory_rva": ctor,
        "map_string_rva": mapping,
        "destructor_rva": destructor,
        "dispatch_table_rva": table,
        "ret_handler_rva": handler,
        "factory_instruction": first["constructor"],
        "map_instruction": first["map"],
        "destructor_instruction": first["destructor"],
        "ip_register": ip,
        "frame_register": int(frame[2]),
        "factory_sret": version >= 98,
        "lazy_string_rva": lazy,
        "symbol_handle_rva": handle,
        "lazy_instruction": first["lazy"],
        "symbol_handle_instruction": first["handle"],
        "lazy_has_runtime": lazy_has_runtime,
        "closure_offset": {96: -56, 97: -72, 98: -64, 99: -64}[version],
        "function_codeblock_offset": codeblock_offset,
        "module_runtime_offset": module_runtime,
        "pointer_key_offset": key_offset,
    }
    return result, dis


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("elf", type=pathlib.Path)
    p.add_argument("version", type=int)
    p.add_argument("output", type=pathlib.Path)
    p.add_argument("--llvm", type=pathlib.Path, required=True)
    a = p.parse_args()
    result, dis = extract(a.elf, a.version, a.llvm)
    a.output.write_text(json.dumps(result, indent=2) + "\n")
    a.output.with_suffix(".disasm").write_text(dis)
    a.output.with_suffix(".h").write_text(
        '#pragma once\n#include "purehermes/purehermes.h"\ninline purehermes::RuntimeProfile test_profile() { return {'
        + ", ".join(
            json.dumps(v) if isinstance(v, (str, bool)) else str(v)
            for v in (result[key] for key in PROFILE_FIELDS)
        )
        + "}; }\n"
    )
    print(json.dumps(result))
