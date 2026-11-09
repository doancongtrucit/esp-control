"""Claude Code hook relay: payload trimming, decision output, and fail-safe behaviour."""
import io
import json
import socket
import sys
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import claude_hook  # noqa: E402


class FakeBridge:
    """One-shot localhost server: records the line it gets, answers with `reply` (or nothing)."""

    def __init__(self, reply: bytes | None):
        self.server = socket.socket()
        self.server.bind(("127.0.0.1", 0))
        self.server.listen(1)
        self.port = self.server.getsockname()[1]
        self.received = b""
        self.reply = reply
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def _serve(self):
        conn, _ = self.server.accept()
        with conn:
            while not self.received.endswith(b"\n"):
                chunk = conn.recv(4096)
                if not chunk:
                    break
                self.received += chunk
            if self.reply is not None:
                conn.sendall(self.reply)
            else:
                conn.recv(1)  # hold the line open until the hook gives up
        self.server.close()


def free_port() -> int:
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def run(payload: dict, port: int, wait: float = 1.0) -> tuple[int, str]:
    out = io.StringIO()
    code = claude_hook.main(["--port", str(port), "--wait", str(wait)], io.StringIO(json.dumps(payload)), out)
    return code, out.getvalue()


ASK = {"hook_event_name": "PermissionRequest", "session_id": "s1", "cwd": "D:/w/alpha",
       "tool_name": "Bash", "tool_input": {"command": "npm test"}, "transcript_path": "C:/secret/t.jsonl"}


class TrimTests(unittest.TestCase):
    def test_drops_transcript_and_long_fields(self):
        trimmed = claude_hook.trim_payload({**ASK, "tool_response": "x" * 5000, "prompt": "y" * 5000,
                                            "last_assistant_message": "z" * 5000})
        self.assertNotIn("transcript_path", trimmed)
        self.assertNotIn("tool_response", trimmed)
        self.assertNotIn("prompt", trimmed)
        self.assertNotIn("last_assistant_message", trimmed)
        self.assertEqual("npm test", trimmed["tool_input"]["command"])

    def test_long_tool_input_values_are_cut(self):
        trimmed = claude_hook.trim_payload({**ASK, "tool_input": {"command": "a" * 900, "content": "b" * 900}})
        self.assertLessEqual(len(trimmed["tool_input"]["command"]), 200)
        self.assertNotIn("content", trimmed["tool_input"])


SUGGESTIONS = [{"type": "addRules", "rules": [{"toolName": "Bash", "ruleContent": "npm test:*"}], "behavior": "allow", "destination": "localSettings"}]


class SuggestionTests(unittest.TestCase):
    def test_rule_suggestion_becomes_a_short_label(self):
        self.assertEqual("Bash(npm test:*)", claude_hook.always_label(SUGGESTIONS))

    def test_mode_suggestion_label(self):
        self.assertEqual("mode: acceptEdits", claude_hook.always_label([{"type": "setMode", "mode": "acceptEdits"}]))

    def test_no_suggestion_no_label(self):
        self.assertEqual("", claude_hook.always_label(None))
        self.assertEqual("", claude_hook.always_label([]))

    def test_trim_sends_the_label_not_the_suggestions(self):
        trimmed = claude_hook.trim_payload({**ASK, "permission_suggestions": SUGGESTIONS})
        self.assertNotIn("permission_suggestions", trimmed)
        self.assertEqual("Bash(npm test:*)", trimmed["always"])


class DecisionOutputTests(unittest.TestCase):
    def test_allow(self):
        self.assertEqual({"hookSpecificOutput": {"hookEventName": "PermissionRequest",
                                                 "decision": {"behavior": "allow"}}},
                         json.loads(claude_hook.decision_output("allow")))

    def test_deny_carries_a_message(self):
        decision = json.loads(claude_hook.decision_output("deny"))["hookSpecificOutput"]["decision"]
        self.assertEqual("deny", decision["behavior"])
        self.assertIn("Control Deck", decision["message"])

    def test_always_allows_and_adds_the_suggested_rules(self):
        decision = json.loads(claude_hook.decision_output("always", SUGGESTIONS))["hookSpecificOutput"]["decision"]
        self.assertEqual({"behavior": "allow", "updatedPermissions": SUGGESTIONS}, decision)

    def test_always_without_suggestions_is_a_plain_allow(self):
        decision = json.loads(claude_hook.decision_output("always", None))["hookSpecificOutput"]["decision"]
        self.assertEqual({"behavior": "allow"}, decision)

    def test_anything_else_prints_nothing(self):
        self.assertEqual("", claude_hook.decision_output(None))
        self.assertEqual("", claude_hook.decision_output("maybe"))


class MainTests(unittest.TestCase):
    def test_status_event_is_forwarded_and_prints_nothing(self):
        bridge = FakeBridge(reply=b"")
        code, out = run({"hook_event_name": "Stop", "session_id": "s1", "cwd": "D:/w/alpha"}, bridge.port)
        bridge.thread.join(2)
        self.assertEqual((0, ""), (code, out))
        self.assertEqual("Stop", json.loads(bridge.received)["hook_event_name"])

    def test_deck_allow_becomes_the_hook_decision(self):
        bridge = FakeBridge(reply=b'{"decision": "allow"}\n')
        code, out = run(ASK, bridge.port)
        self.assertEqual(0, code)
        self.assertEqual("allow", json.loads(out)["hookSpecificOutput"]["decision"]["behavior"])
        self.assertNotIn("transcript_path", json.loads(bridge.received))

    def test_deck_always_uses_the_payloads_suggestions(self):
        bridge = FakeBridge(reply=b'{"decision": "always"}\n')
        _, out = run({**ASK, "permission_suggestions": SUGGESTIONS}, bridge.port)
        decision = json.loads(out)["hookSpecificOutput"]["decision"]
        self.assertEqual(SUGGESTIONS, decision["updatedPermissions"])

    def test_deck_deny_becomes_the_hook_decision(self):
        bridge = FakeBridge(reply=b'{"decision": "deny"}\n')
        _, out = run(ASK, bridge.port)
        self.assertEqual("deny", json.loads(out)["hookSpecificOutput"]["decision"]["behavior"])

    def test_no_answer_in_time_falls_back_to_the_normal_prompt(self):
        bridge = FakeBridge(reply=None)
        code, out = run(ASK, bridge.port, wait=0.3)
        self.assertEqual((0, ""), (code, out))

    def test_bridge_not_running_falls_back(self):
        self.assertEqual((0, ""), run(ASK, free_port()))

    def test_garbage_stdin_never_fails(self):
        out = io.StringIO()
        self.assertEqual(0, claude_hook.main(["--port", str(free_port())], io.StringIO("not json"), out))
        self.assertEqual("", out.getvalue())


if __name__ == "__main__":
    unittest.main()
