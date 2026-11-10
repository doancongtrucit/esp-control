#!/usr/bin/env python3
"""ESP Control Deck host bridge.

The bridge collects local PC telemetry and sends it to a 2.8-inch ESP32
display (Elecrow or ESP32-2432S028) through the board's CH340 USB serial
connection. It maps up to 18 touch buttons (three pages) to locally configured
actions and sends Vietnamese gold prices for the INFO page.

No Wi-Fi, Bluetooth, cloud account, API key, or browser credential is needed.
The gold price feed uses the public vang.today API over the PC's internet.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import platform
import queue
import re
import secrets
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import webbrowser
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable

import psutil
import serial
from serial.tools import list_ports

try:
    from pynput.keyboard import Controller as KeyboardController, Key, KeyCode
except Exception:
    KeyboardController = None
    Key = None
    KeyCode = None

APP_VERSION = "0.4.0"
PROTOCOL_PREFIX = "DD:"
PROTOCOL_VERSION = 3
# 3 pages x 6 buttons on the display (firmware MACRO_COUNT).
MAX_MACROS = 18
# Icon colors the UI v3 Controls screen knows (firmware MACRO_GRADIENTS).
MACRO_COLORS = {"green", "blue", "orange", "purple", "pink", "teal", "gray", "indigo", "yellow"}
CH340_VIDS = {0x1A86}
LOG_FILE: Path | None = None


def log(message: str) -> None:
    line = f"[{datetime.now().strftime('%H:%M:%S')}] {message}"
    print(line, flush=True)
    if LOG_FILE is not None:
        try:
            with LOG_FILE.open("a", encoding="utf-8") as handle:
                handle.write(line + "\n")
        except OSError:
            pass


def load_config(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError("The configuration root must be a JSON object")
    return data


def platform_key() -> str:
    return {
        "Windows": "windows",
        "Linux": "linux",
        "Darwin": "macos",
    }.get(platform.system(), "default")


def resolve_platform_value(value: Any) -> Any:
    if not isinstance(value, dict):
        return value
    return value.get(platform_key(), value.get("default"))


def expand_text(value: Any) -> Any:
    if not isinstance(value, str):
        return value
    return os.path.expanduser(os.path.expandvars(value))


def _port_text(port: Any) -> str:
    values = (
        getattr(port, "description", ""),
        getattr(port, "manufacturer", ""),
        getattr(port, "product", ""),
        getattr(port, "interface", ""),
        getattr(port, "hwid", ""),
    )
    return " ".join(str(value or "") for value in values).upper()


def _choose_windows_serial_port(ports: list[Any]) -> str | None:
    """Keep the existing Windows auto-detection behavior unchanged."""
    preferred: list[str] = []
    for port in ports:
        description = (getattr(port, "description", "") or "").upper()
        manufacturer = (getattr(port, "manufacturer", "") or "").upper()
        if (
            getattr(port, "vid", None) in CH340_VIDS
            or "CH340" in description
            or "CH340" in manufacturer
            or "USB-SERIAL" in description
        ):
            preferred.append(str(port.device))

    if len(preferred) == 1:
        return preferred[0]
    if preferred:
        log("Multiple CH340-like serial ports found: " + ", ".join(preferred))
        return preferred[0]
    return None


def _posix_serial_score(port: Any, system: str) -> int:
    """Score likely Control Deck ports on Linux and macOS."""
    device = str(getattr(port, "device", "") or "")
    device_lower = device.lower()
    text = _port_text(port)
    score = 0

    if getattr(port, "vid", None) in CH340_VIDS:
        score += 120
    if "CH340" in text or "CH341" in text:
        score += 100
    if "QINHENG" in text or "WCH" in text:
        score += 80
    if any(token in text for token in ("USB-SERIAL", "USB SERIAL", "USB2.0-SERIAL")):
        score += 60

    if system == "Darwin":
        if "wchusbserial" in device_lower:
            score += 80
        elif "usbserial" in device_lower:
            score += 50
        if score > 0:
            if device_lower.startswith("/dev/cu."):
                score += 35
            elif device_lower.startswith("/dev/tty."):
                score += 5
    elif system == "Linux":
        if device_lower.startswith("/dev/ttyusb"):
            score += 60
        elif device_lower.startswith("/dev/ttyacm"):
            score += 20

    return score


def choose_serial_port(configured: str, ports: list[Any] | None = None) -> str | None:
    if configured and configured.lower() != "auto":
        return configured

    ports = list(list_ports.comports()) if ports is None else list(ports)
    if not ports:
        return None

    system = platform.system()
    if system == "Windows":
        return _choose_windows_serial_port(ports)

    scored = [
        (_posix_serial_score(port, system), str(port.device))
        for port in ports
    ]
    candidates = sorted(
        ((score, device) for score, device in scored if score > 0),
        key=lambda item: (-item[0], item[1]),
    )
    if not candidates:
        return None

    best_score = candidates[0][0]
    best_devices = [device for score, device in candidates if score == best_score]
    if len(candidates) > 1:
        log(
            "Serial candidates: "
            + ", ".join(f"{device} ({score})" for score, device in candidates)
        )
    return best_devices[0]


def _format_vid_pid(port: Any) -> str:
    vid = getattr(port, "vid", None)
    pid = getattr(port, "pid", None)
    if isinstance(vid, int) and isinstance(pid, int):
        return f"{vid:04X}:{pid:04X}"
    return "unknown"


def print_serial_ports(configured: str) -> int:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports are currently visible.")
        if platform.system() == "Linux":
            print("Connect the board and check USB permissions for /dev/ttyUSB*.")
        return 1

    print("Visible serial ports:\n")
    for port in ports:
        description = str(getattr(port, "description", "") or "unknown")
        manufacturer = str(getattr(port, "manufacturer", "") or "unknown")
        print(f"- {port.device}")
        print(f"  description: {description}")
        print(f"  manufacturer: {manufacturer}")
        print(f"  VID:PID: {_format_vid_pid(port)}")

    selected = choose_serial_port(configured, ports)
    print()
    if selected:
        print(f"Selected port: {selected}")
        return 0

    print("No CH340-like Control Deck port was selected automatically.")
    print("Set serial_port manually in host/config.json.")
    return 1


def safe_number(value: float | None, digits: int = 2) -> float | None:
    if value is None:
        return None
    try:
        numeric = float(value)
        if math.isnan(numeric) or math.isinf(numeric):
            return None
    except (TypeError, ValueError):
        return None
    return round(numeric, digits)


def disk_path_from_config(config: dict[str, Any]) -> str:
    configured = str(config.get("disk_path", "auto"))
    if configured.lower() != "auto":
        return configured
    if platform.system() == "Windows":
        return os.environ.get("SystemDrive", "C:") + "\\"
    return "/"


def read_cpu_temp_psutil() -> float | None:
    """Read a CPU temperature where psutil exposes sensors (mainly POSIX)."""
    function = getattr(psutil, "sensors_temperatures", None)
    if function is None:
        return None
    try:
        sensors = function(fahrenheit=False) or {}
    except Exception:
        return None

    preferred: list[float] = []
    fallback: list[float] = []
    for entries in sensors.values():
        for entry in entries:
            current = getattr(entry, "current", None)
            if current is None:
                continue
            try:
                current = float(current)
            except (TypeError, ValueError):
                continue
            if not 0 < current < 130:
                continue
            label = (getattr(entry, "label", "") or "").lower()
            if any(token in label for token in ("package", "tctl", "tdie", "cpu")):
                preferred.append(current)
            else:
                fallback.append(current)
    values = preferred or fallback
    return max(values) if values else None


def parse_numeric(value: Any) -> float | None:
    if isinstance(value, (int, float)):
        return safe_number(float(value), 2)
    if value is None:
        return None
    match = re.search(r"-?\d+(?:[.,]\d+)?", str(value))
    if not match:
        return None
    try:
        return float(match.group(0).replace(",", "."))
    except ValueError:
        return None


@dataclass(frozen=True)
class TemperatureSensor:
    value: float
    label: str
    sensor_id: str
    context: str


class LibreHardwareMonitorHttpReader:
    """Read temperatures from LibreHardwareMonitor's local data.json tree."""

    def __init__(self, enabled: bool, url: str, timeout: float = 0.5):
        self.enabled = enabled
        self.url = url
        self.timeout = timeout
        self._reachable: bool | None = None
        self._logged_sources: dict[str, str] = {}

    @staticmethod
    def _walk(node: Any, path: tuple[str, ...] = ()) -> Iterable[tuple[dict[str, Any], tuple[str, ...]]]:
        if not isinstance(node, dict):
            return
        text = str(node.get("Text", "") or "")
        next_path = path + ((text,) if text else ())
        yield node, next_path
        for child in node.get("Children") or []:
            yield from LibreHardwareMonitorHttpReader._walk(child, next_path)

    @staticmethod
    def _is_temperature_node(node: dict[str, Any], context: str) -> bool:
        node_type = str(node.get("Type", "") or "").lower()
        sensor_id = str(node.get("SensorId", "") or "").lower()
        text = str(node.get("Text", "") or "").lower()
        return (
            node_type == "temperature"
            or "/temperature/" in sensor_id
            or "temperatures" in context
            or "temperature" in text
        )

    @staticmethod
    def _temperature_sensors(data: Any) -> list[TemperatureSensor]:
        sensors: list[TemperatureSensor] = []
        for node, path in LibreHardwareMonitorHttpReader._walk(data):
            if "Value" not in node:
                continue
            context = " ".join(path).lower()
            if not LibreHardwareMonitorHttpReader._is_temperature_node(node, context):
                continue

            value = parse_numeric(node.get("Value"))
            if value is None or not 0 < value < 130:
                continue

            sensors.append(
                TemperatureSensor(
                    value=value,
                    label=str(node.get("Text", "Temperature") or "Temperature"),
                    sensor_id=str(node.get("SensorId", "") or ""),
                    context=" > ".join(path),
                )
            )
        return sensors

    @staticmethod
    def _score(sensor: TemperatureSensor, kind: str) -> int | None:
        sensor_id = sensor.sensor_id.lower()
        label = sensor.label.lower()
        context = sensor.context.lower()
        haystack = " ".join((sensor_id, label, context))

        if kind == "cpu":
            identity = any(
                token in haystack
                for token in (
                    "amdcpu",
                    "intelcpu",
                    "/cpu/",
                    "amd ryzen",
                    "intel core",
                    "processor",
                )
            )
            if not identity:
                return None
            score = 20
            if "cpu package" in label or "package" in label:
                score += 100
            elif "tctl" in label or "tdie" in label:
                score += 90
            elif "core average" in label:
                score += 80
            elif "core max" in label or "maximum" in label:
                score += 60
            elif "core" in label:
                score += 20
            return score

        if kind == "gpu":
            if not any(
                token in haystack
                for token in ("nvidiagpu", "atigpu", "intelgpu", "/gpu/", "graphics")
            ):
                return None
            score = 20
            if "gpu core" in label or label == "core":
                score += 70
            if "gpu package" in label or "package" in label:
                score += 60
            if "hot spot" in label or "hotspot" in label:
                score += 30
            return score

        if kind == "disk":
            if not any(
                token in haystack
                for token in ("/hdd/", "/storage/", "nvme", "ssd", "disk", "drive")
            ):
                return None
            score = 20
            if "composite" in label:
                score += 60
            if label in {"temperature", "drive temperature"}:
                score += 50
            return score

        if kind == "system":
            if not any(
                token in haystack
                for token in ("/lpc/", "motherboard", "mainboard", "super i/o", "superio", "chipset")
            ):
                return None
            score = 10
            if "motherboard" in label or "mainboard" in label or "system" in label:
                score += 60
            if "ambient" in label:
                score += 45
            if "pch" in label or "chipset" in label:
                score += 25
            if "vrm" in label:
                score += 15
            return score

        return None

    @classmethod
    def _select(cls, sensors: list[TemperatureSensor], kind: str) -> TemperatureSensor | None:
        candidates: list[tuple[int, TemperatureSensor]] = []
        for sensor in sensors:
            score = cls._score(sensor, kind)
            if score is not None:
                candidates.append((score, sensor))
        if not candidates:
            return None
        best_score = max(score for score, _ in candidates)
        best = [sensor for score, sensor in candidates if score == best_score]
        return max(best, key=lambda item: item.value)

    def _fetch(self) -> Any:
        request = urllib.request.Request(
            self.url,
            headers={"User-Agent": f"ESP-Control-Deck/{APP_VERSION}"},
        )
        with urllib.request.urlopen(request, timeout=self.timeout) as response:
            return json.loads(response.read().decode("utf-8", errors="replace"))

    def _log_source(self, kind: str, sensor: TemperatureSensor | None) -> None:
        identity = "none" if sensor is None else f"{sensor.label} [{sensor.sensor_id}]"
        if self._logged_sources.get(kind) == identity:
            return
        self._logged_sources[kind] = identity
        if sensor is not None:
            log(f"Temperature source {kind}: {identity}")

    def read(self) -> dict[str, float | None]:
        result: dict[str, float | None] = {
            "cpu_temp": None,
            "gpu_temp": None,
            "disk_temp": None,
            "system_temp": None,
        }
        if not self.enabled:
            return result

        try:
            data = self._fetch()
            if self._reachable is not True:
                log("LibreHardwareMonitor web server detected")
            self._reachable = True

            sensors = self._temperature_sensors(data)
            for kind, result_key in (
                ("cpu", "cpu_temp"),
                ("gpu", "gpu_temp"),
                ("disk", "disk_temp"),
                ("system", "system_temp"),
            ):
                selected = self._select(sensors, kind)
                self._log_source(kind, selected)
                if selected is not None:
                    result[result_key] = selected.value
        except (OSError, urllib.error.URLError, json.JSONDecodeError, TimeoutError):
            if self._reachable is None:
                if platform.system() == "Windows":
                    log(
                        "Additional temperatures unavailable. On Windows, run "
                        "LibreHardwareMonitor and enable Remote Web Server > Run."
                    )
                else:
                    log(f"Configured temperature endpoint is unavailable: {self.url}")
            self._reachable = False
        except Exception as exc:
            if self._reachable is not False:
                log(f"LibreHardwareMonitor read error: {exc}")
            self._reachable = False
        return result

    def print_temperature_sensors(self) -> int:
        if not self.enabled:
            print("LibreHardwareMonitor integration is disabled in config.json")
            return 2
        try:
            sensors = self._temperature_sensors(self._fetch())
        except Exception as exc:
            print(f"Could not read {self.url}: {exc}", file=sys.stderr)
            return 2

        if not sensors:
            print("No temperature sensors were found in the LibreHardwareMonitor JSON tree.")
            return 1

        print(f"Temperature sensors from {self.url}:\n")
        for sensor in sensors:
            print(f"{sensor.value:6.1f} C  {sensor.label}")
            print(f"          id: {sensor.sensor_id or '(not provided)'}")
            print(f"        path: {sensor.context}\n")
        return 0


def read_nvidia_gpu() -> dict[str, float | None]:
    result: dict[str, float | None] = {
        "gpu": None,
        "vram": None,
        "gpu_temp": None,
        "vram_used_gb": None,
        "vram_total_gb": None,
    }
    executable = shutil.which("nvidia-smi")
    if not executable:
        return result

    command = [
        executable,
        "--query-gpu=utilization.gpu,memory.used,memory.total,temperature.gpu",
        "--format=csv,noheader,nounits",
    ]
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=2,
            check=False,
            creationflags=subprocess.CREATE_NO_WINDOW if platform.system() == "Windows" else 0,
        )
        if completed.returncode != 0 or not completed.stdout.strip():
            return result

        fields = [item.strip() for item in completed.stdout.strip().splitlines()[0].split(",")]
        if len(fields) < 4:
            return result

        gpu = float(fields[0])
        used_mib = float(fields[1])
        total_mib = float(fields[2])
        temperature = float(fields[3])
        result["gpu"] = gpu
        result["vram"] = used_mib * 100.0 / total_mib if total_mib > 0 else None
        result["gpu_temp"] = temperature
        result["vram_used_gb"] = used_mib / 1024.0
        result["vram_total_gb"] = total_mib / 1024.0
    except Exception:
        pass
    return result


@dataclass
class Counters:
    time: float
    net_sent: int
    net_recv: int
    disk_read: int
    disk_write: int


def current_counters() -> Counters:
    network = psutil.net_io_counters()
    disk = psutil.disk_io_counters()
    return Counters(
        time=time.monotonic(),
        net_sent=int(network.bytes_sent if network else 0),
        net_recv=int(network.bytes_recv if network else 0),
        disk_read=int(disk.read_bytes if disk else 0),
        disk_write=int(disk.write_bytes if disk else 0),
    )


def get_stats(
    previous: Counters,
    disk_path: str,
    lhm: LibreHardwareMonitorHttpReader,
) -> tuple[dict[str, Any], Counters]:
    current = current_counters()
    elapsed = max(0.001, current.time - previous.time)

    try:
        disk_used = psutil.disk_usage(disk_path).percent
    except Exception:
        disk_used = None

    memory = psutil.virtual_memory()
    gib = 1024.0 ** 3
    ram_total_gb = memory.total / gib
    ram_used_gb = (memory.total - memory.available) / gib

    nvidia = read_nvidia_gpu()
    lhm_stats = lhm.read()
    psutil_cpu_temp = read_cpu_temp_psutil()
    cpu_temp = psutil_cpu_temp if psutil_cpu_temp is not None else lhm_stats.get("cpu_temp")
    gpu_temp = nvidia.get("gpu_temp")
    if gpu_temp is None:
        gpu_temp = lhm_stats.get("gpu_temp")

    now = datetime.now()
    clock_seconds = now.hour * 3600 + now.minute * 60 + now.second

    payload = {
        "type": "stats",
        "cpu": safe_number(psutil.cpu_percent(interval=None), 1),
        "ram": safe_number(memory.percent, 1),
        "ram_used_gb": safe_number(ram_used_gb, 1),
        "ram_total_gb": safe_number(ram_total_gb, 1),
        "gpu": safe_number(nvidia.get("gpu"), 1),
        "vram": safe_number(nvidia.get("vram"), 1),
        "vram_used_gb": safe_number(nvidia.get("vram_used_gb"), 1),
        "vram_total_gb": safe_number(nvidia.get("vram_total_gb"), 1),
        "cpu_temp": safe_number(cpu_temp, 1),
        "gpu_temp": safe_number(gpu_temp, 1),
        "system_temp": safe_number(lhm_stats.get("system_temp"), 1),
        "disk_temp": safe_number(lhm_stats.get("disk_temp"), 1),
        "disk_used": safe_number(disk_used, 1),
        "disk_read_mbs": safe_number(
            max(0, current.disk_read - previous.disk_read) / elapsed / 1_000_000.0,
            2,
        ),
        "disk_write_mbs": safe_number(
            max(0, current.disk_write - previous.disk_write) / elapsed / 1_000_000.0,
            2,
        ),
        "net_down_mbps": safe_number(
            max(0, current.net_recv - previous.net_recv) * 8.0 / elapsed / 1_000_000.0,
            2,
        ),
        "net_up_mbps": safe_number(
            max(0, current.net_sent - previous.net_sent) * 8.0 / elapsed / 1_000_000.0,
            2,
        ),
        "clock": now.strftime("%H:%M"),
        "clock_seconds": clock_seconds,
        "date": now.date().isoformat(),
        # Unix time and UTC offset: the deck keeps its own clock (and signs Wi-Fi frames) with these.
        "epoch": int(time.time()),
        "tz": int(now.astimezone().utcoffset().total_seconds()),
    }
    return payload, current


def macro_packet(config: dict[str, Any]) -> dict[str, Any]:
    macros: list[dict[str, Any]] = []
    for item in config.get("macros", []):
        try:
            macro_id = int(item.get("id"))
        except Exception:
            continue
        if 1 <= macro_id <= MAX_MACROS:
            entry: dict[str, Any] = {
                "id": macro_id,
                "label": str(item.get("label", f"MACRO {macro_id}"))[:22],
            }
            if item.get("confirm_hold"):
                entry["hold"] = True
            if item.get("icon"):
                entry["icon"] = str(item["icon"])[:16]
            if item.get("color") in MACRO_COLORS:
                entry["color"] = item["color"]
            macros.append(entry)
    return {"type": "config", "macros": macros}


NOTIFY_TEXT_LIMIT = 120


def notify_packet(app: str, sender: str, body: str, source: str = "pc") -> dict[str, Any]:
    """USB frame that shows a notification on the device (UI v2)."""
    return {
        "type": "notify",
        "src": source,
        "app": str(app)[:NOTIFY_TEXT_LIMIT],
        "from": str(sender)[:NOTIFY_TEXT_LIMIT],
        "body": str(body)[:NOTIFY_TEXT_LIMIT],
    }


DEFAULT_NOTIFY_PORT = 4210
NOTIFY_TOKEN_BYTES = 18  # token_urlsafe(18) -> 24 characters


def notify_settings(config: dict[str, Any]) -> dict[str, Any]:
    return config.get("notifications") or {}


def ensure_notify_token(config: dict[str, Any], path: Path) -> str:
    """Returns the UDP token, generating it and saving config.json once if empty."""
    settings = config.setdefault("notifications", {})
    token = str(settings.get("token") or "")
    if not token:
        token = secrets.token_urlsafe(NOTIFY_TOKEN_BYTES)
        settings["token"] = token
        path.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return token


def wifi_packet(config: dict[str, Any]) -> dict[str, Any] | None:
    """USB frame that provisions the device's Wi-Fi, or None when not configured."""
    wifi = config.get("wifi") or {}
    if not wifi.get("ssid"):
        return None
    settings = notify_settings(config)
    return {
        "type": "wifi",
        "ssid": str(wifi["ssid"]),
        "pass": str(wifi.get("password", "")),
        "token": str(settings.get("token", "")),
        "port": int(settings.get("udp_port", DEFAULT_NOTIFY_PORT)),
    }


UDP_NOTIFY_MAX_BYTES = 1024  # device limit (net_notify.h UDP_NOTIFY_MAX)
UDP_FIELD_LIMITS = {"app": 24, "from": 40}  # the device shows 12 / 24 characters anyway


def send_udp_notify(host: str, port: int, token: str, app: str, sender: str, body: str) -> None:
    packet = notify_packet(app, sender, body)
    payload = {
        "token": token,
        "app": packet["app"][: UDP_FIELD_LIMITS["app"]],
        "from": packet["from"][: UDP_FIELD_LIMITS["from"]],
        "body": packet["body"],
    }
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    while len(data) > UDP_NOTIFY_MAX_BYTES and payload["body"]:
        payload["body"] = payload["body"][:-10]
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(data, (host, port))
    finally:
        sock.close()


DEVICE_STATE_FILE: Path | None = None  # set in main(): device_state.json next to config.json (git-ignored)


def device_state(device_wifi: tuple[str, int] | None) -> dict[str, Any]:
    """Where other local apps (e.g. ARAM Helper) can reach the device over Wi-Fi UDP."""
    if device_wifi is None:
        return {"connected": False, "updated": int(time.time())}
    return {"connected": True, "ip": device_wifi[0], "port": device_wifi[1], "updated": int(time.time())}


def write_device_state(device_wifi: tuple[str, int] | None) -> None:
    if DEVICE_STATE_FILE is None:
        return
    try:
        DEVICE_STATE_FILE.write_text(json.dumps(device_state(device_wifi)) + "\n", encoding="utf-8")
    except OSError as exc:
        log(f"Could not write {DEVICE_STATE_FILE.name}: {type(exc).__name__}")


def send_wifi_provisioning(serial_port: serial.Serial, config: dict[str, Any]) -> None:
    packet = wifi_packet(config)
    if packet is not None:
        send_json(serial_port, packet)


def start_pc_notifications(config: dict[str, Any]) -> "queue.Queue[tuple[str, str, str]]":
    """Starts the Windows toast watcher; toasts arrive on the returned queue."""
    toasts: "queue.Queue[tuple[str, str, str]]" = queue.Queue(maxsize=50)
    if not notify_settings(config).get("enabled", True) or platform.system() != "Windows":
        return toasts
    try:
        from pc_notifications import PcNotificationWatcher
    except Exception as exc:
        log(f"PC notifications disabled: {type(exc).__name__}")
        return toasts

    def on_toast(app: str, sender: str, body: str) -> None:
        try:
            toasts.put_nowait((app, sender, body))
        except queue.Full:
            pass

    PcNotificationWatcher(on_toast, log=log).start()
    return toasts


def forward_pc_toasts(
    toasts: "queue.Queue[tuple[str, str, str]]",
    serial_port: serial.Serial,
    device_wifi: tuple[str, int] | None,
    config: dict[str, Any],
) -> None:
    """Sends queued PC toasts over Wi-Fi when the device reported an IP, else over USB."""
    token = str(notify_settings(config).get("token", ""))
    while True:
        try:
            app, sender, body = toasts.get_nowait()
        except queue.Empty:
            return
        if device_wifi is not None and token:
            try:
                send_udp_notify(device_wifi[0], device_wifi[1], token, app, sender, body)
                log("Forwarded a PC notification over Wi-Fi")
                continue
            except OSError as exc:
                log(f"Wi-Fi send failed ({type(exc).__name__}); using USB")
        send_json(serial_port, notify_packet(app, sender, body))
        log("Forwarded a PC notification over USB")


def keyboard_key(token: str):
    if Key is None or KeyCode is None:
        raise RuntimeError("pynput is not available")
    token = token.strip().lower()
    mapping = {
        "ctrl": Key.ctrl,
        "control": Key.ctrl,
        "alt": Key.alt,
        "shift": Key.shift,
        "cmd": Key.cmd,
        "win": Key.cmd,
        "windows": Key.cmd,
        "enter": Key.enter,
        "return": Key.enter,
        "esc": Key.esc,
        "escape": Key.esc,
        "tab": Key.tab,
        "space": Key.space,
        "backspace": Key.backspace,
        "delete": Key.delete,
        "up": Key.up,
        "down": Key.down,
        "left": Key.left,
        "right": Key.right,
        "home": Key.home,
        "end": Key.end,
        "page_up": Key.page_up,
        "page_down": Key.page_down,
        "volume_up": Key.media_volume_up,
        "volume_down": Key.media_volume_down,
        "volume_mute": Key.media_volume_mute,
        "media_play_pause": Key.media_play_pause,
        "media_next": Key.media_next,
        "media_previous": Key.media_previous,
    }
    if token in mapping:
        return mapping[token]
    if token.startswith("f") and token[1:].isdigit() and hasattr(Key, token):
        return getattr(Key, token)
    if len(token) == 1:
        return KeyCode.from_char(token)
    raise ValueError(f"Unsupported key token: {token}")


def send_hotkey(specification: str) -> None:
    if KeyboardController is None:
        raise RuntimeError("pynput is not installed or available")
    controller = KeyboardController()
    keys = [keyboard_key(item) for item in specification.split("+") if item.strip()]
    for key in keys:
        controller.press(key)
    for key in reversed(keys):
        controller.release(key)


def type_text(text: str) -> None:
    if KeyboardController is None:
        raise RuntimeError("pynput is not installed or available")
    KeyboardController().type(text)


def url_reachable(url: str, timeout: float = 0.6) -> bool:
    try:
        request = urllib.request.Request(url, headers={"User-Agent": f"ControlDeck/{APP_VERSION}"})
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return 200 <= int(response.status) < 500
    except Exception:
        return False


def start_process(
    command_value: Any,
    working_directory: Any = None,
    hidden: bool = False,
    new_console: bool = False,
) -> subprocess.Popen[Any]:
    command_value = resolve_platform_value(command_value)
    working_directory = resolve_platform_value(working_directory)
    if not command_value:
        raise ValueError("No command is configured for this operating system")

    cwd = expand_text(working_directory) if working_directory else None
    if cwd and not Path(str(cwd)).exists():
        raise FileNotFoundError(f"Working directory does not exist: {cwd}")

    shell = isinstance(command_value, str)
    if isinstance(command_value, list):
        command: Any = [str(expand_text(item)) for item in command_value]
    else:
        command = str(expand_text(command_value))

    creationflags = 0
    startupinfo = None
    stdout: Any = None
    stderr: Any = None
    if platform.system() == "Windows":
        if hidden:
            creationflags |= subprocess.CREATE_NO_WINDOW
            stdout = subprocess.DEVNULL
            stderr = subprocess.DEVNULL
        elif new_console:
            creationflags |= subprocess.CREATE_NEW_CONSOLE
        if hidden:
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = 0

    return subprocess.Popen(
        command,
        shell=shell,
        cwd=str(cwd) if cwd else None,
        creationflags=creationflags,
        startupinfo=startupinfo,
        stdout=stdout,
        stderr=stderr,
    )


def execute_macro(config: dict[str, Any], macro_id: int) -> None:
    item = next(
        (macro for macro in config.get("macros", []) if int(macro.get("id", -1)) == macro_id),
        None,
    )
    if item is None:
        log(f"Macro {macro_id}: not configured")
        return

    action = str(item.get("action", "none")).lower()
    label = str(item.get("label", f"Macro {macro_id}"))
    log(f"Macro {macro_id} pressed: {label}")

    try:
        if action == "none":
            return
        if action == "url":
            value = resolve_platform_value(item.get("value"))
            if not value:
                raise ValueError("No URL configured")
            webbrowser.open(str(value))
            return
        if action == "hotkey":
            send_hotkey(str(resolve_platform_value(item.get("value"))))
            return
        if action == "text":
            type_text(str(resolve_platform_value(item.get("value"))))
            return
        if action in {"command", "service"}:
            health_url = str(item.get("health_url", "") or "").strip()
            if action == "service" and health_url and url_reachable(health_url):
                log(f"{label} is already running: {health_url}")
                return
            process = start_process(
                item.get("value"),
                working_directory=item.get("working_directory"),
                hidden=bool(item.get("hidden", False)),
                new_console=bool(item.get("new_console", False)),
            )
            log(f"Started {label} (PID {process.pid})")
            return
        raise ValueError(f"Unsupported macro action: {action}")
    except Exception as exc:
        log(f"Macro {macro_id} failed: {exc}")


def encode_frame(obj: dict[str, Any]) -> bytes:
    payload = json.dumps(obj, separators=(",", ":"), ensure_ascii=True)
    return f"{PROTOCOL_PREFIX}{payload}\n".encode("ascii")


def send_json(serial_port: Any, obj: dict[str, Any]) -> None:
    """Writes a frame to the USB port, or hands it to a Wi-Fi link (anything with send_frame)."""
    if hasattr(serial_port, "send_frame"):
        serial_port.send_frame(obj)
        return
    serial_port.write(encode_frame(obj))
    serial_port.flush()


def parse_frame(text: str) -> dict[str, Any] | None:
    if text.startswith(PROTOCOL_PREFIX):
        payload = text[len(PROTOCOL_PREFIX) :]
    elif text.startswith("{"):
        payload = text
    else:
        return None
    try:
        message = json.loads(payload)
    except json.JSONDecodeError:
        return None
    return message if isinstance(message, dict) else None


def build_lhm_reader(config: dict[str, Any]) -> LibreHardwareMonitorHttpReader:
    lhm_config = config.get("librehardwaremonitor", {}) or {}
    enabled = resolve_platform_value(lhm_config.get("enabled", True))
    return LibreHardwareMonitorHttpReader(
        enabled=bool(enabled),
        url=str(lhm_config.get("url", "http://127.0.0.1:8085/data.json")),
        timeout=float(lhm_config.get("timeout_seconds", 0.5)),
    )


# ---------------------------------------------------------------------------
# INFO page: Vietnamese gold prices (vang.today), in thousand VND per chi.
# Fetched on a background thread so slow HTTP never blocks the serial loop.
# ---------------------------------------------------------------------------

DEFAULT_GOLD_ITEMS = [
    ["SJL1L10", "SJC mieng"],
    ["SJ9999", "Nhan SJC"],
    ["DOHNL", "DOJI HN"],
    ["PQHN24NTT", "PNJ 24K"],
    ["BTSJC", "Bao Tin SJC"],
    ["BT9999NTT", "Bao Tin 9999"],
]
VND_PER_LUONG_TO_K_PER_CHI = 1 / 10 / 1000  # 1 luong = 10 chi


def http_json(url: str, timeout: float = 10.0) -> Any:
    request = urllib.request.Request(url, headers={"User-Agent": f"PC-Control-Deck/{APP_VERSION}"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def fetch_gold(gold_config: dict[str, Any]) -> dict[str, Any]:
    data = http_json(str(gold_config.get("url", "https://www.vang.today/api/prices")))
    prices = data.get("prices", {})
    items = []
    for code, label in gold_config.get("items", DEFAULT_GOLD_ITEMS):
        price = prices.get(code)
        if not price:
            continue
        items.append(
            {
                "n": str(label)[:14],
                "b": round(price["buy"] * VND_PER_LUONG_TO_K_PER_CHI),
                "s": round(price["sell"] * VND_PER_LUONG_TO_K_PER_CHI),
                "c": round(price.get("change_sell", 0) * VND_PER_LUONG_TO_K_PER_CHI),
            }
        )
    world = prices.get("XAUUSD", {})
    return {
        "time": str(data.get("time", "")),
        "items": items[:6],
        "xau": world.get("buy"),
        "xauc": round(world.get("change_buy") or 0, 1),
    }


class InfoFeed:
    """Keeps the latest INFO packet, refreshed in the background."""

    def __init__(self, config: dict[str, Any]):
        self.gold_config = config.get("gold") or {}
        self.enabled = bool(self.gold_config.get("enabled", True))
        self.refresh_seconds = max(60.0, float(self.gold_config.get("refresh_minutes", 15)) * 60.0)
        self.lock = threading.Lock()
        self.packet: dict[str, Any] | None = None
        self.version = 0

    def start(self) -> None:
        if self.enabled:
            threading.Thread(target=self._loop, name="info-feed", daemon=True).start()

    def snapshot(self) -> tuple[int, dict[str, Any] | None]:
        with self.lock:
            return self.version, self.packet

    def _loop(self) -> None:
        while True:
            self.refresh()
            time.sleep(self.refresh_seconds)

    def refresh(self) -> None:
        try:
            packet = {"type": "info", "g": fetch_gold(self.gold_config)}
        except Exception as exc:
            log(f"Gold price update failed: {exc}")
            return
        with self.lock:
            if packet != self.packet:
                self.packet = packet
                self.version += 1


def start_claude_relay(config: dict[str, Any]) -> Any:
    """Localhost relay for Claude Code hooks (docs/CLAUDE_CODE.md); None when off or the port is taken."""
    settings = config.get("claude", {})
    if not settings.get("enabled", True):
        return None
    from claude_relay import ClaudeRelay

    relay = ClaudeRelay(port=int(settings.get("port", 47821)),
                        ask_wait=float(settings.get("ask_wait_seconds", 25)), log=log,
                        stats_path=Path(__file__).resolve().parent / "claude_stats.json")
    try:
        relay.start()
    except OSError as exc:
        log(f"Claude Code relay not started: {exc}")
        return None
    log(f"Claude Code relay listening on 127.0.0.1:{relay.port}")
    return relay


def start_claude_usage(config: dict[str, Any]) -> Any:
    """Token usage and estimated cost from ~/.claude/projects for the deck's Usage tab; None when off."""
    settings = config.get("claude", {})
    if not settings.get("enabled", True) or not settings.get("usage", True):
        return None
    from claude_usage import UsageFeed

    feed = UsageFeed(Path.home() / ".claude" / "projects", log=log)
    feed.start()
    return feed


def start_activity(config: dict[str, Any]) -> Any:
    """Time at the PC, break reminders and lock-when-the-phone-leaves; None when off."""
    settings = config.get("activity", {})
    if not settings.get("enabled", True):
        return None
    from activity import ActivityService

    service = ActivityService(config, Path(__file__).with_name("activity_state.json"), log=log)
    service.start()
    return service


def focus_window(name: str) -> None:
    """Best effort: bring forward a window whose title has the session name, else a Claude window."""
    if platform.system() != "Windows":
        return
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    titles: list[tuple[int, str]] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def collect(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length:
                buffer = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buffer, length + 1)
                titles.append((hwnd, buffer.value))
        return True

    user32.EnumWindows(collect, 0)
    wanted = [h for h, t in titles if name and name.lower() in t.lower()] or              [h for h, t in titles if "claude" in t.lower()]
    if not wanted:
        return
    hwnd = wanted[0]
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    user32.keybd_event(0x12, 0, 0, 0)  # a tap of Alt lets this process change the foreground window
    user32.keybd_event(0x12, 0, 2, 0)
    user32.SetForegroundWindow(hwnd)


def handle_claude_message(relay: Any, message: dict[str, Any]) -> bool:
    """Deck -> bridge Claude frames; True when the message was one of them."""
    message_type = message.get("type")
    if message_type not in {"claude_reply", "claude_clear", "claude_open"}:
        return False
    if relay is None:
        return True
    if message_type == "claude_reply":
        relay.answer(int(message.get("id", 0)), message.get("ok") is True, always=message.get("always") is True)
    elif message_type == "claude_clear":
        relay.clear_done()
    else:
        focus_window(str(message.get("name", "")))
    return True


def read_known_deck_ip() -> str | None:
    """The deck's last IP from device_state.json, for Wi-Fi probes while USB is unplugged."""
    if DEVICE_STATE_FILE is None or not DEVICE_STATE_FILE.exists():
        return None
    try:
        return json.loads(DEVICE_STATE_FILE.read_text(encoding="utf-8")).get("ip") or None
    except (OSError, ValueError):
        return None


def start_home_server_feed(config: dict[str, Any]) -> Any:
    """Home server tab (Home Assistant System Monitor sensors); None without url and token."""
    from home_server import start_home_server

    feed = start_home_server(config, log=log)
    if feed is not None:
        log("Home server tab: reading Home Assistant")
    return feed


def start_udp_hub(config: dict[str, Any]) -> Any:
    """Wi-Fi link to the deck when USB is unplugged; None when off or without a token."""
    settings = config.get("wifi_link") or {}
    token = str(notify_settings(config).get("token", ""))
    if not settings.get("enabled", True) or not token:
        return None
    from udp_link import UdpHub

    try:
        hub = UdpHub(token, port=int(settings.get("port", 4211)),
                     deck_port=int(notify_settings(config).get("udp_port", DEFAULT_NOTIFY_PORT)),
                     known_ip=read_known_deck_ip(), log=log)
    except OSError as exc:
        log(f"Wi-Fi link off: cannot open UDP port ({exc})")
        return None
    log(f"Wi-Fi link listening on UDP {hub.port}")
    return hub


WIFI_RESEND_S = 15.0  # over UDP a lost frame is replaced by this periodic full refresh


def serve_device(
    link: Any,
    kind: str,
    config: dict[str, Any],
    ctx: dict[str, Any],
    stop: Any = lambda: False,
) -> None:
    """Exchanges frames with the deck over USB (a serial port) or Wi-Fi (a UdpLink) until it fails or stop()."""
    interval = max(0.5, float(config.get("interval_seconds", 2.0)))
    disk_path, lhm, info_feed, feeds = ctx["disk_path"], ctx["lhm"], ctx["info_feed"], ctx["feeds"]
    toasts, claude, claude_usage, activity = ctx["toasts"], ctx["claude"], ctx["claude_usage"], ctx["activity"]
    usb = kind == "USB"
    hub = ctx.get("hub")

    send_json(link, macro_packet(config))
    if usb:
        send_wifi_provisioning(link, config)  # credentials travel over USB only
    last_send = 0.0
    last_full = time.monotonic()
    sent_info_version = -1
    sent_feed_versions: dict[str, int] = {}
    sent_claude_version = -1
    sent_claude_stats = -1
    sent_claude_usage = -1
    sent_activity = -1
    sent_hserver = -1
    device_wifi: tuple[str, int] | None = None
    if not usb and hub is not None and hub.deck is not None:
        device_wifi = (hub.deck[0], hub.deck_port)
        write_device_state(device_wifi)  # ARAM Helper keeps finding the deck
    receive_buffer = bytearray()

    while True:
        now = time.monotonic()
        if stop():
            return
        if not usb and now - last_full >= WIFI_RESEND_S:
            last_full = now
            sent_info_version = -1
            sent_feed_versions.clear()
            sent_claude_version = sent_claude_stats = sent_claude_usage = sent_activity = sent_hserver = -1
            send_json(link, macro_packet(config))
        if now - last_send >= interval:
            payload, ctx["previous"] = get_stats(ctx["previous"], disk_path, lhm)
            send_json(link, payload)
            last_send = now

        info_version, info_packet = info_feed.snapshot()
        if info_packet is not None and info_version != sent_info_version:
            send_json(link, info_packet)
            sent_info_version = info_version

        for feed_kind, (version, frame) in feeds.snapshot().items():
            if sent_feed_versions.get(feed_kind) != version:
                send_json(link, frame)
                sent_feed_versions[feed_kind] = version

        if claude is not None:
            claude_version, claude_frame = claude.snapshot()
            if claude_version != sent_claude_version:
                send_json(link, claude_frame)
                sent_claude_version = claude_version
            stats_version, stats_frame = claude.stats_snapshot()
            if stats_version != sent_claude_stats:
                send_json(link, stats_frame)
                sent_claude_stats = stats_version
        if claude_usage is not None:
            usage_version, usage_frame = claude_usage.snapshot()
            if usage_frame is not None and usage_version != sent_claude_usage:
                send_json(link, usage_frame)
                sent_claude_usage = usage_version
        if activity is not None:
            activity_version, activity_frame = activity.snapshot()
            if activity_frame is not None and activity_version != sent_activity:
                send_json(link, activity_frame)
                sent_activity = activity_version

        hserver = ctx.get("hserver")
        if hserver is not None:
            hserver_version, hserver_frame = hserver.snapshot()
            if hserver_frame is not None and hserver_version != sent_hserver:
                send_json(link, hserver_frame)
                sent_hserver = hserver_version

        forward_pc_toasts(toasts, link, device_wifi if usb else None, config)

        chunk = link.read(256)
        if chunk:
            receive_buffer.extend(chunk)
            while b"\n" in receive_buffer:
                raw, _, remainder = receive_buffer.partition(b"\n")
                receive_buffer = bytearray(remainder)
                text = raw.decode("utf-8", errors="replace").strip()
                if not text:
                    continue

                message = parse_frame(text)
                if message is None:
                    continue

                message_type = message.get("type")
                if handle_claude_message(claude, message):
                    pass
                elif activity is not None and activity.handle(message):
                    pass
                elif message_type == "macro":
                    execute_macro(config, int(message.get("id", 0)))
                elif message_type in {"hello", "beacon"}:
                    if message_type == "hello":
                        protocol = message.get("protocol", "?")
                        firmware = message.get("firmware", "?")
                        log(
                            f"Connected to {message.get('device', 'ESP32')} "
                            f"firmware {firmware}, protocol {protocol}"
                        )
                        if protocol != PROTOCOL_VERSION:
                            log(
                                f"Warning: bridge protocol {PROTOCOL_VERSION} "
                                f"!= device protocol {protocol}"
                            )
                    send_json(link, macro_packet(config))
                    if usb:
                        send_wifi_provisioning(link, config)
                        device_wifi = None
                    sent_info_version = -1  # device rebooted: resend everything
                    sent_feed_versions.clear()
                    sent_claude_version = -1
                    sent_claude_stats = -1
                    sent_claude_usage = -1
                    sent_activity = -1
                    sent_hserver = -1
                elif message_type == "wifi_status":
                    if message.get("connected") and message.get("ip"):
                        device_wifi = (str(message["ip"]), int(message.get("port", DEFAULT_NOTIFY_PORT)))
                        if hub is not None:
                            hub.known_ip = device_wifi[0]
                        log("Device Wi-Fi connected; PC notifications go over Wi-Fi")
                    else:
                        device_wifi = None
                        log("Device Wi-Fi not connected; PC notifications go over USB")
                    write_device_state(device_wifi)
                elif message_type == "error":
                    log(
                        f"ESP protocol error: {message.get('code', 'unknown')} "
                        f"({message.get('detail', '')})"
                    )
                elif message_type not in {"pong", "rebooting", "phone"}:
                    log(f"ESP: {message}")

        if not usb and not link.alive():
            log("Wi-Fi link to the deck lost")
            return
        time.sleep(0.01)


def find_serial_port(config: dict[str, Any], state: dict[str, Any]) -> str | None:
    configured_port = str(config.get("serial_port", "auto"))
    if configured_port and configured_port.lower() != "auto":
        # A fixed port that is unplugged counts as missing (then the Wi-Fi link can take over).
        present = {str(item.device).upper() for item in list_ports.comports()}
        return configured_port if configured_port.upper() in present else None
    ports = list(list_ports.comports())
    port = choose_serial_port(configured_port, ports)
    if not port and platform.system() != "Windows" and time.monotonic() >= state.get("next_posix_message", 0.0):
        if ports:
            log("Visible serial ports: " + ", ".join(str(item.device) for item in ports))
        else:
            log("No serial ports are currently visible.")
        log("Run the launcher with --list-ports for diagnostics.")
        state["next_posix_message"] = time.monotonic() + 10.0
    return port


def wait_for_wifi_deck(hub: Any, config: dict[str, Any], ctx: dict[str, Any], port_state: dict[str, Any]) -> None:
    """For about 2 s: look for the deck on Wi-Fi; once linked, serve it until it drops or USB comes back."""
    from udp_link import UdpLink

    link = UdpLink(hub)
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        hub.probe()
        link.pump()
        if hub.linked():
            log("Deck connected over Wi-Fi")
            next_check = [time.monotonic() + 2.0]

            def usb_back() -> bool:
                if time.monotonic() < next_check[0]:
                    return False
                next_check[0] = time.monotonic() + 2.0
                return find_serial_port(config, port_state) is not None

            serve_device(link, "Wi-Fi", config, ctx, stop=usb_back)
            return
        time.sleep(0.05)


def run(config: dict[str, Any]) -> None:
    baud = int(config.get("baud", 115200))
    info_feed = InfoFeed(config)
    info_feed.start()
    from info_feeds import InfoFeeds

    feeds = InfoFeeds(config, log=log)
    feeds.start()
    ctx: dict[str, Any] = {
        "disk_path": disk_path_from_config(config),
        "lhm": build_lhm_reader(config),
        "info_feed": info_feed,
        "feeds": feeds,
        "toasts": start_pc_notifications(config),
        "claude": start_claude_relay(config),
        "claude_usage": start_claude_usage(config),
        "activity": start_activity(config),
        "hub": start_udp_hub(config),
        "hserver": start_home_server_feed(config),
    }
    psutil.cpu_percent(interval=None)
    ctx["previous"] = current_counters()
    hub = ctx["hub"]
    port_state: dict[str, Any] = {}
    last_posix_serial_error: tuple[str, str] | None = None
    next_missing_log = 0.0

    while True:
        port = find_serial_port(config, port_state)
        if not port:
            if time.monotonic() >= next_missing_log:
                log("Control Deck USB not found" + ("; looking for it on Wi-Fi" if hub is not None else ". Retrying..."))
                next_missing_log = time.monotonic() + 60.0
            if hub is not None:
                wait_for_wifi_deck(hub, config, ctx, port_state)
            else:
                time.sleep(2.0)
            continue

        try:
            log(f"Opening {port} at {baud} baud")
            with serial.Serial(port, baud, timeout=0.05, write_timeout=1) as serial_port:
                last_posix_serial_error = None
                next_missing_log = 0.0
                time.sleep(2.0)
                serial_port.reset_input_buffer()
                serial_port.reset_output_buffer()
                serve_device(serial_port, "USB", config, ctx)
        except (serial.SerialException, OSError) as exc:
            if platform.system() == "Windows":
                log(f"Serial disconnected: {exc}")
            else:
                error_key = (port, str(exc))
                if error_key != last_posix_serial_error:
                    log(f"Serial disconnected: {exc}")
                    message = str(exc).lower()
                    if platform.system() == "Linux" and (
                        "permission denied" in message or "errno 13" in message
                    ):
                        log(
                            "Linux permission hint: check the device group with "
                            f"'ls -l {port}' and add your user to that group, commonly dialout."
                        )
                    elif platform.system() == "Darwin" and port.startswith("/dev/tty."):
                        log(
                            "macOS hint: prefer the matching /dev/cu.* port in config.json."
                        )
                    last_posix_serial_error = error_key
            time.sleep(2.0)


def main() -> None:
    parser = argparse.ArgumentParser(description="ESP Control Deck host bridge")
    parser.add_argument(
        "--config",
        default=str(Path(__file__).with_name("config.json")),
        help="Path to config.json",
    )
    parser.add_argument(
        "--list-temperatures",
        action="store_true",
        help="List temperature sensors exposed by LibreHardwareMonitor and exit",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List serial ports, show the selected Control Deck port and exit",
    )
    args = parser.parse_args()

    config_path = Path(args.config).resolve()
    if not config_path.exists():
        example = Path(__file__).with_name("config.example.json")
        print(
            f"Config not found: {config_path}\nCopy {example.name} to config.json and edit it.",
            file=sys.stderr,
        )
        raise SystemExit(2)

    config = load_config(config_path)
    if notify_settings(config).get("enabled", True):
        ensure_notify_token(config, config_path)

    global LOG_FILE, DEVICE_STATE_FILE
    log_name = str(config.get("log_file", "control-deck.log") or "").strip()
    if log_name:
        LOG_FILE = (config_path.parent / log_name).resolve()
    DEVICE_STATE_FILE = config_path.parent / "device_state.json"

    if args.list_temperatures:
        raise SystemExit(build_lhm_reader(config).print_temperature_sensors())
    if args.list_ports:
        raise SystemExit(print_serial_ports(str(config.get("serial_port", "auto"))))

    log(f"PC Control Deck Bridge {APP_VERSION} on {platform.system()} {platform.release()}")
    run(config)


if __name__ == "__main__":
    main()
