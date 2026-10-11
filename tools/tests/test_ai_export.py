from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ai_export_companion as export
import dolphin_ini_companion as companion


class AiExportChecks(unittest.TestCase):
    def state(self, request=1, frame=100, data=b"health\tint\t100\n"):
        return {"request": request, "frame": frame, "length": len(data), "data": data}

    def test_header_signature_version_and_length(self):
        valid = struct.pack(">8I", export.AI_MAGIC, 1, 100, 90, 1, 0, 0, 10)
        self.assertEqual(export.decode_ai_header(valid)["length"], 10)
        for raw in (valid[:-1], b"FAIL" + valid[4:], valid[:4] + struct.pack(">I", 2) + valid[8:],
                    valid[:28] + struct.pack(">I", 0), valid[:28] + struct.pack(">I", 65537)):
            with self.assertRaises(ValueError):
                export.decode_ai_header(raw)

    def test_idle_zero_length_header_is_accepted(self):
        self.assertEqual(export.decode_ai_header(struct.pack(">8I", export.AI_MAGIC, 1, 1, 0, 0, 0, 0, 0))["request"], 0)

    def test_response_writes_restricted_to_three_fields(self):
        memory = companion.DolphinMemory.__new__(companion.DolphinMemory)
        memory.mem1, memory.ai_mailbox = 0x100000000, 0x80700000
        for offset in (0, 4, 8, 16, 28, 32, -4, 65564):
            with self.assertRaises(ValueError):
                memory.write_ai_word(offset, 1)

    def test_snapshot_rejects_changed_sequence_during_read(self):
        memory = companion.DolphinMemory.__new__(companion.DolphinMemory)
        memory.mem1, memory.ai_mailbox = 0x100000000, 0x80700000
        header = struct.pack(">8I", export.AI_MAGIC, 1, 100, 90, 1, 0, 0, 4)
        changed = struct.pack(">8I", export.AI_MAGIC, 1, 101, 90, 2, 0, 0, 4)
        memory._read = mock.Mock(side_effect=[header, b"data", changed])
        with self.assertRaisesRegex(ValueError, "changed"):
            memory.ai_snapshot()

    def test_snapshot_preserves_exact_bytes(self):
        memory = companion.DolphinMemory.__new__(companion.DolphinMemory)
        memory.mem1, memory.ai_mailbox = 0x100000000, 0x80700000
        header = struct.pack(">8I", export.AI_MAGIC, 1, 100, 90, 1, 0, 0, 4)
        memory._read = mock.Mock(side_effect=[header, b"data", header])
        self.assertEqual(memory.ai_snapshot()["data"], b"data")

    def test_unique_files_preserve_full_snapshot(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "exports"
            first = export.save_ai_export(directory, b"alpha\n")
            second = export.save_ai_export(directory, b"beta\n")
            self.assertNotEqual(first, second)
            self.assertEqual(first.read_bytes(), b"alpha\n")
            self.assertEqual(second.read_bytes(), b"beta\n")

    def test_invalid_text_does_not_create_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary) / "exports"
            for data in (b"", b"a\0b", b"x" * 65537):
                with self.assertRaises(ValueError):
                    export.save_ai_export(directory, data)
            self.assertFalse(directory.exists())

    def test_acknowledges_only_after_file_save(self):
        events, memory = [], mock.Mock()
        memory.ai_snapshot.return_value = self.state()
        memory.write_ai_word.side_effect = lambda *args: events.append(args)
        with mock.patch.object(export, "save_ai_export", side_effect=lambda *args: events.append("saved") or Path("block.txt")):
            export.AiExportMonitor(Path("exports")).poll(memory)
        self.assertEqual(events, [(12, 100), "saved", (24, 1), (20, 1)])

    def test_mixed_snapshot_is_not_written_or_acknowledged(self):
        memory = mock.Mock()
        memory.ai_snapshot.side_effect = [self.state(), self.state(data=b"changed\n")]
        with mock.patch.object(export, "save_ai_export") as save:
            export.AiExportMonitor(Path("exports")).poll(memory)
        save.assert_not_called()
        self.assertEqual(memory.write_ai_word.call_args_list, [mock.call(12, 100)])

    def test_write_failure_reports_error_then_ack(self):
        memory = mock.Mock()
        memory.ai_snapshot.return_value = self.state()
        with mock.patch.object(export, "save_ai_export", side_effect=OSError("locked")):
            export.AiExportMonitor(Path("exports")).poll(memory)
        self.assertEqual(memory.write_ai_word.call_args_list, [mock.call(12, 100), mock.call(24, 2), mock.call(20, 1)])

    def test_ack_failure_retries_without_creating_duplicate_file(self):
        memory, monitor = mock.Mock(), export.AiExportMonitor(Path("exports"))
        memory.ai_snapshot.return_value = self.state()
        memory.write_ai_word.side_effect = [None, None, OSError("resetting")]
        with mock.patch.object(export, "save_ai_export", return_value=Path("block.txt")) as save:
            with self.assertRaises(OSError):
                monitor.poll(memory)
            memory.write_ai_word.side_effect = None
            monitor.poll(memory)
            self.assertEqual(save.call_count, 1)
            self.assertEqual(monitor.last_success, 1)

    def test_reset_during_save_cannot_acknowledge_new_session(self):
        for same_bytes in (False, True):
            with self.subTest(same_bytes=same_bytes):
                memory, monitor = mock.Mock(), export.AiExportMonitor(Path("exports"))
                old = self.state(frame=1000)
                new = self.state(frame=5, data=old["data"] if same_bytes else b"new\n")
                memory.ai_snapshot.side_effect = [old, old, new]
                with mock.patch.object(export, "save_ai_export", return_value=Path("old.txt")):
                    monitor.poll(memory)
                self.assertEqual(memory.write_ai_word.call_args_list, [mock.call(12, 1000)])
                self.assertEqual(monitor.last_success, 0)

    def test_reset_during_failed_save_cannot_fail_new_session(self):
        memory = mock.Mock()
        old, new = self.state(frame=1000), self.state(frame=5, data=b"new\n")
        memory.ai_snapshot.side_effect = [old, old, new]
        with mock.patch.object(export, "save_ai_export", side_effect=OSError("locked")):
            export.AiExportMonitor(Path("exports")).poll(memory)
        self.assertEqual(memory.write_ai_word.call_args_list, [mock.call(12, 1000)])

    def test_repeated_request_is_saved_once_and_reset_reuses_sequence(self):
        memory, monitor = mock.Mock(), export.AiExportMonitor(Path("exports"))
        memory.ai_snapshot.return_value = self.state()
        with mock.patch.object(export, "save_ai_export", return_value=Path("block.txt")) as save:
            monitor.poll(memory)
            monitor.poll(memory)
            self.assertEqual(save.call_count, 1)
            memory.ai_snapshot.return_value = self.state(request=0, frame=1, data=b"")
            monitor.poll(memory)
            memory.ai_snapshot.return_value = self.state(request=1, frame=2, data=b"new\n")
            monitor.poll(memory)
            self.assertEqual(save.call_count, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
