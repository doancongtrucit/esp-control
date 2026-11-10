"""Bridge <-> deck over Wi-Fi: signed UDP datagrams carrying the same JSON frames as USB.

Datagram:  b"DK1 " + mac + b" " + body
  body  =  b"<ts> <seq> <part> <parts> " + chunk
  mac   =  first 32 hex chars of HMAC-SHA256(token, body)

ts is Unix time in seconds (0 = the sender has no clock yet), seq numbers the
sender's frames, and a frame longer than CHUNK bytes is split into parts that
share ts and seq. The receiver drops datagrams whose MAC is wrong, whose ts is
more than WINDOW_S away from its own clock, or that it already saw (replays).

The deck sends to the bridge's port (4211); the bridge sends to the deck's
notification port (4210). The Wi-Fi password never travels over UDP.

Discovery: while no deck is linked, the bridge sends a signed "probe" to the
deck's last known IP every few seconds, and the deck broadcasts a signed
"beacon". A beacon with ts 0 (deck clock not set) only gets a "time" frame
back; the deck sets its clock from that datagram and beacons again.
"""
from __future__ import annotations

import hashlib
import hmac
import json
import socket
import time
from dataclasses import dataclass, field
from typing import Any, Callable

MAGIC = b"DK1 "
CHUNK = 1200
WINDOW_S = 120
LINK_TIMEOUT_S = 10.0
PROBE_EVERY_S = 3.0
BRIDGE_PORT = 4211
DECK_PORT = 4210
# Deck -> bridge frames that change something on the PC: acknowledged, retried by the deck, run once.
COMMANDS = {"macro", "claude_reply", "claude_clear", "claude_open", "lock_set", "break_reply"}
# Frames the deck must never accept over Wi-Fi (credentials, maintenance).
USB_ONLY = {"wifi", "shot", "recalibrate"}


def mac(token: str, body: bytes) -> bytes:
    return hmac.new(token.encode("utf-8"), body, hashlib.sha256).hexdigest()[:32].encode("ascii")


def encode(frame: dict[str, Any]) -> bytes:
    return json.dumps(frame, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def seal(token: str, frame: dict[str, Any], ts: int, seq: int) -> list[bytes]:
    text = encode(frame)
    chunks = [text[i:i + CHUNK] for i in range(0, len(text), CHUNK)] or [b""]
    out = []
    for i, chunk in enumerate(chunks):
        body = f"{ts} {seq} {i} {len(chunks)} ".encode("ascii") + chunk
        out.append(MAGIC + mac(token, body) + b" " + body)
    return out


@dataclass
class Envelope:
    ts: int
    seq: int
    part: int
    parts: int
    chunk: bytes


def open_datagram(token: str, data: bytes) -> Envelope | None:
    """Envelope of a datagram with a valid MAC, else None."""
    if not token or not data.startswith(MAGIC) or len(data) < len(MAGIC) + 33:
        return None
    given, body = data[len(MAGIC):len(MAGIC) + 32], data[len(MAGIC) + 33:]
    if data[len(MAGIC) + 32:len(MAGIC) + 33] != b" " or not hmac.compare_digest(given, mac(token, body)):
        return None
    head = body.split(b" ", 4)
    if len(head) != 5:
        return None
    try:
        ts, seq, part, parts = (int(x) for x in head[:4])
    except ValueError:
        return None
    if not 0 <= part < parts <= 8 or ts < 0 or seq < 0:
        return None
    return Envelope(ts, seq, part, parts, head[4])


class Reassembler:
    """Joins the parts of one frame; incomplete frames are dropped after a few seconds."""

    def __init__(self, mono: Callable[[], float] = time.monotonic):
        self.mono = mono
        self.pending: dict[tuple[int, int], tuple[float, dict[int, bytes], int]] = {}

    def feed(self, env: Envelope) -> bytes | None:
        if env.parts == 1:
            return env.chunk
        now = self.mono()
        self.pending = {k: v for k, v in self.pending.items() if now - v[0] < 3.0}
        key = (env.ts, env.seq)
        started, parts, total = self.pending.get(key, (now, {}, env.parts))
        parts[env.part] = env.chunk
        if len(parts) == total:
            self.pending.pop(key, None)
            return b"".join(parts[i] for i in range(total))
        self.pending[key] = (started, parts, total)
        return None


class ReplayGuard:
    """Accepts each (ts, seq) once, and only within WINDOW_S of our clock."""

    def __init__(self, window: int = WINDOW_S):
        self.window = window
        self.seen: dict[tuple[int, int], int] = {}

    def fresh(self, ts: int, now: float) -> bool:
        return abs(now - ts) <= self.window

    def seen_before(self, ts: int, seq: int) -> bool:
        return (ts, seq) in self.seen

    def remember(self, ts: int, seq: int, now: float) -> None:
        self.seen[(ts, seq)] = ts
        if len(self.seen) > 512:
            oldest = now - self.window
            self.seen = {k: v for k, v in self.seen.items() if v >= oldest}


@dataclass
class UdpHub:
    """The bridge's UDP end. poll() returns frames from the linked deck; beacons come back as {"type": "beacon"}."""

    token: str
    sock: Any = None
    port: int = BRIDGE_PORT
    deck_port: int = DECK_PORT
    known_ip: str | None = None
    clock: Callable[[], float] = time.time
    mono: Callable[[], float] = time.monotonic
    log: Callable[[str], None] = lambda _m: None
    deck: tuple[str, int] | None = None
    last_rx: float = -1e9
    last_probe: float = -1e9
    seq: int = 0
    guard: ReplayGuard = field(default_factory=ReplayGuard)

    def __post_init__(self) -> None:
        self.reassembler = Reassembler(self.mono)
        if self.sock is None:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind(("0.0.0.0", self.port))
            self.sock.setblocking(False)

    # ---- sending ----

    def _send_to(self, addr: tuple[str, int], frame: dict[str, Any]) -> None:
        self.seq += 1
        for datagram in seal(self.token, frame, int(self.clock()), self.seq):
            try:
                self.sock.sendto(datagram, addr)
            except OSError as exc:
                self.log(f"Wi-Fi send to the deck failed: {type(exc).__name__}")
                return

    def send(self, frame: dict[str, Any]) -> None:
        if self.deck is not None:
            self._send_to(self.deck, frame)

    def linked(self) -> bool:
        return self.deck is not None and self.mono() - self.last_rx <= LINK_TIMEOUT_S

    def probe(self) -> None:
        """While not linked: ask the deck at its last known IP to answer (keeps the firewall path open)."""
        if self.linked() or not self.known_ip or self.mono() - self.last_probe < PROBE_EVERY_S:
            return
        self.last_probe = self.mono()
        self._send_to((self.known_ip, self.deck_port), {"type": "probe"})

    # ---- receiving ----

    def _datagrams(self):
        while True:
            try:
                yield self.sock.recvfrom(2048)
            except (BlockingIOError, InterruptedError):
                return
            except OSError:  # e.g. Windows reports ICMP port unreachable as a reset
                return

    def poll(self) -> list[dict[str, Any]]:
        frames: list[dict[str, Any]] = []
        for data, addr in self._datagrams():
            env = open_datagram(self.token, data)
            if env is None:
                continue
            text = self.reassembler.feed(env)
            if text is None:
                continue
            try:
                frame = json.loads(text.decode("ascii"))
            except (UnicodeDecodeError, ValueError):
                continue
            if not isinstance(frame, dict):
                continue
            kind = frame.get("type")
            now = self.clock()
            if env.ts == 0:
                if kind == "beacon":  # deck has no clock yet: tell it the time, nothing else
                    self._send_to(addr, {"type": "time"})
                continue
            if not self.guard.fresh(env.ts, now):
                continue
            if self.guard.seen_before(env.ts, env.seq):
                if kind in COMMANDS:
                    self._send_to(addr, {"type": "ack", "seq": env.seq})  # our ack was lost: ack again, run once
                continue
            self.guard.remember(env.ts, env.seq, now)
            if self.deck != addr:
                self.log(f"Deck linked over Wi-Fi at {addr[0]}")
            self.deck = addr
            self.known_ip = addr[0]
            self.last_rx = self.mono()
            if kind in COMMANDS:
                self._send_to(addr, {"type": "ack", "seq": env.seq})
            frames.append(frame)
        return frames


class UdpLink:
    """Looks like a serial port to the bridge's frame loop: send_frame() out, read() returns "DD:" lines."""

    kind = "Wi-Fi"

    def __init__(self, hub: UdpHub, prefix: str = "DD:"):
        self.hub = hub
        self.prefix = prefix
        self.pending = bytearray()

    def send_frame(self, frame: dict[str, Any]) -> None:
        self.hub.send(frame)

    def pump(self) -> None:
        for frame in self.hub.poll():
            self.pending += f"{self.prefix}{json.dumps(frame, separators=(',', ':'))}\n".encode("ascii")

    def read(self, size: int = 256) -> bytes:
        self.pump()
        out, self.pending = bytes(self.pending[:size]), self.pending[size:]
        return out

    def alive(self) -> bool:
        return self.hub.linked()
