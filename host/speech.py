"""Vietnamese text-to-speech for the deck's speaker.

Text goes to Microsoft's online neural voices through edge-tts (no key; the text
leaves the PC), the MP3 is decoded with miniaudio to 8 kHz, 8-bit unsigned mono:
what the deck's DAC plays. Needs:  pip install edge-tts miniaudio
"""
from __future__ import annotations

import asyncio
import time
from typing import Awaitable, Callable

RATE = 8000
TRIES = 3  # the voice service sometimes answers with no audio; a retry usually works


def to_deck_pcm(mp3: bytes) -> bytes:
    import miniaudio

    decoded = miniaudio.decode(mp3, output_format=miniaudio.SampleFormat.UNSIGNED8, nchannels=1, sample_rate=RATE)
    return bytes(decoded.samples)


def make_speaker(voice: str = "vi-VN-HoaiMyNeural", log: Callable[[str], None] = lambda _m: None,
                 synth: Callable[[str], Awaitable[bytes]] | None = None) -> Callable[[str], bytes]:
    """A function text -> deck PCM. Raises ImportError when edge-tts or miniaudio is missing."""
    if synth is None:
        import edge_tts  # noqa: F401  (fail early, at bridge start)
        import miniaudio  # noqa: F401

        async def synth(text: str) -> bytes:
            import edge_tts

            chunks = []
            async for part in edge_tts.Communicate(text, voice).stream():
                if part.get("type") == "audio":
                    chunks.append(part["data"])
            return b"".join(chunks)

    def speak(text: str) -> bytes:
        error = None
        for attempt in range(TRIES):
            try:
                return to_deck_pcm(asyncio.run(synth(text)))
            except Exception as exc:  # offline, voice service down: the notice still shows
                error = exc
                if attempt + 1 < TRIES:
                    time.sleep(0.5)
        log(f"Speech failed: {type(error).__name__}")
        return b""

    return speak
