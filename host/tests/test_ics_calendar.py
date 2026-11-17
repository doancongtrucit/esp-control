"""Meetings from an iCalendar (.ics) feed: single, timezone, recurring, moved and cancelled events."""
import sys
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ics_calendar as ic  # noqa: E402

VN = timezone(timedelta(hours=7))


def cal(*events: str, extra: str = "") -> str:
    body = "".join(f"BEGIN:VEVENT\r\n{e.strip()}\r\nEND:VEVENT\r\n" for e in events)
    return f"BEGIN:VCALENDAR\r\nVERSION:2.0\r\n{extra}{body}END:VCALENDAR\r\n"


def titles(text, start, end):
    return [(m.start.strftime("%Y-%m-%d %H:%M"), m.title) for m in ic.meetings_between(text, start, end, VN)]


DAY = datetime(2026, 11, 16)  # a Monday
WINDOW = (DAY, DAY + timedelta(days=7))


class SingleEventTests(unittest.TestCase):
    def test_utc_time_is_shown_in_local_time(self):
        text = cal("UID:a\nDTSTART:20261116T020000Z\nDTEND:20261116T030000Z\nSUMMARY:Weekly sync")
        self.assertEqual([("2026-11-16 09:00", "Weekly sync")], titles(text, *WINDOW))

    def test_tzid_from_a_vtimezone_block(self):
        tz = ("BEGIN:VTIMEZONE\r\nTZID:SE Asia Standard Time\r\nBEGIN:STANDARD\r\nDTSTART:16010101T000000\r\n"
              "TZOFFSETFROM:+0700\r\nTZOFFSETTO:+0700\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n")
        text = cal("UID:b\nDTSTART;TZID=SE Asia Standard Time:20261117T143000\nSUMMARY:Review", extra=tz)
        self.assertEqual([("2026-11-17 14:30", "Review")], titles(text, *WINDOW))

    def test_folded_lines_and_escapes(self):
        text = cal("UID:c\nDTSTART:20261116T030000Z\nSUMMARY:Plan\\, budget and\r\n  roadmap")
        self.assertEqual([("2026-11-16 10:00", "Plan, budget and roadmap")], titles(text, *WINDOW))

    def test_all_day_and_cancelled_events_are_not_meetings(self):
        text = cal("UID:d\nDTSTART;VALUE=DATE:20261116\nSUMMARY:Holiday",
                   "UID:e\nDTSTART:20261116T030000Z\nSTATUS:CANCELLED\nSUMMARY:Gone")
        self.assertEqual([], titles(text, *WINDOW))

    def test_outside_the_window_is_left_out(self):
        text = cal("UID:f\nDTSTART:20261130T030000Z\nSUMMARY:Later")
        self.assertEqual([], titles(text, *WINDOW))

    def test_untitled_event(self):
        text = cal("UID:g\nDTSTART:20261116T030000Z")
        self.assertEqual([("2026-11-16 10:00", "")], titles(text, *WINDOW))


class RecurringTests(unittest.TestCase):
    def test_weekly_on_two_days_with_an_exception_date(self):
        text = cal("UID:w\nDTSTART:20261102T013000Z\nRRULE:FREQ=WEEKLY;BYDAY=MO,WE\n"
                   "EXDATE:20261118T013000Z\nSUMMARY:Standup")
        self.assertEqual([("2026-11-16 08:30", "Standup"), ("2026-11-23 08:30", "Standup")],
                         [t for t in titles(text, DAY, DAY + timedelta(days=8))])

    def test_every_other_week_until_a_date(self):
        text = cal("UID:x\nDTSTART:20261102T020000Z\nRRULE:FREQ=WEEKLY;INTERVAL=2;UNTIL=20261201T000000Z\n"
                   "SUMMARY:Retro")
        got = titles(text, datetime(2026, 11, 1), datetime(2026, 12, 31))
        self.assertEqual(["2026-11-02 09:00", "2026-11-16 09:00", "2026-11-30 09:00"], [g[0] for g in got])

    def test_daily_with_a_count(self):
        text = cal("UID:y\nDTSTART:20261114T020000Z\nRRULE:FREQ=DAILY;COUNT=3\nSUMMARY:Sprint")
        got = titles(text, datetime(2026, 11, 1), datetime(2026, 11, 30))
        self.assertEqual(["2026-11-14 09:00", "2026-11-15 09:00", "2026-11-16 09:00"], [g[0] for g in got])

    def test_monthly_on_the_second_tuesday(self):
        text = cal("UID:z\nDTSTART:20261013T070000Z\nRRULE:FREQ=MONTHLY;BYDAY=2TU\nSUMMARY:Board")
        got = titles(text, datetime(2026, 11, 1), datetime(2026, 12, 31))
        self.assertEqual(["2026-11-10 14:00", "2026-12-08 14:00"], [g[0] for g in got])

    def test_a_moved_occurrence_replaces_the_original(self):
        text = cal("UID:m\nDTSTART:20261102T020000Z\nRRULE:FREQ=WEEKLY\nSUMMARY:1:1",
                   "UID:m\nRECURRENCE-ID:20261116T020000Z\nDTSTART:20261116T070000Z\nSUMMARY:1:1 (moved)")
        self.assertEqual([("2026-11-16 14:00", "1:1 (moved)")], titles(text, DAY, DAY + timedelta(days=1)))

    def test_a_cancelled_occurrence_disappears(self):
        text = cal("UID:k\nDTSTART:20261102T020000Z\nRRULE:FREQ=WEEKLY\nSUMMARY:Sync",
                   "UID:k\nRECURRENCE-ID:20261116T020000Z\nDTSTART:20261116T020000Z\nSTATUS:CANCELLED")
        self.assertEqual([], titles(text, DAY, DAY + timedelta(days=1)))


class FeedTests(unittest.TestCase):
    def test_merges_feeds_sorted_and_survives_a_failing_one(self):
        feeds = {
            "https://a.example/cal.ics": cal("UID:1\nDTSTART:20261116T030000Z\nSUMMARY:Second"),
            "https://b.example/cal.ics": cal("UID:2\nDTSTART:20261116T020000Z\nSUMMARY:First"),
        }

        def http(url):
            if url not in feeds:
                raise OSError("offline")
            return feeds[url]

        feed = ic.CalendarFeed({"calendar": {"ics_urls": list(feeds) + ["https://down.example/x.ics"]}},
                               http=http, now=lambda: DAY, local_tz=VN)
        feed.refresh()
        self.assertEqual(["First", "Second"], [m.title for m in feed.meetings])

    def test_no_urls_means_off(self):
        self.assertIsNone(ic.start_calendar({"calendar": {"ics_urls": []}}))


if __name__ == "__main__":
    unittest.main()
