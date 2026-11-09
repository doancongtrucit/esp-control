"""Claude Code hook: forwards the event to the Control Deck bridge on 127.0.0.1.

Claude Code runs this on its hook events (see docs/CLAUDE_CODE.md), with the
event JSON on stdin. Status events are fire-and-forget. For PermissionRequest
the hook waits up to --wait seconds for the deck's Allow/Deny and prints the
decision; with no answer, no bridge, or any error it prints nothing and exits 0,
so Claude Code falls back to its normal permission prompt. It never fails.

    python claude_hook.py --port 47821 --wait 25
"""
from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from typing import Any, TextIO

DEFAULT_PORT = 47821
DROP_FIELDS = {"transcript_path", "agent_transcript_path", "tool_response", "prompt",
               "last_assistant_message", "permission_suggestions", "scratchpad_dir"}
KEEP_INPUT = ("command", "file_path", "notebook_path", "url", "pattern", "query", "description")
MAX_VALUE = 200


def trim_payload(payload: dict[str, Any]) -> dict[str, Any]:
    """Only what the deck shows: no transcript paths, prompts, responses or file contents."""
    trimmed = {k: v for k, v in payload.items() if k not in DROP_FIELDS}
    tool_input = payload.get("tool_input")
    if isinstance(tool_input, dict):
        trimmed["tool_input"] = {k: str(tool_input[k])[:MAX_VALUE] for k in KEEP_INPUT if tool_input.get(k)}
    for key in ("message", "title"):
        if key in trimmed:
            trimmed[key] = str(trimmed[key])[:MAX_VALUE]
    return trimmed


def decision_output(decision: Any) -> str:
    if decision == "allow":
        body: dict[str, Any] = {"behavior": "allow"}
    elif decision == "deny":
        body = {"behavior": "deny", "message": "Denied on the Control Deck."}
    else:
        return ""
    return json.dumps({"hookSpecificOutput": {"hookEventName": "PermissionRequest", "decision": body}})


def relay(payload: dict[str, Any], port: int, wait: float) -> Any:
    """Sends one JSON line; for a permission request, returns the bridge's decision (or None)."""
    asking = payload.get("hook_event_name") == "PermissionRequest"
    deadline = time.monotonic() + (wait if asking else 1.0)
    with socket.create_connection(("127.0.0.1", port), timeout=0.5) as conn:
        conn.sendall((json.dumps(trim_payload(payload)) + "\n").encode("utf-8"))
        if not asking:
            return None
        reply = b""
        while not reply.endswith(b"\n"):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            conn.settimeout(remaining)
            chunk = conn.recv(1024)
            if not chunk:
                return None
            reply += chunk
    return json.loads(reply).get("decision")


def main(argv: list[str] | None = None, stdin: TextIO = sys.stdin, stdout: TextIO = sys.stdout) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--wait", type=float, default=25.0, help="seconds to wait for Allow/Deny")
    try:
        args = parser.parse_args(argv)
        payload = json.loads(stdin.read())
        if not isinstance(payload, dict):
            return 0
        text = decision_output(relay(payload, args.port, args.wait))
        if text:
            stdout.write(text)
    except (Exception, SystemExit):  # never block Claude Code
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
