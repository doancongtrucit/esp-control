"""Claude Code token usage and estimated API cost, read from local transcripts.

Claude Code writes one JSONL file per session under ~/.claude/projects. Each
assistant line carries message.usage; only those numbers, the model, the time
and the project folder name are kept here (never message text). The deck gets:

  {"type": "cusage", "t": "22:10",
   "day": {"in": 120000, "out": 45000, "cw": 300000, "cr": 9000000, "usd": 12.34},
   "top": [["alpha", 8.1], ["beta", 4.2]],                    up to 3 projects today, USD
   "blk": {"usd": 6.0, "pct": 43, "left": 7200, "start": "14:00"} or null,
   "h": [0, 0, ..., 12, 30, ...]}                              assistant messages per hour today

Costs use Anthropic's first-party API rates (USD per million tokens). On a
Pro/Max plan nothing is billed per token: the number shows how heavy the use
is. "blk" is the current 5-hour window; pct compares it with the biggest
window in the history read, because plan limits are not published as tokens.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable

BLOCK_S = 5 * 3600
HISTORY_DAYS = 30

# USD per million tokens: input, output, and overrides of the default cache
# multipliers (5-minute write 1.25x input, 1-hour write 2x, read 0.1x).
PRICES: dict[str, dict[str, float]] = {
    "claude-fable-5-1": {"in": 10, "out": 50, "cr": 0.25},
    "claude-mythos-5-1": {"in": 10, "out": 50, "cr": 0.25},
    "claude-fable-5": {"in": 10, "out": 50},
    "claude-mythos-5": {"in": 10, "out": 50},
    "claude-opus-5-5": {"in": 4, "out": 20, "cr": 0.20},
    "claude-opus-5": {"in": 5, "out": 25},
    "claude-opus-4-8": {"in": 5, "out": 25},
    "claude-opus-4-7": {"in": 5, "out": 25},
    "claude-opus-4-6": {"in": 5, "out": 25},
    "claude-opus-4-5": {"in": 5, "out": 25},
    "claude-sonnet-5-5": {"in": 2, "out": 10},
    "claude-sonnet-5": {"in": 2, "out": 10},
    "claude-sonnet-4-6": {"in": 3, "out": 15},
    "claude-sonnet-4-5": {"in": 3, "out": 15},
    "claude-haiku-5-5": {"in": 0.10, "out": 0.50},
    "claude-haiku-4-5": {"in": 1, "out": 5},
}


@dataclass
class Entry:
    ts: float
    model: str
    project: str
    inp: int = 0
    out: int = 0
    cw5: int = 0
    cw1h: int = 0
    cr: int = 0


@dataclass
class Block:
    start: float
    last: float
    usd: float


def price(model: str) -> dict[str, float] | None:
    """Rates for a model id; tolerates suffixes such as "[1m]" or a date."""
    base = re.sub(r"\[.*\]$", "", model or "")
    for key in sorted(PRICES, key=len, reverse=True):
        if base == key or base.startswith(key + "-"):
            p = PRICES[key]
            return {"in": p["in"], "out": p["out"], "cw5": p.get("cw5", p["in"] * 1.25),
                    "cw1h": p.get("cw1h", p["in"] * 2), "cr": p.get("cr", p["in"] * 0.1)}
    return None


def cost(e: Entry) -> float:
    p = price(e.model)
    if p is None:
        return 0.0
    return (e.inp * p["in"] + e.out * p["out"] + e.cw5 * p["cw5"] + e.cw1h * p["cw1h"] + e.cr * p["cr"]) / 1e6


def _project(cwd: Any) -> str:
    return re.split(r"[\\/]", str(cwd).rstrip("\\/"))[-1] if cwd else "?"


def parse_line(text: str) -> tuple[tuple[str, str], Entry] | None:
    try:
        d = json.loads(text)
    except ValueError:
        return None
    if not isinstance(d, dict) or d.get("type") != "assistant":
        return None
    msg = d.get("message")
    usage = msg.get("usage") if isinstance(msg, dict) else None
    if not isinstance(usage, dict):
        return None
    try:
        ts = datetime.fromisoformat(str(d["timestamp"]).replace("Z", "+00:00")).timestamp()
    except (KeyError, ValueError):
        return None
    created = usage.get("cache_creation") if isinstance(usage.get("cache_creation"), dict) else {}
    cw1h = int(created.get("ephemeral_1h_input_tokens") or 0)
    cw5 = int(created.get("ephemeral_5m_input_tokens") or 0)
    if not created:
        cw5 = int(usage.get("cache_creation_input_tokens") or 0)
    entry = Entry(ts, str(msg.get("model") or ""), _project(d.get("cwd")),
                  inp=int(usage.get("input_tokens") or 0), out=int(usage.get("output_tokens") or 0),
                  cw5=cw5, cw1h=cw1h, cr=int(usage.get("cache_read_input_tokens") or 0))
    return (str(msg.get("id") or ""), str(d.get("requestId") or "")), entry


class UsageReader:
    """Reads transcripts, re-parsing only files whose size or time changed."""

    def __init__(self, root: Path, history_days: int = HISTORY_DAYS):
        self.root = root
        self.history_days = history_days
        self.cache: dict[Path, tuple[tuple[float, int], list[tuple[tuple[str, str], Entry]]]] = {}

    def entries(self, now: float | None = None) -> list[Entry]:
        now = now if now is not None else datetime.now().timestamp()
        oldest = now - self.history_days * 86400
        seen: set[tuple[str, str]] = set()
        out: list[Entry] = []
        live: set[Path] = set()
        for path in self.root.glob("*/*.jsonl") if self.root.exists() else []:
            try:
                stat = path.stat()
            except OSError:
                continue
            if stat.st_mtime < oldest:
                continue
            live.add(path)
            key = (stat.st_mtime, stat.st_size)
            cached = self.cache.get(path)
            if cached is None or cached[0] != key:
                rows = []
                with path.open(encoding="utf-8", errors="replace") as handle:
                    for text in handle:
                        parsed = parse_line(text)
                        if parsed:
                            rows.append(parsed)
                self.cache[path] = cached = (key, rows)
            for ident, entry in cached[1]:
                if ident != ("", "") and ident in seen:
                    continue  # streamed responses repeat the same message
                seen.add(ident)
                if entry.ts >= oldest:
                    out.append(entry)
        for gone in set(self.cache) - live:
            del self.cache[gone]
        return out


def read_entries(root: Path) -> list[Entry]:
    return UsageReader(root, history_days=36500).entries()


def blocks(entries: Iterable[Entry]) -> list[Block]:
    """5-hour windows: each starts on the hour of its first message."""
    result: list[Block] = []
    for e in sorted(entries, key=lambda x: x.ts):
        current = result[-1] if result else None
        if current is None or e.ts >= current.start + BLOCK_S or e.ts - current.last > BLOCK_S:
            start = e.ts - (e.ts % 3600)
            current = Block(start, e.ts, 0.0)
            result.append(current)
        current.last = e.ts
        current.usd += cost(e)
    return result


def report(entries: list[Entry], now: datetime) -> dict[str, Any]:
    today = now.date()
    day = {"in": 0, "out": 0, "cw": 0, "cr": 0, "usd": 0.0}
    per_project: dict[str, float] = {}
    hours = [0] * 24
    for e in entries:
        when = datetime.fromtimestamp(e.ts)
        if when.date() != today:
            continue
        usd = cost(e)
        day["in"] += e.inp
        day["out"] += e.out
        day["cw"] += e.cw5 + e.cw1h
        day["cr"] += e.cr
        day["usd"] += usd
        per_project[e.project] = per_project.get(e.project, 0.0) + usd
        hours[when.hour] += 1
    day["usd"] = round(day["usd"], 2)
    top = sorted(per_project.items(), key=lambda kv: -kv[1])[:3]

    blk = None
    all_blocks = blocks(entries)
    now_ts = now.timestamp()
    if all_blocks and now_ts < all_blocks[-1].start + BLOCK_S:
        current = all_blocks[-1]
        biggest = max(b.usd for b in all_blocks) or 1.0
        blk = {"usd": round(current.usd, 2), "pct": int(round(100 * current.usd / biggest)),
               "left": int(current.start + BLOCK_S - now_ts),
               "start": datetime.fromtimestamp(current.start).strftime("%H:%M")}
    return {"type": "cusage", "t": now.strftime("%H:%M"), "day": day,
            "top": [[name, round(usd, 2)] for name, usd in top if usd > 0], "blk": blk, "h": hours}


class UsageFeed:
    """Background refresh for the bridge: snapshot() -> (version, frame); version moves on real changes."""

    def __init__(self, root: Path, refresh_seconds: float = 60.0, clock=datetime.now, log=lambda _m: None):
        self.reader = UsageReader(root)
        self.refresh_seconds = refresh_seconds
        self.clock = clock
        self.log = log
        self.version = 0
        self.frame: dict[str, Any] | None = None
        self._key: str | None = None

    def refresh(self) -> None:
        now = self.clock()
        try:
            frame = report(self.reader.entries(now.timestamp()), now)
        except Exception as exc:  # a half-written transcript must not stop the bridge
            self.log(f"Claude usage update failed: {exc}")
            return
        key = json.dumps({**frame, "t": None, "blk": frame["blk"] and {**frame["blk"], "left": frame["blk"]["left"] // 600}},
                         sort_keys=True)
        if key != self._key:
            self._key = key
            self.frame = frame
            self.version += 1

    def snapshot(self) -> tuple[int, dict[str, Any] | None]:
        return self.version, self.frame

    def start(self) -> None:
        import threading
        import time

        def loop() -> None:
            while True:
                self.refresh()
                time.sleep(self.refresh_seconds)

        threading.Thread(target=loop, name="claude-usage", daemon=True).start()
