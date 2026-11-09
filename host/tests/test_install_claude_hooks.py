"""Merging the Control Deck hooks into a Claude Code settings.json and taking them out again."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import install_claude_hooks as ich  # noqa: E402

PY = "C:\\deck\\.venv\\Scripts\\python.exe"
SCRIPT = "C:\\deck\\claude_hook.py"


def ours(group):
    return [h for h in group["hooks"] if ich.is_deck_hook(h)]


class MergeTests(unittest.TestCase):
    def test_adds_status_hooks_async_and_permission_hook_sync(self):
        settings = ich.add_hooks({}, PY, SCRIPT, port=47821, wait=25)
        for event in ich.STATUS_EVENTS:
            handler = settings["hooks"][event][0]["hooks"][0]
            self.assertEqual(PY, handler["command"])
            self.assertEqual([SCRIPT, "--port", "47821"], handler["args"])
            self.assertTrue(handler["async"])
        ask = settings["hooks"]["PermissionRequest"][0]
        self.assertEqual("*", ask["matcher"])
        handler = ask["hooks"][0]
        self.assertNotIn("async", handler)
        self.assertEqual([SCRIPT, "--port", "47821", "--wait", "27"], handler["args"])
        self.assertEqual(40, handler["timeout"])

    def test_keeps_the_users_other_settings_and_hooks(self):
        mine = {"type": "command", "command": "notify.exe"}
        settings = ich.add_hooks({"model": "opus", "hooks": {"Stop": [{"hooks": [mine]}]}}, PY, SCRIPT, 47821, 25)
        self.assertEqual("opus", settings["model"])
        handlers = [h for g in settings["hooks"]["Stop"] for h in g["hooks"]]
        self.assertIn(mine, handlers)
        self.assertEqual(1, sum(ich.is_deck_hook(h) for h in handlers))

    def test_running_twice_does_not_duplicate(self):
        once = ich.add_hooks({}, PY, SCRIPT, 47821, 25)
        twice = ich.add_hooks(json.loads(json.dumps(once)), PY, SCRIPT, 47821, 25)
        self.assertEqual(once, twice)

    def test_remove_takes_out_only_deck_hooks(self):
        mine = {"type": "command", "command": "notify.exe"}
        settings = ich.add_hooks({"hooks": {"Stop": [{"hooks": [mine]}]}}, PY, SCRIPT, 47821, 25)
        cleaned = ich.remove_hooks(settings)
        self.assertEqual({"Stop": [{"hooks": [mine]}]}, cleaned["hooks"])

    def test_remove_drops_an_empty_hooks_key(self):
        self.assertNotIn("hooks", ich.remove_hooks(ich.add_hooks({}, PY, SCRIPT, 47821, 25)))


class FileTests(unittest.TestCase):
    def test_install_writes_a_backup_first(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "settings.json"
            path.write_text('{"model": "opus"}', encoding="utf-8")
            ich.update_file(path, lambda s: ich.add_hooks(s, PY, SCRIPT, 47821, 25))
            backups = list(Path(tmp).glob("settings.json.bak-*"))
            self.assertEqual(1, len(backups))
            self.assertEqual({"model": "opus"}, json.loads(backups[0].read_text(encoding="utf-8")))
            self.assertIn("PermissionRequest", json.loads(path.read_text(encoding="utf-8"))["hooks"])

    def test_missing_file_is_created(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sub" / "settings.json"
            ich.update_file(path, lambda s: ich.add_hooks(s, PY, SCRIPT, 47821, 25))
            self.assertTrue(path.exists())

    def test_invalid_json_is_left_alone(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "settings.json"
            path.write_text("{oops", encoding="utf-8")
            with self.assertRaises(ValueError):
                ich.update_file(path, lambda s: ich.add_hooks(s, PY, SCRIPT, 47821, 25))
            self.assertEqual("{oops", path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
