import sys
from pathlib import Path
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from game_ini import update_scene, write_scene


class GameIniChecks(unittest.TestCase):
    def test_exact_active_assignment_and_comments(self):
        original = (b";SCENE_NAME=example\r\n[Strings]\r\nHERO_NAME=ultimate_spiderman\r\n"
                    b"SCENE_NAME=city_arena\r\n[Flags]\r\nSHOW_FPS=1\r\n")
        self.assertEqual(update_scene(original, "characterb"),
                         original.replace(b"SCENE_NAME=city_arena", b"SCENE_NAME=characterb_arena"))

    def test_same_scene_is_byte_exact(self):
        raw = b"[Strings]\r\nSCENE_NAME=city_arena\r\n\0\0\0\0\0\0\0"
        self.assertEqual(update_scene(raw, "city"), raw)

    def test_padding_keeps_disc_file_extent(self):
        raw = b"[Strings]\nSCENE_NAME=city_arena\n" + b"\0" * 7
        patched = update_scene(raw, "characterb")
        self.assertEqual(len(patched), len(raw))
        self.assertEqual(patched, b"[Strings]\nSCENE_NAME=characterb_arena\n\0")

    def test_case_spacing_and_inline_comment(self):
        self.assertEqual(update_scene(b"[STRINGS]\n scene_name = city_arena ; arena\n", "characterb"),
                         b"[STRINGS]\n scene_name = characterb_arena ; arena\n")

    def test_other_section_and_comment_are_untouched(self):
        raw = b"[Other]\nSCENE_NAME=other\n[Strings]\n;SCENE_NAME=test\nSCENE_NAME=city_arena\n"
        self.assertEqual(update_scene(raw, "characterb"), raw.replace(b"=city_arena", b"=characterb_arena"))

    def test_missing_key_is_inserted_inside_strings(self):
        self.assertEqual(update_scene(b"[Strings]\nHERO_NAME=venom\n[Flags]\nX=1\n", "city"),
                         b"[Strings]\nHERO_NAME=venom\nSCENE_NAME=city_arena\n[Flags]\nX=1\n")

    def test_missing_section_is_created(self):
        self.assertEqual(update_scene(b"[Flags]\nX=1", "city"),
                         b"[Flags]\nX=1\n[Strings]\nSCENE_NAME=city_arena\n")

    def test_duplicate_active_keys_are_coalesced(self):
        self.assertEqual(update_scene(b"[Strings]\nSCENE_NAME=a\nSCENE_NAME=b\n", "city"),
                         b"[Strings]\nSCENE_NAME=city_arena\n")

    def test_utf8_bom_is_preserved(self):
        self.assertEqual(update_scene(b"\xef\xbb\xbf[Strings]\nSCENE_NAME=x\n", "city"),
                         b"\xef\xbb\xbf[Strings]\nSCENE_NAME=city_arena\n")

    def test_unknown_scene_does_not_touch_file(self):
        with self.assertRaises(ValueError):
            update_scene(b"[Strings]\nSCENE_NAME=city_arena\n", "../../bad")

    def test_embedded_binary_tail_is_rejected(self):
        with self.assertRaises(ValueError):
            update_scene(b"[Strings]\n\0hidden", "city")

    def test_first_backup_survives_subsequent_selections(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "game.ini"
            raw = b"[Strings]\r\nSCENE_NAME=city_arena\r\n"
            path.write_bytes(raw)
            write_scene(path, "characterb")
            write_scene(path, "city")
            self.assertEqual(path.read_bytes(), raw)
            self.assertEqual(path.with_name("game.ini.usm-debug-original").read_bytes(), raw)


if __name__ == "__main__":
    unittest.main(verbosity=2)
