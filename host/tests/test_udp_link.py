"""Signed UDP datagrams between the bridge and the deck."""
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import udp_link as ul  # noqa: E402

TOKEN = "t0k3n-for-tests-only-123"
DECK = ("192.0.2.50", 4210)


class FakeSock:
    def __init__(self):
        self.inbox = []
        self.sent = []

    def recvfrom(self, _size):
        if not self.inbox:
            raise BlockingIOError
        return self.inbox.pop(0)

    def sendto(self, data, addr):
        self.sent.append((data, addr))

    def frames_sent(self):
        out = []
        for data, addr in self.sent:
            env = ul.open_datagram(TOKEN, data)
            out.append((json.loads(env.chunk), addr))
        return out


class SealTests(unittest.TestCase):
    def test_round_trip(self):
        [datagram] = ul.seal(TOKEN, {"type": "macro", "id": 3}, ts=1_800_000_000, seq=7)
        env = ul.open_datagram(TOKEN, datagram)
        self.assertEqual((1_800_000_000, 7, 0, 1), (env.ts, env.seq, env.part, env.parts))
        self.assertEqual({"type": "macro", "id": 3}, json.loads(env.chunk))

    def test_known_vector_matches_the_firmware_test(self):
        # test/test_udp_link checks the same bytes on the ESP32.
        [datagram] = ul.seal("key", {"type": "ack", "seq": 5}, ts=1700000000, seq=9)
        self.assertEqual(b'DK1 ' + ul.mac("key", b'1700000000 9 0 1 {"type":"ack","seq":5}')
                         + b' 1700000000 9 0 1 {"type":"ack","seq":5}', datagram)
        # Published HMAC-SHA256 example ("key", "The quick brown fox..."), first 16 bytes.
        self.assertEqual(b"f7bc83f430538424b13298e6aa6fb143",
                         ul.mac("key", b"The quick brown fox jumps over the lazy dog"))

    def test_wrong_token_or_tampering_is_rejected(self):
        [datagram] = ul.seal(TOKEN, {"type": "macro", "id": 3}, 1, 1)
        self.assertIsNone(ul.open_datagram("other", datagram))
        self.assertIsNone(ul.open_datagram(TOKEN, datagram.replace(b'"id":3', b'"id":4')))
        self.assertIsNone(ul.open_datagram(TOKEN, b'{"token":"x","app":"a"}'))
        self.assertIsNone(ul.open_datagram("", datagram))

    def test_long_frames_are_split_and_joined(self):
        frame = {"type": "stock", "h": list(range(600))}
        datagrams = ul.seal(TOKEN, frame, 5, 2)
        self.assertGreater(len(datagrams), 1)
        self.assertTrue(all(len(d) <= ul.CHUNK + 80 for d in datagrams))
        joiner = ul.Reassembler(mono=lambda: 0.0)
        results = [joiner.feed(ul.open_datagram(TOKEN, d)) for d in reversed(datagrams)]
        self.assertEqual(frame, json.loads(results[-1]))
        self.assertTrue(all(r is None for r in results[:-1]))


class HubTests(unittest.TestCase):
    def setUp(self):
        self.sock = FakeSock()
        self.now = [1_800_000_000.0]
        self.mono = [100.0]
        self.hub = ul.UdpHub(TOKEN, sock=self.sock, known_ip="192.0.2.50",
                             clock=lambda: self.now[0], mono=lambda: self.mono[0])

    def deck_sends(self, frame, ts=None, seq=1, addr=DECK):
        for d in ul.seal(TOKEN, frame, int(self.now[0]) if ts is None else ts, seq):
            self.sock.inbox.append((d, addr))

    def test_beacon_links_the_deck(self):
        self.assertFalse(self.hub.linked())
        self.deck_sends({"type": "beacon"})
        self.assertEqual([{"type": "beacon"}], self.hub.poll())
        self.assertTrue(self.hub.linked())
        self.hub.send({"type": "stats", "cpu": 3})
        self.assertEqual(({"type": "stats", "cpu": 3}, DECK), self.sock.frames_sent()[-1])

    def test_beacon_without_a_clock_only_gets_the_time(self):
        self.deck_sends({"type": "beacon"}, ts=0)
        self.assertEqual([], self.hub.poll())
        self.assertFalse(self.hub.linked())
        frame, addr = self.sock.frames_sent()[-1]
        self.assertEqual(({"type": "time"}, DECK), (frame, addr))

    def test_commands_are_acked_and_run_once(self):
        self.deck_sends({"type": "macro", "id": 2}, seq=4)
        self.deck_sends({"type": "macro", "id": 2}, seq=4)  # retry after a lost ack
        self.assertEqual([{"type": "macro", "id": 2}], self.hub.poll())
        acks = [f for f, _ in self.sock.frames_sent() if f["type"] == "ack"]
        self.assertEqual([{"type": "ack", "seq": 4}] * 2, acks)

    def test_old_datagrams_are_replays(self):
        self.deck_sends({"type": "macro", "id": 2}, ts=int(self.now[0]) - 600)
        self.assertEqual([], self.hub.poll())
        self.assertFalse(self.hub.linked())

    def test_forged_datagrams_are_ignored(self):
        self.sock.inbox.append((b"DK1 " + b"0" * 32 + b" 1800000000 1 0 1 {\"type\":\"macro\",\"id\":1}", DECK))
        self.assertEqual([], self.hub.poll())

    def test_link_times_out_without_traffic(self):
        self.deck_sends({"type": "phone", "link": False})
        self.hub.poll()
        self.mono[0] += 11
        self.assertFalse(self.hub.linked())

    def test_probe_goes_to_the_known_ip_every_few_seconds(self):
        self.hub.probe()
        self.hub.probe()
        self.assertEqual([({"type": "probe"}, ("192.0.2.50", 4210))], self.sock.frames_sent())
        self.mono[0] += 4
        self.hub.probe()
        self.assertEqual(2, len(self.sock.sent))

    def test_new_address_follows_the_deck(self):
        self.deck_sends({"type": "beacon"}, seq=1)
        self.deck_sends({"type": "beacon"}, seq=2, addr=("192.0.2.77", 4210))
        self.hub.poll()
        self.assertEqual(("192.0.2.77", 4210), self.hub.deck)


class LinkTests(unittest.TestCase):
    def test_reads_frames_as_protocol_lines(self):
        sock = FakeSock()
        hub = ul.UdpHub(TOKEN, sock=sock, clock=lambda: 1_800_000_000.0, mono=lambda: 1.0)
        for d in ul.seal(TOKEN, {"type": "phone", "link": True, "rssi": -60}, 1_800_000_000, 3):
            sock.inbox.append((d, DECK))
        link = ul.UdpLink(hub)
        self.assertEqual(b'DD:{"type":"phone","link":true,"rssi":-60}\n', link.read(256))
        self.assertTrue(link.alive())


if __name__ == "__main__":
    unittest.main()
