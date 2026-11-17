"""Notify API: Home Assistant / Hermes send notices, sounds and speech to the deck."""
import json
import sys
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import notify_api as na  # noqa: E402

TOKEN = "test-token-123456789"


class ParseTests(unittest.TestCase):
    def test_full_notice(self):
        frames = na.frames_for({"title": "Cửa trước", "body": "Có người bấm chuông", "sound": "doorbell",
                                "say": "Có người bấm chuông cửa", "urgent": True, "app": "Home Assistant"},
                               speak=lambda text: b"\x80" * 2000, next_id=lambda: 7)
        notify = frames[0]
        self.assertEqual({"type": "notify", "src": "pc", "app": "Home Assistant", "from": "Cửa trước",
                          "body": "Có người bấm chuông", "s": "doorbell"}, notify)
        say = frames[1]
        self.assertEqual({"type": "say", "id": 7, "rate": 8000, "len": 2000, "urgent": True}, say)
        pcm = frames[2:]
        self.assertEqual(2, len(pcm))  # 1024-byte chunks
        self.assertEqual([0, 1024], [f["o"] for f in pcm])

    def test_sound_only(self):
        frames = na.frames_for({"sound": "chime"}, speak=None, next_id=lambda: 1)
        self.assertEqual([{"type": "sound", "s": "chime", "urgent": False}], frames)

    def test_unknown_sound_falls_back_to_chime(self):
        frames = na.frames_for({"title": "x", "sound": "trumpet"}, speak=None, next_id=lambda: 1)
        self.assertEqual("chime", frames[0]["s"])

    def test_speech_is_skipped_without_a_voice(self):
        frames = na.frames_for({"say": "xin chào"}, speak=None, next_id=lambda: 1)
        self.assertEqual([], frames)

    def test_long_text_is_cut(self):
        frames = na.frames_for({"title": "t" * 500, "body": "b" * 5000}, speak=None, next_id=lambda: 1)
        self.assertLessEqual(len(frames[0]["from"]), 60)
        self.assertLessEqual(len(frames[0]["body"]), 300)

    def test_speech_longer_than_the_deck_buffer_is_cut(self):
        frames = na.frames_for({"say": "dài"}, speak=lambda text: b"\x80" * 100000, next_id=lambda: 2)
        self.assertEqual(na.SPEECH_MAX, frames[0]["len"])


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.sent = []
        self.api = na.NotifyApi(token=TOKEN, port=0, host="127.0.0.1", deliver=self.sent.append, speak=None)
        self.api.start()
        self.url = f"http://127.0.0.1:{self.api.port}/notify"

    def tearDown(self):
        self.api.stop()

    def post(self, body, token=TOKEN):
        req = urllib.request.Request(self.url, data=json.dumps(body).encode(), method="POST",
                                     headers={"Content-Type": "application/json", "Authorization": f"Bearer {token}"})
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status

    def test_accepts_a_notice_with_the_token(self):
        self.assertEqual(200, self.post({"title": "Máy giặt", "body": "Xong rồi", "sound": "chime"}))
        self.assertEqual("notify", self.sent[0]["type"])

    def test_rejects_a_wrong_token(self):
        with self.assertRaises(urllib.error.HTTPError) as ctx:
            self.post({"title": "x"}, token="nope")
        self.assertEqual(401, ctx.exception.code)
        self.assertEqual([], self.sent)

    def test_rejects_bad_json(self):
        req = urllib.request.Request(self.url, data=b"{not json", method="POST",
                                     headers={"Authorization": f"Bearer {TOKEN}"})
        with self.assertRaises(urllib.error.HTTPError) as ctx:
            urllib.request.urlopen(req, timeout=5)
        self.assertEqual(400, ctx.exception.code)


class StartTests(unittest.TestCase):
    def test_off_without_a_token(self):
        self.assertIsNone(na.start_notify_api({"notify_api": {"enabled": True, "token": ""}}, deliver=lambda f: None))


if __name__ == "__main__":
    unittest.main()
