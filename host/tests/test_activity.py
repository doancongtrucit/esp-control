"""Time at the PC, break reminders and lock-when-the-phone-leaves."""
import json
import sys
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import activity as act  # noqa: E402

T0 = datetime(2026, 11, 10, 9, 0)


def run(tracker, start, minutes, idle=0.0, step=5):
    """Ticks every `step` seconds for `minutes`, with a constant idle time."""
    now = start
    end = start + timedelta(minutes=minutes)
    while now < end:
        now += timedelta(seconds=step)
        tracker.tick(now, idle)
    return now


class TrackerTests(unittest.TestCase):
    def make(self, **cfg):
        return act.ActivityTracker(cfg, state_path=None)

    def test_counts_active_minutes_per_hour(self):
        t = self.make()
        t.tick(T0, 0)
        run(t, T0, 30)
        frame = t.frame(T0 + timedelta(minutes=30))
        self.assertEqual(30, frame["on"])
        self.assertEqual(30, frame["h"][9])
        self.assertEqual(30, frame["sess"])

    def test_idle_time_is_not_counted(self):
        t = self.make()
        t.tick(T0, 0)
        run(t, T0, 10, idle=200)
        self.assertEqual(0, t.frame(T0 + timedelta(minutes=10))["on"])

    def test_five_idle_minutes_is_a_break_and_ends_the_session(self):
        t = self.make()
        t.tick(T0, 0)
        now = run(t, T0, 40)
        now = run(t, now, 6, idle=360)  # idle grows past 5 min
        frame = t.frame(now)
        self.assertEqual(1, frame["brk"])
        self.assertEqual(0, frame["sess"])
        self.assertEqual(40, frame["long"])

    def test_stand_reminder_after_fifty_minutes(self):
        t = self.make()
        t.tick(T0, 0)
        now = run(t, T0, 49)
        self.assertIsNone(t.frame(now)["rem"])
        now = run(t, now, 2)
        self.assertEqual("stand", t.frame(now)["rem"])

    def test_snooze_hides_the_reminder_for_ten_minutes(self):
        t = self.make()
        t.tick(T0, 0)
        now = run(t, T0, 51)
        t.snooze(now)
        now = run(t, now, 9)
        self.assertIsNone(t.frame(now)["rem"])
        now = run(t, now, 2)
        self.assertEqual("stand", t.frame(now)["rem"])

    def test_ok_hides_the_reminder_until_the_next_break(self):
        t = self.make()
        t.tick(T0, 0)
        now = run(t, T0, 51)
        t.ack(now)
        now = run(t, now, 60)
        self.assertIsNone(t.frame(now)["rem"])
        now = run(t, now, 6, idle=360)
        now = run(t, now, 51)
        self.assertEqual("stand", t.frame(now)["rem"])

    def test_a_break_clears_the_reminder(self):
        t = self.make()
        t.tick(T0, 0)
        now = run(t, T0, 51)
        now = run(t, now, 6, idle=360)
        self.assertIsNone(t.frame(now)["rem"])

    def test_eye_reminder_every_twenty_minutes(self):
        t = self.make()
        t.tick(T0, 0)
        now = run(t, T0, 19)
        self.assertEqual(0, t.frame(now)["eye"])
        now = run(t, now, 2)
        self.assertEqual(1, t.frame(now)["eye"])
        now = run(t, now, 20)
        self.assertEqual(2, t.frame(now)["eye"])

    def test_reminders_can_be_turned_off(self):
        t = self.make(stand_minutes=0, eye_minutes=0)
        t.tick(T0, 0)
        now = run(t, T0, 70)
        self.assertEqual((None, 0), (t.frame(now)["rem"], t.frame(now)["eye"]))

    def test_late_flag_after_eleven_pm(self):
        t = self.make()
        late = T0.replace(hour=23, minute=10)
        t.tick(late, 0)
        self.assertTrue(t.frame(late)["late"])
        self.assertFalse(t.frame(T0)["late"])

    def test_new_day_keeps_yesterday_in_the_week(self):
        t = self.make()
        evening = T0.replace(hour=23, minute=50)
        t.tick(evening, 0)
        now = run(t, evening, 30)  # crosses midnight
        frame = t.frame(now)
        self.assertEqual(20, frame["on"])  # 00:00 - 00:20 today
        self.assertEqual(10, frame["wk"][-2])
        self.assertEqual(20, frame["wk"][-1])
        self.assertEqual(7, len(frame["wk"]))

    def test_a_long_gap_between_ticks_is_not_counted(self):
        t = self.make()
        t.tick(T0, 0)
        t.tick(T0 + timedelta(hours=2), 0)  # PC slept; input just now
        self.assertLessEqual(t.frame(T0 + timedelta(hours=2))["on"], 1)

    def test_state_survives_a_restart_on_the_same_day(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "activity.json"
            t = act.ActivityTracker({}, state_path=path)
            t.tick(T0, 0)
            now = run(t, T0, 30)
            t.save()
            again = act.ActivityTracker({}, state_path=path)
            again.load(now)
            self.assertEqual(30, again.frame(now)["on"])
            saved = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual({"day", "hours", "breaks", "longest", "history", "lock"}, set(saved))


class PresenceTests(unittest.TestCase):
    def make(self, **cfg):
        self.locks = []
        return act.PresenceLock(cfg, lock_fn=lambda: self.locks.append(1), enabled=True)

    def feed(self, p, start, seconds, link, rssi, idle):
        now = start
        while now < start + seconds:
            now += 2
            p.phone(link, rssi, now)
            p.tick(now, idle)
        return now

    def test_locks_when_the_phone_leaves_and_nobody_types(self):
        p = self.make()
        now = self.feed(p, 0, 10, True, -55, 0)
        self.feed(p, now, 40, False, None, 120)
        self.assertEqual(1, len(self.locks))

    def test_weak_signal_counts_as_away(self):
        p = self.make(rssi_away=-80)
        now = self.feed(p, 0, 10, True, -55, 0)
        self.feed(p, now, 60, True, -95, 120)
        self.assertEqual(1, len(self.locks))

    def test_no_lock_while_someone_uses_the_pc(self):
        p = self.make()
        now = self.feed(p, 0, 10, True, -55, 0)
        self.feed(p, now, 120, False, None, 5)
        self.assertEqual([], self.locks)

    def test_no_lock_for_a_short_link_drop(self):
        p = self.make()
        now = self.feed(p, 0, 10, True, -55, 0)
        now = self.feed(p, now, 20, False, None, 120)
        self.feed(p, now, 30, True, -55, 120)
        self.assertEqual([], self.locks)

    def test_no_lock_before_the_phone_was_ever_near(self):
        p = self.make()
        self.feed(p, 0, 120, False, None, 120)
        self.assertEqual([], self.locks)

    def test_no_lock_when_the_deck_stops_reporting(self):
        p = self.make()
        now = self.feed(p, 0, 10, True, -55, 0)
        p.tick(now + 120, 300)  # no phone frames for two minutes
        self.assertEqual([], self.locks)
        self.assertIsNone(p.state(now + 120)["near"])

    def test_locks_once_then_rearms_when_the_phone_returns(self):
        p = self.make()
        now = self.feed(p, 0, 10, True, -55, 0)
        now = self.feed(p, now, 120, False, None, 200)
        self.assertEqual(1, len(self.locks))
        now = self.feed(p, now, 10, True, -55, 0)
        self.feed(p, now, 60, False, None, 200)
        self.assertEqual(2, len(self.locks))

    def test_disabled_never_locks(self):
        p = self.make()
        p.enabled = False
        now = self.feed(p, 0, 10, True, -55, 0)
        self.feed(p, now, 120, False, None, 200)
        self.assertEqual([], self.locks)

    def test_state_reports_smoothed_rssi(self):
        p = self.make()
        self.feed(p, 0, 20, True, -60, 0)
        state = p.state(20)
        self.assertTrue(state["near"])
        self.assertEqual(-60, state["rssi"])
        self.assertTrue(state["on"])


class ServiceTests(unittest.TestCase):
    def make(self, tmp, idle=0.0):
        self.locks = []
        self.idle = [idle]
        self.now = [T0]
        return act.ActivityService({"activity": {}}, state_path=Path(tmp) / "a.json",
                                   idle_fn=lambda: self.idle[0], lock_fn=lambda: self.locks.append(1),
                                   clock=lambda: self.now[0], mono=lambda: (self.now[0] - T0).total_seconds())

    def test_frame_has_the_lock_block_and_version_moves_on_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = self.make(tmp)
            s.tick()
            v1, frame = s.snapshot()
            self.assertEqual("activity", frame["type"])
            self.assertIn("lk", frame)
            s.tick()
            self.assertEqual(v1, s.snapshot()[0])
            self.now[0] = T0 + timedelta(minutes=2)
            s.tick()
            self.assertNotEqual(v1, s.snapshot()[0])

    def test_device_messages(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = self.make(tmp)
            s.tick()
            self.assertTrue(s.handle({"type": "phone", "link": True, "rssi": -50}))
            self.assertTrue(s.handle({"type": "lock_set", "on": False}))
            self.assertFalse(s.snapshot()[1]["lk"]["on"])
            self.assertTrue(s.handle({"type": "break_reply", "act": "snooze"}))
            self.assertFalse(s.handle({"type": "macro", "id": 1}))

    def test_lock_setting_is_kept_across_restarts(self):
        with tempfile.TemporaryDirectory() as tmp:
            s = self.make(tmp)
            s.tick()
            s.handle({"type": "lock_set", "on": False})
            again = self.make(tmp)
            again.tick()
            self.assertFalse(again.snapshot()[1]["lk"]["on"])


if __name__ == "__main__":
    unittest.main()
