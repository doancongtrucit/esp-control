# USB serial protocol

ESP32 PC Control Deck v0.3 uses a small line-oriented protocol through the board's CH340 USB serial connection.

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

### `wifi` (`cyd_2usb`)

Provisions the device's Wi-Fi and the token for UDP notifications. Sent by the bridge after it opens the port and after every `hello`; the device saves the values in flash and reconnects only when they change.

```json
{"type": "wifi", "ssid": "<network>", "pass": "<password>", "token": "<24 characters>", "port": 4210}
```

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
  "device": "ESP32 PC Control Deck",
  "firmware": "0.3.3",
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

### `macro`

Sent when a control button is pressed:

```json
{"type":"macro","id":3}
```

The ESP32 sends only the numeric ID. The trusted action remains in the local `host/config.json` file.

### `error`

Used for malformed framed JSON. Unrelated serial text is ignored without returning an error.
