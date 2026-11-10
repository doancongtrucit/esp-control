"""Home server tab: CPU, RAM, disk, temperature, network and uptime from Home Assistant.

The bridge reads the System Monitor integration's sensors through Home
Assistant's REST API (GET /api/states, /api/config) with a long-lived access
token kept in the git-ignored host/config.json:

  "home_server": {"url": "http://<server>:8123", "token": "<long-lived token>", "name": "Home Server"}

Optional "entities" maps cpu / ram / disk / temp / load / dn / up / boot /
battery to entity ids when the defaults do not match. The deck gets:

  {"type": "hserver", "name": "Home Server", "on": true, "ms": 23, "ver": "2026.11.1",
   "cpu": 12.5, "ram": 43.2, "disk": 61.0, "temp": 48.0, "load": 0.42,
   "dn": 4.0, "up": 1.0, "upt": 273600, "upd": 2, "bat": null, "t": "12:00"}
  {"type": "hserver", "name": "Home Server", "on": false, "err": "offline" | "token" | "error", "t": "12:00"}

dn / up are Mbit/s, upt seconds since boot, upd the number of pending updates.
The token and the server address never reach the deck or the log.
"""
from __future__ import annotations

import json
import re
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime
from typing import Any, Callable

REFRESH_S = 30.0

# Entity id endings of the System Monitor integration (new ids start with
# "sensor.system_monitor_", older installs drop the prefix).
PATTERNS = {
    "cpu": [r"processor_use(_percent)?$"],
    "ram": [r"memory_usage$", r"memory_use_percent$"],
    "disk": [r"disk_usage$", r"disk_use_percent$", r"disk_usage_.+", r"disk_use_percent_.+"],
    "temp": [r"processor_temperature$"],
    "load": [r"load_1m$", r"load_1_?min$", r"load_1m_.+"],
    "boot": [r"last_boot$"],
    "battery": [r"battery$"],
}
VIRTUAL_IFACES = re.compile(r"(lo|docker\d*|veth\w*|br-\w+|hassio|virbr\d*|tailscale\d*|wg\d*)$")
TO_MBIT = {"MB/s": 8.0, "MiB/s": 8.388608, "kB/s": 0.008, "KiB/s": 0.008192, "B/s": 8e-6,
           "Mbit/s": 1.0, "kbit/s": 0.001, "bit/s": 1e-6}


def _number(item: dict[str, Any] | None) -> float | None:
    if item is None:
        return None
    try:
        return float(item["state"])
    except (KeyError, TypeError, ValueError):
        return None  # "unavailable", "unknown"


def _find(states: dict[str, dict[str, Any]], key: str, entities: dict[str, str]) -> dict[str, Any] | None:
    if entities.get(key):
        return states.get(entities[key])
    for pattern in PATTERNS.get(key, []):
        rx = re.compile(r"^sensor\.(system_monitor_)?" + pattern)
        for entity_id in sorted(states):
            if rx.match(entity_id):
                return states[entity_id]
    return None


def _throughput(states: dict[str, dict[str, Any]], direction: str, override: str | None) -> float | None:
    if override:
        candidates = [states.get(override)]
    else:
        rx = re.compile(r"^sensor\.(system_monitor_)?network_throughput_" + direction + r"_(.+)$")
        candidates = [states[e] for e in sorted(states)
                      if (m := rx.match(e)) and not VIRTUAL_IFACES.match(m.group(2))]
    for item in candidates:
        value = _number(item)
        if value is not None:
            unit = (item.get("attributes") or {}).get("unit_of_measurement", "MB/s")
            return round(value * TO_MBIT.get(unit, 8.0), 2)
    return None


TO_BYTES = {"B": 1, "kB": 1e3, "KiB": 1024, "MB": 1e6, "MiB": 1048576, "GB": 1e9, "GiB": 1073741824}


def byte_counters(states_list: list[dict[str, Any]]) -> tuple[float, float] | None:
    """Total bytes received and sent on the real interfaces (System Monitor "Network in/out")."""
    totals = {"in": 0.0, "out": 0.0}
    found = {"in": False, "out": False}
    for s in states_list:
        m = re.match(r"^sensor\.(system_monitor_)?network_(in|out)_(.+)$", str(s.get("entity_id", "")))
        if not m or VIRTUAL_IFACES.match(m.group(3)):
            continue
        value = _number(s)
        if value is None:
            continue
        unit = (s.get("attributes") or {}).get("unit_of_measurement", "MiB")
        totals[m.group(2)] += value * TO_BYTES.get(unit, 1048576)
        found[m.group(2)] = True
    return (totals["in"], totals["out"]) if found["in"] and found["out"] else None


def metrics(states_list: list[dict[str, Any]], now: datetime, entities: dict[str, str] | None = None) -> dict[str, Any]:
    entities = entities or {}
    states = {s.get("entity_id", ""): s for s in states_list if isinstance(s, dict)}
    out: dict[str, Any] = {k: _number(_find(states, k, entities)) for k in ("cpu", "ram", "disk", "temp", "load")}
    out["dn"] = _throughput(states, "in", entities.get("dn"))
    out["up"] = _throughput(states, "out", entities.get("up"))
    boot = _find(states, "boot", entities)
    out["upt"] = None
    if boot is not None:
        try:
            out["upt"] = max(0, int((now - datetime.fromisoformat(str(boot["state"]))).total_seconds()))
        except (ValueError, TypeError):
            pass
    out["upd"] = sum(1 for e, s in states.items() if e.startswith("update.") and s.get("state") == "on")
    out["bat"] = _number(_find(states, "battery", entities))
    return out


def http_get(url: str, token: str, timeout: float) -> Any:
    request = urllib.request.Request(url, headers={"Authorization": f"Bearer {token}",
                                                   "Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


class HomeServerFeed:
    def __init__(self, config: dict[str, Any], http: Callable[[str, str, float], Any] = http_get,
                 clock: Callable[[], datetime] = lambda: datetime.now().astimezone(),
                 mono: Callable[[], float] = time.monotonic, log: Callable[[str], None] = lambda _m: None):
        cfg = dict(config.get("home_server") or {})
        self.url = str(cfg.get("url", "")).rstrip("/")
        self.token = str(cfg.get("token", ""))
        self.name = str(cfg.get("name") or "Home Server")[:24]
        self.entities = {k: str(v) for k, v in dict(cfg.get("entities") or {}).items()}
        self.refresh_s = max(10.0, float(cfg.get("refresh_seconds", REFRESH_S)))
        self.http, self.clock, self.mono, self.log = http, clock, mono, log
        self.version = 0
        self.frame: dict[str, Any] | None = None
        self._key: str | None = None
        self._last_error: str | None = None
        self._counters: tuple[float, float, float] | None = None  # (mono time, bytes in, bytes out)

    def _fetch(self) -> dict[str, Any]:
        now = self.clock()
        base = {"type": "hserver", "name": self.name}
        try:
            started = self.mono()
            states = self.http(self.url + "/api/states", self.token, 8.0)
            ms = int(round((self.mono() - started) * 1000))
            config = self.http(self.url + "/api/config", self.token, 8.0)
        except urllib.error.HTTPError as exc:
            return {**base, "on": False, "err": "token" if exc.code in (401, 403) else "error", "t": now.strftime("%H:%M")}
        except (urllib.error.URLError, OSError, TimeoutError):
            return {**base, "on": False, "err": "offline", "t": now.strftime("%H:%M")}
        except ValueError:
            return {**base, "on": False, "err": "error", "t": now.strftime("%H:%M")}
        states = states if isinstance(states, list) else []
        frame = {**base, "on": True, "ms": ms, "ver": str((config or {}).get("version", ""))[:16],
                 **metrics(states, now, self.entities), "t": now.strftime("%H:%M")}
        # Throughput sensors can stay "unknown"; then rate the byte counters between two readings.
        counters = byte_counters(states)
        previous, self._counters = self._counters, (started, *counters) if counters else None
        if counters and previous and started > previous[0]:
            dt = started - previous[0]
            if frame["dn"] is None:
                frame["dn"] = round(max(0.0, counters[0] - previous[1]) * 8 / 1e6 / dt, 2)
            if frame["up"] is None:
                frame["up"] = round(max(0.0, counters[1] - previous[2]) * 8 / 1e6 / dt, 2)
        return frame

    def refresh(self) -> None:
        frame = self._fetch()
        if not frame["on"] and frame["err"] != self._last_error:
            self.log(f"Home server: {frame['err']}")  # never the URL or the token
        self._last_error = None if frame["on"] else frame["err"]
        key = json.dumps({**frame, "t": None, "ms": frame.get("ms", 0) // 50,
                          "upt": (frame.get("upt") or 0) // 3600}, sort_keys=True)
        if key != self._key:
            self._key = key
            self.frame = frame
            self.version += 1

    def snapshot(self) -> tuple[int, dict[str, Any] | None]:
        return self.version, self.frame

    def start(self) -> None:
        def loop() -> None:
            while True:
                try:
                    self.refresh()
                except Exception as exc:  # never stop the bridge over the home server
                    self.log(f"Home server update failed: {type(exc).__name__}")
                time.sleep(self.refresh_s)

        threading.Thread(target=loop, name="home-server", daemon=True).start()


def start_home_server(config: dict[str, Any], log: Callable[[str], None] = lambda _m: None) -> HomeServerFeed | None:
    cfg = config.get("home_server") or {}
    if not cfg.get("enabled", True) or not cfg.get("url") or not cfg.get("token"):
        return None
    feed = HomeServerFeed(config, log=log)
    feed.start()
    return feed
