# USB serial protocol

ESP Control Deck uses a small line-oriented protocol through the board's CH340 USB serial connection (or the Wi-Fi link, see below).

## Framing

Every application frame starts with `DD:` and ends with a newline:

```text
DD:{"type":"stats","cpu":18.2,"ram":61.7}\n
```

The prefix lets both sides ignore ESP32 ROM boot messages and unrelated serial output instead of attempting to parse them as JSON.

Default serial settings:

- 115200 baud
- 8 data bits
- no parity
- 1 stop bit

Protocol version: `3`.

The firmware still accepts a valid raw JSON line from earlier prototypes, but new clients should always use the `DD:` prefix.

## PC to ESP32

### `stats`

```json
{
  "type": "stats",
  "cpu": 18.2,
  "ram": 61.7,
  "ram_used_gb": 19.7,
  "ram_total_gb": 32.0,
  "gpu": 35.0,
  "vram": 48.0,
  "vram_used_gb": 5.8,
  "vram_total_gb": 12.0,
  "cpu_temp": 57.0,
  "gpu_temp": 52.0,
  "system_temp": 34.0,
  "disk_temp": 41.0,
  "disk_used": 71.0,
  "disk_read_mbs": 4.2,
  "disk_write_mbs": 1.3,
  "net_down_mbps": 23.4,
  "net_up_mbps": 2.1,
  "clock": "19:05",
  "clock_seconds": 68700
}
```

Unavailable sensor values are sent as JSON `null`.

`clock_seconds` is the local number of seconds since midnight on the PC. The ESP32 stores the value and advances it locally using `millis()`. The older `clock` string remains as a fallback.

`date` (`YYYY-MM-DD`, the PC's local date) is also sent; the `cyd_2usb` build uses it for the status bar and the lunar calendar.

### `coin`, `stock`, `weather` (`cyd_2usb`)

Info tab data, each sent when it changes and again after `hello`:

```json
{"type": "coin", "items": [{"s": "BTC", "n": "Bitcoin", "p": 76032.0, "c": -5.11}], "time": "19:42"}
{"type": "stock", "index": [{"n": "VN-INDEX", "v": 1740.73, "c": 0.44}], "items": [{"n": "VNM", "p": 57.5, "c": 0.17}], "spark": [1733.0, 1740.7], "time": "15:05"}
{"type": "weather", "city": "Da Nang", "t": 27.9, "f": 33, "h": 79, "wind": 9, "icon": 1, "desc": "Partly cloudy", "days": [{"d": "Sat", "hi": 29, "lo": 24, "rain": 83, "icon": 3}], "time": "19:30"}
```

Stock prices are in thousand VND; `c` is the change in percent; weather `icon` is 0 sun, 1 partly cloudy, 2 cloudy, 3 rain, 4 storm, 5 fog; `rain` is -1 when unknown.

### `config`

Updates the touch-button labels (IDs 1-18, three pages of six):

```json
{
  "type": "config",
  "macros": [
    {"id": 1, "label": "ChatGPT"},
    {"id": 2, "label": "Ollama"},
    {"id": 3, "label": "Voice Server"},
    {"id": 6, "label": "Lock PC", "hold": true}
  ]
}
```

`hold: true` makes the button require a one-second press before the `macro` frame is sent.

Labels can contain up to 22 characters. The firmware wraps longer labels onto two lines.

### `info`

Gold prices for the `INFO` page. Prices are thousand VND per chi; `c` is the daily change of the sell price. `xau` is XAU/USD per ounce.

```json
{
  "type": "info",
  "g": {
    "time": "17:30",
    "items": [{"n": "SJC mieng", "b": 13950, "s": 14250, "c": -50}],
    "xau": 4126.1,
    "xauc": 0.9
  }
}
```

### `notify`

Shows a notification on the `cyd_2usb` build (UI v2): a 2.5 s banner, a blue LED blink, and a row on the NOTIFY screen (newest 10 kept). Text is UTF-8; Vietnamese diacritics are kept, emoji and other symbols are dropped on the device. The bridge cuts each field to 120 characters.

```json
{"type": "notify", "src": "pc", "app": "Teams", "from": "Sender", "body": "Message text"}
```

`src` is `pc` or `phone`.

### `claude` (`cyd_2usb`)

Claude Code sessions reported by the bridge's hook relay (see [CLAUDE_CODE.md](CLAUDE_CODE.md)). Sent whenever something changes; ages are seconds at send time and the device keeps counting.

```json
{"type": "claude", "n": 5,
 "s": [["project-1", "w", "Allow Bash?", 12, 0], ["project-2", "r", "Failed: Bash: pytest", 480, 3]],
 "ask": {"id": 7, "name": "project-1", "tool": "Bash", "cmd": "npm test", "always": "Bash(npm test:*)"}}
```

`s` holds up to 4 rows: name (last folder of the session's working directory), state (`w` waiting for you, `r` running, `d` done), detail, age, failed tool calls in a row (3 or more shows a FAIL badge). `ask.always` is present when Claude Code offers a "don't ask again" rule; the deck then shows an ALWAYS button. `n` is the number of sessions known. `ask` is present while a permission prompt waits for an answer from the deck; a new `ask` opens the CLAUDE CODE screen.

### `cusage`, `cstats` (`cyd_2usb`)

Usage and Stats tabs of the CLAUDE CODE screen. `cusage` comes from the Claude Code transcripts on the PC (about once a minute, only when something changed); `cstats` from the relay's daily counts.

```json
{"type": "cusage", "t": "22:09",
 "day": {"in": 2188, "out": 1261074, "cw": 3007850, "cr": 456942508, "usd": 140.68},
 "top": [["project-1", 101.67]],
 "blk": {"usd": 35.78, "pct": 59, "left": 13845, "start": "21:00"},
 "h": [0, 0, 0, 0, 0, 0, 0, 0, 0, 79, 67]}
{"type": "cstats", "ask": 14, "ok": 9, "al": 2, "no": 1, "pc": 2, "done": 23}
```

`usd` values are estimates at API rates. `blk` is the open 5-hour window (null when none); `pct` compares it with the biggest window in the last 30 days, `left` is seconds until it closes. `h` counts Claude replies per hour today. In `cstats`: asks, Allow / Always / Deny on the deck, prompts answered on the PC (or timed out), finished tasks.

### `activity` (`cyd_2usb`)

Health tab, break reminders and the auto-lock state, from `host/activity.py` (sent when something changes, about once a minute):

```json
{"type": "activity", "on": 312, "sess": 52, "brk": 6, "long": 95,
 "h": [0, 0, 0, 0, 0, 0, 0, 0, 35, 58, 60, 44],
 "wk": [410, 380, 35, 0, 455, 390, 312],
 "late": false, "rem": "stand", "eye": 3,
 "lk": {"on": true, "near": true, "rssi": -58}}
```

Minutes at the PC today, in the current sitting session (0 on a break), breaks (5+ idle minutes), the longest session; `h` per hour today, `wk` per day from 6 days ago to today. `rem` is `"stand"` while the stand-up reminder waits (else null); `eye` grows by one at each eye-rest reminder. `lk`: auto-lock on/off, whether the iPhone is near (null when the deck stopped reporting) and its smoothed RSSI in dBm.

### `hserver` (`cyd_2usb`)

HOME SERVER tab of the Monitor screen, from Home Assistant (see [HOME_SERVER.md](HOME_SERVER.md)), about every 30 s when something changed:

```json
{"type": "hserver", "name": "Home Server", "on": true, "ms": 18, "ver": "2026.11.1",
 "cpu": 14.0, "ram": 47.0, "disk": 38.0, "temp": 52.0, "load": 0.36, "dn": 2.4, "up": 0.6,
 "upt": 273600, "upd": 2, "bat": null, "t": "12:00"}
{"type": "hserver", "name": "Home Server", "on": false, "err": "offline", "t": "12:00"}
```

Percentages, degrees C, Mbit/s; `ms` is the API round trip, `upt` seconds since boot, `upd` pending updates; `err` is `offline`, `token` or `error`. Unknown values are null.

### `hactl` (`cyd_2usb`)

Home screen tiles from Home Assistant (see [HOME_CONTROLS.md](HOME_CONTROLS.md)), sent when a state changes:

```json
{"type": "hactl",
 "items": [{"n": "Air Con", "i": "ac", "k": "climate", "on": 1, "v": "25C Cool", "c": 0},
           {"n": "PC", "i": "power", "k": "toggle", "on": 1, "v": "ON", "c": 1},
           {"n": "Swing", "i": "bolt", "k": "run", "on": -1, "v": "Run", "c": 0}],
 "ac": {"ix": 0, "on": true, "t": 25, "min": 16, "max": 30, "mode": "cool",
        "modes": ["heat_cool", "cool", "dry"], "fan": "auto", "fans": ["auto", "low", "mid", "high"]}}
```

`k`: `toggle`, `run` or `climate`; `on`: 1, 0 or -1 (not a switch / unknown); `c`: 1 = hold 1 s before it runs. `ac` describes the first climate tile (null when there is none).

### `wifi` (`cyd_2usb`)

Provisions the device's Wi-Fi and the token for UDP notifications. Sent by the bridge after it opens the port and after every `hello`; the device saves the values in flash and reconnects only when they change.

```json
{"type": "wifi", "ssid": "<network>", "pass": "<password>", "token": "<24 characters>", "port": 4210}
```

### Wi-Fi link (`cyd_2usb`)

Without USB, every frame in this document can travel as a signed UDP datagram instead (see [WIFI_LINK.md](WIFI_LINK.md)):

```
DK1 <mac> <ts> <seq> <part> <parts> <json chunk>
```

`mac` is the first 32 hex characters of HMAC-SHA256(notification token, everything after `<mac> `); `ts` is Unix time (0 = the deck has no clock yet), `seq` numbers the sender's frames, and a frame over 1200 bytes is split into up to 4 parts sharing `ts` and `seq`. The bridge listens on UDP 4211 and sends to the deck's notification port (4210).

Link-only frames: bridge `probe` (answer with a beacon), `time` (sets the deck clock from the datagram's `ts`), `ack` `{"seq": n}` for each deck command (`macro`, `claude_reply`, `claude_clear`, `claude_open`, `lock_set`, `break_reply`); deck `beacon` `{"fw": "..."}`. The `stats` frame also carries `epoch` (Unix time) and `tz` (UTC offset in seconds) on both links.

### UDP notifications (`cyd_2usb`)

Not a serial frame: a UDP datagram to the device's Wi-Fi address and `port`, at most 1024 bytes:

```json
{"token": "<token>", "app": "Teams", "from": "Sender", "body": "Message text"}
```

Datagrams with a missing or wrong token, invalid JSON or more than 1024 bytes are dropped.

### `ping`

The ESP32 replies with `pong`.

### `recalibrate`

Deletes the saved touch calibration and restarts the ESP32.

## ESP32 to PC

### `hello`

Sent after boot:

```json
{
  "type": "hello",
  "device": "ESP Control Deck",
  "firmware": "0.4.0",
  "protocol": 3
}
```

### `wifi_status` (`cyd_2usb`)

Sent after a `wifi` frame and whenever the connection changes:

```json
{"type": "wifi_status", "connected": true, "ip": "<device address>", "port": 4210}
```

### `ancs` (`cyd_2usb`)

Sent when the iPhone link comes up or goes down, with free heap for diagnostics:

```json
{"type": "ancs", "ready": true, "heap": 152500}
```

### `phone` (`cyd_2usb`)

Every 2 s: whether the paired iPhone is connected over BLE and its signal strength (null when not connected or unreadable). The bridge locks Windows when the phone stays away while the PC is idle.

```json
{"type": "phone", "link": true, "rssi": -61}
```

### `lock_set`, `break_reply` (`cyd_2usb`)

```json
{"type": "lock_set", "on": false}
{"type": "break_reply", "act": "snooze"}
{"type": "break_reply", "act": "ok"}
```

`lock_set` turns auto-lock on or off (kept by the bridge). `break_reply` answers the stand-up reminder: `snooze` reminds again in 10 minutes, `ok` not until the next break.

### `ha_press`, `ha_ac` (`cyd_2usb`)

```json
{"type": "ha_press", "i": 3}
{"type": "ha_ac", "op": "power"}
{"type": "ha_ac", "op": "temp", "d": 1}
{"type": "ha_ac", "op": "mode", "m": "dry"}
{"type": "ha_ac", "op": "fan", "m": "high"}
```

`ha_press` toggles or runs tile `i`; `ha_ac` drives the air conditioner with a choice from the `hactl` lists. The bridge maps them to Home Assistant services; nothing else is accepted.

### `macro`

Sent when a control button is pressed:

```json
{"type":"macro","id":3}
```

The ESP32 sends only the numeric ID. The trusted action remains in the local `host/config.json` file.

### `claude_reply`, `claude_clear`, `claude_open` (`cyd_2usb`)

```json
{"type":"claude_reply","id":7,"ok":true}
{"type":"claude_reply","id":7,"ok":true,"always":true}
{"type":"claude_clear"}
{"type":"claude_open","name":"project-1"}
```

`claude_reply` answers the open `ask` (`ok` true = Allow, anything else = Deny; `always` adds Claude Code's suggested rule, only if that ask offered one); the device sends Allow and Always only after a 1 s hold. Replies for an ask that is no longer open are ignored. `claude_clear` removes finished sessions; `claude_open` asks the bridge to bring that session's window to the front (best effort, Windows only).

### `error`

Used for malformed framed JSON. Unrelated serial text is ignored without returning an error.
