import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pc_notifications  # noqa: E402


class FakeSource:
    def __init__(self, batches):
        self.batches = list(batches)

    def __call__(self):
        return self.batches.pop(0) if self.batches else []


class WatcherTests(unittest.TestCase):
    def test_existing_toasts_are_not_forwarded_on_start(self):
        seen = []
        source = FakeSource([[(1, "Teams", "A", "old")], [(1, "Teams", "A", "old")]])
        watcher = pc_notifications.PcNotificationWatcher(lambda *t: seen.append(t), source=source)
        watcher.poll_once()
        watcher.poll_once()
        self.assertEqual([], seen)

    def test_new_toast_is_forwarded_once(self):
        seen = []
        source = FakeSource([
            [(1, "Teams", "A", "old")],
            [(1, "Teams", "A", "old"), (2, "Outlook", "Boss", "Họp lúc 3h")],
            [(1, "Teams", "A", "old"), (2, "Outlook", "Boss", "Họp lúc 3h")],
        ])
        watcher = pc_notifications.PcNotificationWatcher(lambda *t: seen.append(t), source=source)
        for _ in range(3):
            watcher.poll_once()
        self.assertEqual([("Outlook", "Boss", "Họp lúc 3h")], seen)

    def test_source_errors_do_not_stop_the_watcher(self):
        calls = {"n": 0}

        def flaky():
            calls["n"] += 1
            if calls["n"] == 2:
                raise OSError("listener unavailable")
            return [(calls["n"], "App", "T", "B")]

        seen = []
        watcher = pc_notifications.PcNotificationWatcher(lambda *t: seen.append(t), source=flaky)
        watcher.poll_once()  # baseline
        watcher.poll_once()  # error swallowed
        watcher.poll_once()  # id 3 is new
        self.assertEqual([("App", "T", "B")], seen)


if __name__ == "__main__":
    unittest.main()
