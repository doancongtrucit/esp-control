"""Home server metrics from Home Assistant's REST API (System Monitor sensors)."""
import sys
import unittest
import urllib.error
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import home_server as hs  # noqa: E402

NOW = datetime(2026, 11, 10, 12, 0, tzinfo=timezone.utc)


def state(entity_id, value, unit=None, **attrs):
    a = dict(attrs)
    if unit:
        a["unit_of_measurement"] = unit
    return {"entity_id": entity_id, "state": str(value), "attributes": a}


STATES = [
    state("sensor.system_monitor_processor_use", 12.5, "%"),
    state("sensor.system_monitor_memory_usage", 43.2, "%"),
    state("sensor.system_monitor_disk_usage", 61.0, "%"),
    state("sensor.system_monitor_disk_usage_media", 90.0, "%"),
    state("sensor.system_monitor_processor_temperature", 48.0, "°C"),
    state("sensor.system_monitor_load_1m", 0.42),
    state("sensor.system_monitor_network_throughput_in_docker0", 9.0, "MB/s"),
    state("sensor.system_monitor_network_throughput_in_wlp2s0", 0.5, "MB/s"),
    state("sensor.system_monitor_network_throughput_out_wlp2s0", 0.125, "MB/s"),
    state("sensor.system_monitor_last_boot", (NOW - timedelta(days=3, hours=4)).isoformat()),
    state("update.home_assistant_core_update", "on"),
    state("update.hacs_update", "on"),
    state("update.esphome_update", "off"),
    state("light.desk", "on"),
]


class PickTests(unittest.TestCase):
    def test_reads_system_monitor_sensors(self):
        m = hs.metrics(STATES, now=NOW)
        self.assertEqual(12.5, m["cpu"])
        self.assertEqual(43.2, m["ram"])
        self.assertEqual(61.0, m["disk"])  # the root disk, not /media
        self.assertEqual(48.0, m["temp"])
        self.assertEqual(0.42, m["load"])
        self.assertEqual((3 * 24 + 4) * 3600, m["upt"])
        self.assertEqual(2, m["upd"])

    def test_network_uses_the_real_interface_in_megabits(self):
        m = hs.metrics(STATES, now=NOW)
        self.assertAlmostEqual(4.0, m["dn"])  # 0.5 MB/s, docker0 ignored
        self.assertAlmostEqual(1.0, m["up"])

    def test_missing_or_unavailable_sensors_are_null(self):
        m = hs.metrics([state("sensor.system_monitor_processor_use", "unavailable")], now=NOW)
        self.assertIsNone(m["cpu"])
        self.assertIsNone(m["ram"])
        self.assertIsNone(m["upt"])
        self.assertEqual(0, m["upd"])

    def test_config_can_name_entities(self):
        states = STATES + [state("sensor.laptop_battery", 87, "%")]
        m = hs.metrics(states, now=NOW, entities={"cpu": "sensor.system_monitor_memory_usage",
                                                    "battery": "sensor.laptop_battery"})
        self.assertEqual(43.2, m["cpu"])
        self.assertEqual(87.0, m["bat"])

    def test_battery_sensor_is_found_by_itself(self):
        m = hs.metrics(STATES + [state("sensor.system_monitor_battery", 64, "%")], now=NOW)
        self.assertEqual(64.0, m["bat"])
        m = hs.metrics(STATES + [state("sensor.system_monitor_battery", "unavailable", "%")], now=NOW)
        self.assertIsNone(m["bat"])

    def test_older_entity_ids_without_the_prefix(self):
        m = hs.metrics([state("sensor.processor_use", 7, "%"), state("sensor.memory_use_percent", 30, "%")], now=NOW)
        self.assertEqual(7.0, m["cpu"])
        self.assertEqual(30.0, m["ram"])


class FakeHttp:
    def __init__(self, routes=None, error=None):
        self.routes = routes or {}
        self.error = error
        self.calls = []

    def __call__(self, url, token, timeout):
        self.calls.append((url, token))
        if self.error:
            raise self.error
        return self.routes[url.split("/api", 1)[1]]


class FeedTests(unittest.TestCase):
    CFG = {"home_server": {"url": "http://ha.local:8123/", "token": "secret-token", "name": "Laptop"}}

    def test_frame_when_online(self):
        http = FakeHttp({"/states": STATES, "/config": {"version": "2026.11.1", "location_name": "Home"}})
        feed = hs.HomeServerFeed(self.CFG, http=http, clock=lambda: NOW, mono=iter([0.0, 0.023]).__next__)
        feed.refresh()
        _, frame = feed.snapshot()
        self.assertEqual("hserver", frame["type"])
        self.assertTrue(frame["on"])
        self.assertEqual("Laptop", frame["name"])
        self.assertEqual("2026.11.1", frame["ver"])
        self.assertEqual(23, frame["ms"])
        self.assertEqual(12.5, frame["cpu"])
        self.assertEqual(("http://ha.local:8123/api/states", "secret-token"), http.calls[0])

    def test_offline_and_auth_errors(self):
        feed = hs.HomeServerFeed(self.CFG, http=FakeHttp(error=urllib.error.URLError("refused")), clock=lambda: NOW)
        feed.refresh()
        self.assertEqual({"type": "hserver", "name": "Laptop", "on": False, "err": "offline", "t": "12:00"},
                         {k: v for k, v in feed.snapshot()[1].items()})
        bad = urllib.error.HTTPError("u", 401, "Unauthorized", None, None)
        feed = hs.HomeServerFeed(self.CFG, http=FakeHttp(error=bad), clock=lambda: NOW)
        feed.refresh()
        self.assertEqual("token", feed.snapshot()[1]["err"])

    def test_the_token_never_reaches_the_frame_or_the_log(self):
        logs = []
        feed = hs.HomeServerFeed(self.CFG, http=FakeHttp(error=urllib.error.URLError("x")), clock=lambda: NOW,
                                 log=logs.append)
        feed.refresh()
        self.assertNotIn("secret-token", repr(feed.snapshot()) + repr(logs))

    def test_version_moves_only_on_change(self):
        http = FakeHttp({"/states": STATES, "/config": {"version": "1"}})
        ticks = iter([0.0, 0.02, 1.0, 1.02, 2.0, 2.09])
        feed = hs.HomeServerFeed(self.CFG, http=http, clock=lambda: NOW, mono=ticks.__next__)
        feed.refresh()
        v1 = feed.snapshot()[0]
        feed.refresh()  # same numbers, same latency bucket
        self.assertEqual(v1, feed.snapshot()[0])
        feed.refresh()  # latency 90 ms: another bucket
        self.assertNotEqual(v1, feed.snapshot()[0])

    def test_network_rate_from_byte_counters_when_throughput_is_unknown(self):
        def states(rx_mib, tx_mib):
            return [state("sensor.system_monitor_network_throughput_in_wlp2s0", "unknown", "MB/s"),
                    state("sensor.system_monitor_network_in_wlp2s0", rx_mib, "MiB"),
                    state("sensor.system_monitor_network_out_wlp2s0", tx_mib, "MiB"),
                    state("sensor.system_monitor_network_in_docker0", 999999, "MiB")]
        rounds = iter([states(100.0, 50.0), states(115.0, 51.5)])
        http = FakeHttp()
        http.__call__ = None

        def fake(url, token, timeout):
            return next(rounds) if url.endswith("/states") else {"version": "1"}

        ticks = iter([0.0, 0.0, 30.0, 30.0])
        feed = hs.HomeServerFeed(self.CFG, http=fake, clock=lambda: NOW, mono=ticks.__next__)
        feed.refresh()
        self.assertIsNone(feed.snapshot()[1]["dn"])  # first reading: no rate yet
        feed.refresh()
        frame = feed.snapshot()[1]
        self.assertAlmostEqual(15 * 1.048576 * 8 / 30, frame["dn"], places=2)  # 15 MiB in 30 s
        self.assertAlmostEqual(1.5 * 1.048576 * 8 / 30, frame["up"], places=2)

    def test_off_without_url_or_token(self):
        self.assertIsNone(hs.start_home_server({"home_server": {"url": "", "token": "x"}}))
        self.assertIsNone(hs.start_home_server({"home_server": {"url": "http://h:8123", "token": ""}}))
        self.assertIsNone(hs.start_home_server({}))


if __name__ == "__main__":
    unittest.main()
