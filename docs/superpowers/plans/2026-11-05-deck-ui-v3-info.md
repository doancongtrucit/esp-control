# Deck UI v3 (dark launcher) + Info hub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restyle the `cyd_2usb` UI v2 into the approved dark iPhone-launcher design and replace the Gold screen with an Info screen holding five tabs: Gold, Coin, Stock, Weather, Calendar (solar + Vietnamese lunar).

**Architecture:** Same split as UI v2: pure, unit-tested helpers in their own files (`color_util`, `lunar`), screens in `src/ui_v2_screens.h` inside `main.cpp`'s translation unit, data fetched by the host bridge and sent as separate USB frames per feed (`gold` stays inside `info`; new `coin`, `stock`, `weather` frames) so no frame exceeds the 2048-byte serial line. The calendar is computed on the ESP32 from the PC date sent in the stats frame.

**Tech Stack:** PlatformIO `espressif32@6.6.0`, TFT_eSPI 2.5.31, ArduinoJson 7.4.3, U8g2_for_TFT_eSPI (pinned), NimBLE-Arduino 2.5.2, Unity on-device tests; Python bridge with pytest.

**Spec:** Design canvas "ESP32 Desk Deck UI" (Claude Design) — boards Home, Controls, Notifications, Monitor, Claude Code, AI Tools, AI GPU, Info - Gold/Coin/Stocks/Weather/Calendar (dark launcher version of 2026-11-05) and the review decisions: dark theme with light text, iPhone-launcher icons, English labels, weather city Da Nang.

**Branch:** `ui-v3` cut from `ui-v2` (ui-v2 is not merged yet; it must be squashed before any push because an early commit holds a personal name).

## Global Constraints

- Only `cyd_2usb` changes; `esp32_pc_control_deck` must still build. Protocol version stays 3; new frame types are additive.
- Palette (RGB565 of): background `#0d0f14`, card `#1c2029`, card-alt `#14171e`, header line / home button `#232836`, divider `#2e3442`, text `#f2f4fb`, muted text `#a3abbd`, up `#5fd39a`, down `#ff7a6b`, accent orange `#ff8a3d`, link blue `#0a84ff`, alert ground `#3a2414`. Text on orange/yellow fills is `#0d0f14`.
- App icon gradients (from → to): Monitor `#30d158→#1a9e48`, Controls `#0a84ff→#0060df`, Info `#ffd60a→#f5a300`, Claude `#ff9f0a→#ff6b1a`, AI Tools `#bf5af2→#8e44ec`, AI GPU `#64d2ff→#2b8fd8`, Notify `#ff375f→#e0244a`, Settings `#8e8e93→#5c5c63`; icon box 48 px, radius 13, white glyph 26 px; unread badge `#d92d20` with white text.
- Home: status bar 26 px (date "Thu, 11/05", time, Wi-Fi, Bluetooth, USB), 4×2 launcher, footer line "PC online · iPhone · Wi-Fi" with green/red dots. Other screens: 34 px header, 28 px rounded home button, 1 px bottom line.
- Info: tab strip of 5 equal tabs under the header, active tab = card background + 2 px colored underline (Gold `#ffd60a`, Coin `#ff9f0a`, Stock `#30d158`, Weather `#64d2ff`, Calendar `#ff375f`).
- Weather city Da Nang (16.0544, 108.2022). Coins default BTC, ETH, BNB, SOL, XRP (CoinGecko ids in config). Stocks: VN-Index, VN30, HNX-Index + watchlist from config (default VNM, HPG).
- No personal data in git; repo-local noreply identity; pre-commit hook must pass.

## Review Focus

1. Lunar dates around month starts and leap months (e.g. 2025 leap 6th month, Tết 2026) → correct day/month and leap flag. (Task 2 tests.)
2. A feed fails or the PC is offline → the tab shows "No data yet" or the last values with their update time, never stale-looking zeros. (Task 6/7 tests + hand check.)
3. Stock API returns an unknown ticker or a market-closed day → the row is skipped, other rows still show. (Task 6 test.)
4. PC date missing (old bridge) → Home status bar and Calendar fall back to "--" instead of a wrong date. (Task 3/5 check.)
5. Gradient icons and rounded corners at 48 px never draw outside their box (no artifacts on the black background). (Task 1 tests.)

---

### Task 1: Dark palette, gradients and rounded fills

**Files:** Create `src/color_util.h`, `src/color_util.cpp`, `test/test_color_util/test_main.cpp`; modify `src/theme.h` (replace light tokens with the dark palette, keep names semantic: `T_BG`, `T_CARD`, `T_CARD_ALT`, `T_LINE`, `T_TEXT`, `T_MUTED`, `T_UP`, `T_DOWN`, `T_ACCENT`, `T_LINK`, `T_ALERT_BG`).

**Interfaces — produces:** `uint16_t rgb565(uint32_t rgb);` `uint16_t blend565(uint16_t a, uint16_t b, uint8_t t /*0..255*/);` `int roundedRowInset(int row, int height, int radius);` (pixels to skip at each side of a rounded rect on that row) and `void fillGradientRoundRect(TFT_eSPI &tft, int x, int y, int w, int h, int r, uint16_t top, uint16_t bottom);`.

- [ ] Step 1: failing tests — `rgb565(0xff8a3d)==0xFC47`, `rgb565(0x0d0f14)==0x0862`; `blend565(a,b,0)==a`, `blend565(a,b,255)==b`, mid value between both channels; `roundedRowInset(0,48,13)==13`-ish (> 0), `roundedRowInset(24,48,13)==0`, `roundedRowInset(47,48,13)==roundedRowInset(0,48,13)`, never negative or > radius.
- [ ] Step 2: run `pio test -e cyd_2usb -f test_color_util` → FAIL; implement; → PASS.
- [ ] Step 3: switch `theme.h` to the dark tokens and update every use in `ui_v2_screens.h` (temporary mapping is fine; screens are redrawn in Tasks 3–7).
- [ ] Step 4: `pio run` both envs; commit `feat(cyd): dark palette and gradient helpers`.

### Task 2: Vietnamese lunar calendar

**Files:** Create `src/lunar.h`, `src/lunar.cpp`, `test/test_lunar/test_main.cpp`.

**Interfaces — produces:** `struct LunarDate { int day; int month; int year; bool leap; };` `LunarDate solarToLunar(int day, int month, int year, int tzHours = 7);` `const char *canChiYear(int lunarYear);` (ASCII, e.g. "Binh Ngo") and `int dayOfWeek(int day, int month, int year);` (0 = Monday).

- [ ] Step 1: failing tests (Ho Ngoc Duc algorithm, UTC+7): 2026-02-17 → 1/1/2026; 2026-11-05 → 27/9/2026; 2026-11-09 → 1/10; 2026-11-23 → 15/10; 2025-07-25 → 1/6 leap; 2025-06-25 → 1/6 not leap; `canChiYear(2026)=="Binh Ngo"`; `dayOfWeek(5,11,2026)==3` (Thursday).
- [ ] Step 2: run → FAIL; implement with `double` math; → PASS.
- [ ] Step 3: commit `feat(cyd): Vietnamese lunar calendar`.

### Task 3: Home launcher and status bar

**Files:** Modify `src/ui_v2_screens.h`, `src/main.cpp` (stats frame: optional `"date":"YYYY-MM-DD"` stored in `stats`), `host/control_deck_bridge.py` (`get_stats` adds `date`), `host/tests/test_bridge.py`.

**Interfaces:** consumes `fillGradientRoundRect`, `drawIcon`, `dayOfWeek`; produces `bool deckDate(int &day, int &month, int &year);` (false when the PC never sent a date).

- [ ] Step 1: failing pytest `test_stats_include_local_date` (`get_stats` payload has `date` == today ISO).
- [ ] Step 2: implement bridge; firmware parses `date`; status bar "Thu, 11/05" or "--" when unknown; Wi-Fi / Bluetooth / USB glyphs white when up, muted when down.
- [ ] Step 3: launcher 4×2 (Monitor, Controls, Info, Claude, AI Tools, AI GPU, Notify, Settings) with gradient icon boxes, labels, badge on Notify (unread) and Claude (reserved); footer link line. Touch targets = whole cell.
- [ ] Step 4: build, flash, check on device; commit `feat(cyd): dark launcher home`.

### Task 4: Controls, Notifications and other screens restyled

**Files:** Modify `src/ui_v2_screens.h`.

- [ ] Step 1: shared slim header (34 px, rounded home button, bottom line) for every non-Home screen.
- [ ] Step 2: Controls: 3×2 cells with 50 px gradient icon boxes (per-macro color from config `"color"`, default by page), label below, HOLD pill on the icon, hold bar inside the icon, page dots instead of "1/3". Bridge passes `color` (one of: green, blue, orange, purple, pink, teal, gray, indigo, yellow) — add to `macro_packet` with a failing pytest first.
- [ ] Step 3: Notifications, Monitor and placeholders on the dark palette; banner on `T_ALERT_BG` with orange border.
- [ ] Step 4: build, flash, check; commit `feat(cyd): dark controls and screens`.

### Task 5: Info screen with tabs, Gold and Calendar tabs

**Files:** Modify `src/ui_v2_screens.h` (rename `Screen::Gold` to `Screen::Info`, add `uint8_t infoTab`).

**Interfaces:** consumes `solarToLunar`, `canChiYear`, `dayOfWeek`, `deckDate`; tab hit-test via `gridIndexAt(x, y, 5, 1, …)` on the strip.

- [ ] Step 1: tab strip (5 equal tabs, icon + label, active underline color per Global Constraints).
- [ ] Step 2: Gold tab = existing gold table restyled (rows 21 px, sell in yellow).
- [ ] Step 3: Calendar tab = left card (weekday, big day, month/year, lunar day/month, can-chi year) + Monday-first month grid with lunar day under each solar day; today on blue, lunar 1st and 15th in orange, Sundays in red; "--" when no date.
- [ ] Step 4: build, flash, check; commit `feat(cyd): info screen with gold and calendar tabs`.

### Task 6: Bridge feeds — coin, stock, weather

**Files:** Create `host/info_feeds.py`, `host/tests/test_info_feeds.py`; modify `host/control_deck_bridge.py` (start feeds, send frames), `host/config.example.json`.

**Interfaces — produces frames:**
- `{"type":"coin","items":[{"s":"BTC","p":76497.0,"c":-5.11}],"time":"19:42"}` (USD, 24h %), from `https://api.coingecko.com/api/v3/simple/price?ids=…&vs_currencies=usd&include_24hr_change=true`.
- `{"type":"stock","index":[{"n":"VN-INDEX","v":1740.73,"c":0.44}],"items":[{"n":"VNM","p":57.5,"c":0.17}],"spark":[…],"time":"15:05"}` from VPS `https://bgapidatafeed.vps.com.vn/getlistindexdetail/10,11,02` (index value `cIndex`, change % = 2nd field of `ot`) and `getliststockdata/<tickers>` (`lastPrice` in thousand VND, `changePc`); `spark` = up to 40 VN-Index values sampled by the bridge during the trading day.
- `{"type":"weather","city":"Da Nang","t":27.9,"f":33,"h":79,"wind":9,"icon":1,"desc":"Partly cloudy","days":[{"d":"Fri","hi":29,"lo":24,"rain":83,"icon":3}],"time":"19:30"}` from Open-Meteo (4 days), icon ids as in the earlier weather feed (0 sun … 5 fog).
- Config: `"coins": ["bitcoin","ethereum","binancecoin","solana","ripple"]`, `"stocks": ["VNM","HPG"]`, `"weather": {"city":"Da Nang","latitude":16.0544,"longitude":108.2022}`, refresh minutes per feed (coin 5, stock 2 during 9:00–15:00 else 30, weather 30).
- `class InfoFeeds: def __init__(self, config, http=http_json); def snapshot(self) -> dict[str, tuple[int, dict]]` (version + frame per kind), refreshed on a background thread like `InfoFeed`.

- [ ] Step 1: failing tests with recorded sample JSON (no network): coin frame shape and rounding; stock frame parses `ot` "7.71|0.44%|…" → 0.44, skips unknown tickers, keeps spark ≤ 40; weather maps WMO codes to icon/desc and weekday names in English; a feed error keeps the previous frame (version unchanged).
- [ ] Step 2: implement; bridge sends each changed frame (like `info`), resends all on `hello`.
- [ ] Step 3: `pytest -q host/tests` PASS; live run logs one line per feed update, no data values; commit `feat(bridge): coin, stock and weather feeds`.

### Task 7: Coin, Stock and Weather tabs on the device

**Files:** Modify `src/main.cpp` (`coin`, `stock`, `weather` handlers into fixed-size structs), `src/ui_v2_screens.h`, `src/icons.*` (weather glyphs: sun, partly, cloud, rain, storm, fog drawn in color).

- [ ] Step 1: Coin tab: 5 rows (colored letter circle, symbol, name, price, 24h % up/down), footer "USD · 24h" and update time.
- [ ] Step 2: Stock tab: VN-INDEX big value + change and a polyline sparkline from `spark`, rows VN30, HNX-INDEX and watchlist (price in thousand VND), footer with update time.
- [ ] Step 3: Weather tab: icon, temperature, description, "Da Nang · feels", humidity/wind, 4 day cards with hi/lo and rain %.
- [ ] Step 4: every tab shows "No data yet" until its first frame; build, flash, check with the live bridge; commit `feat(cyd): coin, stock and weather tabs`.

### Task 8: Docs, verification, backup

- [ ] Step 1: README fork section (dark UI, Info tabs, feeds and their sources, config keys), PROTOCOL (`date` in stats, `coin`, `stock`, `weather`), CHANGELOG.
- [ ] Step 2: `pio run` both envs, all device and host tests, hand check list with the user (icons, tabs, lunar month view, feeds updating).
- [ ] Step 3: local flash backup outside the repository; commit `docs: UI v3 and info hub`; no push without the user.

## Out of scope

Claude Code status, AI Tools, AI GPU and Settings content (still placeholders); swipe gestures; the deferred minors from the UI v2 review.
