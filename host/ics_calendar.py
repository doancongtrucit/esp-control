"""Meetings from iCalendar (.ics) feeds, for spoken reminders and the morning brief.

Google Calendar: Settings > your calendar > "Secret address in iCal format".
Outlook.com / Microsoft 365: Settings > Calendar > Shared calendars > Publish a calendar > ICS link.
Put the links in host/config.json (git-ignored):  "calendar": {"ics_urls": ["https://..."]}
The links are secret (anyone with one can read the calendar): they are never logged.

Handles what calendars actually send for meetings: UTC, TZID (IANA names, or
Windows names through the feed's VTIMEZONE block), all-day events (skipped),
cancelled events, RRULE (DAILY, WEEKLY, MONTHLY with INTERVAL, COUNT, UNTIL,
BYDAY, BYMONTHDAY), EXDATE and moved or cancelled single occurrences (RECURRENCE-ID).
"""
from __future__ import annotations

import re
import threading
import time
import urllib.request
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone, tzinfo
from typing import Any, Callable, Iterator

REFRESH_S = 600.0
WEEKDAYS = {"MO": 0, "TU": 1, "WE": 2, "TH": 3, "FR": 4, "SA": 5, "SU": 6}
MAX_STEPS = 40000  # days (or months) walked through one recurrence: about a century


@dataclass(frozen=True)
class Meeting:
    start: datetime  # local time, naive
    title: str
    uid: str


Props = dict[str, list[tuple[dict[str, str], str]]]


def _unfold(text: str) -> list[str]:
    lines: list[str] = []
    for raw in re.split(r"\r?\n", text):
        if raw[:1] in (" ", "\t") and lines:
            lines[-1] += raw[1:]
        elif raw:
            lines.append(raw)
    return lines


def _split(line: str) -> tuple[str, dict[str, str], str]:
    head, _, value = line.partition(":")
    parts = head.split(";")
    params = {}
    for part in parts[1:]:
        key, _, val = part.partition("=")
        params[key.upper()] = val.strip('"')
    return parts[0].upper(), params, value


def _unescape(value: str) -> str:
    out, i = [], 0
    while i < len(value):
        ch = value[i]
        if ch == "\\" and i + 1 < len(value):
            nxt = value[i + 1]
            out.append(" " if nxt in "nN" else nxt)
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out).strip()


def _offset(text: str) -> timedelta | None:
    m = re.fullmatch(r"([+-])(\d\d)(\d\d)(\d\d)?", text.strip())
    if not m:
        return None
    delta = timedelta(hours=int(m.group(2)), minutes=int(m.group(3)), seconds=int(m.group(4) or 0))
    return -delta if m.group(1) == "-" else delta


def _parse(text: str) -> tuple[list[Props], dict[str, timedelta]]:
    events: list[Props] = []
    zones: dict[str, timedelta] = {}
    stack: list[str] = []
    current: Props | None = None
    zone_id, zone_std, zone_any = "", None, None
    for line in _unfold(text):
        name, params, value = _split(line)
        if name == "BEGIN":
            stack.append(value.upper())
            if value.upper() == "VEVENT":
                current = {}
            elif value.upper() == "VTIMEZONE":
                zone_id, zone_std, zone_any = "", None, None
            continue
        if name == "END":
            ended = stack.pop() if stack else ""
            if ended == "VEVENT" and current is not None:
                events.append(current)
                current = None
            elif ended == "VTIMEZONE" and zone_id:
                chosen = zone_std if zone_std is not None else zone_any
                if chosen is not None:
                    zones[zone_id] = chosen
            continue
        inside = stack[-1] if stack else ""
        if inside == "VEVENT" and current is not None:
            current.setdefault(name, []).append((params, value))
        elif inside == "VTIMEZONE" and name == "TZID":
            zone_id = value
        elif inside in ("STANDARD", "DAYLIGHT") and name == "TZOFFSETTO":
            off = _offset(value)
            if inside == "STANDARD" and zone_std is None:
                zone_std = off
            if zone_any is None:
                zone_any = off
    return events, zones


def _zone(tzid: str | None, zones: dict[str, timedelta], local: tzinfo) -> tzinfo:
    if not tzid:
        return local
    try:
        from zoneinfo import ZoneInfo

        return ZoneInfo(tzid)
    except Exception:  # Windows names, or no tz database on this PC
        pass
    if tzid in zones:
        return timezone(zones[tzid])
    return local


def _datetime(value: str, params: dict[str, str], zones: dict[str, timedelta],
              local: tzinfo) -> tuple[datetime | None, bool]:
    """(aware datetime, all day) for a DTSTART/EXDATE/RECURRENCE-ID value."""
    value = value.strip()
    try:
        if params.get("VALUE", "").upper() == "DATE" or re.fullmatch(r"\d{8}", value):
            d = datetime.strptime(value[:8], "%Y%m%d")
            return d.replace(tzinfo=_zone(params.get("TZID"), zones, local)), True
        naive = datetime.strptime(value[:15], "%Y%m%dT%H%M%S")
    except ValueError:
        return None, False
    if value.endswith("Z"):
        return naive.replace(tzinfo=timezone.utc), False
    return naive.replace(tzinfo=_zone(params.get("TZID"), zones, local)), False


def _first(props: Props, name: str) -> tuple[dict[str, str], str] | None:
    values = props.get(name)
    return values[0] if values else None


def _key(uid: str, moment: datetime) -> tuple[str, float]:
    return uid, moment.timestamp()


def _wall(start: datetime, day: date) -> datetime:
    return start.replace(year=day.year, month=day.month, day=day.day)


def _nth_weekday(year: int, month: int, weekday: int, nth: int) -> date | None:
    if nth > 0:
        first = date(year, month, 1)
        d = first + timedelta(days=(weekday - first.weekday()) % 7 + 7 * (nth - 1))
    else:
        nxt = date(year + month // 12, month % 12 + 1, 1)
        last = nxt - timedelta(days=1)
        d = last - timedelta(days=(last.weekday() - weekday) % 7 + 7 * (-nth - 1))
    return d if d.month == month else None


def _occurrences(start: datetime, rrule: str, end: datetime, zones: dict[str, timedelta],
                 local: tzinfo) -> Iterator[datetime]:
    """Every start of a recurring event from its first one up to `end` (aware)."""
    rule = dict(part.partition("=")[::2] for part in rrule.upper().split(";") if "=" in part)
    freq = rule.get("FREQ", "")
    interval = max(1, int(rule.get("INTERVAL", "1") or 1))
    count = int(rule["COUNT"]) if rule.get("COUNT", "").isdigit() else 0
    until = None
    if "UNTIL" in rule:
        until, all_day = _datetime(rule["UNTIL"], {}, zones, start.tzinfo or local)
        if until is not None and all_day:
            until += timedelta(days=1)  # the whole last day is included
    limit = min(end, until) if until else end
    byday = [b for b in rule.get("BYDAY", "").split(",") if b]
    produced = 0

    def emit(moment: datetime) -> bool:
        """False once the rule has ended."""
        nonlocal produced
        if until is not None and moment > until:
            return False
        produced += 1
        return not (count and produced > count)

    if freq in ("DAILY", "WEEKLY"):
        days = {WEEKDAYS[b[-2:]] for b in byday if b[-2:] in WEEKDAYS}
        if freq == "WEEKLY" and not days:
            days = {start.weekday()}
        week0 = start.date() - timedelta(days=start.weekday())
        day = start.date()
        for _ in range(MAX_STEPS):
            moment = _wall(start, day)
            if moment > limit:
                return
            if freq == "DAILY":
                ok = (day - start.date()).days % interval == 0 and (not days or day.weekday() in days)
            else:
                ok = ((day - week0).days // 7) % interval == 0 and day.weekday() in days
            if ok and moment >= start:
                if not emit(moment):
                    return
                yield moment
            day += timedelta(days=1)
    elif freq == "MONTHLY":
        monthdays = [int(d) for d in rule.get("BYMONTHDAY", "").split(",") if d.lstrip("-").isdigit()]
        year, month = start.year, start.month
        for _ in range(MAX_STEPS // 30):
            days: list[date] = []
            if byday:
                for b in byday:
                    m = re.fullmatch(r"([+-]?\d)?(MO|TU|WE|TH|FR|SA|SU)", b)
                    if m and m.group(1):
                        d = _nth_weekday(year, month, WEEKDAYS[m.group(2)], int(m.group(1)))
                        if d:
                            days.append(d)
            else:
                for md in monthdays or [start.day]:
                    try:
                        if md > 0:
                            days.append(date(year, month, md))
                        else:
                            nxt = date(year + month // 12, month % 12 + 1, 1)
                            days.append(nxt + timedelta(days=md))
                    except ValueError:  # the 31st in a short month
                        continue
            for d in sorted(days):
                moment = _wall(start, d)
                if moment > limit:
                    return
                if moment >= start:
                    if not emit(moment):
                        return
                    yield moment
            month += interval
            year += (month - 1) // 12
            month = (month - 1) % 12 + 1
    else:  # YEARLY and others: the first occurrence only
        if start <= limit:
            yield start


def meetings_between(text: str, start: datetime, end: datetime, local: tzinfo | None = None) -> list[Meeting]:
    """Timed, not cancelled meetings starting in [start, end) (naive local times), sorted."""
    local = local or datetime.now().astimezone().tzinfo or timezone.utc
    lo, hi = start.replace(tzinfo=local), end.replace(tzinfo=local)
    events, zones = _parse(text)
    replaced: set[tuple[str, float]] = set()
    singles: list[tuple[Props, datetime]] = []
    masters: list[tuple[Props, datetime]] = []
    for ev in events:
        uid = (_first(ev, "UID") or ({}, ""))[1]
        dt = _first(ev, "DTSTART")
        if dt is None:
            continue
        moment, all_day = _datetime(dt[1], dt[0], zones, local)
        rec = _first(ev, "RECURRENCE-ID")
        if rec is not None:
            original, _ = _datetime(rec[1], rec[0], zones, local)
            if original is not None:
                replaced.add(_key(uid, original))
        if moment is None or all_day:
            continue
        cancelled = (_first(ev, "STATUS") or ({}, ""))[1].upper() == "CANCELLED"
        if cancelled:
            continue
        if _first(ev, "RRULE") is not None and rec is None:
            masters.append((ev, moment))
        else:
            singles.append((ev, moment))

    found: list[Meeting] = []

    def add(ev: Props, moment: datetime) -> None:
        if lo <= moment < hi:
            title = _unescape((_first(ev, "SUMMARY") or ({}, ""))[1])
            uid = (_first(ev, "UID") or ({}, ""))[1]
            found.append(Meeting(moment.astimezone(local).replace(tzinfo=None), title, uid))

    for ev, moment in singles:
        add(ev, moment)
    for ev, first in masters:
        uid = (_first(ev, "UID") or ({}, ""))[1]
        skip = set(replaced)
        for params, value in ev.get("EXDATE", []):
            for part in value.split(","):
                ex, _ = _datetime(part, params, zones, first.tzinfo or local)
                if ex is not None:
                    skip.add(_key(uid, ex))
        rrule = (_first(ev, "RRULE") or ({}, ""))[1]
        for moment in _occurrences(first, rrule, hi, zones, local):
            if _key(uid, moment) not in skip:
                add(ev, moment)
    found.sort(key=lambda m: (m.start, m.title))
    return found


def http_text(url: str, timeout: float = 20.0) -> str:
    if url.lower().startswith("webcal://"):
        url = "https://" + url[len("webcal://"):]
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 ESP-Control-Deck"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8", "replace")


class CalendarFeed:
    """Meetings from today to 3 days ahead, re-read every 10 minutes."""

    def __init__(self, config: dict[str, Any], http: Callable[[str], str] = http_text,
                 now: Callable[[], datetime] = datetime.now, local_tz: tzinfo | None = None,
                 log: Callable[[str], None] = lambda _m: None):
        cfg = config.get("calendar") or {}
        self.urls = [str(u) for u in cfg.get("ics_urls") or [] if str(u).strip()]
        self.http, self.now, self.local_tz, self.log = http, now, local_tz, log
        self.meetings: list[Meeting] = []
        self.lock = threading.Lock()

    def refresh(self) -> None:
        today = self.now().replace(hour=0, minute=0, second=0, microsecond=0)
        found: list[Meeting] = []
        for index, url in enumerate(self.urls, 1):
            try:
                found += meetings_between(self.http(url), today, today + timedelta(days=3), self.local_tz)
            except Exception as exc:  # the URL is secret: say which one by number only
                self.log(f"Calendar {index} not read: {type(exc).__name__}")
        found.sort(key=lambda m: (m.start, m.title))
        with self.lock:
            self.meetings = found

    def snapshot(self) -> list[Meeting]:
        with self.lock:
            return list(self.meetings)

    def start(self) -> None:
        def loop() -> None:
            while True:
                self.refresh()
                time.sleep(REFRESH_S)

        threading.Thread(target=loop, name="calendar", daemon=True).start()


def start_calendar(config: dict[str, Any], log: Callable[[str], None] = lambda _m: None) -> CalendarFeed | None:
    feed = CalendarFeed(config, log=log)
    if not feed.urls:
        return None
    feed.start()
    log(f"Calendar: reading {len(feed.urls)} feed(s) for meeting reminders")
    return feed
