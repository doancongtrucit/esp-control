"""Screenshots of the deck over USB, with sample data, for checking layouts.

    pip install pyserial Pillow psutil
    python tools/deck_screenshot.py --port COM4 --out shots home info:0 info:4 settings:1

Stop the bridge first (it holds the port). The deck gets sample frames only:
example macros from host/config.example.json, test fixtures for coin, stock
and weather, made-up stats, gold and one notification. Each "screen[:tab]"
becomes <out>/<screen>[-tab].png (320x240) plus contact.png with all of them.
"""
from __future__ import annotations

import argparse
import base64
import json
import sys
import time
from datetime import datetime
from pathlib import Path

import serial
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import control_deck_bridge as bridge  # noqa: E402
import info_feeds  # noqa: E402

W, H = 320, 240
FIXTURES = ROOT / "host" / "tests" / "fixtures"


def fixture(name: str):
    return json.loads((FIXTURES / f"{name}.json").read_text(encoding="utf-8"))


def sample_frames() -> list[dict]:
    now = datetime.now()
    stamp, day = now.strftime("%H:%M"), now.strftime("%d/%m")
    config = json.loads((ROOT / "host" / "config.example.json").read_text(encoding="utf-8"))
    coin = info_feeds.coin_frame(fixture("coingecko"), config["coins"], stamp)
    stock = info_feeds.stock_frame(fixture("vps_index"), fixture("vps_stocks"), config["stocks"],
                                   [1700 + (i % 9) * 2.5 + i * 0.6 for i in range(40)], stamp)
    weather = info_feeds.weather_frame(fixture("open_meteo"), "Da Nang", stamp)
    for frame in (coin, stock, weather):
        frame["date"] = day
    return [
        {"type": "stats", "cpu": 23, "cpu_temp": 52, "ram": 61, "ram_used_gb": 19.4, "ram_total_gb": 31.7,
         "gpu": 8, "gpu_temp": 41, "vram": 22, "vram_used_gb": 1.8, "vram_total_gb": 8.0,
         "net_down_mbps": 12.4, "net_up_mbps": 1.1, "disk_used": 67, "disk_read_mbs": 0.4,
         "disk_write_mbs": 2.1, "clock": stamp, "date": now.strftime("%Y-%m-%d"),
         "clock_seconds": now.hour * 3600 + now.minute * 60 + now.second},
        bridge.macro_packet(config),
        {"type": "info", "g": {"time": stamp, "items": [
            {"n": "SJC 1L", "b": 12150, "s": 12350, "c": 50}, {"n": "Nhan 9999", "b": 11620, "s": 11920, "c": -30}],
            "xau": 2381.4, "xauc": -6.2}},
        coin, stock, weather,
        bridge.notify_packet("Zalo", "Nhóm Dự án", "Chiều nay họp lúc 3 giờ, nhớ mang tài liệu nhé!", source="phone"),
    ]


class Deck:
    def __init__(self, port: str):
        self.ser = serial.Serial()
        self.ser.port, self.ser.baudrate, self.ser.timeout = port, 115200, 2
        self.ser.dtr = self.ser.rts = False  # do not reset the board on open
        self.ser.open()

    def send(self, frame: dict) -> None:
        self.ser.write(("DD:" + json.dumps(frame, ensure_ascii=False) + "\n").encode("utf-8"))
        time.sleep(0.15)

    def shot(self, screen: str, tab: int) -> Image.Image:
        self.ser.reset_input_buffer()
        self.send({"type": "shot", "screen": screen, "tab": tab})
        rows: dict[int, bytes] = {}
        deadline = time.monotonic() + 60
        while len(rows) < H and time.monotonic() < deadline:
            line = self.ser.readline().decode("utf-8", "replace").strip()
            if not line.startswith('DD:{"type":"shot_row"'):
                continue
            frame = json.loads(line[3:])
            rows[frame["y"]] = base64.b64decode(frame["d"])
        if len(rows) < H:
            raise RuntimeError(f"{screen}: got {len(rows)} of {H} rows")
        image = Image.new("RGB", (W, H))
        pixels = image.load()
        for y, data in rows.items():
            for x in range(W):
                v = (data[2 * x] << 8) | data[2 * x + 1]
                pixels[x, y] = ((v >> 11) << 3, ((v >> 5) & 0x3F) << 2, (v & 0x1F) << 3)
        return image


def contact(shots: list[tuple[str, Image.Image]], out: Path) -> None:
    cols = 3
    rows = (len(shots) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (W * 2 + 16) + 16, rows * (H * 2 + 40) + 16), (24, 24, 28))
    draw = ImageDraw.Draw(sheet)
    for i, (name, image) in enumerate(shots):
        x, y = 16 + (i % cols) * (W * 2 + 16), 16 + (i // cols) * (H * 2 + 40)
        draw.text((x, y), name, fill=(230, 230, 230))
        sheet.paste(image.resize((W * 2, H * 2), Image.NEAREST), (x, y + 18))
    sheet.save(out)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--out", type=Path, default=Path("shots"))
    parser.add_argument("--no-data", action="store_true", help="do not send sample frames first")
    parser.add_argument("targets", nargs="+", help="screen[:tab], e.g. home info:2 settings:0")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    deck = Deck(args.port)
    if not args.no_data:
        for frame in sample_frames():
            deck.send(frame)
        time.sleep(3)  # let the notification banner time out
    shots = []
    for target in args.targets:
        screen, _, tab = target.partition(":")
        image = deck.shot(screen, int(tab) if tab else -1)
        name = screen + (f"-{tab}" if tab else "")
        image.save(args.out / f"{name}.png")
        shots.append((name, image))
        print(f"{name}.png")
    contact(shots, args.out / "contact.png")


if __name__ == "__main__":
    main()
