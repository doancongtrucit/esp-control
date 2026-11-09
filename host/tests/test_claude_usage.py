"""Token usage and estimated cost from Claude Code transcripts (~/.claude/projects)."""
import json
import sys
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import claude_usage as cu  # noqa: E402

DAY = datetime(2026, 11, 9)


def at(hour, minute=0, day=DAY):
    return day.replace(hour=hour, minute=minute)


def line(ts, model="claude-opus-5-5", inp=1000, out=500, cw5=0, cw1h=0, cr=0, mid=None, rid=None, cwd="C:/w/alpha"):
    usage = {"input_tokens": inp, "output_tokens": out, "cache_read_input_tokens": cr,
             "cache_creation_input_tokens": cw5 + cw1h,
             "cache_creation": {"ephemeral_5m_input_tokens": cw5, "ephemeral_1h_input_tokens": cw1h}}
    return json.dumps({"type": "assistant", "timestamp": ts.astimezone().isoformat(), "cwd": cwd,
                       "requestId": rid or f"r{ts.timestamp()}{inp}",
                       "message": {"id": mid or f"m{ts.timestamp()}{inp}", "model": model, "usage": usage,
                                   "content": [{"type": "text", "text": "SECRET"}]}})


class CostTests(unittest.TestCase):
    def test_opus_5_5_prices_every_token_kind(self):
        entry = cu.Entry(0.0, "claude-opus-5-5", "a", inp=1_000_000, out=1_000_000, cw5=1_000_000,
                         cw1h=1_000_000, cr=1_000_000)
        self.assertAlmostEqual(4 + 20 + 5 + 8 + 0.20, cu.cost(entry))

    def test_default_cache_multipliers_for_a_model_without_overrides(self):
        entry = cu.Entry(0.0, "claude-sonnet-4-6", "a", inp=0, out=0, cw5=1_000_000, cw1h=1_000_000, cr=1_000_000)
        self.assertAlmostEqual(3 * 1.25 + 3 * 2 + 3 * 0.1, cu.cost(entry))

    def test_dated_or_suffixed_model_ids_match_their_family(self):
        self.assertEqual(cu.price("claude-opus-5-5"), cu.price("claude-opus-5-5[1m]"))
        self.assertEqual(cu.price("claude-haiku-4-5"), cu.price("claude-haiku-4-5-20251001"))

    def test_unknown_model_costs_nothing(self):
        self.assertEqual(0.0, cu.cost(cu.Entry(0.0, "<synthetic>", "a", inp=10**6, out=10**6)))


class ParseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "proj").mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, lines, name="s.jsonl"):
        (self.root / "proj" / name).write_text("\n".join(lines) + "\n", encoding="utf-8")

    def test_reads_assistant_usage_and_skips_other_lines(self):
        self.write([line(at(9)), json.dumps({"type": "user", "message": {"content": "hi"}}), "not json"])
        entries = cu.read_entries(self.root)
        self.assertEqual(1, len(entries))
        self.assertEqual((1000, 500, "alpha"), (entries[0].inp, entries[0].out, entries[0].project))

    def test_streaming_duplicates_are_counted_once(self):
        self.write([line(at(9), mid="m1", rid="r1"), line(at(9), mid="m1", rid="r1")])
        self.assertEqual(1, len(cu.read_entries(self.root)))

    def test_cache_creation_without_breakdown_counts_as_5_minute(self):
        raw = json.loads(line(at(9)))
        raw["message"]["usage"] = {"input_tokens": 1, "output_tokens": 1, "cache_creation_input_tokens": 300}
        self.write([json.dumps(raw)])
        self.assertEqual((300, 0), (cu.read_entries(self.root)[0].cw5, cu.read_entries(self.root)[0].cw1h))

    def test_no_message_text_is_kept(self):
        self.write([line(at(9))])
        self.assertNotIn("SECRET", repr(cu.read_entries(self.root)))


class BlockTests(unittest.TestCase):
    def e(self, when, usd_tokens=1_000_000):
        return cu.Entry(when.timestamp(), "claude-opus-5-5", "a", inp=usd_tokens, out=0)

    def test_blocks_start_on_the_hour_and_last_five_hours(self):
        blocks = cu.blocks([self.e(at(9, 40)), self.e(at(13, 59)), self.e(at(14, 5))])
        self.assertEqual([at(9).timestamp(), at(14).timestamp()], [b.start for b in blocks])

    def test_a_long_gap_starts_a_new_block(self):
        blocks = cu.blocks([self.e(at(1)), self.e(at(2)), self.e(at(10, 30))])
        self.assertEqual(2, len(blocks))
        self.assertEqual(at(10).timestamp(), blocks[1].start)

    def test_block_cost_sums_its_entries(self):
        blocks = cu.blocks([self.e(at(9)), self.e(at(10))])
        self.assertAlmostEqual(8.0, blocks[0].usd)


class ReportTests(unittest.TestCase):
    def e(self, when, project="a", tokens=1_000_000, out=0):
        return cu.Entry(when.timestamp(), "claude-opus-5-5", project, inp=tokens, out=out)

    def test_today_totals_and_top_projects(self):
        entries = [self.e(at(9), "alpha"), self.e(at(10), "beta"), self.e(at(10, 30), "beta"),
                   self.e(at(23) - timedelta(days=1), "old")]
        frame = cu.report(entries, now=at(11))
        self.assertEqual(3_000_000, frame["day"]["in"])
        self.assertAlmostEqual(12.0, frame["day"]["usd"])
        self.assertEqual([["beta", 8.0], ["alpha", 4.0]], frame["top"])

    def test_active_block_shows_percent_of_the_biggest_block_and_time_left(self):
        old = [self.e(at(1) - timedelta(days=2)), self.e(at(2) - timedelta(days=2)),
               self.e(at(3) - timedelta(days=2)), self.e(at(4) - timedelta(days=2))]  # $16 block
        frame = cu.report(old + [self.e(at(9, 10)), self.e(at(10))], now=at(11))
        block = frame["blk"]
        self.assertAlmostEqual(8.0, block["usd"])
        self.assertEqual(50, block["pct"])
        self.assertEqual(3 * 3600, block["left"])
        self.assertEqual("09:00", block["start"])

    def test_no_active_block_after_five_hours(self):
        frame = cu.report([self.e(at(3))], now=at(11))
        self.assertIsNone(frame["blk"])

    def test_hourly_messages_for_today(self):
        frame = cu.report([self.e(at(9)), self.e(at(9, 30)), self.e(at(14))], now=at(15))
        self.assertEqual(24, len(frame["h"]))
        self.assertEqual(2, frame["h"][9])
        self.assertEqual(1, frame["h"][14])

    def test_empty_history(self):
        frame = cu.report([], now=at(11))
        self.assertEqual(0, frame["day"]["in"])
        self.assertIsNone(frame["blk"])
        self.assertEqual([], frame["top"])


class FeedTests(unittest.TestCase):
    def test_version_moves_only_when_the_frame_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "p").mkdir()
            feed = cu.UsageFeed(root, clock=lambda: at(11))
            feed.refresh()
            v1, f1 = feed.snapshot()
            feed.refresh()
            self.assertEqual(v1, feed.snapshot()[0])
            (root / "p" / "s.jsonl").write_text(line(at(10)) + "\n", encoding="utf-8")
            feed.refresh()
            v2, f2 = feed.snapshot()
            self.assertNotEqual(v1, v2)
            self.assertEqual(1000, f2["day"]["in"])

    def test_clock_minute_alone_does_not_count_as_a_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            now = [at(11)]
            feed = cu.UsageFeed(Path(tmp), clock=lambda: now[0])
            feed.refresh()
            v1 = feed.snapshot()[0]
            now[0] = at(11, 1)
            feed.refresh()
            self.assertEqual(v1, feed.snapshot()[0])


if __name__ == "__main__":
    unittest.main()
