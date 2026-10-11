import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import debug_disc
import dolphin_ini_companion as companion


class DiscChecks(unittest.TestCase):
    def setUp(self):
        debug_disc.WORKSPACE_BUILD.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=debug_disc.WORKSPACE_BUILD)
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.source, self.dol, self.ini, self.output = [self.folder / name for name in
                                                      ("source.iso", "main.dol", "game.ini", "debug.iso")]
        source = bytearray(0x1000)
        source[:6] = b"GUTE52"
        struct.pack_into(">I", source, 0x1C, debug_disc.GC_MAGIC)
        struct.pack_into(">III", source, 0x420, 0x800, 0x500, 33)
        struct.pack_into(">6I", source, 0x500, 0x01000000, 0, 2, 0, 0x600, 42)
        source[0x518:0x521] = b"game.ini\0"
        self.source.write_bytes(source)
        dol = bytearray(0x104)
        struct.pack_into(">I", dol, 0, 0x100)
        struct.pack_into(">I", dol, 0x48, 0x80003100)
        struct.pack_into(">I", dol, 0x90, 4)
        struct.pack_into(">I", dol, 0xE0, 0x80003100)
        dol[0x100:] = b"\x4e\x80\x00\x20"
        self.dol.write_bytes(dol)
        self.ini.write_bytes(b"[Strings]\r\nSCENE_NAME=city_arena\r\nHERO_NAME=ultimate_spiderman\r\n\0")

    def build(self):
        return debug_disc.make_debug_disc(self.source, self.dol, self.ini, self.output)

    def test_build_relocates_boot_dol_and_ini_without_changing_inputs(self):
        before = [p.read_bytes() for p in (self.source, self.dol, self.ini)]
        manifest = self.build()
        self.assertEqual([p.read_bytes() for p in (self.source, self.dol, self.ini)], before)
        result = self.output.read_bytes()
        self.assertEqual(struct.unpack_from(">I", result, 0x420)[0], manifest["dol_offset"])
        self.assertEqual(result[manifest["dol_offset"]:manifest["dol_offset"] + len(before[1])], before[1])
        with self.output.open("rb") as stream:
            entry, offset, capacity = debug_disc.ini_entry(stream)
        self.assertEqual((entry, offset, capacity), (0x50C, manifest["ini_offset"], 8192))
        self.assertTrue(result[offset:offset + capacity].startswith(before[2]))

    def test_scene_save_changes_only_reserved_iso_extent_and_backs_up_ini(self):
        manifest = self.build()
        before = self.output.read_bytes()
        original_ini = self.ini.read_bytes()
        companion.save_request(self.ini, self.output, "characterb_arena")
        after = self.output.read_bytes()
        start, end = manifest["ini_offset"], manifest["ini_offset"] + manifest["ini_capacity"]
        self.assertEqual(before[:start], after[:start])
        self.assertEqual(before[end:], after[end:])
        self.assertIn(b"SCENE_NAME=characterb_arena\r\n", after[start:end])
        self.assertIn(b"HERO_NAME=ultimate_spiderman", self.ini.read_bytes())
        self.assertEqual(self.ini.with_name("game.ini.usm-debug-original").read_bytes(), original_ini)

    def test_changed_fst_cannot_be_written(self):
        self.build()
        with self.output.open("r+b") as stream:
            stream.seek(0x510)
            stream.write(struct.pack(">I", 0x600))
        before = self.output.read_bytes()
        with self.assertRaisesRegex(ValueError, "extent changed"):
            debug_disc.write_disc_ini(self.output, b"bad")
        self.assertEqual(before, self.output.read_bytes())

    def test_capacity_error_does_not_write_disc(self):
        self.build()
        before = self.output.read_bytes()
        with self.assertRaisesRegex(ValueError, "reserved disc extent"):
            debug_disc.write_disc_ini(self.output, b"X" * 8193)
        self.assertEqual(before, self.output.read_bytes())

    def test_output_alias_and_existing_copy_are_rejected(self):
        with self.assertRaises(ValueError):
            debug_disc.make_debug_disc(self.source, self.dol, self.ini, self.source, True)
        self.build()
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.build()

    def test_missing_root_ini_is_rejected_before_copy(self):
        data = bytearray(self.source.read_bytes())
        data[0x518:0x521] = b"fake.ini\0"
        self.source.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "root game.ini"):
            self.build()
        self.assertFalse(self.output.exists())

    def test_manifest_input_alias_is_rejected_without_changes(self):
        sidecar = self.output.with_suffix(".iso.usm.json")
        original = self.ini.read_bytes()
        sidecar.write_bytes(original)
        with self.assertRaisesRegex(ValueError, "manifest must not alias"):
            debug_disc.make_debug_disc(self.source, self.dol, sidecar, self.output, True)
        self.assertEqual(sidecar.read_bytes(), original)
        self.assertFalse(self.output.exists())

    def test_existing_sidecar_requires_force(self):
        sidecar = self.output.with_suffix(".iso.usm.json")
        sidecar.write_bytes(b"keep")
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.build()
        self.assertEqual(sidecar.read_bytes(), b"keep")

    def test_repacked_directory_ordinal_and_subtree_paths(self):
        data = bytearray(self.source.read_bytes())
        names = b"game.ini\0packs\0gc\0asset\0"
        struct.pack_into(">I", data, 0x428, 5 * 12 + len(names))
        struct.pack_into(">15I", data, 0x500,
                         0x01000000, 0, 5, 0, 0x600, 42,
                         0x01000009, 0, 5, 0x0100000F, 1, 5,
                         0x00000012, 0x700, 4)
        data[0x53C:0x53C + len(names)] = names
        self.source.write_bytes(data)
        self.build()
        with self.output.open("rb") as stream:
            self.assertEqual(debug_disc.ini_entry(stream)[0], 0x50C)


class MailboxChecks(unittest.TestCase):
    def raw(self, scene=b"city_arena", request=1):
        return struct.pack(">7I", companion.MAGIC, 1, 100, 0, request, 0, 0) + scene.ljust(32, b"\0")

    def test_decode_and_validate(self):
        self.assertEqual(companion.decode_mailbox(self.raw())["scene"], "city_arena")
        for raw in (self.raw()[:-1], b"FAIL" + self.raw()[4:], self.raw(b"bad_arena")):
            with self.assertRaises(ValueError):
                companion.decode_mailbox(raw)

    def test_response_writes_are_restricted(self):
        memory = companion.DolphinMemory.__new__(companion.DolphinMemory)
        memory.mem1 = 0x100000000
        for offset in (0, 4, 8, 16, 28, -4, 60):
            with self.assertRaises(ValueError):
                memory.write_word(offset, 1)

    def test_save_is_acknowledged_only_after_both_files_flush(self):
        events = []
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, False]
        memory.snapshot.return_value = companion.decode_mailbox(self.raw())
        memory.write_word.side_effect = lambda *args: events.append(args)
        with mock.patch.object(companion, "save_request", side_effect=lambda *args: events.append("saved")), \
                mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        self.assertEqual(events, [(12, 100), "saved", (24, 1), (20, 1)])

    def test_mixed_request_is_not_saved_or_acknowledged(self):
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, False]
        memory.snapshot.side_effect = [companion.decode_mailbox(self.raw()),
                                       companion.decode_mailbox(self.raw(b"characterb_arena", 2))]
        with mock.patch.object(companion, "save_request") as save, mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        save.assert_not_called()
        self.assertEqual(memory.write_word.call_args_list, [mock.call(12, 100)])

    def test_failed_save_reports_error(self):
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, False]
        memory.snapshot.return_value = companion.decode_mailbox(self.raw())
        with mock.patch.object(companion, "save_request", side_effect=OSError("locked")), \
                mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        self.assertEqual(memory.write_word.call_args_list, [mock.call(12, 100), mock.call(24, 2), mock.call(20, 1)])

    def test_reset_allows_reused_request_sequence(self):
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, True, True, False]
        first = companion.decode_mailbox(self.raw())
        reset = companion.decode_mailbox(self.raw(b"", 0))
        reset["frame"] = 1
        second = companion.decode_mailbox(self.raw(b"characterb_arena"))
        memory.snapshot.side_effect = [first, first, first, reset, second, second, second]
        with mock.patch.object(companion, "save_request") as save, mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        self.assertEqual([call.args[2] for call in save.call_args_list], ["city_arena", "characterb_arena"])

    def test_transient_reset_mailbox_is_rediscovered(self):
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, True, False]
        state = companion.decode_mailbox(self.raw())
        memory.snapshot.side_effect = [ValueError("resetting"), state, state, state]
        with mock.patch.object(companion, "save_request") as save, mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        memory.discover.assert_called_once()
        save.assert_called_once()

    def test_reset_during_flush_cannot_acknowledge_new_scene(self):
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, False]
        old = companion.decode_mailbox(self.raw())
        new = companion.decode_mailbox(self.raw(b"characterb_arena"))
        new["frame"] = 1
        memory.snapshot.side_effect = [old, old, new]
        with mock.patch.object(companion, "save_request"), mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        self.assertEqual(memory.write_word.call_args_list, [mock.call(12, 100)])

    def test_reset_during_failed_flush_cannot_fail_new_scene(self):
        memory = mock.Mock(mem1=1)
        memory.alive.side_effect = [True, False]
        old = companion.decode_mailbox(self.raw())
        new = companion.decode_mailbox(self.raw(b"characterb_arena"))
        new["frame"] = 1
        memory.snapshot.side_effect = [old, old, new]
        with mock.patch.object(companion, "save_request", side_effect=OSError("locked")), mock.patch.object(companion.time, "sleep"):
            companion.monitor(memory, Path("game.ini"), Path("debug.iso"))
        self.assertEqual(memory.write_word.call_args_list, [mock.call(12, 100)])


if __name__ == "__main__":
    unittest.main(verbosity=2)
