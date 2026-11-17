"""Notify API: Home Assistant, Hermes or any script on the LAN sends notices to the deck.

  POST http://<pc>:47822/notify
  Authorization: Bearer <notify_api.token from host/config.json>
  {"title": "Front door", "body": "Someone rang", "sound": "doorbell",
   "say": "Có người bấm chuông cửa", "urgent": true, "app": "Home Assistant"}

All fields are optional. "title"/"body" show a notification on the deck;
"sound" picks a tone (chime, doorbell, alert, reminder, timer, alarm, ...);
"say" is spoken in Vietnamese (text-to-speech on the PC, see speech.py);
"urgent" rings through the deck's quiet hours. The token is required.
"""
from __future__ import annotations

import base64
import hmac
import itertools
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Callable

SOUNDS = {"chime", "doorbell", "alert", "reminder", "timer", "alarm", "focus", "break",
          "claude_ask", "claude_done", "claude_fail", "phone", "pc", "test"}
PCM_CHUNK = 1024
SPEECH_MAX = 48000  # what the deck buffers (6 s at 8 kHz)
SPEECH_RATE = 8000


def frames_for(message: dict[str, Any], speak: Callable[[str], bytes] | None,
               next_id: Callable[[], int]) -> list[dict[str, Any]]:
    """Deck frames for one notice: notify (with its sound), or a bare sound, then speech."""
    frames: list[dict[str, Any]] = []
    urgent = bool(message.get("urgent"))
    sound = str(message.get("sound") or "")
    if sound and sound not in SOUNDS:
        sound = "chime"
    title, body = str(message.get("title") or ""), str(message.get("body") or "")
    if title or body:
        frames.append({"type": "notify", "src": "pc", "app": str(message.get("app") or "Home")[:30],
                       "from": title[:60], "body": body[:300], "s": sound or "chime"})
    elif sound:
        frames.append({"type": "sound", "s": sound, "urgent": urgent})
    text = str(message.get("say") or "").strip()
    if text and speak is not None:
        pcm = speak(text[:300])[:SPEECH_MAX]
        if pcm:
            clip = next_id()
            frames.append({"type": "say", "id": clip, "rate": SPEECH_RATE, "len": len(pcm), "urgent": urgent})
            for offset in range(0, len(pcm), PCM_CHUNK):
                frames.append({"type": "pcm", "id": clip, "o": offset,
                               "d": base64.b64encode(pcm[offset:offset + PCM_CHUNK]).decode("ascii")})
    return frames


class NotifyApi:
    def __init__(self, token: str, deliver: Callable[[dict[str, Any]], None], speak: Callable[[str], bytes] | None,
                 port: int = 47822, host: str = "0.0.0.0", log: Callable[[str], None] = lambda _m: None):
        self.token = token
        self.deliver = deliver
        self.speak = speak
        self.log = log
        ids = itertools.count(1)
        api = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_args):  # quiet: no request lines in the bridge log
                pass

            def _reply(self, code: int, text: str) -> None:
                body = json.dumps({"ok": code == 200, "message": text}).encode()
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_POST(self):  # noqa: N802 (http.server API)
                try:  # read the body first, so an early reply does not reset the connection
                    raw = self.rfile.read(min(int(self.headers.get("Content-Length", "0")), 16384))
                except ValueError:
                    raw = b""
                if self.path.rstrip("/") != "/notify":
                    return self._reply(404, "use POST /notify")
                given = self.headers.get("Authorization", "").removeprefix("Bearer ").strip()
                if not hmac.compare_digest(given.encode(), api.token.encode()):
                    return self._reply(401, "bad token")
                try:
                    message = json.loads(raw.decode("utf-8") or "{}")
                    if not isinstance(message, dict):
                        raise ValueError
                except (ValueError, UnicodeDecodeError):
                    return self._reply(400, "body must be a JSON object")
                frames = frames_for(message, api.speak, lambda: next(ids))
                if not frames:
                    return self._reply(400, "nothing to show, play or say")
                for frame in frames:
                    api.deliver(frame)
                api.log(f"Notify API: {len(frames)} frame(s) for the deck")
                return self._reply(200, "sent")

        self.server = ThreadingHTTPServer((host, port), Handler)
        self.port = self.server.server_address[1]

    def start(self) -> None:
        threading.Thread(target=self.server.serve_forever, name="notify-api", daemon=True).start()

    def stop(self) -> None:
        self.server.shutdown()
        self.server.server_close()


def start_notify_api(config: dict[str, Any], deliver: Callable[[dict[str, Any]], None],
                     log: Callable[[str], None] = lambda _m: None) -> NotifyApi | None:
    cfg = config.get("notify_api") or {}
    token = str(cfg.get("token") or "")
    if not cfg.get("enabled", True) or len(token) < 16:
        return None
    speak = None
    if cfg.get("speech", True):
        try:
            from speech import make_speaker

            speak = make_speaker(str(cfg.get("voice") or "vi-VN-HoaiMyNeural"), log)
        except ImportError:
            log("Notify API: speech off (pip install edge-tts miniaudio)")
    try:
        api = NotifyApi(token, deliver, speak, port=int(cfg.get("port", 47822)), log=log)
    except OSError as exc:
        log(f"Notify API off: cannot open its port ({exc})")
        return None
    api.start()
    log(f"Notify API listening on port {api.port}")
    return api
