"""Text-to-speech wrapper: retries the voice service, gives up quietly."""
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import speech  # noqa: E402


class RetryTests(unittest.TestCase):
    def test_a_failed_request_is_tried_again(self):
        calls = []

        async def synth(text):
            calls.append(text)
            if len(calls) < 2:
                raise RuntimeError("no audio received")
            return b"mp3"

        with patch.object(speech, "to_deck_pcm", return_value=b"\x80" * 10), patch.object(speech.time, "sleep"):
            speak = speech.make_speaker(synth=synth)
            self.assertEqual(b"\x80" * 10, speak("xin chào"))
        self.assertEqual(2, len(calls))

    def test_gives_up_after_three_tries(self):
        logs = []

        async def synth(_text):
            raise RuntimeError("down")

        with patch.object(speech.time, "sleep"):
            self.assertEqual(b"", speech.make_speaker(log=logs.append, synth=synth)("xin chào"))
        self.assertEqual(["Speech failed: RuntimeError"], logs)


if __name__ == "__main__":
    unittest.main()
