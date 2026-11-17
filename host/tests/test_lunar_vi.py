"""Vietnamese lunar calendar, same results as the firmware's src/lunar.cpp."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import lunar_vi  # noqa: E402


class LunarTests(unittest.TestCase):
    def check(self, d, m, y, ld, lm, ly, leap):
        self.assertEqual((ld, lm, ly, leap), lunar_vi.solar_to_lunar(d, m, y), f"{d}/{m}/{y}")

    def test_same_dates_as_the_firmware_tests(self):
        self.check(17, 2, 2026, 1, 1, 2026, False)  # Tet
        self.check(5, 11, 2026, 27, 9, 2026, False)
        self.check(9, 11, 2026, 1, 10, 2026, False)
        self.check(23, 11, 2026, 15, 10, 2026, False)
        self.check(25, 7, 2025, 1, 6, 2025, True)  # leap sixth month
        self.check(25, 6, 2025, 1, 6, 2025, False)
        self.check(16, 2, 2026, 29, 12, 2025, False)  # before Tet: previous lunar year


if __name__ == "__main__":
    unittest.main()
