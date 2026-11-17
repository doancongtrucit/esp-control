"""Petrolimex retail fuel prices for the Info > Prices tab."""
import json
import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fuel_prices as fp  # noqa: E402

PAGE = (Path(__file__).parent / "fixtures" / "webgia_fuel.html").read_text(encoding="utf-8")
NOW = datetime(2026, 11, 11, 20, 5)


class ParseTests(unittest.TestCase):
    def test_reads_every_product_for_both_zones(self):
        rows = fp.parse(PAGE)
        self.assertEqual(6, len(rows))
        self.assertEqual(("Xăng E10 RON 95 mức 5", 22650, 23100), rows[0])
        self.assertEqual(("Dầu hỏa 2-K", 28600, 29170), rows[-1])

    def test_short_names(self):
        self.assertEqual("E10 RON95 M5", fp.short_name("Xăng E10 RON 95 mức 5"))
        self.assertEqual("E5 RON92 M2", fp.short_name("Xăng E5 RON 92 mức 2"))
        self.assertEqual("DO 0,001S M5", fp.short_name("Dầu DO 0,001S mức 5"))
        self.assertEqual("Dầu hỏa", fp.short_name("Dầu hỏa 2-K"))

    def test_page_without_the_table(self):
        self.assertEqual([], fp.parse("<html><body>maintenance</body></html>"))


class FeedTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.state = Path(self.tmp.name) / "fuel_state.json"

    def tearDown(self):
        self.tmp.cleanup()

    def feed(self, page=PAGE, zone=1):
        return fp.FuelFeed({"fuel": {"zone": zone}}, state_path=self.state, http=lambda url: page,
                           now=lambda: NOW)

    def test_frame_for_zone_one(self):
        f = self.feed()
        f.refresh()
        _, frame = f.snapshot()
        self.assertEqual("fuel", frame["type"])
        self.assertEqual(1, frame["z"])
        self.assertEqual(["E10 RON95 M5", 22650, 0], frame["items"][0])
        self.assertEqual("20:05", frame["t"])

    def test_a_new_price_set_is_reported_once(self):
        changes = []
        f = self.feed()
        f.on_change = lambda prev, cur: changes.append((prev, cur))
        f.refresh()  # first prices ever: nothing to compare with
        f.http = lambda url: PAGE.replace("22.650", "23.070")
        f.refresh()
        f.refresh()  # same prices again
        self.assertEqual(1, len(changes))
        prev, cur = changes[0]
        self.assertEqual(22650, prev["Xăng E10 RON 95 mức 5"])
        self.assertEqual(23070, cur["Xăng E10 RON 95 mức 5"])

    def test_zone_two(self):
        f = self.feed(zone=2)
        f.refresh()
        self.assertEqual(23100, f.snapshot()[1]["items"][0][1])

    def test_change_since_the_previous_price_survives_restarts(self):
        f = self.feed()
        f.refresh()
        cheaper = PAGE.replace("22.650", "22.150")
        f = self.feed(cheaper)  # a new bridge run, after the price changed
        f.refresh()
        self.assertEqual(["E10 RON95 M5", 22150, -500], f.snapshot()[1]["items"][0])
        f = self.feed(cheaper)  # same prices again: the change stays until the next adjustment
        f.refresh()
        self.assertEqual(-500, f.snapshot()[1]["items"][0][2])
        self.assertIn("prev", json.loads(self.state.read_text(encoding="utf-8")))

    def test_failed_download_keeps_the_last_frame(self):
        f = self.feed()
        f.refresh()
        v1 = f.snapshot()[0]
        f.http = lambda url: (_ for _ in ()).throw(OSError("down"))
        f.refresh()
        self.assertEqual(v1, f.snapshot()[0])

    def test_empty_page_sends_nothing(self):
        f = self.feed("<html></html>")
        f.refresh()
        self.assertIsNone(f.snapshot()[1])


if __name__ == "__main__":
    unittest.main()
