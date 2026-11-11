"""Home screen: Home Assistant device tiles and the commands behind them."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ha_controls as hc  # noqa: E402


def st(entity_id, state, **attrs):
    return {"entity_id": entity_id, "state": state, "attributes": attrs}


STATES = [
    st("climate.dieu_hoa", "off", friendly_name="Air Conditioner", hvac_modes=["off", "heat_cool", "cool", "dry"],
       min_temp=16.0, max_temp=30.0, target_temp_step=1.0, temperature=24, fan_modes=["auto", "low", "mid", "high"],
       fan_mode="auto", last_on_operation="cool"),
    st("fan.quat_dung", "on", friendly_name="Standing Fan"),
    st("media_player.living_tv", "on", friendly_name="Living TV"),
    st("switch.desk_pc", "on", friendly_name="PC"),
    st("script.quat_dung_xoay", "off", friendly_name="Quat dung - Xoay"),
    st("input_boolean.quat_dung_on", "on", friendly_name="quat_dung_on"),
    st("automation.roi_nha", "on", friendly_name="Roi nha"),
    st("sensor.temp", "25"),
]


class Calls:
    def __init__(self, states=STATES):
        self.states = states
        self.posts = []

    def get(self, url, token, timeout):
        return self.states

    def post(self, url, token, body, timeout):
        self.posts.append((url.split("/api/services/", 1)[1], body))
        return []


def make(items=None, calls=None):
    calls = calls or Calls()
    cfg = {"home_server": {"url": "http://ha:8123", "token": "t"}}
    if items is not None:
        cfg["home_controls"] = items
    return hc.HaControls(cfg, http_get=calls.get, http_post=calls.post), calls


class TileTests(unittest.TestCase):
    def test_default_tiles_are_the_controllable_devices(self):
        ctl, _ = make()
        ctl.refresh()
        frame = ctl.snapshot()[1]
        names = [t["n"] for t in frame["items"]]
        self.assertEqual(["Air Conditioner", "Standing Fan", "Living TV", "PC", "Quat dung - Xoay"], names)
        kinds = [t["k"] for t in frame["items"]]
        self.assertEqual(["climate", "toggle", "toggle", "toggle", "run"], kinds)

    def test_tile_state_and_value(self):
        ctl, _ = make()
        ctl.refresh()
        items = ctl.snapshot()[1]["items"]
        self.assertEqual((0, "24C off"), (items[0]["on"], items[0]["v"]))
        self.assertEqual((1, "ON"), (items[1]["on"], items[1]["v"]))
        self.assertEqual((-1, "Run"), (items[4]["on"], items[4]["v"]))

    def test_configured_tiles_with_labels_and_icons(self):
        ctl, _ = make([{"entity": "fan.quat_dung", "label": "Quat", "icon": "fan"},
                       {"entity": "script.quat_dung_xoay", "label": "Xoay"},
                       {"entity": "light.missing", "label": "Den"}])
        ctl.refresh()
        items = ctl.snapshot()[1]["items"]
        self.assertEqual(["Quat", "Xoay", "Den"], [t["n"] for t in items])
        self.assertEqual("fan", items[0]["i"])
        self.assertEqual("bolt", items[1]["i"])
        self.assertEqual("N/A", items[2]["v"])  # entity not in Home Assistant

    def test_switches_and_configured_tiles_ask_for_a_hold(self):
        ctl, _ = make()
        ctl.refresh()
        items = ctl.snapshot()[1]["items"]
        self.assertEqual([0, 0, 0, 1, 0], [t["c"] for t in items])  # the PC switch
        ctl, _ = make([{"entity": "fan.quat_dung", "confirm": True}, {"entity": "switch.desk_pc", "confirm": False}])
        ctl.refresh()
        self.assertEqual([1, 0], [t["c"] for t in ctl.snapshot()[1]["items"]])

    def test_air_conditioner_details(self):
        ctl, _ = make()
        ctl.refresh()
        ac = ctl.snapshot()[1]["ac"]
        self.assertEqual({"ix": 0, "on": False, "t": 24, "min": 16, "max": 30, "mode": "cool",
                          "modes": ["heat_cool", "cool", "dry"], "fan": "auto",
                          "fans": ["auto", "low", "mid", "high"]}, ac)

    def test_at_most_twelve_tiles(self):
        many = [st(f"switch.s{i}", "off") for i in range(20)]
        ctl, _ = make(calls=Calls(many))
        ctl.refresh()
        self.assertEqual(12, len(ctl.snapshot()[1]["items"]))


class CommandTests(unittest.TestCase):
    def setUp(self):
        self.ctl, self.calls = make()
        self.ctl.refresh()

    def test_press_toggles_or_runs(self):
        self.ctl.press(1)
        self.ctl.press(4)
        self.ctl.press(3)
        self.assertEqual([("fan/toggle", {"entity_id": "fan.quat_dung"}),
                          ("script/turn_on", {"entity_id": "script.quat_dung_xoay"}),
                          ("switch/toggle", {"entity_id": "switch.desk_pc"})], self.calls.posts)

    def test_press_on_the_air_conditioner_turns_it_on_with_the_last_mode(self):
        self.ctl.press(0)
        self.assertEqual([("climate/set_hvac_mode", {"entity_id": "climate.dieu_hoa", "hvac_mode": "cool"})],
                         self.calls.posts)

    def test_air_conditioner_controls(self):
        self.ctl.ac({"op": "temp", "d": 1})
        self.ctl.ac({"op": "temp", "d": -20})  # clamped to min
        self.ctl.ac({"op": "mode", "m": "dry"})
        self.ctl.ac({"op": "fan", "m": "high"})
        self.ctl.ac({"op": "power"})
        self.assertEqual([
            ("climate/set_temperature", {"entity_id": "climate.dieu_hoa", "temperature": 25}),
            ("climate/set_temperature", {"entity_id": "climate.dieu_hoa", "temperature": 16}),
            ("climate/set_hvac_mode", {"entity_id": "climate.dieu_hoa", "hvac_mode": "dry"}),
            ("climate/set_fan_mode", {"entity_id": "climate.dieu_hoa", "fan_mode": "high"}),
            ("climate/set_hvac_mode", {"entity_id": "climate.dieu_hoa", "hvac_mode": "off"}),  # dry turned it on
        ], self.calls.posts)

    def test_unknown_modes_and_indexes_are_ignored(self):
        self.ctl.ac({"op": "mode", "m": "heat"})
        self.ctl.ac({"op": "fan", "m": "turbo"})
        self.ctl.press(99)
        self.ctl.press(-1)
        self.assertEqual([], self.calls.posts)

    def test_press_updates_the_tile_at_once(self):
        v1 = self.ctl.snapshot()[0]
        self.ctl.press(1)  # fan on -> off, before Home Assistant confirms
        version, frame = self.ctl.snapshot()
        self.assertNotEqual(v1, version)
        self.assertEqual(0, frame["items"][1]["on"])

    def test_device_messages(self):
        self.assertTrue(self.ctl.handle({"type": "ha_press", "i": 2}))
        self.assertTrue(self.ctl.handle({"type": "ha_ac", "op": "temp", "d": 1}))
        self.assertFalse(self.ctl.handle({"type": "macro", "id": 1}))
        self.assertEqual(2, len(self.calls.posts))


class StartTests(unittest.TestCase):
    def test_needs_home_assistant(self):
        self.assertIsNone(hc.start_ha_controls({}))
        self.assertIsNone(hc.start_ha_controls({"home_server": {"url": "http://x", "token": ""}}))


if __name__ == "__main__":
    unittest.main()
