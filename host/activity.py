"""Time at the PC, break reminders and locking the PC when the phone leaves.

ActivityTracker reads how long the keyboard and mouse have been idle and keeps:
active minutes per hour today, the current sitting session, breaks (5+ idle
minutes), the longest session and the last 7 days. It raises a stand-up
reminder after a long session and an eye-rest reminder every 20 minutes.

PresenceLock locks Windows when the iPhone paired with the deck goes away
(BLE link lost or weak signal) and nobody has touched the PC for a while.

The deck gets one frame:

  {"type": "activity", "on": 312, "sess": 47, "brk": 6, "long": 95,
   "h": [0, ..., 55, 60, 12, ...],          active minutes per hour today
   "wk": [410, 380, 0, 0, 455, 390, 312],   active minutes, 6 days ago .. today
   "late": false, "rem": "stand" | null, "eye": 3,
   "lk": {"on": true, "near": true | false | null, "rssi": -62 | null}}

Only counts and the lock setting are stored (host/activity_state.json, git-ignored).
"""
from __future__ import annotations

import json
import platform
import time
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Any, Callable

ACTIVE_IDLE_S = 60  # input within the last minute counts as "at the PC"


class ActivityTracker:
    def __init__(self, cfg: dict[str, Any], state_path: Path | None):
        self.state_path = state_path
        self.stand_minutes = int(cfg.get("stand_minutes", 50))
        self.eye_minutes = int(cfg.get("eye_minutes", 20))
        self.break_s = int(cfg.get("break_minutes", 5)) * 60
        self.snooze_s = int(cfg.get("snooze_minutes", 10)) * 60
        self.late_hour = int(cfg.get("late_hour", 23))
        self.lock_on = bool(cfg.get("lock", {}).get("enabled", True))
        self.day: date | None = None
        self.hours = [0.0] * 24  # active seconds per hour today
        self.breaks = 0
        self.longest_s = 0.0
        self.history: dict[str, int] = {}  # iso date -> active minutes
        self.last_tick: datetime | None = None
        self.active = False
        self.session_start: datetime | None = None
        self.last_active: datetime | None = None
        self.acked = False
        self.snooze_until: datetime | None = None
        self.eye = 0
        self.next_eye = self.eye_minutes

    # ---- time keeping ----

    def _end_session(self) -> None:
        if self.session_start is not None and self.last_active is not None:
            self.longest_s = max(self.longest_s, (self.last_active - self.session_start).total_seconds())
            self.breaks += 1
        self.session_start = None
        self.acked = False
        self.snooze_until = None
        self.next_eye = self.eye_minutes

    def _roll_day(self, now: datetime) -> None:
        if self.day is not None and now.date() != self.day:
            self.history[self.day.isoformat()] = self._today_minutes()
            oldest = (now.date() - timedelta(days=14)).isoformat()
            self.history = {k: v for k, v in self.history.items() if k >= oldest}
            self.hours = [0.0] * 24
            self.breaks = 0
            self.longest_s = 0.0
        self.day = now.date()

    def tick(self, now: datetime, idle: float) -> None:
        self._roll_day(now)
        gap = (now - self.last_tick).total_seconds() if self.last_tick else 0.0
        self.last_tick = now
        if gap > self.break_s:  # the PC slept or the bridge stopped: that was a break
            self._end_session()
            gap = 0.0
        self.active = idle < ACTIVE_IDLE_S
        if self.active:
            self.hours[now.hour] += max(0.0, gap)
            if self.session_start is None:
                self.session_start = now - timedelta(seconds=idle)
            self.last_active = now
        elif idle >= self.break_s and self.session_start is not None:
            self._end_session()
        if self.session_start is not None and self.eye_minutes > 0 and not self._stand_due(now):
            if self._session_s(now) >= self.next_eye * 60:
                self.eye += 1
                self.next_eye += self.eye_minutes

    def _session_s(self, now: datetime) -> float:
        return (now - self.session_start).total_seconds() if self.session_start else 0.0

    def _stand_due(self, now: datetime) -> bool:
        if self.stand_minutes <= 0 or self.session_start is None or self.acked:
            return False
        if self.snooze_until is not None and now < self.snooze_until:
            return False
        return self._session_s(now) >= self.stand_minutes * 60

    def _today_minutes(self) -> int:
        return int(round(sum(self.hours) / 60))

    # ---- answers from the deck ----

    def snooze(self, now: datetime) -> None:
        self.snooze_until = now + timedelta(seconds=self.snooze_s)

    def ack(self, now: datetime) -> None:
        self.acked = True

    # ---- output ----

    def frame(self, now: datetime) -> dict[str, Any]:
        today = now.date()
        week = [self.history.get((today - timedelta(days=i)).isoformat(), 0) for i in range(6, 0, -1)]
        session_s = self._session_s(now)
        return {
            "on": self._today_minutes(),
            "sess": int(session_s // 60),
            "brk": self.breaks,
            "long": int(max(self.longest_s, session_s) // 60),
            "h": [int(round(s / 60)) for s in self.hours],
            "wk": week + [self._today_minutes()],
            "late": self.active and (now.hour >= self.late_hour or now.hour < 5),
            "rem": "stand" if self._stand_due(now) else None,
            "eye": self.eye,
        }

    # ---- persistence ----

    def save(self) -> None:
        if self.state_path is None or self.day is None:
            return
        data = {"day": self.day.isoformat(), "hours": [round(s) for s in self.hours], "breaks": self.breaks,
                "longest": round(self.longest_s), "history": self.history, "lock": self.lock_on}
        tmp = self.state_path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data), encoding="utf-8")
        tmp.replace(self.state_path)

    def load(self, now: datetime) -> None:
        if self.state_path is None or not self.state_path.exists():
            return
        try:
            data = json.loads(self.state_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return
        self.history = {str(k): int(v) for k, v in dict(data.get("history", {})).items()}
        self.lock_on = bool(data.get("lock", self.lock_on))
        if data.get("day") == now.date().isoformat():
            self.day = now.date()
            hours = list(data.get("hours", []))
            self.hours = [float(h) for h in hours[:24]] + [0.0] * (24 - len(hours[:24]))
            self.breaks = int(data.get("breaks", 0))
            self.longest_s = float(data.get("longest", 0))


class PresenceLock:
    """Locks once per absence: phone away for away_seconds and the PC idle for idle_seconds."""

    def __init__(self, cfg: dict[str, Any], lock_fn: Callable[[], None], enabled: bool = True):
        self.lock_fn = lock_fn
        self.enabled = enabled
        self.rssi_away = int(cfg.get("rssi_away", -85))
        self.away_s = float(cfg.get("away_seconds", 30))
        self.idle_s = float(cfg.get("idle_seconds", 60))
        self.stale_s = 10.0
        self.link = False
        self.smooth: float | None = None
        self.last_frame: float | None = None
        self.seen_near = False
        self.armed = True
        self.away_since: float | None = None

    def phone(self, link: bool, rssi: int | None, now: float) -> None:
        self.last_frame = now
        self.link = link
        if link and rssi is not None:
            self.smooth = float(rssi) if self.smooth is None else self.smooth * 0.7 + rssi * 0.3
        else:
            self.smooth = None

    def near(self, now: float) -> bool | None:
        if self.last_frame is None or now - self.last_frame > self.stale_s:
            return None  # the deck is off or unplugged: unknown, never lock
        return self.link and self.smooth is not None and self.smooth >= self.rssi_away

    def tick(self, now: float, idle: float) -> bool:
        """Returns True when it locked the PC."""
        near = self.near(now)
        if near is None:
            self.away_since = None
            return False
        if near:
            self.seen_near = True
            self.armed = True
            self.away_since = None
            return False
        if self.away_since is None:
            self.away_since = now
        if (self.enabled and self.seen_near and self.armed and now - self.away_since >= self.away_s
                and idle >= self.idle_s):
            self.armed = False
            self.lock_fn()
            return True
        return False

    def state(self, now: float) -> dict[str, Any]:
        return {"on": self.enabled, "near": self.near(now),
                "rssi": int(round(self.smooth)) if self.smooth is not None else None}


# ---- Windows ----

def idle_seconds() -> float:
    """Seconds since the last keyboard or mouse input (0 when unknown)."""
    if platform.system() != "Windows":
        return 0.0
    import ctypes
    from ctypes import wintypes

    class LASTINPUTINFO(ctypes.Structure):
        _fields_ = [("cbSize", wintypes.UINT), ("dwTime", wintypes.DWORD)]

    info = LASTINPUTINFO()
    info.cbSize = ctypes.sizeof(info)
    if not ctypes.windll.user32.GetLastInputInfo(ctypes.byref(info)):
        return 0.0
    now = ctypes.windll.kernel32.GetTickCount() & 0xFFFFFFFF
    return ((now - info.dwTime) & 0xFFFFFFFF) / 1000.0


def lock_workstation() -> None:
    if platform.system() == "Windows":
        import ctypes

        ctypes.windll.user32.LockWorkStation()


class ActivityService:
    """Glue for the bridge: tick() from the run loop, handle() deck messages, snapshot() the frame."""

    def __init__(self, config: dict[str, Any], state_path: Path | None,
                 idle_fn: Callable[[], float] = idle_seconds, lock_fn: Callable[[], None] = lock_workstation,
                 clock: Callable[[], datetime] = datetime.now, mono: Callable[[], float] = time.monotonic,
                 log: Callable[[str], None] = lambda _m: None):
        cfg = dict(config.get("activity") or {})
        self.clock = clock
        self.mono = mono
        self.idle_fn = idle_fn
        self.log = log
        self.tracker = ActivityTracker(cfg, state_path)
        self.tracker.load(clock())

        def locked() -> None:
            log("Phone away and PC idle: locking Windows")
            lock_fn()

        self.presence = PresenceLock(dict(cfg.get("lock") or {}), locked, enabled=self.tracker.lock_on)
        self.version = 0
        self.frame: dict[str, Any] | None = None
        self._key: str | None = None
        self._saved_at = mono()

    def _refresh(self) -> None:
        frame = {"type": "activity", **self.tracker.frame(self.clock()), "lk": self.presence.state(self.mono())}
        lk = frame["lk"]
        key = json.dumps({**frame, "lk": {**lk, "rssi": None if lk["rssi"] is None else lk["rssi"] // 3}},
                         sort_keys=True)
        if key != self._key:
            self._key = key
            self.frame = frame
            self.version += 1

    def tick(self) -> None:
        idle = self.idle_fn()
        self.tracker.tick(self.clock(), idle)
        self.presence.tick(self.mono(), idle)
        if self.mono() - self._saved_at >= 60:
            self._save()
        self._refresh()

    def _save(self) -> None:
        self._saved_at = self.mono()
        try:
            self.tracker.save()
        except OSError as exc:
            self.log(f"Activity state not saved: {exc}")

    def handle(self, message: dict[str, Any]) -> bool:
        kind = message.get("type")
        if kind == "phone":
            rssi = message.get("rssi")
            self.presence.phone(bool(message.get("link")), int(rssi) if isinstance(rssi, (int, float)) else None,
                                self.mono())
        elif kind == "lock_set":
            self.presence.enabled = self.tracker.lock_on = bool(message.get("on"))
            self.log(f"Auto-lock {'on' if self.presence.enabled else 'off'}")
            self._save()
        elif kind == "break_reply":
            if message.get("act") == "snooze":
                self.tracker.snooze(self.clock())
            else:
                self.tracker.ack(self.clock())
        else:
            return False
        self._refresh()
        return True

    def snapshot(self) -> tuple[int, dict[str, Any] | None]:
        return self.version, self.frame

    def start(self, every: float = 5.0) -> None:
        """Ticks in the background, so time counts even while the deck is unplugged."""
        import threading

        def loop() -> None:
            while True:
                try:
                    self.tick()
                except Exception as exc:  # never stop the bridge over a bad reading
                    self.log(f"Activity update failed: {exc}")
                time.sleep(every)

        threading.Thread(target=loop, name="activity", daemon=True).start()
