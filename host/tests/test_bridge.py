import importlib.util
import json
import sys
import unittest
from dataclasses import dataclass
from unittest.mock import patch
from typing import Optional

import types

if "serial" not in sys.modules:
    serial_stub = types.ModuleType("serial")
    serial_stub.SerialException = OSError
    serial_stub.Serial = object
    tools_stub = types.ModuleType("serial.tools")
    list_ports_stub = types.ModuleType("serial.tools.list_ports")
    list_ports_stub.comports = lambda: []
    tools_stub.list_ports = list_ports_stub
    serial_stub.tools = tools_stub
    sys.modules["serial"] = serial_stub
    sys.modules["serial.tools"] = tools_stub
    sys.modules["serial.tools.list_ports"] = list_ports_stub

from pathlib import Path

BRIDGE_PATH = Path(__file__).resolve().parents[1] / "control_deck_bridge.py"
SPEC = importlib.util.spec_from_file_location("control_deck_bridge", BRIDGE_PATH)
bridge = importlib.util.module_from_spec(SPEC)
sys.modules["control_deck_bridge"] = bridge
assert SPEC.loader is not None
SPEC.loader.exec_module(bridge)


class TemperatureParsingTests(unittest.TestCase):
    def setUp(self):
        self.sample = {
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "AMD Ryzen 9 5900X",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                {
                                    "Text": "CPU Package",
                                    "Value": "57.0 C",
                                    "SensorId": "/amdcpu/0/temperature/0",
                                    "Children": [],
                                },
                                {
                                    "Text": "Core Average",
                                    "Value": "52.0 C",
                                    "SensorId": "/amdcpu/0/temperature/1",
                                    "Children": [],
                                },
                            ],
                        }
                    ],
                },
                {
                    "Text": "NVIDIA GeForce",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                {
                                    "Text": "GPU Core",
                                    "Value": "48.0 C",
                                    "SensorId": "/nvidiagpu/0/temperature/0",
                                    "Children": [],
                                }
                            ],
                        }
                    ],
                },
                {
                    "Text": "Samsung NVMe",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                {
                                    "Text": "Composite",
                                    "Value": "41.0 C",
                                    "SensorId": "/storage/nvme/0/temperature/0",
                                    "Children": [],
                                }
                            ],
                        }
                    ],
                },
                {
                    "Text": "ASUS Mainboard",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                {
                                    "Text": "Motherboard",
                                    "Value": "34.0 C",
                                    "SensorId": "/lpc/nct/0/temperature/0",
                                    "Children": [],
                                }
                            ],
                        }
                    ],
                },
            ],
        }

    def test_selects_expected_temperature_sources(self):
        sensors = bridge.LibreHardwareMonitorHttpReader._temperature_sensors(self.sample)
        expected = {
            "cpu": ("CPU Package", 57.0),
            "gpu": ("GPU Core", 48.0),
            "disk": ("Composite", 41.0),
            "system": ("Motherboard", 34.0),
        }
        for kind, (label, value) in expected.items():
            selected = bridge.LibreHardwareMonitorHttpReader._select(sensors, kind)
            self.assertIsNotNone(selected)
            self.assertEqual(label, selected.label)
            self.assertEqual(value, selected.value)


class ProtocolTests(unittest.TestCase):
    def test_frame_encoding(self):
        frame = bridge.encode_frame({"type": "stats", "clock_seconds": 3600})
        self.assertEqual(b'DD:{"type":"stats","clock_seconds":3600}\n', frame)

    def test_default_top_row(self):
        config_path = Path(__file__).resolve().parents[1] / "config.example.json"
        config = json.loads(config_path.read_text(encoding="utf-8"))
        labels = [item["label"] for item in bridge.macro_packet(config)["macros"]]
        self.assertEqual(["ChatGPT", "Ollama", "Voice Server"], labels[:3])

    def test_macro_packet_keeps_three_pages_and_hold_flag(self):
        config = {
            "macros": [
                {"id": 1, "label": "One"},
                {"id": 6, "label": "Lock PC", "confirm_hold": True},
                {"id": 18, "label": "Last"},
                {"id": 19, "label": "Too far"},
            ]
        }
        macros = bridge.macro_packet(config)["macros"]
        self.assertEqual([1, 6, 18], [item["id"] for item in macros])
        self.assertTrue(macros[1]["hold"])
        self.assertNotIn("hold", macros[0])

    def test_macro_packet_passes_known_colors_only(self):
        config = {"macros": [
            {"id": 1, "label": "A", "color": "green"},
            {"id": 2, "label": "B", "color": "chartreuse"},
            {"id": 3, "label": "C"},
        ]}
        macros = bridge.macro_packet(config)["macros"]
        self.assertEqual("green", macros[0]["color"])
        self.assertNotIn("color", macros[1])
        self.assertNotIn("color", macros[2])

    def test_macro_packet_passes_icon_names(self):
        config = {"macros": [{"id": 1, "label": "LOCK PC", "icon": "lock"}, {"id": 2, "label": "X"}]}
        macros = bridge.macro_packet(config)["macros"]
        self.assertEqual("lock", macros[0]["icon"])
        self.assertNotIn("icon", macros[1])


class FakeRelay:
    def __init__(self):
        self.calls = []

    def answer(self, ask_id, allow, always=False):
        self.calls.append(("answer", ask_id, allow, always))

    def clear_done(self):
        self.calls.append(("clear",))


class ClaudeMessageTests(unittest.TestCase):
    def test_reply_is_passed_to_the_relay(self):
        relay = FakeRelay()
        self.assertTrue(bridge.handle_claude_message(relay, {"type": "claude_reply", "id": 4, "ok": True}))
        self.assertTrue(bridge.handle_claude_message(relay, {"type": "claude_reply", "id": 5, "ok": False}))
        self.assertTrue(bridge.handle_claude_message(relay, {"type": "claude_reply", "id": 6, "ok": True, "always": True}))
        self.assertEqual([("answer", 4, True, False), ("answer", 5, False, False), ("answer", 6, True, True)],
                         relay.calls)

    def test_clear_done(self):
        relay = FakeRelay()
        self.assertTrue(bridge.handle_claude_message(relay, {"type": "claude_clear"}))
        self.assertEqual([("clear",)], relay.calls)

    def test_reply_without_ok_is_never_an_allow(self):
        relay = FakeRelay()
        bridge.handle_claude_message(relay, {"type": "claude_reply", "id": 4})
        self.assertEqual([("answer", 4, False, False)], relay.calls)

    def test_other_messages_and_no_relay_are_not_handled(self):
        self.assertFalse(bridge.handle_claude_message(FakeRelay(), {"type": "macro", "id": 1}))
        self.assertTrue(bridge.handle_claude_message(None, {"type": "claude_clear"}))


class StatsDateTests(unittest.TestCase):
    def test_stats_include_local_date(self):
        from datetime import date

        class FakeReader:
            def read(self):
                return {}

        previous = bridge.current_counters()
        with patch.object(bridge, "read_nvidia_gpu", return_value={}):
            payload, _ = bridge.get_stats(previous, ".", FakeReader())
        self.assertEqual(date.today().isoformat(), payload["date"])


class NotifyPacketTests(unittest.TestCase):
    def test_notify_packet_shape(self):
        self.assertEqual(
            {"type": "notify", "src": "pc", "app": "Teams", "from": "A", "body": "B"},
            bridge.notify_packet("Teams", "A", "B"),
        )

    def test_notify_packet_cuts_long_text_and_keeps_vietnamese(self):
        packet = bridge.notify_packet("Zalo", "Mẹ", "Đi ngủ đi " * 30, source="phone")
        self.assertEqual("phone", packet["src"])
        self.assertEqual(120, len(packet["body"]))
        self.assertTrue(packet["body"].startswith("Đi ngủ đi"))


class WifiProvisioningTests(unittest.TestCase):
    def test_no_wifi_section_means_no_packet(self):
        self.assertIsNone(bridge.wifi_packet({}))
        self.assertIsNone(bridge.wifi_packet({"wifi": {"ssid": ""}}))

    def test_wifi_packet_shape(self):
        config = {
            "wifi": {"ssid": "net", "password": "pw"},
            "notifications": {"udp_port": 4210, "token": "t" * 24},
        }
        self.assertEqual(
            {"type": "wifi", "ssid": "net", "pass": "pw", "token": "t" * 24, "port": 4210},
            bridge.wifi_packet(config),
        )

    def test_token_is_generated_once_and_saved(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "config.json"
            config = {"notifications": {"token": ""}}
            path.write_text(json.dumps(config), encoding="utf-8")
            token = bridge.ensure_notify_token(config, path)
            self.assertEqual(24, len(token))
            saved = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(token, saved["notifications"]["token"])
            self.assertEqual(token, bridge.ensure_notify_token(saved, path))

    def test_udp_notify_payload(self):
        sent = []

        class FakeSocket:
            def __init__(self, *args):
                pass

            def sendto(self, data, addr):
                sent.append((json.loads(data.decode("utf-8")), addr))

            def close(self):
                pass

        with patch.object(bridge.socket, "socket", FakeSocket):
            bridge.send_udp_notify("10.0.0.5", 4210, "tok", "Teams", "A", "B" * 300)
        payload, addr = sent[0]
        self.assertEqual(("10.0.0.5", 4210), addr)
        self.assertEqual({"token", "app", "from", "body"}, set(payload))
        self.assertEqual("tok", payload["token"])
        self.assertEqual(120, len(payload["body"]))

    def test_udp_notify_fits_device_limit_with_multibyte_text(self):
        sent = []

        class FakeSocket:
            def __init__(self, *args):
                pass

            def sendto(self, data, addr):
                sent.append(data)

            def close(self):
                pass

        with patch.object(bridge.socket, "socket", FakeSocket):
            bridge.send_udp_notify("10.0.0.5", 4210, "t" * 24, "ữ" * 200, "ữ" * 200, "ữ" * 200)
        self.assertLessEqual(len(sent[0]), 1024)
        self.assertTrue(json.loads(sent[0].decode("utf-8"))["body"])


class ToastForwardingTests(unittest.TestCase):
    def setUp(self):
        import queue

        self.toasts = queue.Queue()
        self.toasts.put(("Teams", "A", "B"))
        self.config = {"notifications": {"token": "tok"}}

    def test_without_device_wifi_toasts_go_over_usb(self):
        with patch.object(bridge, "send_json") as usb, patch.object(bridge, "send_udp_notify") as udp, \
                patch.object(bridge, "log"):
            bridge.forward_pc_toasts(self.toasts, object(), None, self.config)
        usb.assert_called_once()
        self.assertEqual("notify", usb.call_args[0][1]["type"])
        udp.assert_not_called()

    def test_with_device_wifi_toasts_go_over_udp(self):
        with patch.object(bridge, "send_json") as usb, patch.object(bridge, "send_udp_notify") as udp, \
                patch.object(bridge, "log"):
            bridge.forward_pc_toasts(self.toasts, object(), ("10.0.0.5", 4210), self.config)
        udp.assert_called_once_with("10.0.0.5", 4210, "tok", "Teams", "A", "B")
        usb.assert_not_called()

    def test_udp_error_falls_back_to_usb(self):
        with patch.object(bridge, "send_json") as usb, \
                patch.object(bridge, "send_udp_notify", side_effect=OSError("down")), \
                patch.object(bridge, "log"):
            bridge.forward_pc_toasts(self.toasts, object(), ("10.0.0.5", 4210), self.config)
        usb.assert_called_once()


class GoldFeedTests(unittest.TestCase):
    SAMPLE = {
        "time": "17:30",
        "prices": {
            "SJL1L10": {"buy": 139500000, "sell": 142500000, "change_sell": -500000},
            "XAUUSD": {"buy": 4126.1, "change_buy": 0.9000000000005},
        },
    }

    def test_prices_are_thousand_vnd_per_chi(self):
        config = {"items": [["SJL1L10", "SJC mieng"], ["MISSING", "Skipped"]]}
        with patch.object(bridge, "http_json", return_value=self.SAMPLE):
            gold = bridge.fetch_gold(config)
        self.assertEqual(
            [{"n": "SJC mieng", "b": 13950, "s": 14250, "c": -50}], gold["items"]
        )
        self.assertEqual(4126.1, gold["xau"])
        self.assertEqual(0.9, gold["xauc"])
        self.assertEqual("17:30", gold["time"])

    def test_info_feed_keeps_last_packet_on_failure(self):
        feed = bridge.InfoFeed({"gold": {"items": [["SJL1L10", "SJC"]]}})
        with patch.object(bridge, "http_json", return_value=self.SAMPLE):
            feed.refresh()
        version, packet = feed.snapshot()
        with patch.object(bridge, "http_json", side_effect=OSError("offline")), \
                patch.object(bridge, "log"):
            feed.refresh()
        self.assertEqual((version, packet), feed.snapshot())
        self.assertEqual("info", packet["type"])


@dataclass
class FakePort:
    device: str
    description: str = ""
    manufacturer: str = ""
    vid: Optional[int] = None
    pid: Optional[int] = None
    product: str = ""
    interface: str = ""
    hwid: str = ""


class SerialPortSelectionTests(unittest.TestCase):
    def test_explicit_port_is_never_replaced(self):
        selected = bridge.choose_serial_port("/dev/cu.manual", [])
        self.assertEqual("/dev/cu.manual", selected)

    def test_macos_prefers_callout_device_over_tty_device(self):
        ports = [
            FakePort(
                "/dev/tty.wchusbserial1420",
                description="USB2.0-Serial",
                manufacturer="wch.cn",
                vid=0x1A86,
                pid=0x7523,
            ),
            FakePort(
                "/dev/cu.wchusbserial1420",
                description="USB2.0-Serial",
                manufacturer="wch.cn",
                vid=0x1A86,
                pid=0x7523,
            ),
        ]
        with patch.object(bridge.platform, "system", return_value="Darwin"):
            with patch.object(bridge, "log"):
                selected = bridge.choose_serial_port("auto", ports)
        self.assertEqual("/dev/cu.wchusbserial1420", selected)

    def test_macos_recognizes_usbserial_without_vid_metadata(self):
        ports = [
            FakePort("/dev/cu.Bluetooth-Incoming-Port", description="Bluetooth-Incoming-Port"),
            FakePort("/dev/cu.usbserial-110", description="USB Serial"),
        ]
        with patch.object(bridge.platform, "system", return_value="Darwin"):
            selected = bridge.choose_serial_port("auto", ports)
        self.assertEqual("/dev/cu.usbserial-110", selected)

    def test_linux_recognizes_ttyusb_without_vid_metadata(self):
        ports = [
            FakePort("/dev/ttyS0", description="serial"),
            FakePort("/dev/ttyUSB0", description="USB2.0-Serial"),
        ]
        with patch.object(bridge.platform, "system", return_value="Linux"):
            selected = bridge.choose_serial_port("auto", ports)
        self.assertEqual("/dev/ttyUSB0", selected)

    def test_windows_keeps_original_first_match_behavior(self):
        ports = [
            FakePort("COM7", description="USB-SERIAL CH340", vid=0x1A86),
            FakePort("COM9", description="USB-SERIAL CH340", vid=0x1A86),
        ]
        with patch.object(bridge.platform, "system", return_value="Windows"):
            with patch.object(bridge, "log"):
                selected = bridge.choose_serial_port("auto", ports)
        self.assertEqual("COM7", selected)


class PlatformConfigurationTests(unittest.TestCase):
    def setUp(self):
        config_path = Path(__file__).resolve().parents[1] / "config.example.json"
        self.config = json.loads(config_path.read_text(encoding="utf-8"))

    def test_librehardwaremonitor_stays_enabled_on_windows(self):
        with patch.object(bridge.platform, "system", return_value="Windows"):
            reader = bridge.build_lhm_reader(self.config)
        self.assertTrue(reader.enabled)

    def test_librehardwaremonitor_is_disabled_by_default_on_linux_and_macos(self):
        for system in ("Linux", "Darwin"):
            with self.subTest(system=system):
                with patch.object(bridge.platform, "system", return_value=system):
                    reader = bridge.build_lhm_reader(self.config)
                self.assertFalse(reader.enabled)

    def test_platform_specific_macro_values_are_resolved(self):
        terminal = next(item for item in self.config["macros"] if item["id"] == 4)
        with patch.object(bridge.platform, "system", return_value="Darwin"):
            self.assertEqual(
                ["open", "-a", "Terminal"],
                bridge.resolve_platform_value(terminal["value"]),
            )
        with patch.object(bridge.platform, "system", return_value="Linux"):
            self.assertEqual(
                ["x-terminal-emulator"],
                bridge.resolve_platform_value(terminal["value"]),
            )

    def test_windows_default_macro_commands_remain_unchanged(self):
        expected = {
            2: ["ollama", "serve"],
            3: ["cmd.exe", "/c", "C:\\Path\\To\\START_VOICE_SERVER.cmd"],
            4: ["wt.exe"],
            5: ["taskmgr.exe"],
            6: ["rundll32.exe", "user32.dll,LockWorkStation"],
        }
        with patch.object(bridge.platform, "system", return_value="Windows"):
            actual = {
                item["id"]: bridge.resolve_platform_value(item.get("value"))
                for item in self.config["macros"]
                if item["id"] in expected
            }
        self.assertEqual(expected, actual)


class DeviceStateTests(unittest.TestCase):
    """device_state.json lets other local apps (ARAM Helper) reach the device over Wi-Fi."""

    def test_connected_state(self):
        state = bridge.device_state(("10.0.0.5", 4210))
        self.assertTrue(state["connected"])
        self.assertEqual(("10.0.0.5", 4210), (state["ip"], state["port"]))

    def test_disconnected_state_has_no_ip(self):
        state = bridge.device_state(None)
        self.assertFalse(state["connected"])
        self.assertNotIn("ip", state)

    def test_write_device_state(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "device_state.json"
            with patch.object(bridge, "DEVICE_STATE_FILE", path):
                bridge.write_device_state(("10.0.0.5", 4210))
            self.assertEqual("10.0.0.5", json.loads(path.read_text(encoding="utf-8"))["ip"])


if __name__ == "__main__":
    unittest.main()
