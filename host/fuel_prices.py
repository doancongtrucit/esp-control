"""Petrolimex retail fuel prices (VND per litre) for the Info > Prices tab.

Read about once an hour from webgia.com's copy of the Petrolimex price board
(the official site's certificate does not verify everywhere). The deck gets:

  {"type": "fuel", "z": 1, "t": "20:05",
   "items": [["E10 RON95 M5", 22650, -500], ["E5 RON92 M2", 20890, 0], ...]}

The third number is the change since the previous price set (prices move on
the 1st, 11th and 21st of the month), kept in host/fuel_state.json (git-ignored)
so it survives restarts. "zone" 1 or 2 in the "fuel" config picks the region.
"""
from __future__ import annotations

import html
import json
import re
import threading
import time
import urllib.request
from datetime import datetime
from pathlib import Path
from typing import Any, Callable

URL = "https://webgia.com/gia-xang-dau/petrolimex/"
REFRESH_S = 3600.0

ROW = re.compile(r"<tr>\s*<th>(?:<a[^>]*>)?([^<]+)(?:</a>)?</th>\s*<td[^>]*>([\d.]+)</td>\s*<td[^>]*>([\d.]+)</td>",
                 re.IGNORECASE)


def http_text(url: str, timeout: float = 15.0) -> str:
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 ESP-Control-Deck"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8", "replace")


def parse(page: str) -> list[tuple[str, int, int]]:
    """(product, zone 1 price, zone 2 price) for each row of the price table."""
    rows = []
    for name, zone1, zone2 in ROW.findall(page):
        try:
            rows.append((html.unescape(name).strip(), int(zone1.replace(".", "")), int(zone2.replace(".", ""))))
        except ValueError:
            continue
    return rows


def short_name(name: str) -> str:
    """"Xăng E10 RON 95 mức 5" -> "E10 RON95 M5"; fits a row on the deck."""
    if name.lower().startswith("dầu hỏa"):
        return "Dầu hỏa"
    s = re.sub(r"^(Xăng|Dầu)\s+", "", name.strip())
    s = re.sub(r"RON\s+(\d+)", r"RON\1", s)
    s = re.sub(r"\s*mức\s*(\d+)", r" M\1", s, flags=re.IGNORECASE)
    return s.strip()[:16]


class FuelFeed:
    def __init__(self, config: dict[str, Any], state_path: Path | None, http: Callable[[str], str] = http_text,
                 now: Callable[[], datetime] = datetime.now, log: Callable[[str], None] = lambda _m: None):
        cfg = dict(config.get("fuel") or {})
        self.zone = 2 if int(cfg.get("zone", 1)) == 2 else 1
        self.state_path = state_path
        self.http, self.now, self.log = http, now, log
        self.version = 0
        self.frame: dict[str, Any] | None = None
        self.on_change: Callable[[dict[str, int], dict[str, int]], None] | None = None  # new price set
        self._state = self._load()

    def _load(self) -> dict[str, Any]:
        if self.state_path is None or not self.state_path.exists():
            return {}
        try:
            return dict(json.loads(self.state_path.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            return {}

    def _save(self) -> None:
        if self.state_path is None:
            return
        try:
            self.state_path.write_text(json.dumps(self._state, ensure_ascii=False), encoding="utf-8")
        except OSError as exc:
            self.log(f"Fuel state not saved: {type(exc).__name__}")

    def refresh(self) -> None:
        try:
            rows = parse(self.http(URL))
        except Exception as exc:  # keep showing the last prices
            self.log(f"Fuel prices update failed: {type(exc).__name__}")
            return
        if not rows:
            self.log("Fuel prices: no table on the page")
            return
        prices = {name: (z1, z2)[self.zone - 1] for name, z1, z2 in rows}
        current = self._state.get("cur") or {}
        if current and current != prices:  # a new price set: the old one becomes "previous"
            self._state["prev"] = current
            if self.on_change is not None:
                self.on_change(dict(current), dict(prices))
        self._state["cur"] = prices
        self._save()
        previous = self._state.get("prev") or {}
        items = [[short_name(name), price, price - previous[name] if name in previous else 0]
                 for name, price in prices.items()]
        frame = {"type": "fuel", "z": self.zone, "t": self.now().strftime("%H:%M"), "items": items[:6]}
        if frame != self.frame:
            self.frame = frame
            self.version += 1

    def snapshot(self) -> tuple[int, dict[str, Any] | None]:
        return self.version, self.frame

    def start(self) -> None:
        def loop() -> None:
            while True:
                self.refresh()
                time.sleep(REFRESH_S)

        threading.Thread(target=loop, name="fuel", daemon=True).start()
