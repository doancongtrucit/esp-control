"""Checks the VLW writer against the layout TFT_eSPI's loadFont()/loadMetrics() read.

Run with Pillow installed:  python -m pytest tools/tests
"""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import make_smooth_fonts as msf  # noqa: E402

NOTO = Path("C:/Windows/Fonts/NotoSans-Regular.ttf")


def parse(vlw: bytes):
    count, version, size, _, ascent, descent = struct.unpack(">6i", vlw[:24])
    glyphs = [struct.unpack(">7i", vlw[24 + i * 28: 52 + i * 28]) for i in range(count)]
    return count, version, size, ascent, descent, glyphs


@unittest.skipUnless(NOTO.exists(), "Noto Sans not installed")
class VlwTests(unittest.TestCase):
    def test_header_and_records_follow_tft_espi_layout(self):
        vlw = msf.build_vlw(NOTO, 15, "Hp ")
        count, version, size, ascent, descent, glyphs = parse(vlw)
        self.assertEqual(3, count)
        self.assertEqual(11, version)
        self.assertEqual(15, size)
        self.assertGreater(ascent, 8)    # top of "d"
        self.assertGreater(descent, 1)   # bottom of "p"
        bitmap_bytes = sum(g[1] * g[2] for g in glyphs)
        self.assertEqual(24 + count * 28 + bitmap_bytes, len(vlw))

    def test_glyphs_are_sorted_with_metrics(self):
        _, _, _, ascent, _, glyphs = parse(msf.build_vlw(NOTO, 15, "pH "))
        self.assertEqual([0x20, ord("H"), ord("p")], [g[0] for g in glyphs])
        space, h, p = glyphs
        self.assertEqual((0, 0), (space[1], space[2]))  # no bitmap
        self.assertGreater(space[3], 0)                  # but it advances
        self.assertEqual(h[4], h[1])                     # "H" sits on the baseline
        self.assertLess(p[4], p[1])                      # "p" hangs below it

    def test_vietnamese_marks_rise_above_the_latin_ascent(self):
        _, _, _, ascent, _, glyphs = parse(msf.build_vlw(NOTO, 15, "Ễ"))
        self.assertGreater(glyphs[0][4], ascent)

    def test_vietnamese_set_covers_what_the_deck_lets_through(self):
        chars = msf.charset("vi")
        for ch in "Nguyễn Văn Đức gửi: Chiều nay họp lúc 3 giờ ơ ư ñ ü ç °":
            self.assertIn(ch, chars)

    def test_header_file_is_a_progmem_array(self):
        text = msf.c_header("NOTO_TEST", b"\x00\x01\xff")
        self.assertIn("const uint8_t NOTO_TEST[] PROGMEM = {", text)
        self.assertIn("0x00, 0x01, 0xFF", text)


if __name__ == "__main__":
    unittest.main()
