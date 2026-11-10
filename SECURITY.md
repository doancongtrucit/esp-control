# Security

## Local-first design

ESP Control Deck needs no cloud service and no account:

- the bridge talks to the deck over USB, or over your local network with signed packets (see [docs/WIFI_LINK.md](docs/WIFI_LINK.md));
- the Claude Code relay listens on `127.0.0.1` only;
- the only Internet requests are the public feeds of the Info tabs (gold, coin, stock, weather) and NTP on the deck;
- the deck stores the Wi-Fi credentials and the shared token it got over USB, nothing else (no API keys, passwords or cookies).

## Action execution

The deck sends only a button number from 1 to 18, never a command or shell text. The bridge maps the number to an action in your local `host/config.json`, which can open URLs, run commands, start services, type text and send hotkeys.

Treat `host/config.json` as executable configuration:

- add only commands you trust,
- check paths before enabling autostart,
- never use configuration files from unknown sources,
- keep it out of Git (ignored by default).

## Wi-Fi traffic

- Deck commands (macros, Claude answers, auto-lock and reminder settings) are signed with HMAC-SHA256 and the shared token, carry a timestamp and a sequence number, and are dropped when forged, stale or replayed.
- Frames are signed, not encrypted: someone on the same network can read what is shown on the deck (stats, Claude session names, notification text). Turn `wifi_link` off on networks you do not trust; the deck's "Hide content" switch keeps message text off the screen.
- The Wi-Fi password is sent to the deck over USB only.

## Claude Code hooks

The permission hook never allows anything by itself: without an answer from the deck it returns nothing and Claude Code asks on the PC. Your `deny` and `ask` rules still apply after an Allow from the deck. Hooks forward the session id, folder name, event, tool and a short target only.

## Local HTTP checks

The default configuration makes read-only requests to LibreHardwareMonitor (`http://127.0.0.1:8085/data.json`, Windows) and Ollama (`http://127.0.0.1:11434/api/tags`, when the Ollama button is used). Do not point these at remote endpoints without thinking about privacy.

## Reporting a vulnerability

Use GitHub private vulnerability reporting. Please do not publish exploit details in a public issue before a fix is available.
