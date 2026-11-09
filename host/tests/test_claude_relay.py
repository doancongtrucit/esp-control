"""Bridge side of the Claude Code relay: localhost server plus the session tracker."""
import json
import socket
import sys
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from claude_relay import ClaudeRelay  # noqa: E402

ASK = {"hook_event_name": "PermissionRequest", "session_id": "s1", "cwd": "D:/w/alpha",
       "tool_name": "Bash", "tool_input": {"command": "npm test"}}


def send(port: int, payload: dict, read_reply: bool = False, timeout: float = 3.0):
    conn = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    conn.sendall((json.dumps(payload) + "\n").encode())
    if not read_reply:
        conn.close()
        return None
    data = b""
    while not data.endswith(b"\n"):
        chunk = conn.recv(1024)
        if not chunk:
            break
        data += chunk
    conn.close()
    return json.loads(data) if data else None


def wait_for(predicate, timeout=2.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return True
        time.sleep(0.02)
    return False


class RelayTests(unittest.TestCase):
    def setUp(self):
        self.relay = ClaudeRelay(port=0, ask_wait=1.0)
        self.relay.start()
        self.port = self.relay.port

    def tearDown(self):
        self.relay.stop()

    def frame(self):
        return self.relay.snapshot()[1]

    def ask_in_background(self):
        result = {}

        def run():
            result["reply"] = send(self.port, ASK, read_reply=True)

        thread = threading.Thread(target=run, daemon=True)
        thread.start()
        self.assertTrue(wait_for(lambda: "ask" in self.frame()))
        return thread, result

    def test_status_event_updates_the_frame(self):
        send(self.port, {"hook_event_name": "UserPromptSubmit", "session_id": "s1", "cwd": "D:/w/alpha"})
        self.assertTrue(wait_for(lambda: self.frame()["s"]))
        self.assertEqual(["alpha", "r", "Working"], self.frame()["s"][0][:3])

    def test_version_moves_on_change(self):
        v0 = self.relay.snapshot()[0]
        send(self.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/alpha"})
        self.assertTrue(wait_for(lambda: self.relay.snapshot()[0] != v0))

    def test_deck_allow_reaches_the_hook(self):
        thread, result = self.ask_in_background()
        self.relay.answer(self.frame()["ask"]["id"], True)
        thread.join(3)
        self.assertEqual({"decision": "allow"}, result["reply"])
        self.assertNotIn("ask", self.frame())

    def test_deck_deny_reaches_the_hook(self):
        thread, result = self.ask_in_background()
        self.relay.answer(self.frame()["ask"]["id"], False)
        thread.join(3)
        self.assertEqual({"decision": "deny"}, result["reply"])

    def test_always_reaches_the_hook_when_offered(self):
        result = {}
        payload = {**ASK, "always": "Bash(npm test:*)"}
        thread = threading.Thread(target=lambda: result.update(reply=send(self.port, payload, read_reply=True)),
                                  daemon=True)
        thread.start()
        self.assertTrue(wait_for(lambda: "ask" in self.frame()))
        self.relay.answer(self.frame()["ask"]["id"], True, always=True)
        thread.join(3)
        self.assertEqual({"decision": "always"}, result["reply"])

    def test_always_without_an_offer_is_a_plain_allow(self):
        thread, result = self.ask_in_background()
        self.relay.answer(self.frame()["ask"]["id"], True, always=True)
        thread.join(3)
        self.assertEqual({"decision": "allow"}, result["reply"])

    def test_always_never_turns_a_deny_into_an_allow(self):
        thread, result = self.ask_in_background()
        self.relay.answer(self.frame()["ask"]["id"], False, always=True)
        thread.join(3)
        self.assertEqual({"decision": "deny"}, result["reply"])

    def test_stale_answer_is_ignored(self):
        thread, result = self.ask_in_background()
        self.relay.answer(999, True)
        thread.join(3)
        self.assertEqual({"decision": None}, result["reply"])  # timed out instead

    def test_unanswered_ask_times_out_and_waits_on_the_pc(self):
        thread, result = self.ask_in_background()
        thread.join(3)
        self.assertEqual({"decision": None}, result["reply"])
        self.assertNotIn("ask", self.frame())
        self.assertEqual("Answer on PC", self.frame()["s"][0][2])

    def test_answer_on_the_pc_releases_the_hook(self):
        thread, result = self.ask_in_background()
        send(self.port, {"hook_event_name": "PostToolUse", "session_id": "s1", "cwd": "D:/w/alpha",
                         "tool_name": "Bash", "tool_input": {"command": "npm test"}})
        thread.join(3)
        self.assertEqual({"decision": None}, result["reply"])
        self.assertEqual("r", self.frame()["s"][0][1])

    def test_clear_done(self):
        send(self.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/alpha"})
        self.assertTrue(wait_for(lambda: self.frame()["s"]))
        self.relay.clear_done()
        self.assertEqual([], self.frame()["s"])

    def test_garbage_line_is_ignored(self):
        conn = socket.create_connection(("127.0.0.1", self.port))
        conn.sendall(b"not json\n")
        conn.close()
        send(self.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/alpha"})
        self.assertTrue(wait_for(lambda: self.frame()["s"]))

    def test_listens_on_localhost_only(self):
        self.assertEqual("127.0.0.1", self.relay.host)


class StatsTests(unittest.TestCase):
    def setUp(self):
        import tempfile
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "claude_stats.json"
        self.day = "2026-11-09"
        self.relay = ClaudeRelay(port=0, ask_wait=1.0, stats_path=self.path, today=lambda: self.day)
        self.relay.start()

    def tearDown(self):
        self.relay.stop()
        self.tmp.cleanup()

    def stats(self):
        return self.relay.stats_snapshot()[1]

    def ask_and_answer(self, allow, always=False, offer=False):
        payload = {**ASK, "always": "Bash(x:*)"} if offer else ASK
        thread = threading.Thread(target=lambda: send(self.relay.port, payload, read_reply=True), daemon=True)
        thread.start()
        self.assertTrue(wait_for(lambda: "ask" in self.relay.snapshot()[1]))
        if allow is not None:
            self.relay.answer(self.relay.snapshot()[1]["ask"]["id"], allow, always=always)
        thread.join(3)

    def test_counts_asks_and_answers(self):
        self.ask_and_answer(True)
        self.ask_and_answer(False)
        self.ask_and_answer(True, always=True, offer=True)
        self.ask_and_answer(None)  # nobody answered: Claude Code asked on the PC
        s = self.stats()
        self.assertEqual({"type": "cstats", "ask": 4, "ok": 1, "al": 1, "no": 1, "pc": 1, "done": 0}, s)

    def test_counts_finished_tasks(self):
        send(self.relay.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/a"})
        self.assertTrue(wait_for(lambda: self.stats()["done"] == 1))

    def test_a_new_day_starts_from_zero(self):
        send(self.relay.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/a"})
        self.assertTrue(wait_for(lambda: self.stats()["done"] == 1))
        self.day = "2026-11-10"
        self.assertEqual(0, self.stats()["done"])

    def test_counts_survive_a_restart(self):
        send(self.relay.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/a"})
        self.assertTrue(wait_for(lambda: self.stats()["done"] == 1))
        self.relay.stop()
        again = ClaudeRelay(port=0, stats_path=self.path, today=lambda: self.day)
        self.assertEqual(1, again.stats_snapshot()[1]["done"])

    def test_version_moves_when_counts_change(self):
        v0 = self.relay.stats_snapshot()[0]
        send(self.relay.port, {"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/a"})
        self.assertTrue(wait_for(lambda: self.relay.stats_snapshot()[0] != v0))


if __name__ == "__main__":
    unittest.main()
