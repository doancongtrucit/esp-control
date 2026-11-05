"""Coin, Vietnamese stock and weather feeds for the deck's Info tabs.

Each feed produces one USB frame ({"type": "coin" | "stock" | "weather", ...})
that stays well under the device's 2048-byte line limit. A failed fetch keeps
the previous frame; frames are versioned so the bridge only resends changes.
"""
from __future__ import annotations

import json
import threading
import time
import urllib.request
from datetime import datetime
from typing import Any, Callable

COIN_URL = "https://api.coingecko.com/api/v3/simple/price?ids={ids}&vs_currencies=usd&include_24hr_change=true"
INDEX_URL = "https://bgapidatafeed.vps.com.vn/getlistindexdetail/10,11,02"
STOCK_URL = "https://bgapidatafeed.vps.com.vn/getliststockdata/{tickers}"
WEATHER_URL = (
    "https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&timezone=auto&forecast_days=5"
)

COIN_NAMES = {
    "bitcoin": ("BTC", "Bitcoin"),
    "ethereum": ("ETH", "Ethereum"),
    "binancecoin": ("BNB", "BNB"),
    "solana": ("SOL", "Solana"),
    "ripple": ("XRP", "XRP"),
}
INDEX_NAMES = {"10": "VN-INDEX", "11": "VN30", "02": "HNX-INDEX"}
WEEKDAYS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
SPARK_POINTS = 40
# Minutes between refreshes.
COIN_MINUTES = 5
STOCK_MINUTES_OPEN = 2
STOCK_MINUTES_CLOSED = 30
WEATHER_MINUTES = 30


def http_json(url: str, timeout: float = 10.0) -> Any:
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 PC-Control-Deck"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def weather_icon_and_text(code: int) -> tuple[int, str]:
    """WMO code -> (icon id, text). Icons: 0 sun, 1 partly, 2 cloud, 3 rain, 4 storm, 5 fog."""
    if code == 0:
        return 0, "Clear"
    if code in (1, 2):
        return 1, "Partly cloudy"
    if code == 3:
        return 2, "Cloudy"
    if code in (45, 48):
        return 5, "Fog"
    if 51 <= code <= 57:
        return 3, "Drizzle"
    if 61 <= code <= 67:
        return 3, "Rain"
    if 80 <= code <= 82:
        return 3, "Showers"
    if code >= 95:
        return 4, "Thunderstorm"
    return 2, "Cloudy"


def pct_change(value: float, reference: float) -> float:
    return round((value - reference) / reference * 100, 2) if reference else 0.0


def coin_frame(data: dict[str, Any], ids: list[str], time_text: str) -> dict[str, Any]:
    items = []
    for coin_id in ids:
        quote = data.get(coin_id)
        if not quote or "usd" not in quote:
            continue
        symbol, name = COIN_NAMES.get(coin_id, (coin_id[:4].upper(), coin_id.title()))
        items.append({
            "s": symbol,
            "n": name[:10],
            "p": round(float(quote["usd"]), 4 if quote["usd"] < 1 else 2),
            "c": round(float(quote.get("usd_24h_change") or 0), 2),
        })
    return {"type": "coin", "items": items[:5], "time": time_text}


def stock_frame(
    index_data: list[dict[str, Any]],
    stock_data: list[dict[str, Any]],
    tickers: list[str],
    spark: list[float],
    time_text: str,
) -> dict[str, Any]:
    by_code = {str(i.get("mc")): i for i in index_data}
    index = []
    for code, name in INDEX_NAMES.items():
        row = by_code.get(code)
        if row and row.get("cIndex"):
            index.append({"n": name, "v": round(float(row["cIndex"]), 2),
                          "c": pct_change(float(row["cIndex"]), float(row.get("oIndex") or 0))})
    by_symbol = {str(s.get("sym")).upper(): s for s in stock_data}
    items = []
    for ticker in tickers:
        row = by_symbol.get(ticker.upper())
        if not row or not row.get("lastPrice"):
            continue
        # changePc from VPS has no sign; compute it from the reference price.
        items.append({"n": ticker.upper(), "p": float(row["lastPrice"]),
                      "c": pct_change(float(row["lastPrice"]), float(row.get("r") or 0))})
    vnindex = by_code.get("10") or {}
    return {"type": "stock", "index": index, "items": items[:4], "spark": downsample(spark, SPARK_POINTS),
            "open": float(vnindex.get("oIndex") or 0), "time": time_text}


def weather_frame(data: dict[str, Any], city: str, time_text: str) -> dict[str, Any]:
    current = data["current"]
    icon, desc = weather_icon_and_text(int(current["weather_code"]))
    daily = data["daily"]
    days = []
    for i in range(1, min(5, len(daily["time"]))):
        day = datetime.strptime(daily["time"][i], "%Y-%m-%d")
        day_icon, _ = weather_icon_and_text(int(daily["weather_code"][i]))
        rain = daily["precipitation_probability_max"][i]
        days.append({"d": WEEKDAYS[day.weekday()], "hi": round(daily["temperature_2m_max"][i]),
                     "lo": round(daily["temperature_2m_min"][i]),
                     "rain": -1 if rain is None else round(rain), "icon": day_icon})
    return {
        "type": "weather",
        "city": str(city)[:16],
        "t": round(float(current["temperature_2m"]), 1),
        "f": round(float(current["apparent_temperature"])),
        "h": round(float(current["relative_humidity_2m"])),
        "wind": round(float(current["wind_speed_10m"])),
        "icon": icon,
        "desc": desc,
        "days": days,
        "time": time_text,
    }


def market_open(now: datetime) -> bool:
    return now.weekday() < 5 and 9 <= now.hour < 15


def lunch_break(now: datetime) -> bool:
    """HOSE/HNX pause 11:30-13:00; the index is flat, so it is not sampled."""
    return (11, 30) <= (now.hour, now.minute) < (13, 0)


def downsample(values: list[float], points: int) -> list[float]:
    """Evenly spaced picks that keep the first and the latest value."""
    if len(values) <= points:
        return list(values)
    last = len(values) - 1
    return [values[round(i * last / (points - 1))] for i in range(points)]


class InfoFeeds:
    """Keeps the latest coin/stock/weather frames, refreshed on a background thread."""

    KINDS = ("coin", "stock", "weather")

    def __init__(
        self,
        config: dict[str, Any],
        http: Callable[..., Any] = http_json,
        now: Callable[[], datetime] = datetime.now,
        log: Callable[[str], None] = lambda message: None,
    ):
        self.coins = list(config.get("coins") or COIN_NAMES)
        self.tickers = [str(t).upper() for t in (config.get("stocks") or ["VNM", "HPG"])]
        self.weather = config.get("weather") or {"city": "Da Nang", "latitude": 16.0544, "longitude": 108.2022}
        self.http = http
        self.now = now
        self.log = log
        self.lock = threading.Lock()
        self.frames: dict[str, tuple[int, dict[str, Any]]] = {}
        self.spark: list[float] = []
        self.spark_day = ""
        self.next_due = {kind: 0.0 for kind in self.KINDS}

    def snapshot(self) -> dict[str, tuple[int, dict[str, Any]]]:
        with self.lock:
            return dict(self.frames)

    def refresh(self, kind: str) -> None:
        now = self.now()
        stamp = now.strftime("%H:%M")
        try:
            if kind == "coin":
                frame = coin_frame(self.http(COIN_URL.format(ids=",".join(self.coins))), self.coins, stamp)
                if not frame["items"]:
                    raise ValueError("empty coin response")
            elif kind == "stock":
                index = self.http(INDEX_URL)
                if not index:
                    raise ValueError("empty index response")
                stocks = self.http(STOCK_URL.format(tickers=",".join(self.tickers))) if self.tickers else []
                self._sample_spark(index, now)
                frame = stock_frame(index, stocks, self.tickers, self.spark, stamp)
            else:
                frame = weather_frame(
                    self.http(WEATHER_URL.format(lat=self.weather["latitude"], lon=self.weather["longitude"])),
                    self.weather.get("city", ""), stamp)
        except Exception as exc:
            self.log(f"{kind.title()} update failed: {type(exc).__name__}")
            return
        with self.lock:
            version = self.frames.get(kind, (0, None))[0]
            self.frames[kind] = (version + 1, frame)
        self.log(f"{kind.title()} feed updated")

    def _sample_spark(self, index: list[dict[str, Any]], now: datetime) -> None:
        day = now.strftime("%Y-%m-%d")
        if day != self.spark_day:
            self.spark, self.spark_day = [], day
        vnindex = next((i for i in index if str(i.get("mc")) == "10"), None)
        if vnindex and vnindex.get("cIndex") and market_open(now) and not lunch_break(now):
            self.spark.append(round(float(vnindex["cIndex"]), 2))

    def _minutes(self, kind: str) -> float:
        if kind == "coin":
            return COIN_MINUTES
        if kind == "stock":
            return STOCK_MINUTES_OPEN if market_open(self.now()) else STOCK_MINUTES_CLOSED
        return WEATHER_MINUTES

    def start(self) -> None:
        threading.Thread(target=self._loop, name="info-feeds", daemon=True).start()

    def _loop(self) -> None:
        while True:
            for kind in self.KINDS:
                if time.monotonic() >= self.next_due[kind]:
                    self.refresh(kind)
                    self.next_due[kind] = time.monotonic() + self._minutes(kind) * 60
            time.sleep(5)
