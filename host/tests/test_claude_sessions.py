"""Claude Code session tracking: hook payloads in, deck frames out."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from claude_sessions import ClaudeTracker, summarize_tool  # noqa: E402


def hook(event: str, sid: str = "s1", cwd: str = "C:/work/alpha", **extra):
    return {"hook_event_name": event, "session_id": sid, "cwd": cwd, **extra}


class SummarizeToolTests(unittest.TestCase):
    def test_bash_shows_first_line_of_the_command(self):
        self.assertEqual("Bash: npm test", summarize_tool("Bash", {"command": "npm test\necho done"}))

    def test_file_tools_show_only_the_file_name(self):
        self.assertEqual("Edit: main.cpp", summarize_tool("Edit", {"file_path": "D:/repo/src/main.cpp"}))
        self.assertEqual("Write: a.py", summarize_tool("Write", {"file_path": "C:\\x\\a.py"}))

    def test_web_fetch_shows_the_host(self):
        self.assertEqual("WebFetch: example.com", summarize_tool("WebFetch", {"url": "https://example.com/a?b=1"}))

    def test_mcp_tools_use_the_short_name(self):
        self.assertEqual("navigate", summarize_tool("mcp__claude-in-chrome__navigate", {}))

    def test_long_text_is_cut(self):
        text = summarize_tool("Bash", {"command": "x" * 200})
        self.assertLessEqual(len(text), 60)
        self.assertTrue(text.endswith("..."))

    def test_unknown_tool_is_its_name(self):
        self.assertEqual("Mystery", summarize_tool("Mystery", None))


class TrackerTests(unittest.TestCase):
    def setUp(self):
        self.t = ClaudeTracker()

    def rows(self, now=100.0):
        return self.t.frame(now)["s"]

    def test_prompt_marks_the_session_running(self):
        self.t.apply(hook("UserPromptSubmit"), 10.0)
        self.assertEqual([["alpha", "r", "Working", 90]], self.rows())

    def test_tool_use_shows_what_it_is_doing(self):
        self.t.apply(hook("PreToolUse", tool_name="Bash", tool_input={"command": "pytest"}), 10.0)
        self.assertEqual("Bash: pytest", self.rows()[0][2])

    def test_stop_marks_it_done(self):
        self.t.apply(hook("UserPromptSubmit"), 10.0)
        self.t.apply(hook("Stop"), 20.0)
        self.assertEqual(["alpha", "d", "Task complete", 80], self.rows()[0])

    def test_permission_request_waits_and_opens_an_ask(self):
        ask = self.t.apply(hook("PermissionRequest", tool_name="Bash", tool_input={"command": "rm -rf build"}), 10.0)
        self.assertIsNotNone(ask)
        frame = self.t.frame(12.0)
        self.assertEqual("w", frame["s"][0][1])
        self.assertEqual({"id": ask, "name": "alpha", "tool": "Bash", "cmd": "rm -rf build"}, frame["ask"])

    def test_permission_notification_without_the_hook_still_waits(self):
        self.t.apply(hook("Notification", notification_type="permission_prompt",
                          message="Claude needs your permission to use Bash"), 10.0)
        row = self.rows()[0]
        self.assertEqual("w", row[1])
        self.assertIn("permission", row[2])
        self.assertNotIn("ask", self.t.frame(11.0))

    def test_idle_notification_does_not_change_a_done_session(self):
        self.t.apply(hook("Stop"), 10.0)
        self.t.apply(hook("Notification", notification_type="idle_prompt", message="waiting"), 70.0)
        self.assertEqual("d", self.rows()[0][1])

    def test_any_later_event_closes_the_ask(self):
        ask = self.t.apply(hook("PermissionRequest", tool_name="Bash", tool_input={"command": "ls"}), 10.0)
        self.t.apply(hook("PostToolUse", tool_name="Bash", tool_input={"command": "ls"}), 15.0)
        self.assertFalse(self.t.ask_open(ask))
        self.assertNotIn("ask", self.t.frame(16.0))
        self.assertEqual("r", self.rows()[0][1])

    def test_late_pre_tool_use_keeps_the_ask_open(self):
        # status hooks run async, so PreToolUse for the same call can arrive after PermissionRequest
        ask = self.t.apply(hook("PermissionRequest", tool_name="Bash", tool_input={"command": "ls"}), 10.0)
        self.t.apply(hook("PreToolUse", tool_name="Bash", tool_input={"command": "ls"}), 10.5)
        self.assertTrue(self.t.ask_open(ask))
        self.assertEqual("w", self.rows()[0][1])

    def test_resolve_closes_the_ask_and_the_session_runs_again(self):
        ask = self.t.apply(hook("PermissionRequest", tool_name="Bash", tool_input={"command": "ls"}), 10.0)
        self.t.resolve(ask)
        self.assertFalse(self.t.ask_open(ask))
        self.assertEqual("r", self.rows()[0][1])

    def test_expired_ask_leaves_the_session_waiting_on_the_pc(self):
        ask = self.t.apply(hook("PermissionRequest", tool_name="Bash", tool_input={"command": "ls"}), 10.0)
        self.t.expire_ask(ask)
        self.assertFalse(self.t.ask_open(ask))
        self.assertEqual(["alpha", "w", "Answer on PC"], self.rows()[0][:3])

    def test_oldest_ask_is_shown_first(self):
        first = self.t.apply(hook("PermissionRequest", sid="a", cwd="/p/one", tool_name="Bash",
                                  tool_input={"command": "1"}), 10.0)
        self.t.apply(hook("PermissionRequest", sid="b", cwd="/p/two", tool_name="Bash",
                          tool_input={"command": "2"}), 11.0)
        self.assertEqual(first, self.t.frame(12.0)["ask"]["id"])

    def test_waiting_sessions_sort_first_then_running_then_done(self):
        self.t.apply(hook("Stop", sid="d", cwd="/p/done"), 30.0)
        self.t.apply(hook("UserPromptSubmit", sid="r", cwd="/p/run"), 20.0)
        self.t.apply(hook("PermissionRequest", sid="w", cwd="/p/wait", tool_name="Bash",
                          tool_input={"command": "x"}), 10.0)
        self.assertEqual(["wait", "run", "done"], [row[0] for row in self.rows()])

    def test_at_most_four_sessions_are_sent(self):
        for i in range(6):
            self.t.apply(hook("UserPromptSubmit", sid=str(i), cwd=f"/p/s{i}"), float(i))
        self.assertEqual(4, len(self.rows()))
        self.assertEqual(6, self.t.frame(100.0)["n"])

    def test_session_end_removes_it(self):
        self.t.apply(hook("UserPromptSubmit"), 10.0)
        self.t.apply(hook("SessionEnd"), 11.0)
        self.assertEqual([], self.rows())

    def test_clear_done_keeps_active_sessions(self):
        self.t.apply(hook("Stop", sid="d", cwd="/p/done"), 10.0)
        self.t.apply(hook("UserPromptSubmit", sid="r", cwd="/p/run"), 10.0)
        self.t.clear_done()
        self.assertEqual(["run"], [row[0] for row in self.rows()])

    def test_old_sessions_expire(self):
        self.t.apply(hook("Stop", sid="d", cwd="/p/done"), 0.0)
        self.t.apply(hook("UserPromptSubmit", sid="r", cwd="/p/run"), 0.0)
        self.assertEqual(["run"], [row[0] for row in self.t.frame(2 * 3600 + 1)["s"]])
        self.assertEqual([], self.t.frame(6 * 3600 + 1)["s"])

    def test_version_changes_only_when_something_changes(self):
        v0 = self.t.version
        self.t.apply(hook("UserPromptSubmit"), 10.0)
        v1 = self.t.version
        self.assertNotEqual(v0, v1)
        self.t.apply({"hook_event_name": "Bogus"}, 11.0)
        self.assertEqual(v1, self.t.version)

    def test_payload_without_session_is_ignored(self):
        self.assertIsNone(self.t.apply({"hook_event_name": "UserPromptSubmit"}, 1.0))
        self.assertEqual([], self.rows())

    def test_blank_cwd_falls_back_to_a_short_session_id(self):
        self.t.apply(hook("UserPromptSubmit", sid="abcdef123456", cwd=""), 1.0)
        self.assertEqual("abcdef", self.rows()[0][0])


if __name__ == "__main__":
    unittest.main()
