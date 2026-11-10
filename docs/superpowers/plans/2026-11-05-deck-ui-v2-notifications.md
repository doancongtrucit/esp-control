# Deck UI v2 + Notifications Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the footer-tab UI of the `cyd_2usb` firmware with the approved Home-grid design and add a Notifications page fed by an iPhone over BLE (ANCS) and by the PC over Wi-Fi.

**Architecture:** The ESP32 keeps USB serial as the main link (stats, macros, gold, config). New: a NimBLE peripheral that bonds with the iPhone and reads ANCS, and a Wi-Fi station that receives PC notifications as token-checked UDP JSON. Wi-Fi credentials and the UDP token are provisioned over USB by the bridge from the local, git-ignored `host/config.json`, so no secret is compiled in. The bridge gains a Windows notification watcher that forwards every toast over UDP.

**Tech Stack:** PlatformIO `espressif32@6.6.0` (Arduino), TFT_eSPI 2.5.31, ArduinoJson 7.4.3, XPT2046_Touchscreen v1.4, NimBLE-Arduino ^2.1.0 (spike used 2.5.2), Unity on-device tests (`pio test`); Python 3 bridge with unittest/pytest.

**Spec:** Design canvas "ESP32 Desk Deck UI" (Claude Design, Agentic design system) — boards Home, Monitor, Controls, Gold price, Claude Code, AI Tools, AI GPU, Notifications; decisions from the 2026-11-05 review; results of a throwaway local spike (display + Wi-Fi + BLE ≈ 153 KB free heap, ANCS and UDP verified).

## Global Constraints

- Only the `cyd_2usb` environment gets BLE/Wi-Fi/UI v2; `esp32_pc_control_deck` must still build (guard new code with `DECK_UI_V2`).
- UI labels are English (built-in TFT_eSPI fonts). Notification and message content (app, sender, body) keeps Vietnamese with diacritics, drawn with a Vietnamese-capable u8g2 font through U8g2_for_TFT_eSPI; code points the font lacks (emoji, symbols) are dropped.
- Colors (RGB565 of the Agentic tokens): surface `#ffffff`, ink `#1e2771`, header-button `#2c3684`, tile `#eef1fa`, muted text `#4a5290` (only text color besides ink/white), border `#9fa9d4` (decor only), accent `#ff8a3d` and accent-2 `#7fa9ee` (fills only, never text), up `#1a7f4b`, down `#c0362c`.
- Screen 320×240 landscape; header 32 px (navy, white text) with a 40×32 Home button on every page except Home; no footer.
- Buttons on one page are equal size; Controls buttons show a 24 px icon above the label.
- Notifications: accept all apps; keep the latest 10; "HIDE TEXT" toggle default OFF.
- No personal data in git (no SSID, password, token, IP, names). Commit identity is the repo-local GitHub noreply one; the pre-commit hook must pass.
- Protocol version stays 3; new frame types are additive.

## Review Focus

1. Vietnamese text with diacritics, emoji and 300-char messages → diacritics shown correctly, emoji removed, cut on a character boundary (never mid UTF-8 sequence) with "...", never overflowing its row. (Task 1 tests.)
2. iPhone walks out of range and back → PHONE chip turns red, device re-advertises, reconnects without reboot or heap leak. (Task 5 manual check + heap log.)
3. 15 notifications in 5 seconds → store keeps the newest 10 in order, UI stays responsive. (Task 2 test.)
4. Wi-Fi missing or wrong password → USB features keep working, PC chip red, retries every 30 s. (Task 6 manual check.)
5. UDP packet with a wrong or missing token from another LAN host → ignored and counted. (Task 6 test.)

---

### Task 1: Vietnamese text — sanitizer and font

**Files:**
- Create: `src/text_vi.h`, `src/text_vi.cpp`
- Create: `test/test_text_vi/test_main.cpp`
- Modify: `platformio.ini` (`[env:cyd_2usb]`: add `-DDECK_UI_V2=1`, `test_build_src = yes`, lib_deps `olikraus/U8g2@2.36.18` and `https://github.com/Bodmer/U8g2_for_TFT_eSPI.git` pinned to a commit)

**Interfaces:**
- Produces:
  - `String sanitizeVi(const String &utf8, size_t maxChars);` — keeps ASCII printable and the Vietnamese letters (Latin-1 Supplement, Latin Extended-A/B and U+1EA0–U+1EF9 that Vietnamese uses), drops every other code point (emoji, symbols, invalid bytes), collapses whitespace runs (newlines included) to one space, trims both ends, cuts to at most `maxChars` characters (code points, not bytes) with a trailing `...` when cut.
  - `size_t utf8Length(const String &s);` (code points)
  - `void drawViText(int x, int y, const String &utf8, uint16_t color, uint16_t bg);` — draws with the u8g2 Vietnamese font (`u8g2_font_unifont_t_vietnamese1` or `_vietnamese2`, whichever covers all of `Đ đ Ă ă Â â Ê ê Ô ô Ơ ơ Ư ư` and the five tone marks — check in Step 3), baseline-aligned so a 16 px line fits the 34 px notification row twice.

- [ ] **Step 1: Write failing on-device tests**

```cpp
TEST_ASSERT_EQUAL_STRING("Đi ngủ đi", sanitizeVi("Đi ngủ đi", 40).c_str());
TEST_ASSERT_EQUAL_STRING("Bạn An", sanitizeVi("Bạn An 👩", 40).c_str());
TEST_ASSERT_EQUAL_STRING("Nguyễn Văn A", sanitizeVi("Nguyễn  Văn\nA", 40).c_str());
TEST_ASSERT_EQUAL_STRING("Tiếng V...", sanitizeVi("Tiếng Việt có dấu", 10).c_str());
TEST_ASSERT_EQUAL(10u, utf8Length(sanitizeVi("Tiếng Việt có dấu", 10)));
TEST_ASSERT_EQUAL(40u, utf8Length(sanitizeVi(String(300, 'x'), 40)));
```

- [ ] **Step 2: Run, expect FAIL** — stop the bridge first (it holds COM4). Run: `pio test -e cyd_2usb -f test_text_vi --upload-port COM4 --test-port COM4` → compile error "sanitizeVi not declared".
- [ ] **Step 3: Implement** — decode UTF-8 manually (1–4 byte sequences, skip invalid); `drawViText` through `U8g2_for_TFT_eSPI` (`begin(tft)`, `setFont`, `setForegroundColor`, `setBackgroundColor`, `drawUTF8`). Draw a test line "Đi ngủ đi – Ơn trời, ưng ý! ĂÂÊÔƠƯ" and check every glyph on the panel.
- [ ] **Step 4: Run, expect PASS** (same command).
- [ ] **Step 5: Commit** `feat(cyd): Vietnamese text sanitizer and font for notifications`

### Task 2: Notification store

**Files:**
- Create: `src/notify_store.h`, `src/notify_store.cpp`
- Create: `test/test_notify_store/test_main.cpp`

**Interfaces:**
- Consumes: `sanitizeVi` (Task 1).
- Produces:
  - `enum class NotifySource : uint8_t { Phone, Pc };`
  - `struct Notification { NotifySource source; String app; String from; String body; uint32_t receivedMs; bool unread; };` (fields already sanitized with `sanitizeVi`: app ≤ 12, from ≤ 24, body ≤ 48 characters)
  - `class NotifyStore { public: static constexpr size_t CAPACITY = 10; void add(NotifySource, const String &app, const String &from, const String &body, uint32_t nowMs); size_t size() const; const Notification &at(size_t i) const; /* 0 = newest */ size_t unreadCount() const; void markAllRead(); uint32_t version() const; /* bumps on every change */ };`

- [ ] **Step 1: Failing tests** — `add` 15 items with bodies "n0".."n14": `size()==10`, `at(0).body=="n14"`, `at(9).body=="n5"`, `unreadCount()==10`; after `markAllRead()` `unreadCount()==0` and `version()` increased; an `add` with a 300-char Vietnamese body yields `at(0).body.length()<=48` and contains no emoji bytes.
- [ ] **Step 2: Run, expect FAIL**: `pio test -e cyd_2usb -f test_notify_store --upload-port COM4 --test-port COM4`
- [ ] **Step 3: Implement** as a fixed ring buffer of 10 (no heap growth beyond the Strings).
- [ ] **Step 4: Run, expect PASS.**
- [ ] **Step 5: Commit** `feat(cyd): add bounded notification store`

### Task 3: UI v2 shell — Home grid, header, page router, ported pages

**Files:**
- Create: `src/theme.h` (RGB565 constants for Global Constraints colors)
- Create: `src/icons.h`, `src/icons.cpp`
- Create: `src/ui_v2.h`, `src/ui_v2.cpp`
- Modify: `src/main.cpp` (under `#ifdef DECK_UI_V2`: route rendering/touch to `ui_v2`, drop footer; keep existing drawing for the Elecrow env)

**Interfaces:**
- Consumes: existing `stats`, `info`, `macroLabels`, `macroHold`, `holdToConfirm`, `sendMacro`, `currentClockText` in `main.cpp` (declare them `extern` in `ui_v2.h`); `NotifyStore` (Task 2).
- Produces:
  - `enum class Screen : uint8_t { Home, Monitor, Controls, Gold, Claude, AiTools, AiGpu, Notify, Settings };`
  - `enum class Icon : uint8_t { Monitor, Controls, Gold, Sparkle, Clipboard, Chip, Bell, Gear, Plus, Activity, Download, Lock, Screenshot, Globe, Sliders, Gem, Chat, Users, Mail, Bubble, Gamepad, Headset, Bars, VolDown, Mute, VolUp, Phone, Wifi, Home, ChevronLeft, ChevronRight };`
  - `void drawIcon(Icon icon, int x, int y, int size, uint16_t color);` (stroke-style drawing with TFT_eSPI lines/rects/circles, 2 px stroke at size 24)
  - `void uiShow(Screen s);` `void uiRenderFull();` `void uiRenderDynamic();` `void uiHandleTouch(uint16_t x, uint16_t y);` `void uiNotifyChanged();`
- Macro icons come from the bridge: config frame entries gain optional `"icon": "<name>"` (names = `Icon` enum names lower-case, e.g. `"lock"`); missing → `Icon::Controls`.

- [ ] **Step 1:** Implement `theme.h` and `drawIcon` for every `Icon`; add a temporary `#ifdef DECK_ICON_SHEET` mode that draws all icons in a grid, flash it and check each against the canvas (manual, screenshot photo).
- [ ] **Step 2:** Implement the router and Home (3×3 equal tiles: MONITOR, CONTROLS, GOLD, CLAUDE, AI TOOLS, AI GPU, NOTIFY, SETTINGS, ADD PAGE; NOTIFY badge = `unreadCount()`, orange badge on CLAUDE reserved for phase 2).
- [ ] **Step 3:** Port Monitor (4 bars + NETWORK/DISK C: boxes), Controls (3 pages, ‹ › in header, `1/3` pager, 6 equal buttons with icon + label, HOLD badge + bar), Gold (table, `x1000 VND / chi`, BUY/SELL/+/-, World + Updated line). Claude, AI Tools, AI GPU, Settings render the header and a centered "Coming in phase 2" note.
- [ ] **Step 4:** Bridge: send `"icon"` per macro from `config.json` (`macro_packet` passes `item["icon"]` when present); add `icon` to all 18 local macros and to `config.example.json` (generic ones); extend `test_macro_packet_keeps_three_pages_and_hold_flag` to assert `icon` passes through.
- [ ] **Step 5:** Verify: `pio run` (both envs SUCCESS), `pytest -q host/tests` PASS, flash `cyd_2usb`, walk every tile and the Home button; every Controls button still sends its macro (bridge log shows `Macro N pressed`).
- [ ] **Step 6: Commit** `feat(cyd): home-grid UI v2 with icons, English labels`

### Task 4: Notifications page + USB `notify` frame

**Files:**
- Modify: `src/ui_v2.cpp` (Notify screen), `src/main.cpp` (`processSerialLine`: `notify` type)
- Modify: `docs/PROTOCOL.md`
- Test: `host/tests/test_bridge.py`

**Interfaces:**
- Consumes: `NotifyStore` (Task 2), `uiNotifyChanged()` (Task 3).
- Produces: USB frame PC→ESP32 `{"type":"notify","src":"pc"|"phone","app":"Teams","from":"Sender","body":"Text"}`; bridge helper `notify_packet(app: str, sender: str, body: str, source: str = "pc") -> dict`.

- [ ] **Step 1:** Failing pytest `test_notify_packet_shape`: `notify_packet("Teams","A","B")` equals the frame above with `src "pc"`; strings longer than 120 chars are cut to 120.
- [ ] **Step 2:** Implement `notify_packet`; firmware handles `notify` → `store.add(...)`, LED flashes blue 300 ms, a 2.5 s banner (top 40 px, ink on `#fff1e8`, orange border) appears on any screen except Notify.
- [ ] **Step 3:** Notify screen: PHONE and PC chips (green/red dot from link state, Tasks 5–6 set them; default red), list of newest 4 rows (source icon; `App - From` and body drawn with `drawViText`, keeping Vietnamese diacritics; body replaced by "Content hidden" when HIDE TEXT is on); the 2.5 s banner also uses `drawViText`, HIDE TEXT toggle in header, opening the screen calls `markAllRead()`.
- [ ] **Step 4:** Verify: pytest PASS; send a frame with a small Python snippet through the bridge's `send_json`, see banner, badge and row.
- [ ] **Step 5: Commit** `feat(cyd): notifications page, banner and notify frame`

### Task 5: iPhone notifications over BLE (ANCS)

**Files:**
- Create: `src/ancs.h`, `src/ancs.cpp` (port of the spike)
- Modify: `src/main.cpp` (`setup`/`loop` hooks), `platformio.ini` (`[env:cyd_2usb]` lib_deps `h2zero/NimBLE-Arduino@^2.1.0`)

**Interfaces:**
- Consumes: `NotifyStore::add` with `NotifySource::Phone`.
- Produces: `void ancsBegin(const char *deviceName);` `void ancsLoop();` `bool ancsConnected();` `bool ancsReady();`

- [ ] **Step 1:** Port the spike: ANCS solicitation advertising, bond (no MITM, SC), `NimBLEServer::getClient(connInfo)`, subscribe Data Source then Notification Source, queue UIDs from callbacks, request AppIdentifier/Title(32)/Message(64) from `loop`.
- [ ] **Step 2:** Reassemble fragmented Data Source replies until all three attributes are parsed (spike ignored this); drop `pre-existing` notifications (EventFlags bit 2) on reconnect; map bundle ids to names: `com.apple.MobileSMS`→Messages, `com.apple.mobilephone`→Phone, `com.facebook.Messenger`→Messenger, `com.vng.zalo`→Zalo, `com.microsoft.skype.teams`→Teams, `com.microsoft.Office.Outlook`→Outlook, else last id segment.
- [ ] **Step 3:** Verify on device with the bonded iPhone: receive 3 messages; toggle iPhone Bluetooth off/on twice — PHONE chip red then green, serial heap line after each reconnect within 4 KB of the first.
- [ ] **Step 4: Commit** `feat(cyd): iPhone notifications over BLE ANCS`

### Task 6: Wi-Fi provisioning + UDP notify listener

**Files:**
- Create: `src/net_notify.h`, `src/net_notify.cpp`
- Create: `test/test_net_notify/test_main.cpp`
- Modify: `src/main.cpp` (`wifi` frame, `wifi_status` reply)

**Interfaces:**
- Produces:
  - USB PC→ESP32 `{"type":"wifi","ssid":"…","pass":"…","token":"…","port":4210}` → saved in Preferences namespace `deckwifi`; ESP32→PC `{"type":"wifi_status","connected":true,"ip":"a.b.c.d","port":4210}` on connect and on change.
  - UDP payload `{"token":"…","app":"…","from":"…","body":"…"}`.
  - `bool parseUdpNotify(const char *payload, const String &token, String &app, String &from, String &body);` (pure, unit-tested) `void netBegin();` `void netLoop();` `bool netConnected();` `uint32_t netRejectedCount();`

- [ ] **Step 1:** Failing tests: valid payload with matching token → true and fields set; wrong token, missing token, non-JSON, 2 KB payload → false.
- [ ] **Step 2:** Run `pio test -e cyd_2usb -f test_net_notify …` → FAIL; implement; → PASS.
- [ ] **Step 3:** `netLoop` reconnects every 30 s when down, never blocks more than 50 ms, keeps `WiFi.setSleep(true)` (required with BLE).
- [ ] **Step 4:** Verify: with no `wifi` frame ever sent, all USB features work and PC chip is red.
- [ ] **Step 5: Commit** `feat(cyd): Wi-Fi provisioning over USB and UDP notifications`

### Task 7: Bridge — provisioning and Windows notification forwarding

**Files:**
- Create: `host/pc_notifications.py`
- Modify: `host/control_deck_bridge.py`, `host/requirements.txt`, `host/config.example.json`
- Test: `host/tests/test_pc_notifications.py`, `host/tests/test_bridge.py`

**Interfaces:**
- Consumes: `notify_packet` (Task 4), `wifi`/`wifi_status` frames (Task 6).
- Produces: `class PcNotificationWatcher: def __init__(self, on_toast: Callable[[str, str, str], None]); def start(self) -> None` — background thread; `wifi_packet(config: dict) -> dict | None`; `send_udp_notify(host: str, port: int, token: str, app: str, sender: str, body: str) -> None`.
- Config (`host/config.json`, git-ignored): `"wifi": {"ssid": "…", "password": "…"}`, `"notifications": {"enabled": true, "udp_port": 4210, "token": "<random 24 chars, generated on first run if empty>"}`.

- [ ] **Step 1: Spike (time-box 30 min)** — confirm a plain Python process can read toasts with `winsdk` `UserNotificationListener` (`RequestAccessAsync` then `GetNotificationsAsync(Toast)`). If access is denied, use the fallback: poll `%LOCALAPPDATA%\Microsoft\Windows\Notifications\wpndatabase.db` (SQLite, read-only copy) every 2 s. Record which path works in this plan.
- [ ] **Step 2:** Failing tests: `wifi_packet` returns None without `wifi.ssid`, else the Task 6 frame; token auto-generated (24 chars, `secrets.token_urlsafe`) and written back once; watcher de-duplicates the same toast id; `send_udp_notify` payload matches Task 6 shape (patch `socket.socket`).
- [ ] **Step 3:** Implement; bridge sends `wifi` frame after `hello`, records the IP from `wifi_status`, forwards each toast with `send_udp_notify`; if no IP yet, falls back to the USB `notify` frame.
- [ ] **Step 4:** Verify: `pytest -q host/tests` PASS; a Teams/Outlook toast on the PC appears on the device; the bridge log never prints SSID, password, token or message text.
- [ ] **Step 5: Commit** `feat(bridge): Windows toast forwarding over Wi-Fi`

### Task 8: Docs, hand-off and release

**Files:**
- Modify: `README.md` (UI v2, iPhone pairing steps, Wi-Fi provisioning, privacy), `docs/PROTOCOL.md`, `docs/MACROS.md` (`icon` field), `CHANGELOG.md`

- [ ] **Step 1:** Update docs; README pairing steps: Settings → Bluetooth → "ESP32 Deck" (or nRF Connect) → allow notifications.
- [ ] **Step 2:** Full verification: `pio run` both envs, `pytest -q host/tests`, flash, run the Review Focus list by hand, heap after 30 min ≥ 100 KB free.
- [ ] **Step 3:** Back up the full flash image to a local folder outside the repository (it holds Wi-Fi settings and pairing keys).
- [ ] **Step 4: Commit** `docs: UI v2 and notifications`; push only after the user says so.

## Out of scope (phase 2)

Claude Code status (hooks → bridge), AI Tools (Ollama clipboard actions), AI GPU page, Settings page. Their Home tiles exist and open a placeholder.
