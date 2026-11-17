"""Spoken announcements on the deck's speaker (Vietnamese text-to-speech, see speech.py).

- Claude Code: names the session that waits for a permission answer, finished, or keeps failing.
- PC: CPU or GPU too hot, RAM nearly full (sustained, with a cooldown).
- Fuel: a new Petrolimex price set, per product family.
- Meetings: a few minutes before and at the start (calendar feeds, ics_calendar.py).
- Morning brief: greeting, date and lunar date, weather, today's meetings, fuel price;
  on a tap on the deck, or every day at "brief.auto_at".

The deck plays clips of at most 6 seconds, so long text is split into sentences,
each its own clip; the bridge waits for one clip to finish before sending the next.
Config (host/config.json, all optional):
  "voice_alerts": {"claude": true, "pc": true, "fuel": true, "meetings": true,
                   "remind_minutes": 5, "speak_titles": true,
                   "cpu_temp": 90, "gpu_temp": 85, "ram": 90},
  "brief": {"auto_at": "07:30"}
"""
from __future__ import annotations

import itertools
import math
import queue
import re
import threading
import time
from datetime import datetime, timedelta
from typing import Any, Callable, Iterable

from lunar_vi import solar_to_lunar
from notify_api import frames_for

CLIP_CHARS = 80  # about 6 s of Vietnamese speech
SUSTAIN_S = 20.0  # a PC alert must hold this long
COOLDOWN_S = 15 * 60.0
HYSTERESIS = 5.0
DONE_REPEAT_S = 60.0
START_GRACE_S = 120.0

WEEKDAYS_VI = ["thứ Hai", "thứ Ba", "thứ Tư", "thứ Năm", "thứ Sáu", "thứ Bảy", "Chủ nhật"]
WEATHER_VI = {"Clear": "trời quang", "Partly cloudy": "có mây", "Cloudy": "nhiều mây", "Fog": "có sương mù",
              "Drizzle": "mưa phùn", "Rain": "có mưa", "Showers": "có mưa rào", "Thunderstorm": "có giông"}
CITY_VI = {"da nang": "Đà Nẵng", "danang": "Đà Nẵng", "ha noi": "Hà Nội", "hanoi": "Hà Nội",
           "ho chi minh": "Hồ Chí Minh", "ho chi minh city": "Hồ Chí Minh", "saigon": "Sài Gòn",
           "hue": "Huế", "hoi an": "Hội An", "nha trang": "Nha Trang", "da lat": "Đà Lạt", "hai phong": "Hải Phòng"}


def split_speech(text: str, limit: int = CLIP_CHARS) -> list[str]:
    """Sentences of at most `limit` characters (long ones split at commas, then at spaces)."""
    parts: list[str] = []
    for sentence in re.split(r"(?<=[.!?])\s+", text.strip()):
        pieces = [sentence]
        if len(sentence) > limit:
            pieces = [p for p in re.split(r"(?<=,)\s+", sentence)]
        for piece in pieces:
            while len(piece) > limit:
                cut = piece.rfind(" ", 0, limit)
                cut = cut if cut > 0 else limit
                parts.append(piece[:cut].strip())
                piece = piece[cut:].strip()
            if piece:
                parts.append(piece)
    return [p for p in parts if p]


def spoken_name(name: str) -> str:
    return re.sub(r"[-_.]+", " ", str(name)).strip()


def thousand_dong(price: int) -> str:
    price = int(price)
    if price >= 1000:
        return f"{price // 1000} nghìn {price % 1000} đồng" if price % 1000 else f"{price // 1000} nghìn đồng"
    return f"{price} đồng"


def clock_text(moment: datetime) -> str:
    return f"{moment.hour} giờ {moment.minute}" if moment.minute else f"{moment.hour} giờ"


def lunar_text(day: datetime) -> str:
    ld, lm, _ly, leap = solar_to_lunar(day.day, day.month, day.year)
    month = {1: "tháng Giêng", 12: "tháng Chạp"}.get(lm, f"tháng {lm}")
    return f"âm lịch ngày {ld} {month}{' nhuận' if leap else ''}"


def fuel_family(name: str) -> tuple[str, str]:
    """(spoken name, short label) of a Petrolimex product, one per family."""
    base = re.sub(r"\s*mức\s*\d+", "", name, flags=re.IGNORECASE).strip()
    low = base.lower()
    if low.startswith("dầu do"):
        return "Dầu diesel", "DO"
    if low.startswith("dầu hỏa"):
        return "Dầu hỏa", "Dầu hỏa"
    short = re.sub(r"^(Xăng|Dầu)\s+", "", base)
    return base, re.sub(r"RON\s+(\d+)", r"RON\1", short)


def brief_text(now: datetime, weather: dict[str, Any] | None, fuel: dict[str, Any] | None,
               meetings: list[Any] | None, speak_titles: bool = True) -> str:
    greeting = ("Chào buổi sáng" if now.hour < 11 else "Chào buổi trưa" if now.hour < 13
                else "Chào buổi chiều" if now.hour < 18 else "Chào buổi tối")
    out = [f"{greeting}.",
           f"Hôm nay {WEEKDAYS_VI[now.weekday()]}, ngày {now.day} tháng {now.month}, {lunar_text(now)}."]
    if weather and weather.get("t") is not None:
        city = str(weather.get("city") or "")
        city = CITY_VI.get(city.lower().strip(), city)
        desc = WEATHER_VI.get(str(weather.get("desc") or ""), "")
        line = f"{city} {round(float(weather['t']))} độ".strip()
        out.append(f"{line}, {desc}." if desc else f"{line}.")
    if meetings is not None:
        today = [m for m in meetings if m.start.date() == now.date()]
        upcoming = [m for m in today if m.start >= now]
        started = len(today) > len(upcoming)
        if not upcoming:
            out.append("Hôm nay không còn cuộc họp nào." if started else "Hôm nay không có cuộc họp nào.")
        else:
            first = upcoming[0]
            verb, which = ("còn", "cuộc tiếp theo") if started else ("có", "cuộc đầu tiên")
            line = f"Hôm nay {verb} {len(upcoming)} cuộc họp, {which} lúc {clock_text(first.start)}"
            out.append(f"{line}: {first.title}." if speak_titles and first.title else f"{line}.")
    if fuel and fuel.get("items"):
        items = fuel["items"]
        pick = next((i for i in items if "RON95" in str(i[0]).replace(" ", "")), items[0])
        name = "Xăng RON 95" if "RON95" in str(pick[0]).replace(" ", "") else str(pick[0])
        line = f"{name} giá {thousand_dong(pick[1])}"
        change = int(pick[2]) if len(pick) > 2 else 0
        if change:
            line += f", {'tăng' if change > 0 else 'giảm'} {abs(change)} đồng"
        out.append(line + ".")
    return " ".join(out)


class Announcer:
    def __init__(self, config: dict[str, Any], deliver: Callable[[list[dict[str, Any]]], None],
                 speak: Callable[[str], bytes] | None, now: Callable[[], datetime] = datetime.now,
                 log: Callable[[str], None] = lambda _m: None, inline: bool = False):
        cfg = dict(config.get("voice_alerts") or {})
        self.on = {k: bool(cfg.get(k, True)) for k in ("claude", "pc", "fuel", "meetings")}
        self.remind_s = max(1, int(cfg.get("remind_minutes", 5))) * 60
        self.speak_titles = bool(cfg.get("speak_titles", True))
        self.limits = {"cpu_temp": float(cfg.get("cpu_temp", 90)), "ram": float(cfg.get("ram", 90)),
                       "gpu_temp": float(cfg.get("gpu_temp", 85))}
        self.auto_at = str((config.get("brief") or {}).get("auto_at") or "").strip()
        self.deliver, self.speak, self.now, self.log, self.inline = deliver, speak, now, log, inline
        self.spoken: list[str] = []
        self._ids = itertools.count(100000)  # apart from the notify API's clip ids
        self._queue: "queue.Queue[dict[str, Any]]" = queue.Queue()
        self._claude: dict[str, tuple[str, int]] | None = None
        self._ask_id = 0
        self._done_at: dict[str, datetime] = {}
        self._over_since: dict[str, datetime | None] = {}
        self._armed: dict[str, bool] = {}
        self._alert_at: dict[str, datetime] = {}
        self._meeting_done: set[tuple[str, str, str]] = set()
        self._brief_day = None
        if not inline:
            threading.Thread(target=self._work, name="announcer", daemon=True).start()

    # ---------- output ----------
    def say(self, text: str, sound: str = "", title: str = "", body: str = "", app: str = "Deck",
            urgent: bool = False) -> None:
        parts = split_speech(text) or [""]
        for index, part in enumerate(parts):
            message: dict[str, Any] = {"say": part, "urgent": urgent}
            if index == 0:
                message.update({"sound": sound, "title": title, "body": body, "app": app})
            if self.inline:
                self._send(message)
            else:
                self._queue.put(message)

    def _send(self, message: dict[str, Any]) -> None:
        if message.get("say"):
            self.spoken.append(message["say"])
        frames = frames_for(message, self.speak, lambda: next(self._ids))
        if frames:
            self.deliver(frames)

    def _work(self) -> None:
        while True:
            message = self._queue.get()
            try:
                self._send(message)
            except Exception as exc:  # one bad clip must not stop the announcer
                self.log(f"Announcer: {type(exc).__name__}")

    # ---------- Claude Code ----------
    def on_claude(self, frame: dict[str, Any] | None) -> None:
        if not frame or frame.get("type") != "claude":
            return
        rows = {str(r[0]): (str(r[1]), int(r[4]) if len(r) > 4 else 0) for r in frame.get("s") or [] if r}
        ask = frame.get("ask") or {}
        ask_id = int(ask.get("id") or 0)
        previous, self._claude = self._claude, rows
        old_ask, self._ask_id = self._ask_id, ask_id
        if previous is None or not self.on["claude"]:
            return
        if ask_id and ask_id != old_ask:
            tool = str(ask.get("tool") or "")
            text = f"Phiên {spoken_name(ask.get('name') or '')} đang chờ bạn duyệt"
            self.say(f"{text} lệnh {tool}" if tool else text)
        now = self.now()
        for name, (state, fails) in rows.items():
            before = previous.get(name)
            if before is None:
                continue
            if before[0] == "r" and state == "d":
                last = self._done_at.get(name)
                if last is None or (now - last).total_seconds() >= DONE_REPEAT_S:
                    self._done_at[name] = now
                    self.say(f"Phiên {spoken_name(name)} đã xong")
            if before[1] < 3 <= fails:
                self.say(f"Phiên {spoken_name(name)} lỗi {fails} lần liên tiếp")

    # ---------- PC ----------
    def on_stats(self, payload: dict[str, Any]) -> None:
        if not self.on["pc"]:
            return
        now = self.now()
        for key in ("cpu_temp", "ram", "gpu_temp"):
            value = payload.get(key)
            if value is None:
                self._over_since[key] = None
                continue
            value = float(value)
            limit = self.limits[key]
            if value < limit - HYSTERESIS:
                self._armed[key] = True
            if value < limit:
                self._over_since[key] = None
                continue
            since = self._over_since.get(key)
            if since is None:
                self._over_since[key] = now
                continue
            if (now - since).total_seconds() < SUSTAIN_S or not self._armed.get(key, True):
                continue
            last = self._alert_at.get(key)
            if last is not None and (now - last).total_seconds() < COOLDOWN_S:
                continue
            self._armed[key] = False
            self._alert_at[key] = now
            shown = round(value)
            if key == "ram":
                self.say(f"RAM đã dùng {shown} phần trăm", sound="alert", title="RAM cao", body=f"RAM {shown}%",
                         app="PC")
            else:
                part = "CPU" if key == "cpu_temp" else "GPU"
                self.say(f"{part} đang {shown} độ", sound="alert", title="PC nóng", body=f"{part} {shown}°C",
                         app="PC")

    # ---------- fuel ----------
    def on_fuel_change(self, previous: dict[str, int], current: dict[str, int]) -> None:
        if not self.on["fuel"]:
            return
        seen: set[str] = set()
        spoken, labels = [], []
        for name, price in current.items():
            spoken_label, short = fuel_family(name)
            if spoken_label in seen or name not in previous:
                continue
            seen.add(spoken_label)
            change = int(price) - int(previous[name])
            if not change:
                continue
            spoken.append(f"{spoken_label} {'tăng' if change > 0 else 'giảm'} {abs(change)} đồng.")
            labels.append(f"{short} {change:+d}")
        if not spoken:
            return
        self.say("Giá xăng dầu vừa điều chỉnh. " + " ".join(spoken[:3]), sound="chime", title="Giá xăng dầu",
                 body=" · ".join(labels[:3]), app="Petrolimex")

    # ---------- meetings ----------
    def check_meetings(self, meetings: Iterable[Any]) -> None:
        if not self.on["meetings"]:
            return
        now = self.now()
        for m in meetings:
            delta = (m.start - now).total_seconds()
            stamp = m.start.isoformat()
            title = m.title if self.speak_titles else ""
            if 0 < delta <= self.remind_s and (m.uid, stamp, "soon") not in self._meeting_done:
                self._meeting_done.add((m.uid, stamp, "soon"))
                minutes = max(1, math.ceil(delta / 60))
                text = f"{minutes} phút nữa họp {title}" if title else f"{minutes} phút nữa có cuộc họp"
                self.say(text, sound="reminder", title=f"Họp lúc {m.start:%H:%M}", body=title, app="Calendar")
            elif -START_GRACE_S <= delta <= 0 and (m.uid, stamp, "start") not in self._meeting_done:
                self._meeting_done.add((m.uid, stamp, "start"))
                self._meeting_done.add((m.uid, stamp, "soon"))
                self.say(f"Cuộc họp {title} đã bắt đầu" if title else "Cuộc họp đã bắt đầu", sound="chime")
        if len(self._meeting_done) > 500:
            self._meeting_done.clear()

    # ---------- morning brief ----------
    def brief(self, weather: dict[str, Any] | None, fuel: dict[str, Any] | None,
              meetings: list[Any] | None) -> None:
        self.say(brief_text(self.now(), weather, fuel, meetings, self.speak_titles))

    def auto_brief(self, sources: Callable[[], tuple[Any, Any, Any]]) -> None:
        if not self.auto_at:
            return
        try:
            hour, minute = (int(x) for x in self.auto_at.split(":", 1))
        except ValueError:
            return
        now = self.now()
        at = now.replace(hour=hour, minute=minute, second=0, microsecond=0)
        if self._brief_day == now.date() or not (at <= now < at + timedelta(hours=1)):
            return
        self._brief_day = now.date()
        self.brief(*sources())

    # ---------- polling ----------
    def start_polling(self, claude: Callable[[], Any], meetings: Callable[[], Any],
                      brief_sources: Callable[[], tuple[Any, Any, Any]]) -> None:
        def loop() -> None:
            while True:
                try:
                    self.on_claude(claude())
                    found = meetings()
                    if found is not None:
                        self.check_meetings(found)
                    self.auto_brief(brief_sources)
                except Exception as exc:
                    self.log(f"Announcer poll: {type(exc).__name__}")
                time.sleep(1.0)

        threading.Thread(target=loop, name="announcer-poll", daemon=True).start()
