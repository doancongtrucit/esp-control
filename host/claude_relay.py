"""Localhost server that receives Claude Code hook events (host/claude_hook.py).

Status events update a ClaudeTracker. A PermissionRequest connection stays open
until the deck answers (answer()), the prompt is answered on the PC (a later
event for that session), or ask_wait passes; the hook then gets one line:
{"decision": "allow" | "deny" | null}. Listens on 127.0.0.1 only.
"""
from __future__ import annotations

import json
import socket
import threading
import time
from typing import Any, Callable

from claude_sessions import ClaudeTracker


class ClaudeRelay:
    host = "127.0.0.1"

    def __init__(self, port: int = 47821, ask_wait: float = 25.0, log: Callable[[str], None] = lambda _m: None):
        self.requested_port = port
        self.port = port
        self.ask_wait = ask_wait
        self.log = log
        self.tracker = ClaudeTracker()
        self.lock = threading.Lock()
        self.answers: dict[int, str] = {}
        self.server: socket.socket | None = None
        self.running = False

    # ---- bridge side ----
    def start(self) -> None:
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.bind((self.host, self.requested_port))
        self.server.listen(16)
        self.port = self.server.getsockname()[1]
        self.running = True
        threading.Thread(target=self._accept_loop, name="claude-relay", daemon=True).start()

    def stop(self) -> None:
        self.running = False
        if self.server:
            self.server.close()

    def snapshot(self) -> tuple[int, dict[str, Any]]:
        with self.lock:
            frame = self.tracker.frame(time.monotonic())
            return self.tracker.version, frame

    def answer(self, ask_id: int, allow: bool) -> None:
        with self.lock:
            if self.tracker.ask_open(ask_id):
                self.answers[ask_id] = "allow" if allow else "deny"
                self.tracker.resolve(ask_id)

    def clear_done(self) -> None:
        with self.lock:
            self.tracker.clear_done()

    # ---- hook side ----
    def _accept_loop(self) -> None:
        while self.running:
            try:
                conn, _ = self.server.accept()
            except OSError:
                return
            threading.Thread(target=self._serve, args=(conn,), daemon=True).start()

    def _serve(self, conn: socket.socket) -> None:
        with conn:
            try:
                conn.settimeout(2.0)
                data = b""
                while not data.endswith(b"\n") and len(data) < 65536:
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    data += chunk
                payload = json.loads(data)
                if not isinstance(payload, dict):
                    return
            except (OSError, ValueError):
                return
            with self.lock:
                ask_id = self.tracker.apply(payload, time.monotonic())
            if ask_id is None:
                return
            decision = self._wait_for_answer(ask_id)
            try:
                conn.sendall((json.dumps({"decision": decision}) + "\n").encode("utf-8"))
            except OSError:
                pass

    def _wait_for_answer(self, ask_id: int) -> str | None:
        deadline = time.monotonic() + self.ask_wait
        while time.monotonic() < deadline:
            with self.lock:
                if ask_id in self.answers:
                    return self.answers.pop(ask_id)
                if not self.tracker.ask_open(ask_id):
                    return None  # answered on the PC, or the session ended
            time.sleep(0.05)
        with self.lock:
            if ask_id in self.answers:
                return self.answers.pop(ask_id)
            if self.tracker.ask_open(ask_id):
                self.tracker.expire_ask(ask_id)
        return None
