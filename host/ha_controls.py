"""Home screen: tiles that show and switch Home Assistant devices.

Uses the same Home Assistant as the HOME SERVER tab ("home_server" url and
token). "home_controls" in host/config.json lists the tiles (up to 12, two pages):

  "home_controls": [
    {"entity": "climate.living_room", "label": "Air Con"},
    {"entity": "fan.standing_fan", "label": "Fan", "icon": "fan"},
    {"entity": "script.fan_swing", "label": "Swing"}
  ]

Without it the bridge picks the controllable devices (climate, light, fan,
cover, media_player, switch, scene, script, button). The deck gets:

  {"type": "hactl",
   "items": [{"n": "Air Con", "i": "ac", "k": "climate", "on": 0, "v": "24C off", "c": 0},
             {"n": "PC", "i": "power", "k": "toggle", "on": 1, "v": "ON", "c": 1},
             {"n": "Swing", "i": "bolt", "k": "run", "on": -1, "v": "Run", "c": 0}],
   "ac": {"ix": 0, "on": false, "t": 24, "min": 16, "max": 30, "mode": "cool",
          "modes": ["heat_cool", "cool", "dry"], "fan": "auto", "fans": ["auto", "low", "mid", "high"]}}

and answers with {"type": "ha_press", "i": 1} or {"type": "ha_ac", "op": "temp", "d": 1}
("power", "temp" +/-1, "mode" m, "fan" m). The deck only sends tile numbers and
choices from the lists above; entity ids and services stay on the PC.
"""
from __future__ import annotations

import copy
import json
import queue
import threading
import time
import urllib.error
import urllib.request
from typing import Any, Callable

MAX_TILES = 12
POLL_S = 5.0
DOMAIN_ORDER = ["climate", "light", "fan", "cover", "media_player", "switch", "scene", "script", "button"]
RUN_DOMAINS = {"scene": "turn_on", "script": "turn_on", "button": "press"}
HOLD_DOMAINS = {"switch", "lock"}
DEFAULT_ICON = {"climate": "ac", "light": "sun", "fan": "fan", "cover": "home", "media_player": "tv",
                "switch": "power", "scene": "bolt", "script": "bolt", "button": "bolt",
                "input_boolean": "power", "automation": "bolt"}
MODE_LABEL = {"cool": "Cool", "dry": "Dry", "heat_cool": "Auto", "auto": "Auto", "heat": "Heat", "fan_only": "Fan"}


def http_post(url: str, token: str, body: dict[str, Any], timeout: float) -> Any:
    request = urllib.request.Request(url, data=json.dumps(body).encode("utf-8"), method="POST",
                                     headers={"Authorization": f"Bearer {token}", "Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8") or "null")


def _num(value: Any) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _int_or_float(value: float) -> int | float:
    return int(value) if float(value).is_integer() else round(value, 1)


class HaControls:
    def __init__(self, config: dict[str, Any], http_get: Callable | None = None,
                 http_post: Callable = http_post, log: Callable[[str], None] = lambda _m: None):
        from home_server import http_get as default_get

        ha = dict(config.get("home_server") or {})
        self.url = str(ha.get("url", "")).rstrip("/")
        self.token = str(ha.get("token", ""))
        self.configured = [dict(t) for t in (config.get("home_controls") or []) if isinstance(t, dict) and t.get("entity")]
        self.get = http_get or default_get
        self.post = http_post
        self.log = log
        self.states: dict[str, dict[str, Any]] = {}
        self.tiles: list[dict[str, str]] = []  # entity, label, icon
        self.version = 0
        self.frame: dict[str, Any] | None = None
        self._key: str | None = None
        self._queue: queue.Queue | None = None
        self._lock = threading.Lock()

    # ---- state ----

    def _pick_tiles(self) -> list[dict[str, str]]:
        if self.configured:
            tiles = self.configured
        else:
            rank = {d: i for i, d in enumerate(DOMAIN_ORDER)}
            entities = sorted((e for e in self.states if e.split(".")[0] in rank),
                              key=lambda e: (rank[e.split(".")[0]], e))
            tiles = [{"entity": e} for e in entities]
        out = []
        for t in tiles[:MAX_TILES]:
            entity = str(t["entity"])
            domain = entity.split(".")[0]
            name = (self.states.get(entity) or {}).get("attributes", {}).get("friendly_name") or entity.split(".")[-1]
            confirm = t.get("confirm")
            out.append({"entity": entity, "label": str(t.get("label") or name)[:18],
                        "icon": str(t.get("icon") or DEFAULT_ICON.get(domain, "power")),
                        # hold 1 s on the deck: switches (a PC, a heater) unless the config says otherwise
                        "confirm": bool(confirm) if confirm is not None else domain in HOLD_DOMAINS})
        return out

    def _ac_index(self) -> int | None:
        for i, t in enumerate(self.tiles):
            if t["entity"].startswith("climate.") and t["entity"] in self.states:
                return i
        return None

    def _build(self) -> dict[str, Any]:
        items = []
        for t in self.tiles:
            domain = t["entity"].split(".")[0]
            s = self.states.get(t["entity"])
            kind = "climate" if domain == "climate" else ("run" if domain in RUN_DOMAINS else "toggle")
            item = {"n": t["label"], "i": t["icon"], "k": kind, "on": -1, "v": "N/A", "c": int(t["confirm"])}
            if s is not None and s.get("state") not in ("unavailable", "unknown"):
                state = s["state"]
                if kind == "run":
                    item["v"] = "Run"
                elif kind == "climate":
                    temp = _num(s["attributes"].get("temperature"))
                    t_text = f"{_int_or_float(temp)}C " if temp is not None else ""
                    item["on"] = 0 if state == "off" else 1
                    item["v"] = t_text + ("off" if state == "off" else MODE_LABEL.get(state, state.title()))
                else:
                    on = state in ("on", "playing", "open", "paused", "idle", "home")
                    item["on"] = 1 if on else 0
                    item["v"] = "ON" if on else "OFF"
            elif s is not None and kind == "run":
                item["v"] = "Run"
            items.append(item)
        frame: dict[str, Any] = {"type": "hactl", "items": items, "ac": None}
        ix = self._ac_index()
        if ix is not None:
            a = self.states[self.tiles[ix]["entity"]]["attributes"]
            state = self.states[self.tiles[ix]["entity"]]["state"]
            modes = [m for m in a.get("hvac_modes", []) if m != "off"][:4]
            temp = _num(a.get("temperature"))
            frame["ac"] = {
                "ix": ix, "on": state != "off",
                "t": _int_or_float(temp) if temp is not None else None,
                "min": _int_or_float(_num(a.get("min_temp")) or 16), "max": _int_or_float(_num(a.get("max_temp")) or 30),
                "mode": state if state != "off" else (a.get("last_on_operation") or (modes[0] if modes else "")),
                "modes": modes, "fan": a.get("fan_mode"), "fans": list(a.get("fan_modes", []))[:5],
            }
        return frame

    def _publish(self) -> None:
        frame = self._build()
        key = json.dumps(frame, sort_keys=True)
        if key != self._key:
            self._key = key
            self.frame = frame
            self.version += 1

    def refresh(self) -> None:
        try:
            states = self.get(self.url + "/api/states", self.token, 8.0)
        except (urllib.error.URLError, OSError, TimeoutError, ValueError) as exc:
            self.log(f"Home controls: Home Assistant unreachable ({type(exc).__name__})")
            return
        with self._lock:
            self.states = {s["entity_id"]: copy.deepcopy(s) for s in states if isinstance(s, dict) and "entity_id" in s}
            self.tiles = self._pick_tiles()
            self._publish()

    def snapshot(self) -> tuple[int, dict[str, Any] | None]:
        return self.version, self.frame

    # ---- commands ----

    def _call(self, entity: str, service: str, **data: Any) -> None:
        domain = entity.split(".")[0]
        try:
            self.post(f"{self.url}/api/services/{domain}/{service}", self.token, {"entity_id": entity, **data}, 8.0)
        except (urllib.error.URLError, OSError, TimeoutError, ValueError) as exc:
            self.log(f"Home controls: {domain}.{service} failed ({type(exc).__name__})")

    def _set_state(self, entity: str, state: str | None = None, **attrs: Any) -> None:
        s = self.states.get(entity)
        if s is None:
            return
        if state is not None:
            s["state"] = state
        s["attributes"].update(attrs)
        self._publish()

    def press(self, index: int) -> None:
        if not 0 <= index < len(self.tiles):
            return
        entity = self.tiles[index]["entity"]
        domain = entity.split(".")[0]
        s = self.states.get(entity)
        if domain == "climate":
            self.ac({"op": "power"})
        elif domain in RUN_DOMAINS:
            self._call(entity, RUN_DOMAINS[domain])
        else:
            self._call(entity, "toggle")
            if s is not None:  # show it at once; the next poll confirms
                self._set_state(entity, "off" if s["state"] in ("on", "playing", "open", "paused", "idle") else "on")

    def ac(self, message: dict[str, Any]) -> None:
        ix = self._ac_index()
        if ix is None:
            return
        entity = self.tiles[ix]["entity"]
        s = self.states[entity]
        a = s["attributes"]
        op = message.get("op")
        modes = [m for m in a.get("hvac_modes", []) if m != "off"]
        if op == "power":
            if s["state"] == "off":
                mode = a.get("last_on_operation") if a.get("last_on_operation") in modes else (modes[0] if modes else None)
                if mode:
                    self._call(entity, "set_hvac_mode", hvac_mode=mode)
                    self._set_state(entity, mode)
            else:
                self._call(entity, "set_hvac_mode", hvac_mode="off")
                self._set_state(entity, "off", last_on_operation=s["state"])
        elif op == "temp":
            step = _num(a.get("target_temp_step")) or 1.0
            low, high = _num(a.get("min_temp")) or 16.0, _num(a.get("max_temp")) or 30.0
            current = _num(a.get("temperature")) or round((low + high) / 2)
            delta = int(message.get("d", 0)) if isinstance(message.get("d"), (int, float)) else 0
            target = _int_or_float(min(high, max(low, current + delta * step)))
            if delta and target != current:
                self._call(entity, "set_temperature", temperature=target)
                self._set_state(entity, temperature=target)
        elif op == "mode" and message.get("m") in modes:
            self._call(entity, "set_hvac_mode", hvac_mode=message["m"])
            self._set_state(entity, message["m"])
        elif op == "fan" and message.get("m") in a.get("fan_modes", []):
            self._call(entity, "set_fan_mode", fan_mode=message["m"])
            self._set_state(entity, fan_mode=message["m"])

    def _submit(self, fn: Callable[[], None]) -> None:
        if self._queue is None:
            fn()  # tests: run at once
        else:
            self._queue.put(fn)

    def handle(self, message: dict[str, Any]) -> bool:
        kind = message.get("type")
        if kind == "ha_press":
            index = message.get("i")
            if isinstance(index, int):
                self._submit(lambda: self.press(index))
        elif kind == "ha_ac":
            self._submit(lambda: self.ac(dict(message)))
        else:
            return False
        return True

    def start(self) -> None:
        """Polls every few seconds and runs commands in the background (HTTP never blocks the deck link)."""
        self._queue = queue.Queue()

        def worker() -> None:
            next_poll = 0.0
            while True:
                try:
                    fn = self._queue.get(timeout=max(0.0, next_poll - time.monotonic()))
                    with self._lock:
                        fn()
                    next_poll = time.monotonic() + 1.0  # confirm the change soon
                except queue.Empty:
                    self.refresh()
                    next_poll = time.monotonic() + POLL_S
                except Exception as exc:  # never stop the bridge over a device
                    self.log(f"Home controls error: {type(exc).__name__}")

        threading.Thread(target=worker, name="ha-controls", daemon=True).start()


def start_ha_controls(config: dict[str, Any], log: Callable[[str], None] = lambda _m: None) -> HaControls | None:
    ha = config.get("home_server") or {}
    if not ha.get("url") or not ha.get("token") or (config.get("home_controls_enabled") is False):
        return None
    controls = HaControls(config, log=log)
    controls.start()
    return controls
