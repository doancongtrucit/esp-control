import json
import sys
import unittest
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import info_feeds  # noqa: E402

FIXTURES = Path(__file__).resolve().parent / "fixtures"


def fixture(name):
    return json.loads((FIXTURES / f"{name}.json").read_text(encoding="utf-8"))


CONFIG = {
    "coins": ["bitcoin", "ethereum", "binancecoin", "solana", "ripple"],
    "stocks": ["VNM", "HPG", "ZZZZ"],
    "weather": {"city": "Da Nang", "latitude": 16.0544, "longitude": 108.2022},
}


class FakeHttp:
    def __init__(self, fail=False):
        self.fail = fail

    def __call__(self, url, timeout=10.0):
        if self.fail:
            raise OSError("offline")
        if "coingecko" in url:
            return fixture("coingecko")
        if "getlistindexdetail" in url:
            return fixture("vps_index")
        if "getliststockdata" in url:
            return fixture("vps_stocks")
        if "open-meteo" in url:
            return fixture("open_meteo")
        raise AssertionError(url)


TRADING = datetime(2026, 11, 6, 10, 30)  # Friday 10:30


class CoinTests(unittest.TestCase):
    def test_coin_frame_shape_and_rounding(self):
        frame = info_feeds.coin_frame(fixture("coingecko"), CONFIG["coins"], "19:42")
        self.assertEqual("coin", frame["type"])
        self.assertEqual(["BTC", "ETH", "BNB", "SOL", "XRP"], [i["s"] for i in frame["items"]])
        btc = frame["items"][0]
        raw = fixture("coingecko")["bitcoin"]
        self.assertEqual("Bitcoin", btc["n"])
        self.assertEqual(round(raw["usd"], 2), btc["p"])
        self.assertEqual(round(raw["usd_24h_change"], 2), btc["c"])
        self.assertEqual("19:42", frame["time"])


class StockTests(unittest.TestCase):
    def test_index_change_comes_from_previous_close(self):
        frame = info_feeds.stock_frame(fixture("vps_index"), fixture("vps_stocks"), ["VNM", "HPG"], [], "15:05")
        self.assertEqual(["VN-INDEX", "VN30", "HNX-INDEX"], [i["n"] for i in frame["index"]])
        self.assertEqual(1740.73, frame["index"][0]["v"])
        self.assertEqual(0.44, frame["index"][0]["c"])

    def test_stock_change_is_signed_from_reference_price(self):
        frame = info_feeds.stock_frame(fixture("vps_index"), fixture("vps_stocks"), ["VNM", "HPG"], [], "15:05")
        hpg = next(i for i in frame["items"] if i["n"] == "HPG")
        self.assertEqual(21.5, hpg["p"])
        self.assertLess(hpg["c"], 0)  # 21.5 vs reference 21.7, although changePc reads 0.92

    def test_unknown_ticker_is_skipped(self):
        frame = info_feeds.stock_frame(fixture("vps_index"), fixture("vps_stocks"), CONFIG["stocks"], [], "15:05")
        self.assertEqual(["VNM", "HPG"], [i["n"] for i in frame["items"]])

    def test_sparkline_is_capped_at_40_points(self):
        feeds = info_feeds.InfoFeeds(CONFIG, http=FakeHttp(), now=lambda: TRADING)
        for _ in range(45):
            feeds.refresh("stock")
        _, frame = feeds.snapshot()["stock"]
        self.assertEqual(40, len(frame["spark"]))


class CountingIndexHttp(FakeHttp):
    """VPS index whose VN-INDEX value grows by 1 per call."""

    def __init__(self):
        super().__init__()
        self.calls = 0

    def __call__(self, url, timeout=10.0):
        if "getlistindexdetail" in url:
            self.calls += 1
            rows = fixture("vps_index")
            rows[0]["cIndex"] = 1700 + self.calls
            return rows
        return super().__call__(url, timeout)


class SparkTests(unittest.TestCase):
    def test_lunch_break_is_not_sampled(self):
        clock = {"now": datetime(2026, 11, 6, 11, 45)}
        feeds = info_feeds.InfoFeeds(CONFIG, http=FakeHttp(), now=lambda: clock["now"])
        feeds.refresh("stock")
        self.assertEqual([], feeds.snapshot()["stock"][1]["spark"])
        clock["now"] = datetime(2026, 11, 6, 13, 10)
        feeds.refresh("stock")
        self.assertEqual(1, len(feeds.snapshot()["stock"][1]["spark"]))

    def test_whole_session_is_downsampled_to_40_points(self):
        http = CountingIndexHttp()
        clock = {"now": datetime(2026, 11, 6, 9, 0)}
        feeds = info_feeds.InfoFeeds(CONFIG, http=http, now=lambda: clock["now"])
        for minute in range(0, 360, 2):  # 9:00 .. 14:58 every 2 minutes
            clock["now"] = datetime(2026, 11, 6, 9 + minute // 60, minute % 60)
            feeds.refresh("stock")
        spark = feeds.snapshot()["stock"][1]["spark"]
        self.assertEqual(40, len(spark))
        self.assertLess(spark[0], 1710)         # starts near the open, not 80 minutes ago
        self.assertEqual(1700 + http.calls, spark[-1])  # ends at the latest value

    def test_frame_carries_reference_level(self):
        frame = info_feeds.stock_frame(fixture("vps_index"), fixture("vps_stocks"), ["VNM"], [], "15:05")
        self.assertEqual(fixture("vps_index")[0]["oIndex"], frame["open"])


class EmptyResponseTests(unittest.TestCase):
    def test_empty_coin_response_keeps_previous_frame(self):
        http = FakeHttp()
        feeds = info_feeds.InfoFeeds(CONFIG, http=http, now=lambda: TRADING)
        feeds.refresh("coin")
        before = feeds.snapshot()["coin"]
        feeds.http = lambda url, timeout=10.0: {}
        feeds.refresh("coin")
        self.assertEqual(before, feeds.snapshot()["coin"])

    def test_empty_index_response_keeps_previous_frame(self):
        feeds = info_feeds.InfoFeeds(CONFIG, http=FakeHttp(), now=lambda: TRADING)
        feeds.refresh("stock")
        before = feeds.snapshot()["stock"]
        feeds.http = lambda url, timeout=10.0: []
        feeds.refresh("stock")
        self.assertEqual(before, feeds.snapshot()["stock"])


class WeatherTests(unittest.TestCase):
    def test_weather_frame_maps_codes_and_weekdays(self):
        frame = info_feeds.weather_frame(fixture("open_meteo"), "Da Nang", "19:30")
        self.assertEqual("weather", frame["type"])
        self.assertEqual("Da Nang", frame["city"])
        self.assertEqual(4, len(frame["days"]))
        self.assertEqual("Sat", frame["days"][0]["d"])  # day after 2026-11-06
        self.assertEqual(3, frame["days"][0]["icon"])    # code 80 = rain showers
        self.assertEqual(4, frame["days"][2]["icon"])    # code 95 = thunderstorm
        self.assertIn(frame["icon"], range(6))
        self.assertTrue(frame["desc"])


class FrameDateTests(unittest.TestCase):
    def test_every_feed_frame_carries_its_date(self):
        feeds = info_feeds.InfoFeeds(CONFIG, http=FakeHttp(), now=lambda: TRADING)
        for kind in feeds.KINDS:
            feeds.refresh(kind)
            self.assertEqual("06/11", feeds.snapshot()[kind][1]["date"], kind)


class FeedErrorTests(unittest.TestCase):
    def test_feed_error_keeps_previous_frame(self):
        http = FakeHttp()
        feeds = info_feeds.InfoFeeds(CONFIG, http=http, now=lambda: TRADING)
        feeds.refresh("coin")
        before = feeds.snapshot()["coin"]
        http.fail = True
        feeds.refresh("coin")
        self.assertEqual(before, feeds.snapshot()["coin"])

    def test_no_frame_before_first_success(self):
        feeds = info_feeds.InfoFeeds(CONFIG, http=FakeHttp(fail=True), now=lambda: TRADING)
        feeds.refresh("weather")
        self.assertNotIn("weather", feeds.snapshot())


if __name__ == "__main__":
    unittest.main()
