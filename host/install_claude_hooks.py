"""Adds (or removes) the Control Deck hooks in Claude Code's user settings.

    python install_claude_hooks.py            # ~/.claude/settings.json, port and wait from config.json
    python install_claude_hooks.py --remove

Status events run claude_hook.py in the background (async); PermissionRequest
runs it in the foreground so the deck can answer. A timestamped backup of the
settings file is written before every change. Other hooks and settings are kept.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import time
from pathlib import Path
from typing import Any, Callable

HERE = Path(__file__).resolve().parent
STATUS_EVENTS = ("SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "Notification",
                 "Stop", "SubagentStop", "SessionEnd")
TOOL_EVENTS = {"PreToolUse", "PostToolUse"}


def is_deck_hook(handler: Any) -> bool:
    return isinstance(handler, dict) and any(str(a).endswith("claude_hook.py") for a in handler.get("args", []))


def remove_hooks(settings: dict[str, Any]) -> dict[str, Any]:
    hooks = settings.get("hooks", {})
    for event in list(hooks):
        groups = []
        for group in hooks[event]:
            kept = [h for h in group.get("hooks", []) if not is_deck_hook(h)]
            if kept:
                groups.append({**group, "hooks": kept})
        if groups:
            hooks[event] = groups
        else:
            del hooks[event]
    if "hooks" in settings and not hooks:
        del settings["hooks"]
    return settings


def add_hooks(settings: dict[str, Any], python: str, script: str, port: int, wait: float) -> dict[str, Any]:
    settings = remove_hooks(settings)
    hooks = settings.setdefault("hooks", {})
    for event in STATUS_EVENTS:
        group: dict[str, Any] = {"hooks": [{"type": "command", "command": python,
                                            "args": [script, "--port", str(port)], "async": True, "timeout": 5}]}
        if event in TOOL_EVENTS:
            group = {"matcher": "*", **group}
        hooks.setdefault(event, []).append(group)
    hook_wait = int(wait) + 2  # a little longer than the bridge, so the bridge always answers first
    hooks.setdefault("PermissionRequest", []).append({"matcher": "*", "hooks": [{
        "type": "command", "command": python,
        "args": [script, "--port", str(port), "--wait", str(hook_wait)], "timeout": hook_wait + 13}]})
    return settings


def update_file(path: Path, change: Callable[[dict[str, Any]], dict[str, Any]]) -> None:
    settings: dict[str, Any] = {}
    if path.exists():
        text = path.read_text(encoding="utf-8")
        settings = json.loads(text) if text.strip() else {}  # ValueError: leave a broken file alone
        shutil.copy2(path, path.with_name(f"{path.name}.bak-{time.strftime('%Y%m%d-%H%M%S')}"))
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(change(settings), indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--settings", type=Path, default=Path.home() / ".claude" / "settings.json")
    parser.add_argument("--config", type=Path, default=HERE / "config.json")
    parser.add_argument("--remove", action="store_true")
    args = parser.parse_args()
    if args.remove:
        update_file(args.settings, remove_hooks)
        print(f"Removed Control Deck hooks from {args.settings}")
        return 0
    claude = {}
    if args.config.exists():
        claude = json.loads(args.config.read_text(encoding="utf-8")).get("claude", {})
    python = HERE / ".venv" / "Scripts" / "python.exe"
    if not python.exists():
        python = Path(sys.executable)
    update_file(args.settings, lambda s: add_hooks(s, str(python), str(HERE / "claude_hook.py"),
                                                   int(claude.get("port", 47821)),
                                                   float(claude.get("ask_wait_seconds", 25))))
    print(f"Added Control Deck hooks to {args.settings} (restart Claude Code sessions to load them)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
