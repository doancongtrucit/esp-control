# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added

- `cyd_2usb` environment for the ESP32-2432S028 (USB-C + micro-USB, ST7789) with its separate XPT2046 touch bus.
- 18 macros on three pages, hold-to-confirm, per-macro icons, gold price page (vang.today, thousand VND per chi).
- UI v2 on `cyd_2usb`: Home grid, header Home button, Agentic light theme, English labels.
- Notifications on `cyd_2usb`: iPhone over BLE (ANCS), Windows toasts over Wi-Fi (UDP with a shared token) or USB; Vietnamese text kept with diacritics.
- Touch filter and RGB status LED on `cyd_2usb`.
- UI v3 on `cyd_2usb`: dark launcher with gradient app icons and status bar, gradient Controls icons with per-macro colors.
- Info screen with Gold, Coin (CoinGecko), Stock (VPS), Weather (Open-Meteo) and Calendar tabs; Vietnamese lunar calendar on the device.
- CLAUDE CODE screen on `cyd_2usb`: Claude Code hooks (`host/claude_hook.py`, installed by `host/install_claude_hooks.py`) report sessions to a localhost relay in the bridge; the deck lists them as waiting / running / done, blinks the LED orange while one waits, and shows permission prompts with DENY (tap) and ALLOW (hold 1 s). No answer within 25 s, no bridge, or any error falls back to the normal prompt on the PC. ALWAYS (hold 1 s) accepts Claude Code's suggested "don't ask again" rule; a red FAIL badge marks 3+ failed tool calls in a row; USAGE tab (tokens, estimated cost at API rates, 5-hour window vs your peak, top projects) and STATS tab (replies per hour, daily ask / answer / done counts).
- Wi-Fi link on `cyd_2usb` (`host/udp_link.py`, `src/udp_link.cpp`): without a USB data cable the bridge and the deck exchange the same frames as signed UDP datagrams (HMAC-SHA256 with the notification token, timestamp and sequence against replays, acked commands, split long frames); discovery by bridge probes and deck beacons; USB wins when plugged in; the clock runs from NTP without the PC; Settings > Connect shows the PC link. The radio favours Wi-Fi over BLE (median round trip about 60 ms).
- Health tab and break reminders on `cyd_2usb` (`host/activity.py`): minutes at the PC per hour and for the last 7 days, sitting session, breaks, longest session, a late-night warning; a STAND UP card after 50 minutes without a 5-minute break (SNOOZE 10M / DONE, LED breathes cyan) and a 20-second REST YOUR EYES card every 20 minutes. Counts are kept in `host/activity_state.json` (git-ignored).
- Auto-lock on `cyd_2usb`: the deck reports the paired iPhone's BLE link and RSSI every 2 s; the bridge locks Windows once the phone is away for 30 s and the PC has been idle for 60 s (switch on the Health tab).
- Vietnamese holidays (solar and lunar, including Giao thừa) on the calendar grid, the next holiday with a countdown on the Date tab, and on the standby screen within a week.
- ARAM screen on `cyd_2usb`: ARAM Helper on the PC sends `{"type":"aram"}` UDP packets (same port and token as notifications); the deck opens the screen during a game (champion, ranked augment offer, best combo, two items against the enemy team, or core build and taken augments between picks) and returns to the previous screen when the game ends or after 15 s without packets. The bridge writes `host/device_state.json` (device IP, git-ignored) so other local apps can reach the device.
