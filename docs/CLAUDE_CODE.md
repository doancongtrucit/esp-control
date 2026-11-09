# Claude Code on the deck

The CLAUDE CODE screen (`cyd_2usb`) lists your Claude Code sessions and lets you answer permission prompts from the deck.

- **WAITING** (orange, LED blinks orange): the session needs you, for a permission prompt or another question.
- **RUNNING** (blue): Claude is working; the detail line shows the current tool, e.g. `Bash: npm test` or `Edit: main.cpp`.
- **DONE**: the last task finished.
- **OPEN WAITING** brings the waiting session's window to the front (best effort: a window whose title has the project name, else a Claude window).
- **CLEAR DONE** removes finished sessions from the list.

When a session asks for permission, the deck opens a card with the project, the tool and the command:

- **DENY**: tap.
- **ALLOW**: hold for 1 second, so a stray touch never allows anything.

If nobody answers on the deck within `ask_wait_seconds` (25 s by default), the deck is off, or the bridge is not running, Claude Code shows its normal prompt on the PC. The deck never allows anything by itself. `deny` and `ask` rules in your Claude Code settings still apply after an Allow from the deck.

In the terminal, Claude Code waits for the hook before it shows its own prompt. That is why the wait is short; set `ask_wait_seconds` to `0`-`5` if you mostly sit at the PC.

## How it works

```
Claude Code hook events ──> host/claude_hook.py ──TCP 127.0.0.1:47821──> bridge (claude_relay.py)
                                   ^                                          │ "claude" frame (USB)
                     Allow / Deny  └────────────── claude_reply <─────────────┴─> deck
```

- Status events (`SessionStart`, `UserPromptSubmit`, `PreToolUse`, `PostToolUse`, `Notification`, `Stop`, `SubagentStop`, `SessionEnd`) run the hook in the background, so they never slow Claude down.
- `PermissionRequest` runs in the foreground: the hook waits for the deck's answer and prints the decision.
- The hook sends only the session id, the folder name, the event, the tool name and a short target (command, file name, URL host). It drops transcripts, prompts, tool output and file contents. The relay listens on 127.0.0.1 only, and nothing goes to the internet.

## Setup

1. Start the bridge as usual. Its log shows `Claude Code relay listening on 127.0.0.1:47821`.
2. Add the hooks to `~/.claude/settings.json` (a timestamped backup is written first; your other settings and hooks are kept):

   ```powershell
   host\.venv\Scripts\python.exe host\install_claude_hooks.py
   ```

3. Restart your Claude Code sessions. Hooks apply to the terminal, the IDE extensions and the Desktop app.

To take the hooks out again:

```powershell
host\.venv\Scripts\python.exe host\install_claude_hooks.py --remove
```

## Settings (`host/config.json`)

```json
"claude": {"enabled": true, "port": 47821, "ask_wait_seconds": 25}
```

Run the installer again after changing `port` or `ask_wait_seconds`.
