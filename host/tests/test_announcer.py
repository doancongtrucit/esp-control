"""Spoken announcements: Claude Code sessions, PC alerts, fuel price changes, meetings, morning brief."""
import sys
import unittest
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import announcer as an  # noqa: E402
from ics_calendar import Meeting  # noqa: E402

MON = datetime(2026, 11, 16, 7, 30)  # Monday; lunar 8/10 (see test_lunar_vi for the algorithm)


class Clock:
    def __init__(self, t):
        self.t = t

    def __call__(self):
        return self.t

    def advance(self, seconds):
        self.t += timedelta(seconds=seconds)


def make(config=None, clock=None):
    frames = []
    clock = clock or Clock(MON)
    a = an.Announcer(config or {}, deliver=frames.extend, speak=lambda text: b"\x80" * 800, now=clock,
                     inline=True)
    return a, frames, clock


def notices(frames):
    return [(f["from"], f["body"], f["s"]) for f in frames if f["type"] == "notify"]


def claude_frame(rows, ask=None):
    frame = {"type": "claude", "n": len(rows), "s": [[n, s, "", 5, f] for n, s, f in rows]}
    if ask:
        frame["ask"] = ask
    return frame


class SplitTests(unittest.TestCase):
    def test_long_text_becomes_short_sentences(self):
        parts = an.split_speech("Câu một ngắn. " + "từ " * 40 + "hết.")
        self.assertEqual("Câu một ngắn.", parts[0])
        self.assertTrue(all(len(p) <= an.CLIP_CHARS for p in parts))
        self.assertEqual("".join(parts).replace(" ", ""), ("Câu một ngắn. " + "từ " * 40 + "hết.").replace(" ", ""))


class ClaudeTests(unittest.TestCase):
    def test_first_frame_is_only_remembered(self):
        a, frames, _ = make()
        a.on_claude(claude_frame([("deck-src", "w", 0)], {"id": 1, "name": "deck-src", "tool": "Bash"}))
        self.assertEqual([], frames)

    def test_new_permission_question_names_the_session(self):
        a, frames, _ = make()
        a.on_claude(claude_frame([("deck-src", "r", 0)]))
        a.on_claude(claude_frame([("deck-src", "w", 0)], {"id": 4, "name": "deck-src", "tool": "Bash"}))
        self.assertEqual(["Phiên deck src đang chờ bạn duyệt lệnh Bash"], a.spoken)
        self.assertEqual([], notices(frames))  # the deck already shows the question and plays its tone

    def test_a_finished_session_and_a_failing_one(self):
        a, frames, _ = make()
        a.on_claude(claude_frame([("api", "r", 0), ("web_app", "r", 2)]))
        a.on_claude(claude_frame([("api", "d", 0), ("web_app", "r", 3)]))
        self.assertEqual(["Phiên api đã xong", "Phiên web app lỗi 3 lần liên tiếp"], a.spoken)

    def test_done_again_within_a_minute_is_not_repeated(self):
        a, frames, clock = make()
        a.on_claude(claude_frame([("api", "r", 0)]))
        a.on_claude(claude_frame([("api", "d", 0)]))
        clock.advance(20)
        a.on_claude(claude_frame([("api", "r", 0)]))
        a.on_claude(claude_frame([("api", "d", 0)]))
        self.assertEqual(1, len(a.spoken))

    def test_can_be_turned_off(self):
        a, frames, _ = make({"voice_alerts": {"claude": False}})
        a.on_claude(claude_frame([("api", "r", 0)]))
        a.on_claude(claude_frame([("api", "d", 0)]))
        self.assertEqual([], frames)


class PcAlertTests(unittest.TestCase):
    def stats(self, cpu_temp=60.0, ram=50.0, gpu_temp=50.0):
        return {"type": "stats", "cpu_temp": cpu_temp, "ram": ram, "gpu_temp": gpu_temp}

    def test_hot_cpu_is_announced_after_it_stays_hot(self):
        a, frames, clock = make()
        a.on_stats(self.stats(cpu_temp=93))
        self.assertEqual([], frames)  # a short spike is ignored
        clock.advance(25)
        a.on_stats(self.stats(cpu_temp=92.4))
        self.assertEqual(["CPU đang 92 độ"], a.spoken)
        self.assertEqual([("PC nóng", "CPU 92°C", "alert")], notices(frames))

    def test_once_until_it_cools_down_and_the_cooldown_passes(self):
        a, frames, clock = make()
        for _ in range(3):
            a.on_stats(self.stats(cpu_temp=95))
            clock.advance(30)
        self.assertEqual(1, len(a.spoken))
        a.on_stats(self.stats(cpu_temp=80))  # cooled: armed again, but within the cooldown
        a.on_stats(self.stats(cpu_temp=95))
        clock.advance(30)
        a.on_stats(self.stats(cpu_temp=95))
        self.assertEqual(1, len(a.spoken))
        clock.advance(16 * 60)
        a.on_stats(self.stats(cpu_temp=80))
        a.on_stats(self.stats(cpu_temp=95))
        clock.advance(30)
        a.on_stats(self.stats(cpu_temp=95))
        self.assertEqual(2, len(a.spoken))

    def test_ram_and_gpu_with_custom_limits_and_missing_sensors(self):
        a, frames, clock = make({"voice_alerts": {"ram": 85, "gpu_temp": 80}})
        a.on_stats({"type": "stats", "cpu_temp": None, "ram": 88.0, "gpu_temp": 82.0})
        clock.advance(25)
        a.on_stats({"type": "stats", "cpu_temp": None, "ram": 88.0, "gpu_temp": 82.0})
        self.assertEqual(["RAM đã dùng 88 phần trăm", "GPU đang 82 độ"], a.spoken)


class FuelTests(unittest.TestCase):
    PREV = {"Xăng E10 RON 95 mức 5": 22650, "Xăng E10 RON 95 mức 3": 22450, "Xăng E5 RON 92 mức 2": 21000,
            "Dầu DO 0,001S mức 5": 19500, "Dầu DO 0,05S mức 2": 19300, "Dầu hỏa 2-K": 19000}

    def test_announces_each_product_family_once(self):
        a, frames, _ = make()
        cur = dict(self.PREV, **{"Xăng E10 RON 95 mức 5": 23070, "Xăng E10 RON 95 mức 3": 22870,
                                 "Dầu DO 0,001S mức 5": 19000, "Dầu DO 0,05S mức 2": 18800})
        a.on_fuel_change(self.PREV, cur)
        self.assertEqual(["Giá xăng dầu vừa điều chỉnh.", "Xăng E10 RON 95 tăng 420 đồng.",
                          "Dầu diesel giảm 500 đồng."], a.spoken)
        self.assertEqual([("Giá xăng dầu", "E10 RON95 +420 · DO -500", "chime")], notices(frames))

    def test_nothing_changed_says_nothing(self):
        a, frames, _ = make()
        a.on_fuel_change(self.PREV, dict(self.PREV))
        self.assertEqual([], frames)


class MeetingTests(unittest.TestCase):
    def test_reminder_before_and_at_the_start_once_each(self):
        clock = Clock(datetime(2026, 11, 16, 8, 50))
        a, frames, _ = make(clock=clock)
        meetings = [Meeting(datetime(2026, 11, 16, 9, 0), "Weekly sync", "u1")]
        a.check_meetings(meetings)
        self.assertEqual([], frames)
        clock.advance(5 * 60 + 10)  # 08:55:10
        a.check_meetings(meetings)
        a.check_meetings(meetings)
        self.assertEqual(["5 phút nữa họp Weekly sync"], a.spoken)
        self.assertEqual([("Họp lúc 09:00", "Weekly sync", "reminder")], notices(frames))
        clock.advance(5 * 60)  # 09:00:10
        a.check_meetings(meetings)
        a.check_meetings(meetings)
        self.assertEqual(["5 phút nữa họp Weekly sync", "Cuộc họp Weekly sync đã bắt đầu"], a.spoken)

    def test_titles_can_stay_private(self):
        clock = Clock(datetime(2026, 11, 16, 8, 56))
        a, frames, _ = make({"voice_alerts": {"speak_titles": False, "remind_minutes": 10}}, clock)
        a.check_meetings([Meeting(datetime(2026, 11, 16, 9, 0), "Salary review", "u2")])
        self.assertEqual(["4 phút nữa có cuộc họp"], a.spoken)
        self.assertEqual([("Họp lúc 09:00", "", "reminder")], notices(frames))

    def test_a_long_past_meeting_is_ignored(self):
        clock = Clock(datetime(2026, 11, 16, 9, 30))
        a, frames, _ = make(clock=clock)
        a.check_meetings([Meeting(datetime(2026, 11, 16, 9, 0), "Done", "u3")])
        self.assertEqual([], frames)


class BriefTests(unittest.TestCase):
    WEATHER = {"type": "weather", "city": "Da Nang", "t": 25.6, "desc": "Showers"}
    FUEL = {"type": "fuel", "items": [["E10 RON95 M5", 22650, -500], ["E5 RON92 M2", 21000, 0]]}

    def test_full_brief(self):
        meetings = [Meeting(datetime(2026, 11, 16, 9, 0), "Weekly sync", "a"),
                    Meeting(datetime(2026, 11, 16, 14, 30), "Review", "b"),
                    Meeting(datetime(2026, 11, 17, 9, 0), "Tomorrow", "c")]
        text = an.brief_text(MON, self.WEATHER, self.FUEL, meetings)
        self.assertEqual(
            "Chào buổi sáng. Hôm nay thứ Hai, ngày 16 tháng 11, âm lịch ngày 8 tháng 10. "
            "Đà Nẵng 26 độ, có mưa rào. Hôm nay có 2 cuộc họp, cuộc đầu tiên lúc 9 giờ: Weekly sync. "
            "Xăng RON 95 giá 22 nghìn 650 đồng, giảm 500 đồng.", text)

    def test_without_data_and_late_in_the_day(self):
        evening = datetime(2026, 11, 16, 19, 5)
        text = an.brief_text(evening, None, None, [Meeting(datetime(2026, 11, 16, 9, 0), "Gone", "a")])
        self.assertEqual("Chào buổi tối. Hôm nay thứ Hai, ngày 16 tháng 11, âm lịch ngày 8 tháng 10. "
                         "Hôm nay không còn cuộc họp nào.", text)

    def test_some_meetings_already_started(self):
        noon = datetime(2026, 11, 16, 12, 0)
        meetings = [Meeting(datetime(2026, 11, 16, 9, 0), "Gone", "a"),
                    Meeting(datetime(2026, 11, 16, 14, 30), "Review", "b")]
        text = an.brief_text(noon, None, None, meetings, speak_titles=False)
        self.assertIn("Hôm nay còn 1 cuộc họp, cuộc tiếp theo lúc 14 giờ 30.", text)

    def test_brief_is_spoken_in_clips(self):
        a, frames, _ = make()
        a.brief(self.WEATHER, self.FUEL, [])
        texts = a.spoken
        self.assertGreater(len(texts), 2)
        self.assertEqual("Chào buổi sáng.", texts[0])

    def test_auto_brief_once_a_day_at_the_set_time(self):
        clock = Clock(datetime(2026, 11, 16, 7, 0))
        a, frames, _ = make({"brief": {"auto_at": "07:30"}}, clock)
        a.auto_brief(lambda: (None, None, []))
        self.assertEqual([], frames)
        clock.advance(31 * 60)
        a.auto_brief(lambda: (None, None, []))
        a.auto_brief(lambda: (None, None, []))
        self.assertEqual(1, a.spoken.count("Chào buổi sáng."))
        clock.advance(4 * 3600)  # PC turned on late: the morning brief is skipped
        b, later, _ = make({"brief": {"auto_at": "07:30"}}, Clock(datetime(2026, 11, 16, 11, 45)))
        b.auto_brief(lambda: (None, None, []))
        self.assertEqual([], later)


if __name__ == "__main__":
    unittest.main()
