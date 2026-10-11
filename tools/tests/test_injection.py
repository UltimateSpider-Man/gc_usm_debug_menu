"""Exercise the injector against the supported retail DOL and fresh ELF.

Run after build_gamecube.ps1 -DryRun (or build_gamecube.sh --dry-run).
"""
import contextlib
import dataclasses
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))
import inject_gamecube_payload as inject


class InjectionChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original_path = ROOT / "main.original.dol"
        cls.original = cls.original_path.read_bytes()
        cls.elf = ROOT / "build/gamecube/usm_debug_menu.elf"
        cls.symbols = ROOT / "build/gamecube/usm_debug_menu.symbols.txt"
        cls.payload = inject.prepare_payload(
            cls.elf, "elf", cls.symbols, inject.TARGET_LOAD_ADDRESS,
            (inject.DEFAULT_HOOK_SYMBOL, inject.DEFAULT_UPDATE_HOOK_SYMBOL,
             inject.DEFAULT_PAD_HOOK_SYMBOL, inject.DEFAULT_ARENA_SYMBOL),
            inject.DEFAULT_END_SYMBOL)
        cls.patches = (
            inject.PatchSpec("render", 0x8016E55C, 0x4812636D, inject.DEFAULT_HOOK_SYMBOL),
            inject.PatchSpec("update", 0x8017BEE0, 0x4BFC7909, inject.DEFAULT_UPDATE_HOOK_SYMBOL),
            inject.PatchSpec("pad", 0x801ACCC8, 0x48165239, inject.DEFAULT_PAD_HOOK_SYMBOL),
            inject.PatchSpec("arena1", 0x802F39B8, 0x480012A1, inject.DEFAULT_ARENA_SYMBOL),
            inject.PatchSpec("arena2", 0x802F39F4, 0x48001265, inject.DEFAULT_ARENA_SYMBOL),
        ) + inject.progression_patches()

    def arguments(self, directory, *extra):
        return inject.make_argument_parser().parse_args([
            "--dol", str(self.original_path), "--payload", str(self.elf),
            "--symbols", str(self.symbols), "--output", str(directory / "patched.dol"),
            "--backup", str(directory / "original.dol"), *extra])

    def test_supported_original_revision(self):
        self.assertEqual(hashlib.sha256(self.original).hexdigest(), inject.EXPECTED_DOL_SHA256)

    def test_branch_encoding_independent_examples(self):
        self.assertEqual(inject.encode_relative_branch(0x80001000, 0x80001010, True), 0x48000011)
        self.assertEqual(inject.encode_relative_branch(0x80001010, 0x80001000, True), 0x4BFFFFF1)
        with self.assertRaises(inject.InjectionError):
            inject.encode_relative_branch(0x80000000, 0x84000000, True)

    def test_only_header_and_verified_binding_words_change(self):
        patched, applied = inject.build_patched_dol(self.original, self.payload, self.patches)
        source = inject.DolImage(self.original)
        result = inject.DolImage(patched)
        allowed = set()
        for header_offset in (0x0C, 0x54, 0x9C):
            allowed.update(range(header_offset, header_offset + 4))
        for patch in self.patches:
            offset = source.address_to_file_offset(patch.address, 4)
            allowed.update(range(offset, offset + 4))
            instruction = inject.read_u32_be(patched, offset)
            if patch.kind == "absolute":
                self.assertEqual(instruction, self.payload.symbols[patch.target_symbol])
                continue
            displacement = instruction & 0x03FFFFFC
            if displacement & 0x02000000:
                displacement -= 0x04000000
            self.assertEqual(patch.address + displacement, self.payload.symbols[patch.target_symbol])
            self.assertEqual(instruction & 3, 1)  # Relative branch with link.
        differences = {i for i, (a, b) in enumerate(zip(self.original, patched)) if a != b}
        self.assertTrue(differences <= allowed)
        self.assertEqual(len(applied), 26)
        offset, address, size = result.text_slot(3)
        self.assertEqual(address, 0x80636040)
        self.assertEqual(size, self.payload.end_address - address)
        self.assertEqual(patched[offset:offset + size], self.payload.image)

    def test_changed_opcode_is_rejected(self):
        altered = bytearray(self.original)
        offset = inject.DolImage(self.original).address_to_file_offset(0x8016E55C, 4)
        inject.write_u32_be(altered, offset, 0x60000000)
        with self.assertRaisesRegex(inject.InjectionError, "opcode mismatch"):
            inject.build_patched_dol(bytes(altered), self.payload, self.patches)

    def test_double_injection_is_rejected(self):
        patched, _ = inject.build_patched_dol(self.original, self.payload, self.patches)
        with self.assertRaises(inject.InjectionError):
            inject.build_patched_dol(patched, self.payload, self.patches)

    def test_changed_progression_vtable_is_rejected(self):
        altered = bytearray(self.original)
        offset = inject.DolImage(self.original).address_to_file_offset(0x80618714, 4)
        inject.write_u32_be(altered, offset, 0x7F1705D8)
        with self.assertRaisesRegex(inject.InjectionError, "opcode mismatch"):
            inject.build_patched_dol(bytes(altered), self.payload, self.patches)

    def test_duplicate_and_invalid_patch_kinds_are_rejected(self):
        for patches in (self.patches + (self.patches[0],),
                        (dataclasses.replace(self.patches[0], kind="unknown"),)):
            with self.assertRaises(inject.InjectionError):
                inject.build_patched_dol(self.original, self.payload, patches)

    def test_overlap_is_rejected(self):
        payload = dataclasses.replace(self.payload, load_address=0x80003100,
                                      end_address=0x80003100 + len(self.payload.image))
        with self.assertRaises(inject.InjectionError):
            inject.build_patched_dol(self.original, payload, self.patches)

    def test_dry_run_writes_nothing(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            with contextlib.redirect_stdout(io.StringIO()):
                inject.run(self.arguments(directory, "--dry-run"))
            self.assertEqual(list(directory.iterdir()), [])

    def test_wrong_hash_writes_nothing(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            with self.assertRaisesRegex(inject.InjectionError, "SHA-256 mismatch"):
                inject.run(self.arguments(directory, "--expected-sha256", "0" * 64))
            self.assertEqual(list(directory.iterdir()), [])

    def test_backup_and_repeated_build_are_exact(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            arguments = self.arguments(directory)
            with contextlib.redirect_stdout(io.StringIO()):
                inject.run(arguments)
                first = arguments.output.read_bytes()
                inject.run(arguments)
            self.assertEqual(arguments.backup.read_bytes(), self.original)
            self.assertEqual(arguments.output.read_bytes(), first)

    def test_nonmatching_backup_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            arguments = self.arguments(directory)
            arguments.backup.write_bytes(b"do not replace")
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(inject.InjectionError, "non-matching backup"):
                    inject.run(arguments)
            self.assertEqual(arguments.backup.read_bytes(), b"do not replace")
            self.assertFalse(arguments.output.exists())

    def test_package_main_matches_fresh_payload(self):
        expected, _ = inject.build_patched_dol(self.original, self.payload, self.patches)
        self.assertEqual((ROOT / "main.dol").read_bytes(), expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
