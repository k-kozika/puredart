"""Inspect a real host ELF, including file bounds and relative relocations."""

import importlib.util
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
HAS_ELFTOOLS = importlib.util.find_spec("elftools") is not None
if HAS_ELFTOOLS:
    from hooktools.elf import ElfImage, LlvmTools


@unittest.skipUnless(
    HAS_ELFTOOLS and sys.platform == "linux" and shutil.which("cc"),
    "requires pyelftools and a Linux C compiler",
)
class ElfToolsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix="hooktools-elf-")
        cls.addClassCleanup(cls.work.cleanup)
        cls.path = pathlib.Path(cls.work.name) / "fixture.so"
        source = pathlib.Path(__file__).with_name("fixtures") / "elf_probe.c"
        subprocess.run(
            [
                "cc",
                "-g",
                "-shared",
                "-fPIC",
                "-Wl,--build-id=sha1",
                str(source),
                "-o",
                str(cls.path),
            ],
            check=True,
        )

    def test_build_id_and_symbol_identity(self):
        with ElfImage(self.path) as image:
            self.assertRegex(image.build_id, r"^[0-9a-f]{40}$")
            self.assertTrue(image.has_symbol("probe_function"))
            self.assertFalse(image.has_symbol("missing_symbol"))
            with self.assertRaises(ValueError):
                image.symbol("probe_")
            with self.assertRaises(ValueError):
                image.symbol("missing_symbol")

    def test_relative_pointer_and_file_backed_bounds(self):
        with ElfImage(self.path) as image:
            value = image.symbol("probe_value")
            self.assertEqual(image.word(value), 23)
            self.assertEqual(
                image.relative_pointer(image.symbol("shared_pointer")), value
            )
            with self.assertRaises(ValueError):
                image.relative_pointer(image.symbol("external_pointer"))
            with self.assertRaises(ValueError):
                image.word(image.symbol("bss_sample"))
            with self.assertRaises(ValueError):
                image.word(2**63)

    @unittest.skipUnless(
        shutil.which("llvm-dwarfdump") and shutil.which("llvm-objdump"),
        "requires LLVM tools",
    )
    def test_disassembly_and_dwarf_member(self):
        llvm = LlvmTools()
        with ElfImage(self.path) as image:
            address = image.symbol("probe_function")
        self.assertIn("probe_function", llvm.disassemble(self.path, address, 32))
        self.assertEqual(llvm.member_offset(self.path, "hooktools_member"), 8)
        with self.assertRaises(ValueError):
            llvm.member_offset(self.path, "missing_member")


if __name__ == "__main__":
    unittest.main()
