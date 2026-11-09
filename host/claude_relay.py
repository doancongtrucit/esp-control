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
from datetime import date
from pathlib import Path
from typing import Any, Callable

from claude_sessions import ClaudeTracker


class ClaudeRelay:
    host = "127.0.0.1"

    STAT_KEYS = ("ask", "ok", "al", "no", "pc", "done")  # asks; allow, always, deny on the deck; on the PC; tasks

    def __init__(self, port: int = 47821, ask_wait: float = 25.0, log: Callable[[str], None] = lambda _m: None,
                 stats_path: Path | None = None, today: Callable[[], str] = lambda: date.today().isoformat()):
        self.requested_port = port
        self.port = port
        self.ask_wait = ask_wait
        self.log = log
        self.tracker = ClaudeTracker()
        self.lock = threading.Lock()
        self.answers: dict[int, str] = {}
        self.server: socket.socket | None = None
        self.running = False
        self.stats_path = stats_path
        self.today = today
        self.stats_version = 0
        self.stats: dict[str, Any] = self._load_stats()

    # ---- daily counts (Stats tab), kept in stats_path across restarts ----
    def _load_stats(self) -> dict[str, Any]:
        try:
            saved = json.loads(self.stats_path.read_text(encoding="utf-8")) if self.stats_path else {}
        except (OSError, ValueError):
            saved = {}
        if saved.get("date") != self.today():
            saved = {"date": self.today()}
        return {"date": saved["date"], **{k: int(saved.get(k, 0)) for k in self.STAT_KEYS}}

    def _count(self, key: str) -> None:
        """Caller holds the lock."""
        if self.stats["date"] != self.today():
            self.stats = {"date": self.today(), **{k: 0 for k in self.STAT_KEYS}}
        self.stats[key] += 1
        self.stats_version += 1
        if self.stats_path:
            try:
                self.stats_path.write_text(json.dumps(self.stats), encoding="utf-8")
            except OSError:
                pass

    def stats_snapshot(self) -> tuple[int, dict[str, Any]]:
        with self.lock:
            if self.stats["date"] != self.today():
                self.stats = {"date": self.today(), **{k: 0 for k in self.STAT_KEYS}}
                self.stats_version += 1
            return self.stats_version, {"type": "cstats", **{k: self.stats[k] for k in self.STAT_KEYS}}

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

    def answer(self, ask_id: int, allow: bool, always: bool = False) -> None:
        """Deck answer; "always" (allow + Claude Code's suggested rule) only when that ask offered one."""
        with self.lock:
            ask = self.tracker.asks.get(ask_id)
            if ask is None:
                return
            if not allow:
                decision = "deny"
            else:
                decision = "always" if always and ask.always else "allow"
            self.answers[ask_id] = decision
            self._count({"deny": "no", "allow": "ok", "always": "al"}[decision])
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
                if payload.get("hook_event_name") == "Stop":
                    self._count("done")
                if ask_id is not None:
                    self._count("ask")
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
                    self._count("pc")
                    return None  # answered on the PC, or the session ended
            time.sleep(0.05)
        with self.lock:
            if ask_id in self.answers:
                return self.answers.pop(ask_id)
            if self.tracker.ask_open(ask_id):
                self.tracker.expire_ask(ask_id)
                self._count("pc")
        return None
