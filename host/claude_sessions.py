"""Tracks Claude Code sessions from hook payloads and builds the deck's "claude" frame.

Claude Code runs host/claude_hook.py on its hook events; the hook forwards the
event's JSON to the bridge, which feeds it to ClaudeTracker.apply(). Frame:

  {"type": "claude", "n": 5,                       sessions known (may exceed the rows)
   "s": [["alpha", "w", "Allow Bash?", 12], ...],  up to 4: name, state, detail, age (s)
   "ask": {"id": 3, "name": "alpha", "tool": "Bash", "cmd": "npm test"}}  oldest open ask

States: "w" waiting for the user, "r" running, "d" done. Names are the last
folder of the session's working directory; nothing leaves the PC except to the deck.
"""
from __future__ import annotations

import itertools
import re
from dataclasses import dataclass, field
from typing import Any
from urllib.parse import urlparse

MAX_ROWS = 4
MAX_TEXT = 60
DONE_EXPIRE_S = 2 * 3600
IDLE_EXPIRE_S = 6 * 3600
STATE_ORDER = {"w": 0, "r": 1, "d": 2}


def _cut(text: str, limit: int = MAX_TEXT) -> str:
    text = " ".join(str(text).split())
    return text if len(text) <= limit else text[: limit - 3] + "..."


def _file_name(path: Any) -> str:
    return re.split(r"[\\/]", str(path).rstrip("\\/"))[-1] if path else ""


def summarize_tool(tool_name: Any, tool_input: Any) -> str:
    """Short "Tool: target" text for the deck, e.g. "Bash: npm test" or "Edit: main.cpp"."""
    name = str(tool_name or "?")
    data = tool_input if isinstance(tool_input, dict) else {}
    if name.startswith("mcp__"):
        return _cut(name.split("__")[-1])
    target = ""
    if name == "Bash" or name == "PowerShell":
        target = str(data.get("command", "")).strip().splitlines()[0] if data.get("command") else ""
    elif data.get("file_path") or data.get("notebook_path"):
        target = _file_name(data.get("file_path") or data.get("notebook_path"))
    elif data.get("url"):
        target = urlparse(str(data["url"])).hostname or str(data["url"])
    else:
        for key in ("pattern", "query", "description", "prompt"):
            if data.get(key):
                target = str(data[key])
                break
    return _cut(f"{name}: {target}" if target else name)


@dataclass
class Session:
    name: str
    state: str = "d"
    detail: str = "Ready"
    since: float = 0.0       # when the state last changed (drives the age column)
    last_seen: float = 0.0


@dataclass
class Ask:
    id: int
    session: str
    tool: str
    cmd: str
    opened: float


@dataclass
class ClaudeTracker:
    sessions: dict[str, Session] = field(default_factory=dict)
    asks: dict[int, Ask] = field(default_factory=dict)
    version: int = 0
    _ids: Any = field(default_factory=lambda: itertools.count(1))

    def _set(self, sid: str, state: str, detail: str, now: float) -> None:
        s = self.sessions[sid]
        if s.state != state:
            s.since = now
        s.state, s.detail, s.last_seen = state, detail, now
        self.version += 1

    def _close_asks(self, sid: str) -> None:
        for ask_id in [a.id for a in self.asks.values() if a.session == sid]:
            del self.asks[ask_id]

    def apply(self, payload: dict[str, Any], now: float) -> int | None:
        """Updates from one hook payload; returns the ask id for a PermissionRequest."""
        sid = str(payload.get("session_id") or "")
        event = str(payload.get("hook_event_name") or "")
        if not sid:
            return None
        if event == "SessionEnd":
            if self.sessions.pop(sid, None):
                self._close_asks(sid)
                self.version += 1
            return None
        handlers = {"SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "PermissionRequest",
                    "Notification", "Stop", "SubagentStop"}
        if event not in handlers:
            return None
        if sid not in self.sessions:
            name = _file_name(payload.get("cwd")) or sid[:6]
            self.sessions[sid] = Session(name=_cut(name, 24), since=now, last_seen=now)
        tool = summarize_tool(payload.get("tool_name"), payload.get("tool_input"))

        if event == "PermissionRequest":
            self._close_asks(sid)
            ask = Ask(next(self._ids), sid, str(payload.get("tool_name") or "?"),
                      tool.split(": ", 1)[1] if ": " in tool else "", now)
            self.asks[ask.id] = ask
            self._set(sid, "w", _cut(f"Allow {ask.tool}?"), now)
            return ask.id
        if event == "Notification":
            kind = str(payload.get("notification_type") or "")
            if kind == "idle_prompt" and self.sessions[sid].state == "d":
                self.sessions[sid].last_seen = now
                return None
            self._set(sid, "w", _cut(payload.get("message") or "Needs your input"), now)
            return None

        if event == "PreToolUse" and any(a.session == sid for a in self.asks.values()):
            return None  # async status hook arriving after the PermissionRequest it precedes
        self._close_asks(sid)  # anything else means the prompt was answered on the PC
        if event == "SessionStart":
            self._set(sid, "d", "Ready", now)
        elif event == "UserPromptSubmit":
            self._set(sid, "r", "Working", now)
        elif event in ("PreToolUse", "PostToolUse"):
            self._set(sid, "r", tool, now)
        elif event == "Stop":
            self._set(sid, "d", "Task complete", now)
        elif event == "SubagentStop" and self.sessions[sid].state != "d":
            self._set(sid, "r", "Subagent finished", now)
        return None

    def ask_open(self, ask_id: int) -> bool:
        return ask_id in self.asks

    def resolve(self, ask_id: int) -> None:
        """The deck (or a timeout) answered the ask: the session carries on."""
        ask = self.asks.pop(ask_id, None)
        if ask and ask.session in self.sessions:
            s = self.sessions[ask.session]
            self._set(ask.session, "r", s.detail if s.state == "r" else "Working", s.last_seen)
        self.version += 1

    def expire_ask(self, ask_id: int) -> None:
        """The hook stopped waiting: Claude Code now shows its own prompt on the PC."""
        ask = self.asks.pop(ask_id, None)
        if ask and ask.session in self.sessions:
            s = self.sessions[ask.session]
            self._set(ask.session, "w", "Answer on PC", s.last_seen)
        self.version += 1

    def clear_done(self) -> None:
        done = [sid for sid, s in self.sessions.items() if s.state == "d"]
        for sid in done:
            del self.sessions[sid]
        if done:
            self.version += 1

    def _expire(self, now: float) -> None:
        old = [sid for sid, s in self.sessions.items()
               if now - s.last_seen > IDLE_EXPIRE_S or (s.state == "d" and now - s.last_seen > DONE_EXPIRE_S)]
        for sid in old:
            del self.sessions[sid]
            self._close_asks(sid)
        if old:
            self.version += 1

    def frame(self, now: float) -> dict[str, Any]:
        self._expire(now)
        ordered = sorted(self.sessions.values(),
                         key=lambda s: (STATE_ORDER[s.state], -s.last_seen if s.state != "w" else s.since))
        frame: dict[str, Any] = {
            "type": "claude",
            "n": len(ordered),
            "s": [[s.name, s.state, s.detail, int(max(0, now - s.since))] for s in ordered[:MAX_ROWS]],
        }
        if self.asks:
            ask = min(self.asks.values(), key=lambda a: a.opened)
            frame["ask"] = {"id": ask.id, "name": self.sessions[ask.session].name, "tool": ask.tool, "cmd": ask.cmd}
        return frame
