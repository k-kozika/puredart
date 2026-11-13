"""VM-independent ELF, relocation, disassembly and DWARF inspection."""

import pathlib
import re
import subprocess

from elftools.elf.elffile import ELFFile


class ElfImage:
    def __init__(self, path):
        self.path = pathlib.Path(path)
        self._file = self.path.open("rb")
        try:
            self.elf = ELFFile(self._file)
            table = self.elf.get_section_by_name(
                ".symtab"
            ) or self.elf.get_section_by_name(".dynsym")
            self.symbols = list(table.iter_symbols()) if table else []
        except BaseException:
            self._file.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self._file.close()

    @property
    def build_id(self):
        ids = {
            note["n_desc"]
            for segment in self.elf.iter_segments()
            if segment["p_type"] == "PT_NOTE"
            for note in segment.iter_notes()
            if note["n_type"] == "NT_GNU_BUILD_ID"
        }
        if len(ids) != 1:
            raise ValueError(f"ambiguous/missing ELF GNU Build ID: {ids}")
        return ids.pop()

    def has_symbol(self, fragment):
        return any(
            fragment in symbol.name and symbol["st_value"] for symbol in self.symbols
        )

    def symbol(self, fragment):
        values = {
            symbol["st_value"]
            for symbol in self.symbols
            if fragment in symbol.name and symbol["st_value"]
        }
        if len(values) != 1:
            raise ValueError(f"ambiguous/missing symbol {fragment}: {values}")
        return values.pop()

    def word(self, address, width=4):
        if width <= 0:
            raise ValueError("ELF word width must be positive")
        for segment in self.elf.iter_segments():
            offset = address - segment["p_vaddr"]
            if (
                segment["p_type"] == "PT_LOAD"
                and 0 <= offset <= segment["p_filesz"] - width
            ):
                self._file.seek(segment["p_offset"] + offset)
                data = self._file.read(width)
                if len(data) != width:
                    raise ValueError("truncated ELF word")
                return int.from_bytes(
                    data,
                    "little" if self.elf.little_endian else "big",
                )
        raise ValueError(f"address outside file-backed ELF segments: {address:#x}")

    def relative_pointer(self, address):
        relocations = [
            relocation
            for section in self.elf.iter_sections()
            if section["sh_type"] == "SHT_RELA"
            for relocation in section.iter_relocations()
            if relocation["r_offset"] == address
        ]
        if not relocations:
            return self.word(address, self.elf.elfclass // 8)
        relative = {"EM_AARCH64": 1027, "EM_X86_64": 8}.get(self.elf["e_machine"])
        if len(relocations) != 1 or relocations[0]["r_info_type"] != relative:
            raise ValueError(
                f"expected one relative pointer relocation at {address:#x}"
            )
        return relocations[0]["r_addend"]


class LlvmTools:
    def __init__(self, directory=None):
        self.directory = pathlib.Path(directory) if directory else None

    def _run(self, tool, *args):
        executable = str(self.directory / tool) if self.directory else tool
        return subprocess.check_output([executable, *map(str, args)], text=True)

    def disassemble(self, path, address, size=192):
        return self._run(
            "llvm-objdump",
            "-d",
            f"--start-address={address}",
            f"--stop-address={address + size}",
            path,
        )

    def member_offset(self, path, name, *, source=None):
        dump = self._run("llvm-dwarfdump", "--name=" + name, path)
        values = {
            int(match[1], 16)
            for block in dump.split("DW_TAG_member")
            if source is None or source in block
            for match in re.finditer(
                r"DW_AT_data_member_location\s+\(0x([0-9a-f]+)\)", block
            )
        }
        if len(values) != 1:
            raise ValueError(f"ambiguous/missing DWARF member {name}: {values}")
        return values.pop()
