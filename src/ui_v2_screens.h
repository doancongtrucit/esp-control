// UI v2 (Home grid design). Included into main.cpp's anonymous namespace so the
// screens share the deck state (stats, info, macros, notifyStore) without
// exporting it. Only built with DECK_UI_V2.
#pragma once

enum class Screen : uint8_t { Home, Monitor, Controls, Info, Claude, AiTools, Standby, Notify, Settings, Aram, Break };
Screen screen = Screen::Home;
uint8_t controlsPage = 0;
uint32_t drawnNotifyVersion = 0;
bool bannerActive = false;
uint32_t bannerUntil = 0;
constexpr uint32_t BANNER_MS = 2500;
constexpr int BANNER_H = 40;

// Home launcher: 4x2 app icons with vertical gradients (from -> to, 0xRRGGBB).
struct HomeApp {
  const char *label;
  Icon icon;
  Screen target;
  uint32_t from;
  uint32_t to;
};

constexpr HomeApp HOME_APPS[8] = {
    {"Monitor", Icon::Monitor, Screen::Monitor, 0x30d158, 0x1a9e48},
    {"Controls", Icon::Controls, Screen::Controls, 0x0a84ff, 0x0060df},
    {"Info", Icon::Pie, Screen::Info, 0xffd60a, 0xf5a300},
    {"Claude", Icon::Sparkle, Screen::Claude, 0xff9f0a, 0xff6b1a},
    {"AI Tools", Icon::Clipboard, Screen::AiTools, 0xbf5af2, 0x8e44ec},
    {"Standby", Icon::Clock, Screen::Standby, 0x64d2ff, 0x2b8fd8},
    {"Notify", Icon::Bell, Screen::Notify, 0xff375f, 0xe0244a},
    {"Settings", Icon::Gear, Screen::Settings, 0x8e8e93, 0x5c5c63},
};
constexpr int NOTIFY_APP = 6;
constexpr int STATUS_BAR_H = 26;
constexpr int HOME_FOOTER_Y = 216;
constexpr int APP_ICON = 48;
// Launcher grid box: below the status bar, above the footer.
constexpr int HOME_GRID_X = 8, HOME_GRID_Y = 30, HOME_GRID_W = 304, HOME_GRID_H = 184;
const char *const CONTROLS_TITLES[MACRO_PAGES] = {"PC", "AI / WORK", "GAME"};

// Header: home button area x < 40; Controls page arrows at 40-80 and 150-190.
constexpr int HOME_BTN_W = 40;
constexpr int PREV_BTN_X = 40;
constexpr int NEXT_BTN_X = 150;
constexpr int ARROW_BTN_W = 36;
constexpr int STATUS_X = 228;

void uiShow(Screen target) {
  screen = target;
  fullRedrawRequested = true;
}

// Slim dark header: rounded home button, title, 1 px line under it.
void drawHeaderV2(const char *title, bool homeButton) {
  tft.fillRect(0, 0, UI_SCREEN_W, UI_HEADER_H - 1, T_BG);
  tft.drawFastHLine(0, UI_HEADER_H - 1, UI_SCREEN_W, T_LINE);
  if (homeButton) {
    tft.fillRoundRect(6, 2, 28, 28, 8, T_LINE);
    drawIcon(tft, Icon::Home, 12, 8, 16, T_TEXT, T_LINE);
  }
  if (title) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_TEXT, T_BG);
    uiTextIn(UiFont::BodyBold, title, homeButton ? 44 : 10, UI_HEADER_H / 2);
  }
}

// USB link dot and PC clock, refreshed every second.
void drawHeaderStatusV2() {
  // The banner covers the header; the Notifications header uses this area for HIDE.
  if (bannerActive || screen == Screen::Notify || screen == Screen::Home || screen == Screen::Standby ||
      screen == Screen::Break)
    return;
  const bool stale = stats.everUpdated && millis() - stats.lastUpdateMs > STALE_AFTER_MS;
  const uint16_t dot = !stats.everUpdated ? T_DIVIDER : (stale ? T_DOWN : T_UP);
  tft.fillRect(STATUS_X, 0, UI_SCREEN_W - STATUS_X, UI_HEADER_H - 1, T_BG);
  tft.fillCircle(STATUS_X + 30, UI_HEADER_H / 2, 3, dot);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  uiText(currentClockText(), UI_SCREEN_W - 10, UI_HEADER_H / 2, 2);
}

// ---------- Home ----------

const char *const WEEKDAY_SHORT[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// PC local date from the stats frame; false until the bridge has sent one.
bool deckDate(int &day, int &month, int &year) {
  if (stats.dateYear == 0) return false;
  day = stats.dateDay;
  month = stats.dateMonth;
  year = stats.dateYear;
  return true;
}

bool usbLive() { return stats.everUpdated && millis() - stats.lastUpdateMs <= STALE_AFTER_MS; }

// Date, clock and link glyphs; redrawn every second.
void drawHomeStatusBar() {
  tft.fillRect(0, 0, UI_SCREEN_W, STATUS_BAR_H, T_BG);
  int d, m, y;
  String date = "--";
  if (deckDate(d, m, y)) {
    char text[16];
    snprintf(text, sizeof(text), "%s, %02d/%02d", WEEKDAY_SHORT[dayOfWeek(d, m, y)], m, d);
    date = text;
  }
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  uiText(date, 12, STATUS_BAR_H / 2, 2);

  // Right to left: USB box, Bluetooth, Wi-Fi, clock.
  const uint16_t usb = usbLive() ? T_TEXT : T_MUTED;
  tft.drawRoundRect(284, 7, 28, 13, 4, usb);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(usb, T_BG);
  uiText("USB", 298, 14, 1);
  drawIcon(tft, Icon::Bluetooth, 266, 6, 14, phoneLinkUp ? T_TEXT : T_MUTED, T_BG);
  drawIcon(tft, Icon::Wifi, 246, 6, 14, pcLinkUp ? T_TEXT : T_MUTED, T_BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  uiText(currentClockText(), 240, STATUS_BAR_H / 2, 2);
}

void drawHomeApp(int index) {
  const CellRect c = gridCellIn(index, 4, 2, HOME_GRID_X, HOME_GRID_Y, HOME_GRID_W, HOME_GRID_H, 0);
  const HomeApp &app = HOME_APPS[index];
  const int ix = c.x + (c.w - APP_ICON) / 2;
  const int iy = c.y + 12;
  tft.fillRect(c.x, c.y, c.w, c.h, T_BG);
  const uint16_t top = rgb565(app.from);
  const uint16_t bottom = rgb565(app.to);
  fillGradientRoundRect(tft, ix, iy, APP_ICON, APP_ICON, 13, top, bottom);
  drawIcon(tft, app.icon, ix + 11, iy + 11, 26, 0xFFFF, blend565(top, bottom, 128));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  uiText(app.label, c.x + c.w / 2, iy + APP_ICON + 14, 2);

  if (index == NOTIFY_APP) {
    const size_t unread = notifyStore.unreadCount();
    if (unread > 0) {
      const String count = unread > 9 ? String("9+") : String(unread);
      tft.fillCircle(ix + APP_ICON - 2, iy + 2, 10, T_BG);
      tft.fillCircle(ix + APP_ICON - 2, iy + 2, 8, T_BADGE);
      tft.setTextColor(0xFFFF, T_BADGE);
      uiText(count, ix + APP_ICON - 2, iy + 3, 1);
    }
    drawnNotifyVersion = notifyStore.version();
  }
}

// "PC online · iPhone · Wi-Fi" with a green/red dot per link.
void drawHomeFooter() {
  tft.fillRect(0, HOME_FOOTER_Y, UI_SCREEN_W, UI_SCREEN_H - HOME_FOOTER_Y, T_BG);
  const char *labels[3] = {"PC online", "iPhone", "Wi-Fi"};
  const bool up[3] = {usbLive(), phoneLinkUp, pcLinkUp};
  int total = 0;
  for (const char *label : labels) total += 10 + uiTextWidth(label, 2) + 12;
  int x = (UI_SCREEN_W - total + 12) / 2;
  const int midY = HOME_FOOTER_Y + 12;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_BG);
  for (int i = 0; i < 3; ++i) {
    tft.fillCircle(x + 3, midY, 3, up[i] ? T_UP : T_DOWN);
    uiText(labels[i], x + 10, midY, 2);
    x += 10 + uiTextWidth(labels[i], 2) + 12;
  }
}

// Everything the status bar and footer show; they repaint only when it changes.
String homeChromeKey() {
  int d = 0, m = 0, y = 0;
  deckDate(d, m, y);
  return currentClockText() + d + "/" + m + (usbLive() ? "U" : "u") + (phoneLinkUp ? "P" : "p") +
         (pcLinkUp ? "W" : "w");
}
String drawnHomeKey;

void drawHomeV2() {
  drawHomeStatusBar();
  for (int i = 0; i < 8; ++i) drawHomeApp(i);
  drawHomeFooter();
  drawnHomeKey = homeChromeKey();
}

void touchHomeV2(uint16_t x, uint16_t y) {
  const int index = gridIndexIn(x, y, 4, 2, HOME_GRID_X, HOME_GRID_Y, HOME_GRID_W, HOME_GRID_H, 0);
  if (index >= 0) uiShow(HOME_APPS[index].target);
}

// ---------- Monitor ----------

constexpr int MON_ROW_Y[4] = {42, 70, 98, 126};
constexpr int MON_BAR_X = 56;
constexpr int MON_BAR_W = 126;
constexpr int MON_BOX_Y = 156;
constexpr int MON_BOX_H = 74;

String tempText(float celsius) { return isnan(celsius) ? String("N/A") : fmtValue(celsius) + "C"; }

void drawMonitorV2Static() {
  drawHeaderV2("MONITOR", true);
  const char *names[4] = {"CPU", "RAM", "GPU", "VRAM"};
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_SURFACE);
  for (int i = 0; i < 4; ++i) uiText(names[i], 10, MON_ROW_Y[i] + 8, 2);
  const char *boxes[2] = {"NETWORK", "DISK C:"};
  for (int i = 0; i < 2; ++i) {
    const int x = 10 + i * 153;
    tft.fillRoundRect(x, MON_BOX_Y, 147, MON_BOX_H, 8, T_TILE);
    tft.setTextColor(T_MUTED, T_TILE);
    uiText(boxes[i], x + 8, MON_BOX_Y + 12, 1);
  }
}

void drawMonitorRow(int row, float pct, const String &aux) {
  const int y = MON_ROW_Y[row];
  tft.fillRoundRect(MON_BAR_X, y + 3, MON_BAR_W, 10, 5, T_TILE);
  const int fill = static_cast<int>((MON_BAR_W - 2) * clampPct(pct) / 100.0f);
  if (fill > 0) tft.fillRoundRect(MON_BAR_X + 1, y + 4, fill, 8, 4, pct >= 80.0f ? T_ACCENT : T_ACCENT_2);
  tft.fillRect(MON_BAR_X + MON_BAR_W + 4, y - 2, UI_SCREEN_W - MON_BAR_X - MON_BAR_W - 4, 20, T_SURFACE);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_INK, T_SURFACE);
  uiText(fmtValue(pct) + "%", 226, y + 8, 2);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiText(aux, UI_SCREEN_W - 8, y + 8, 2);
}

void drawMonitorBox(int index, const String &value, const String &detail) {
  const int x = 10 + index * 153;
  tft.fillRect(x + 6, MON_BOX_Y + 22, 135, 46, T_TILE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_TILE);
  uiText(value, x + 8, MON_BOX_Y + 32, 2);
  tft.setTextColor(T_MUTED, T_TILE);
  uiText(detail, x + 8, MON_BOX_Y + 54, 2);
}

void drawMonitorV2Dynamic() {
  drawMonitorRow(0, stats.cpu, tempText(isnan(stats.cpuTemp) ? stats.systemTemp : stats.cpuTemp));
  drawMonitorRow(1, stats.ram, memoryText(stats.ramUsedGB, stats.ramTotalGB));
  drawMonitorRow(2, stats.gpu, tempText(stats.gpuTemp));
  drawMonitorRow(3, stats.vram, memoryText(stats.vramUsedGB, stats.vramTotalGB));
  drawMonitorBox(0, "DN " + fmtValue(stats.netDownMbps, 1) + " Mb/s",
                 "UP " + fmtValue(stats.netUpMbps, 1) + " Mb/s");
  drawMonitorBox(1, fmtValue(stats.diskUsed) + "% used",
                 "R " + fmtValue(stats.diskReadMBs, 1) + " W " + fmtValue(stats.diskWriteMBs, 1) + " MB/s");
}

// ---------- Controls ----------

uint8_t controlId(int slot) { return controlsPage * MACROS_PER_PAGE + slot + 1; }

constexpr int CONTROL_ICON = 50;

// Top-left corner of the icon box inside a Controls cell.
void controlIconOrigin(int slot, int &ix, int &iy) {
  const CellRect c = gridCell(slot, 3, 2, UI_HEADER_H);
  ix = c.x + (c.w - CONTROL_ICON) / 2;
  iy = c.y + 8;
}

// Per-macro gradient: the bridge's "color", else a default per page.
const MacroGradient &controlGradient(uint8_t id) {
  static const uint8_t PAGE_DEFAULT[MACRO_PAGES] = {1, 3, 6};  // blue, purple, gray
  const int8_t color = macroColor[id - 1];
  return MACRO_GRADIENTS[color >= 0 ? color : PAGE_DEFAULT[(id - 1) / MACROS_PER_PAGE]];
}

void drawControlButton(int slot, bool flash = false) {
  const CellRect c = gridCell(slot, 3, 2, UI_HEADER_H);
  const uint8_t id = controlId(slot);
  tft.fillRect(c.x, c.y, c.w, c.h, T_BG);
  if (macroLabels[id - 1].length() == 0) return;

  int ix, iy;
  controlIconOrigin(slot, ix, iy);
  const MacroGradient &g = controlGradient(id);
  const uint16_t top = rgb565(g.from);
  const uint16_t bottom = rgb565(g.to);
  fillGradientRoundRect(tft, ix, iy, CONTROL_ICON, CONTROL_ICON, 14, top, bottom);
  if (flash) tft.drawRoundRect(ix - 2, iy - 2, CONTROL_ICON + 4, CONTROL_ICON + 4, 15, T_TEXT);
  drawIcon(tft, macroIcon[id - 1], ix + 12, iy + 12, 26, 0xFFFF, blend565(top, bottom, 128));

  String line1;
  String line2;
  splitMacroLabel(macroLabels[id - 1], line1, line2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  if (line2.length() > 0) {
    uiText(line1, c.x + c.w / 2, iy + CONTROL_ICON + 11, 2);
    uiText(line2, c.x + c.w / 2, iy + CONTROL_ICON + 26, 2);
  } else {
    uiText(line1, c.x + c.w / 2, iy + CONTROL_ICON + 14, 2);
  }
  if (macroHold[id - 1]) {
    tft.fillRoundRect(ix + CONTROL_ICON - 24, iy - 7, 38, 16, 8, T_BG);
    tft.fillRoundRect(ix + CONTROL_ICON - 22, iy - 5, 34, 12, 6, T_ACCENT);
    tft.setTextColor(T_ON_ACCENT, T_ACCENT);
    uiTextIn(UiFont::Tiny, "HOLD", ix + CONTROL_ICON - 5, iy + 1);
  }
}

void drawControlsV2() {
  drawHeaderV2(nullptr, true);
  drawIcon(tft, Icon::ChevronLeft, PREV_BTN_X + 10, 8, 16, T_TEXT, T_BG);
  drawIcon(tft, Icon::ChevronRight, NEXT_BTN_X + 10, 8, 16, T_TEXT, T_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  uiText(CONTROLS_TITLES[controlsPage], (PREV_BTN_X + ARROW_BTN_W + NEXT_BTN_X) / 2, UI_HEADER_H / 2, 2);
  for (int page = 0; page < MACRO_PAGES; ++page) {
    tft.fillCircle(NEXT_BTN_X + ARROW_BTN_W + 8 + page * 10, UI_HEADER_H / 2, 3,
                   page == controlsPage ? T_TEXT : T_DIVIDER);
  }
  for (int slot = 0; slot < MACROS_PER_PAGE; ++slot) drawControlButton(slot);
}

// Finger must stay on the button for HOLD_CONFIRM_MS while a bar fills inside the icon.
bool holdToConfirmV2(int slot) {
  int ix, iy;
  controlIconOrigin(slot, ix, iy);
  const int barX = ix + 7;
  const int barY = iy + CONTROL_ICON - 8;
  const int barW = CONTROL_ICON - 14;
  const MacroGradient &g = controlGradient(controlId(slot));
  tft.fillRoundRect(barX, barY, barW, 3, 1, blend565(rgb565(g.to), T_BG, 140));
  const uint32_t start = millis();
  int misses = 0;
  while (millis() - start < HOLD_CONFIRM_MS) {
    misses = touchHeld() ? 0 : misses + 1;
    if (misses >= 3) {
      drawControlButton(slot);
      return false;
    }
    tft.fillRoundRect(barX, barY, barW * (millis() - start) / HOLD_CONFIRM_MS, 3, 1, 0xFFFF);
    delay(20);
  }
  drawControlButton(slot);
  return true;
}

void touchControlsV2(uint16_t x, uint16_t y) {
  if (y < UI_HEADER_H) {
    if (x >= PREV_BTN_X && x < PREV_BTN_X + ARROW_BTN_W) {
      controlsPage = (controlsPage + MACRO_PAGES - 1) % MACRO_PAGES;
      fullRedrawRequested = true;
    } else if (x >= NEXT_BTN_X && x < NEXT_BTN_X + ARROW_BTN_W) {
      controlsPage = (controlsPage + 1) % MACRO_PAGES;
      fullRedrawRequested = true;
    }
    return;
  }
  const int slot = gridIndexAt(x, y, 3, 2, UI_HEADER_H);
  if (slot < 0) return;
  const uint8_t id = controlId(slot);
  if (macroLabels[id - 1].length() == 0) return;
  if (macroHold[id - 1]) {
    const bool confirmed = holdToConfirmV2(slot);
    waitForRelease();
    lastTouchMs = millis();
    if (!confirmed) return;
  }
  ledFlashUntil = millis() + 150;
  drawControlButton(slot, true);
  delay(60);
  drawControlButton(slot);
  sendMacro(id);
}

// ---------- Prices ----------

String fmtPrice(long value) {
  String text = fmtThousands(value);
  text.replace(".", ",");
  return text;
}

// ---------- Info: tab strip ----------

enum InfoTab : uint8_t { TAB_GOLD, TAB_COIN, TAB_STOCK, TAB_WEATHER, TAB_CALENDAR, TAB_HEALTH, INFO_TABS };
uint8_t infoTab = TAB_GOLD;

struct InfoTabSpec {
  const char *label;
  Icon icon;
  uint32_t line;  // underline color of the active tab
};
constexpr InfoTabSpec INFO_TAB_SPECS[INFO_TABS] = {
    {"Gold", Icon::Gold, 0xffd60a},   {"Coin", Icon::Coin, 0xff9f0a},          {"Stock", Icon::Trend, 0x30d158},
    {"Weather", Icon::Cloud, 0x64d2ff}, {"Date", Icon::Calendar, 0xff375f},
    {"Health", Icon::Activity, 0x30d158},
};
// Tab strip box under the header; content area below it.
constexpr int TAB_X = 6, TAB_Y = UI_HEADER_H + 1, TAB_W = 308, TAB_H = 30, TAB_GAP = 3;
constexpr int INFO_Y = TAB_Y + TAB_H;  // 63

void drawTabStrip(const InfoTabSpec *specs, int count, int activeTab) {
  tft.fillRect(0, TAB_Y, UI_SCREEN_W, TAB_H, T_BG);
  for (int i = 0; i < count; ++i) {
    const CellRect c = gridCellIn(i, count, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
    const bool active = i == activeTab;
    const uint16_t bg = active ? T_CARD : T_BG;
    const uint16_t fg = active ? T_TEXT : T_MUTED;
    tft.fillRoundRect(c.x, c.y, c.w, c.h + 4, 6, bg);  // +4: square bottom joins the content
    if (active) tft.fillRect(c.x, c.y + c.h - 2, c.w, 2, rgb565(specs[i].line));
    drawIcon(tft, specs[i].icon, c.x + (c.w - 12) / 2, c.y + 2, 12, fg, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    uiText(specs[i].label, c.x + c.w / 2, c.y + 21, 1);
  }
}

void drawInfoTabs() { drawTabStrip(INFO_TAB_SPECS, INFO_TABS, infoTab); }

void drawInfoEmpty(const char *message) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText(message, UI_SCREEN_W / 2, (INFO_Y + UI_SCREEN_H) / 2, 2);
}

// ---------- Info: Gold ----------

void drawGoldTab() {
  constexpr int colBuy = 180;
  constexpr int colSell = 250;
  constexpr int colChange = 312;
  constexpr int headY = INFO_Y;
  constexpr int rowY0 = headY + 18;
  constexpr int rowH = 21;

  tft.setTextColor(T_MUTED, T_CARD);
  tft.setTextDatum(ML_DATUM);
  uiText("x1000 VND / chi", 10, headY + 9, 1);
  tft.setTextDatum(MR_DATUM);
  uiText("BUY", colBuy, headY + 9, 1);
  uiText("SELL", colSell, headY + 9, 1);
  uiText("+/-", colChange, headY + 9, 1);

  if (!info.hasGold) {
    drawInfoEmpty("No data yet");
    return;
  }

  for (int i = 0; i < info.goldCount; ++i) {
    const int y = rowY0 + i * rowH;
    const uint16_t bg = (i % 2 == 0) ? T_CARD_ALT : T_CARD;
    const int mid = y + rowH / 2;
    tft.fillRect(0, y, UI_SCREEN_W, rowH, bg);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_TEXT, bg);
    uiText(info.goldName[i], 10, mid, 2);
    tft.setTextDatum(MR_DATUM);
    uiText(fmtPrice(info.goldBuy[i]), colBuy, mid, 2);
    tft.setTextColor(T_GOLD, bg);
    uiText(fmtPrice(info.goldSell[i]), colSell, mid, 2);
    const long change = info.goldChange[i];
    tft.setTextColor(change > 0 ? T_UP : (change < 0 ? T_DOWN : T_MUTED), bg);
    uiText(change > 0 ? "+" + fmtPrice(change) : fmtPrice(change), colChange, mid, 2);
  }

  constexpr int footY = 226;
  tft.setTextDatum(ML_DATUM);
  if (!isnan(info.xau)) {
    const String world = "World $" + String(info.xau, 1) + "/oz ";
    tft.setTextColor(T_MUTED, T_CARD);
    uiText(world, 10, footY, 2);
    tft.setTextColor(info.xauChange > 0 ? T_UP : (info.xauChange < 0 ? T_DOWN : T_MUTED), T_CARD);
    uiText((info.xauChange > 0 ? "+" : "") + String(info.xauChange, 1), 10 + uiTextWidth(world, 2), footY, 2);
  }
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText("Updated " + info.goldTime, colChange, footY, 2);
}

// ---------- Info: Calendar ----------

const char *const WEEKDAY_LONG[7] = {"MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY"};
const char *const MONTH_SHORT[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *const MONTH_LONG[12] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
                                    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};

int daysInMonth(int month, int year) {
  static const int DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return month == 2 && leapYear ? 29 : DAYS[month - 1];
}

void drawCalendarTab() {
  int day, month, year;
  if (!deckDate(day, month, year)) {
    drawInfoEmpty("Waiting for the PC date");
    return;
  }
  const LunarDate today = solarToLunar(day, month, year);

  // Left card: today.
  constexpr int cardX = 8, cardY = INFO_Y + 5, cardW = 92, cardH = UI_SCREEN_H - INFO_Y - 10;
  tft.fillRoundRect(cardX, cardY, cardW, cardH, 10, T_CARD_ALT);
  const int cx = cardX + cardW / 2;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(rgb565(0xff375f), T_CARD_ALT);
  uiText(WEEKDAY_LONG[dayOfWeek(day, month, year)], cx, cardY + 14, 1);
  tft.setTextColor(T_TEXT, T_CARD_ALT);
  char text[24];
  snprintf(text, sizeof(text), "%02d", day);
  uiText(text, cx, cardY + 40, 4);
  tft.setTextColor(T_MUTED, T_CARD_ALT);
  snprintf(text, sizeof(text), "%s %d", MONTH_SHORT[month - 1], year);
  uiText(text, cx, cardY + 64, 2);
  tft.drawFastHLine(cardX + 16, cardY + 78, cardW - 32, T_DIVIDER);
  uiText(today.leap ? "LUNAR (LEAP)" : "LUNAR", cx, cardY + 88, 1);
  tft.setTextColor(T_ACCENT, T_CARD_ALT);
  snprintf(text, sizeof(text), "%d/%d", today.day, today.month);
  uiText(text, cx, cardY + 106, 4);
  tft.setTextColor(T_MUTED, T_CARD_ALT);
  uiText(canChiYear(today.year), cx, cardY + 124, 2);

  // Next holiday: name on up to two lines, then how far away.
  const VnHoliday *next = nullptr;
  const int toHoliday = daysToHoliday(day, month, year, &next);
  if (toHoliday >= 0) {
    const int maxW = cardW - 8;
    const String name = next->vi;
    String first = name, second;
    if (viTextWidth(name) > maxW) {
      int cut = -1;
      for (int i = name.indexOf(' '); i > 0; i = name.indexOf(' ', i + 1)) {
        if (viTextWidth(name.substring(0, i)) > maxW) break;
        cut = i;
      }
      if (cut > 0) {
        first = name.substring(0, cut);
        second = fitViText(name.substring(cut + 1), maxW);
      } else {
        first = fitViText(name, maxW);
      }
    }
    const uint16_t color = toHoliday == 0 ? T_GOLD : T_TEXT;
    drawViText(tft, cx - viTextWidth(first) / 2, cardY + 131, first, color, T_CARD_ALT);
    if (second.length()) drawViText(tft, cx - viTextWidth(second) / 2, cardY + 145, second, color, T_CARD_ALT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(toHoliday == 0 ? T_GOLD : T_MUTED, T_CARD_ALT);
    const String when = toHoliday == 0 ? String("today") : (toHoliday == 1 ? String("tomorrow") : String("in ") + toHoliday + " days");
    uiTextIn(UiFont::Tiny, when, cx, cardY + (second.length() ? 162 : 152));
  }

  // Right: Monday-first month grid with the lunar day under each solar day.
  constexpr int gridX = 108, gridW = 204, headY = INFO_Y + 7, weekY = INFO_Y + 20, rowsY = INFO_Y + 30;
  constexpr int rowH = (UI_SCREEN_H - 2 - rowsY) / 6;
  const int colW = gridW / 7;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  snprintf(text, sizeof(text), "%s %d", MONTH_LONG[month - 1], year);
  uiTextIn(UiFont::Tiny, text, gridX + 2, headY);
  const char *const WEEK_INITIALS = "MTWTFSS";
  tft.setTextDatum(MC_DATUM);
  for (int c = 0; c < 7; ++c) {
    tft.setTextColor(c == 6 ? T_DOWN : T_MUTED, T_CARD);
    uiTextIn(UiFont::Tiny, String(WEEK_INITIALS[c]), gridX + c * colW + colW / 2, weekY);
  }
  const int lead = dayOfWeek(1, month, year);
  const int count = daysInMonth(month, year);
  for (int d = 1; d <= count; ++d) {
    const int cell = lead + d - 1;
    const int x = gridX + (cell % 7) * colW;
    const int y = rowsY + (cell / 7) * rowH;
    const LunarDate l = solarToLunar(d, month, year);
    const bool isToday = d == day;
    const uint16_t bg = isToday ? T_LINK : T_CARD;
    if (isToday) tft.fillRoundRect(x + 1, y, colW - 2, rowH - 1, 5, T_LINK);
    const bool holiday = vnHoliday(d, month, year) != nullptr;
    tft.setTextColor(isToday ? 0xFFFF : (holiday ? T_GOLD : (cell % 7 == 6 ? T_DOWN : T_TEXT)), bg);
    uiText(String(d), x + colW / 2, y + 7, 2);
    const bool special = l.day == 1 || l.day == 15;
    snprintf(text, sizeof(text), l.day == 1 ? "%d/%d" : "%d", l.day, l.month);
    tft.setTextColor(isToday ? 0xFFFF : (special ? T_ACCENT : T_MUTED), bg);
    uiTextIn(UiFont::Tiny, text, x + colW / 2, y + rowH - 6);
  }
}

// ---------- Info: Coin, Stock, Weather ----------

// "dd/mm" of the PC date, empty before the bridge sends one.
String todayText() {
  int d, m, y;
  if (!deckDate(d, m, y)) return "";
  char text[6];
  snprintf(text, sizeof(text), "%02d/%02d", d, m);
  return text;
}

void drawInfoFooter(const String &left, const char *date, const char *time) {
  const String updated = updatedText(date, time, todayText());
  constexpr int footY = UI_INFO_FOOTER_Y;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText(left, 10, footY, 1);
  tft.setTextDatum(MR_DATUM);
  uiText("Updated " + updated, UI_SCREEN_W - 10, footY, 1);
}

uint16_t changeColorFor(float change) { return change > 0 ? T_UP : (change < 0 ? T_DOWN : T_MUTED); }

uint16_t coinColor(const char *symbol) {
  if (strcmp(symbol, "BTC") == 0) return rgb565(0xff9f0a);
  if (strcmp(symbol, "ETH") == 0) return rgb565(0x8e9bff);
  if (strcmp(symbol, "BNB") == 0) return rgb565(0xffd60a);
  if (strcmp(symbol, "SOL") == 0) return rgb565(0xbf5af2);
  return T_MUTED;
}

void drawCoinTab() {
  if (!coins.has) {
    drawInfoEmpty("No data yet");
    return;
  }
  constexpr int rowH = 29;
  for (int i = 0; i < coins.count; ++i) {
    const CoinRow &row = coins.rows[i];
    const int mid = INFO_Y + 6 + i * rowH + rowH / 2;
    tft.fillCircle(22, mid, 11, coinColor(row.symbol));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_ON_ACCENT, coinColor(row.symbol));
    uiText(String(row.symbol[0]), 22, mid + 1, 2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_TEXT, T_CARD);
    uiText(row.symbol, 40, mid, 2);
    tft.setTextColor(T_MUTED, T_CARD);
    uiText(row.name, 40 + uiTextWidth(row.symbol, 2) + 6, mid + 1, 1);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(T_TEXT, T_CARD);
    uiText(coinPriceText(row.price), 248, mid, 2);
    tft.setTextColor(changeColorFor(row.change), T_CARD);
    uiText(signedPercent(row.change), UI_SCREEN_W - 10, mid, 2);
  }
  drawInfoFooter("USD - 24h change", coins.date, coins.time);
}

void drawSparkline(int x, int y, int w, int h) {
  if (stocks.sparkCount < 2) return;
  // Reference = the index's open level (falls back to the first sample).
  const float ref = stocks.open > 0 ? stocks.open : stocks.spark[0];
  float lo = ref, hi = ref;
  for (int i = 0; i < stocks.sparkCount; ++i) {
    lo = min(lo, stocks.spark[i]);
    hi = max(hi, stocks.spark[i]);
  }
  const float span = hi - lo > 0.01f ? hi - lo : 1.0f;
  const uint16_t color = stocks.spark[stocks.sparkCount - 1] >= ref ? T_UP : T_DOWN;
  auto py = [&](float v) { return y + h - static_cast<int>((v - lo) / span * h); };
  for (int px = x; px < x + w; px += 6) tft.drawPixel(px, py(ref), T_DIVIDER);  // open level
  for (int i = 1; i < stocks.sparkCount; ++i) {
    const int x0 = x + (i - 1) * w / (stocks.sparkCount - 1);
    const int x1 = x + i * w / (stocks.sparkCount - 1);
    tft.drawWideLine(x0, py(stocks.spark[i - 1]), x1, py(stocks.spark[i]), 2, color, T_CARD);
  }
}

void drawQuoteRow(int y, const char *name, const char *kind, const String &value, float change) {
  const int mid = y + 11;
  tft.drawFastHLine(10, y, UI_SCREEN_W - 20, T_DIVIDER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiText(name, 10, mid, 2);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText(kind, 104, mid + 1, 1);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiText(value, 248, mid, 2);
  tft.setTextColor(changeColorFor(change), T_CARD);
  uiText(signedPercent(change), UI_SCREEN_W - 10, mid, 2);
}

void drawStockTab() {
  if (!stocks.has || stocks.indexCount == 0) {
    drawInfoEmpty("No data yet");
    return;
  }
  const QuoteRow &main = stocks.index[0];
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText(main.name, 10, INFO_Y + 6, 1);
  tft.setTextColor(T_TEXT, T_CARD);
  uiText(fmtGrouped(main.value, 2), 10, INFO_Y + 17, 4);
  tft.setTextColor(changeColorFor(main.change), T_CARD);
  uiText(signedPercent(main.change), 10, INFO_Y + 44, 2);
  drawSparkline(176, INFO_Y + 10, 134, 40);

  int y = INFO_Y + 64;
  for (int i = 1; i < stocks.indexCount; ++i, y += 23) {
    drawQuoteRow(y, stocks.index[i].name, "Index", fmtGrouped(stocks.index[i].value, 2), stocks.index[i].change);
  }
  for (int i = 0; i < stocks.itemCount && y < 214; ++i, y += 23) {
    drawQuoteRow(y, stocks.items[i].name, "VND", stockPriceText(stocks.items[i].value), stocks.items[i].change);
  }
  drawInfoFooter("VPS - HOSE / HNX", stocks.date, stocks.time);
}

// Weather icon ids from the bridge: 0 sun, 1 partly, 2 cloud, 3 rain, 4 storm, 5 fog.
void drawWeatherIcon(uint8_t icon, int x, int y, int size, uint16_t bg) {
  switch (icon) {
    case 0: drawIcon(tft, Icon::Sun, x, y, size, T_GOLD, bg); break;
    case 1:
      drawIcon(tft, Icon::Sun, x + size / 3, y - size / 6, size * 2 / 3, T_GOLD, bg);
      drawIcon(tft, Icon::Cloud, x, y + size / 8, size, T_TEXT, bg);
      break;
    case 3: drawIcon(tft, Icon::Rain, x, y, size, rgb565(0x64d2ff), bg); break;
    case 4: drawIcon(tft, Icon::Storm, x, y, size, T_GOLD, bg); break;
    case 5: drawIcon(tft, Icon::Fog, x, y, size, T_MUTED, bg); break;
    default: drawIcon(tft, Icon::Cloud, x, y, size, T_TEXT, bg); break;
  }
}

void drawWeatherTab() {
  if (!weather.has) {
    drawInfoEmpty("No data yet");
    return;
  }
  drawWeatherIcon(weather.icon, 12, INFO_Y + 10, 40, T_CARD);
  const String temp = isnan(weather.temp) ? String("--") : String(static_cast<int>(lroundf(weather.temp)));
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiText(temp, 62, INFO_Y + 12, 4);
  const int tempW = uiTextWidth(temp, 4);
  tft.drawCircle(66 + tempW, INFO_Y + 15, 3, T_TEXT);  // degree sign
  uiText(weather.desc, 128, INFO_Y + 8, 2);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText(String(weather.city) + " - feels " + String(weather.feels) + "C", 128, INFO_Y + 26, 1);
  uiText("Humidity " + String(weather.humidity) + "%  Wind " + String(weather.wind) + " km/h", 128, INFO_Y + 40, 1);

  const CellRect box = weatherDaysBox();
  for (int i = 0; i < weather.dayCount; ++i) {
    const CellRect c = gridCellIn(i, 4, 1, box.x, box.y, box.w, box.h, 5);
    const ForecastDay &day = weather.days[i];
    tft.fillRoundRect(c.x, c.y, c.w, c.h, 8, T_CARD_ALT);
    const int cx = c.x + c.w / 2;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_CARD_ALT);
    uiText(day.day, cx, c.y + 12, 2);
    drawWeatherIcon(day.icon, cx - 14, c.y + 24, 28, T_CARD_ALT);
    tft.setTextColor(T_TEXT, T_CARD_ALT);
    uiText(String(day.hi) + "/" + String(day.lo), cx, c.y + 66, 2);
    if (day.rain >= 0) {
      tft.setTextColor(rgb565(0x64d2ff), T_CARD_ALT);
      uiText(String(day.rain) + "%", cx, c.y + 86, 2);
    }
  }
  drawInfoFooter("Open-Meteo", weather.date, weather.time);
}

// ---------- Info: Health (time at the PC, auto-lock) ----------

constexpr int HL_TILE_Y = INFO_Y + 5, HL_TILE_H = 34;
constexpr int HL_CHART_Y = INFO_Y + 46;  // labels; bars below
constexpr int HL_BAR_TOP = HL_CHART_Y + 12, HL_BAR_H = 46;
constexpr int HL_LOCK_Y = 184, HL_LOCK_H = 50;
constexpr int HL_SWITCH_W = 40, HL_SWITCH_H = 20;

void drawHealthTile(int i, const char *label, const String &value, uint16_t color) {
  const int w = (UI_SCREEN_W - 12 - 3 * 4) / 4;
  const int x = 6 + i * (w + 4);
  tft.fillRoundRect(x, HL_TILE_Y, w, HL_TILE_H, 6, T_CARD_ALT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, T_CARD_ALT);
  uiTextIn(UiFont::Tiny, label, x + 6, HL_TILE_Y + 4);
  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(color, T_CARD_ALT);
  uiTextIn(UiFont::BodyBold, value, x + 6, HL_TILE_Y + HL_TILE_H - 3);
}

void drawHealthLock() {
  const ActivityInfo &a = activity;
  tft.fillRoundRect(6, HL_LOCK_Y, UI_SCREEN_W - 12, HL_LOCK_H, 8, T_CARD_ALT);
  drawIcon(tft, Icon::Lock, 16, HL_LOCK_Y + 8, 16, a.lockOn ? T_TEXT : T_MUTED, T_CARD_ALT);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_CARD_ALT);
  uiTextIn(UiFont::BodyBold, "AUTO-LOCK", 40, HL_LOCK_Y + 16);
  tft.setTextColor(T_MUTED, T_CARD_ALT);
  uiTextIn(UiFont::Tiny, "when the iPhone leaves", 40 + uiTextWidthIn(UiFont::BodyBold, "AUTO-LOCK") + 8, HL_LOCK_Y + 17);
  String status = "No iPhone link on the deck";
  uint16_t color = T_MUTED;
  if (a.near == PhoneNear::Near) {
    status = "iPhone near";
    color = T_UP;
  } else if (a.near == PhoneNear::Away) {
    status = "iPhone away";
    color = T_DOWN;
  }
  if (a.hasRssi) status += String("   ") + a.rssi + " dBm";
  tft.setTextColor(color, T_CARD_ALT);
  uiTextIn(UiFont::Small, status, 16, HL_LOCK_Y + 36);
  const int sx = UI_SCREEN_W - 12 - HL_SWITCH_W - 8, sy = HL_LOCK_Y + (HL_LOCK_H - HL_SWITCH_H) / 2;
  tft.fillRoundRect(sx, sy, HL_SWITCH_W, HL_SWITCH_H, HL_SWITCH_H / 2, a.lockOn ? T_UP : T_LINE);
  tft.fillCircle(a.lockOn ? sx + HL_SWITCH_W - HL_SWITCH_H / 2 : sx + HL_SWITCH_H / 2, sy + HL_SWITCH_H / 2,
                 HL_SWITCH_H / 2 - 3, T_TEXT);
}

void drawHealthTab() {
  const ActivityInfo &a = activity;
  if (!a.has) {
    drawInfoEmpty("Waiting for the PC bridge");
    return;
  }
  drawHealthTile(0, "TODAY", activityDuration(a.todayMin), T_TEXT);
  drawHealthTile(1, "SITTING", a.sessionMin ? activityDuration(a.sessionMin) : String("break"),
                 a.sessionMin >= 50 ? T_ACCENT : T_TEXT);
  drawHealthTile(2, "BREAKS", String(a.breaks), T_UP);
  drawHealthTile(3, "LONGEST", activityDuration(a.longestMin), T_TEXT);

  // Active minutes per hour today (0-60 each).
  constexpr int slot = 8, chartX = 10;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(a.late ? T_DOWN : T_MUTED, T_CARD);
  uiTextIn(UiFont::Tiny, a.late ? "LATE - TIME TO REST" : "TODAY BY HOUR", chartX, HL_CHART_Y);
  const int hourNow = clockHour();
  const int base = HL_BAR_TOP + HL_BAR_H;
  tft.drawFastHLine(chartX, base, slot * 24, T_DIVIDER);
  for (int h = 0; h < 24; ++h) {
    const int barH = a.hours[h] ? max(2, HL_BAR_H * a.hours[h] / 60) : 0;
    if (barH) tft.fillRect(chartX + h * slot + 1, base - barH, slot - 2, barH, h == hourNow ? T_ACCENT : T_UP);
  }
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  for (int h = 0; h < 24; h += 6) uiTextIn(UiFont::Tiny, String(h), chartX + h * slot + slot / 2, base + 3);

  // Last 7 days.
  constexpr int weekX = 216, weekSlot = 13;
  tft.setTextDatum(TL_DATUM);
  uiTextIn(UiFont::Tiny, "7 DAYS", weekX, HL_CHART_Y);
  uint16_t most = 60;
  for (uint16_t v : a.week) most = max(most, v);
  tft.drawFastHLine(weekX, base, weekSlot * 7, T_DIVIDER);
  int d, m, y;
  const int todayDow = deckDate(d, m, y) ? dayOfWeek(d, m, y) : -1;
  const char *const INITIALS = "MTWTFSS";
  tft.setTextDatum(TC_DATUM);
  for (int i = 0; i < 7; ++i) {
    const int barH = a.week[i] ? max(2, static_cast<int>(HL_BAR_H * static_cast<uint32_t>(a.week[i]) / most)) : 0;
    const int x = weekX + i * weekSlot;
    if (barH) tft.fillRect(x + 2, base - barH, weekSlot - 4, barH, i == 6 ? T_ACCENT : T_ACCENT_2);
    if (todayDow >= 0) uiTextIn(UiFont::Tiny, String(INITIALS[(todayDow - (6 - i) + 14) % 7]), x + weekSlot / 2, base + 3);
  }
  drawHealthLock();
}

void touchHealthTab(uint16_t x, uint16_t y) {
  if (!activity.has || y < HL_LOCK_Y || y > HL_LOCK_Y + HL_LOCK_H) return;
  activity.lockOn = !activity.lockOn;
  sendLockSet(activity.lockOn);
  drawHealthLock();
  waitForRelease();
}

// ---------- Break reminders (full screen, back to the previous screen) ----------

enum class BreakKind : uint8_t { Stand, Eye };
BreakKind breakKind = BreakKind::Stand;
Screen breakReturnTo = Screen::Home;
uint32_t breakEyeUntil = 0;
uint32_t eyeSeqSeen = 0;
int breakDrawnSecond = -1;
constexpr uint32_t EYE_REST_MS = 20000;
constexpr int BRK_BTN_Y = 176, BRK_BTN_H = 46, BRK_BTN_W = 138;

void drawBreakCountdown() {
  const int left = max<int32_t>(0, static_cast<int32_t>(breakEyeUntil - millis()) / 1000 + 1);
  if (left == breakDrawnSecond) return;
  breakDrawnSecond = left;
  tft.fillRect(100, 160, 120, 56, T_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_GOLD, T_BG);
  uiText(String(left), UI_SCREEN_W / 2, 188, 6);
}

void drawBreakV2() {
  tft.fillScreen(T_BG);
  tft.setTextDatum(MC_DATUM);
  if (breakKind == BreakKind::Eye) {
    drawIcon(tft, Icon::Sun, UI_SCREEN_W / 2 - 20, 22, 40, T_GOLD, T_BG);
    tft.setTextColor(T_TEXT, T_BG);
    uiText("REST YOUR EYES", UI_SCREEN_W / 2, 96, 4);
    tft.setTextColor(T_MUTED, T_BG);
    uiTextIn(UiFont::Small, "Look at something 6 m away", UI_SCREEN_W / 2, 126);
    uiTextIn(UiFont::Tiny, "tap to close", UI_SCREEN_W / 2, 228);
    breakDrawnSecond = -1;
    drawBreakCountdown();
    return;
  }
  drawIcon(tft, Icon::Activity, UI_SCREEN_W / 2 - 20, 18, 40, T_UP, T_BG);
  tft.setTextColor(T_TEXT, T_BG);
  uiText("STAND UP", UI_SCREEN_W / 2, 92, 4);
  tft.setTextColor(T_MUTED, T_BG);
  uiTextIn(UiFont::Small, String("At the PC for ") + activityDuration(activity.sessionMin) + " without a break",
           UI_SCREEN_W / 2, 124);
  uiTextIn(UiFont::Small, activity.late ? "It's late - wrap up soon" : "Walk, stretch, refill your water",
           UI_SCREEN_W / 2, 146);
  tft.fillRoundRect(16, BRK_BTN_Y, BRK_BTN_W, BRK_BTN_H, 10, T_CARD);
  tft.fillRoundRect(UI_SCREEN_W - 16 - BRK_BTN_W, BRK_BTN_Y, BRK_BTN_W, BRK_BTN_H, 10, T_UP);
  tft.setTextColor(T_TEXT, T_CARD);
  uiTextIn(UiFont::BodyBold, "SNOOZE 10M", 16 + BRK_BTN_W / 2, BRK_BTN_Y + BRK_BTN_H / 2);
  tft.setTextColor(T_BG, T_UP);
  uiTextIn(UiFont::BodyBold, "DONE", UI_SCREEN_W - 16 - BRK_BTN_W / 2, BRK_BTN_Y + BRK_BTN_H / 2);
}

void openBreak(BreakKind kind) {
  breakKind = kind;
  if (screen != Screen::Break) breakReturnTo = screen;
  if (kind == BreakKind::Eye) breakEyeUntil = millis() + EYE_REST_MS;
  bannerActive = false;
  noteActivity();
  uiShow(Screen::Break);
}

void closeBreak() { uiShow(breakReturnTo); }

void touchBreakV2(uint16_t x, uint16_t y) {
  if (breakKind == BreakKind::Eye) {
    waitForRelease();
    closeBreak();
    return;
  }
  if (y < BRK_BTN_Y || y > BRK_BTN_Y + BRK_BTN_H) return;
  if (x < 16 + BRK_BTN_W) {
    sendBreakReply("snooze");
  } else if (x > UI_SCREEN_W - 16 - BRK_BTN_W) {
    sendBreakReply("ok");
  } else {
    return;
  }
  activity.standDue = false;  // the bridge's next frame confirms it
  waitForRelease();
  closeBreak();
}

void breakTick() {
  if (screen != Screen::Break || breakKind != BreakKind::Eye) return;
  if (static_cast<int32_t>(breakEyeUntil - millis()) <= 0) {
    closeBreak();
    return;
  }
  drawBreakCountdown();
}

bool activityLockOnlyChanged(const ActivityInfo &a, const ActivityInfo &b) {
  if (a.todayMin != b.todayMin || a.sessionMin != b.sessionMin || a.breaks != b.breaks ||
      a.longestMin != b.longestMin || a.late != b.late)
    return false;
  return memcmp(a.hours, b.hours, sizeof(a.hours)) == 0 && memcmp(a.week, b.week, sizeof(a.week)) == 0;
}

void uiActivityFrame(const ActivityInfo &previous) {
  if (!previous.has) eyeSeqSeen = activity.eyeSeq;  // first frame after boot: no stale eye reminder
  // Never cover a permission prompt or a running game.
  const bool canOpen = !claude.askId && screen != Screen::Aram && screen != Screen::Break;
  if (activity.standDue && !previous.standDue && canOpen) {
    openBreak(BreakKind::Stand);
  } else if (activity.eyeSeq != eyeSeqSeen) {
    eyeSeqSeen = activity.eyeSeq;
    if (canOpen && !activity.standDue) openBreak(BreakKind::Eye);
  }
  if (screen == Screen::Break && breakKind == BreakKind::Stand && !activity.standDue) {
    closeBreak();  // a real break happened
    return;
  }
  if (screen == Screen::Info && infoTab == TAB_HEALTH) {
    if (activityLockOnlyChanged(previous, activity) && previous.has) drawHealthLock();
    else fullRedrawRequested = true;
  }
}

// ---------- Info: screen ----------

void drawInfoV2() {
  drawHeaderV2("INFO", true);
  drawInfoTabs();
  tft.fillRect(0, INFO_Y, UI_SCREEN_W, UI_SCREEN_H - INFO_Y, T_CARD);
  switch (infoTab) {
    case TAB_GOLD: drawGoldTab(); break;
    case TAB_COIN: drawCoinTab(); break;
    case TAB_STOCK: drawStockTab(); break;
    case TAB_WEATHER: drawWeatherTab(); break;
    case TAB_CALENDAR: drawCalendarTab(); break;
    case TAB_HEALTH: drawHealthTab(); break;
    default: break;
  }
}

void touchInfoV2(uint16_t x, uint16_t y) {
  const int tab = gridIndexIn(x, y, INFO_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
  if (tab >= 0 && tab != infoTab) {
    infoTab = tab;
    fullRedrawRequested = true;
    return;
  }
  if (infoTab == TAB_HEALTH) touchHealthTab(x, y);
}

// ---------- Notifications ----------

constexpr int NOTIFY_CHIP_Y = 38;
constexpr int NOTIFY_LIST_Y = 66;
constexpr int NOTIFY_ROW_H = 42;
constexpr int NOTIFY_ROWS = 4;
constexpr int NOTIFY_TEXT_X = 44;
constexpr int NOTIFY_TEXT_W = 236;

String agoText(uint32_t receivedMs) {
  const uint32_t minutes = (millis() - receivedMs) / 60000UL;
  if (minutes < 1) return "now";
  if (minutes < 60) return String(minutes) + "m";
  return String(minutes / 60) + "h";
}

String notifyTitle(const Notification &n) {
  return n.from.length() ? n.app + " - " + n.from : n.app;
}

void drawLinkChip(int x, const char *label, Icon icon, bool up) {
  tft.fillRoundRect(x, NOTIFY_CHIP_Y, 149, 22, 6, T_TILE);
  drawIcon(tft, icon, x + 8, NOTIFY_CHIP_Y + 4, 14, T_INK, T_TILE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_TILE);
  uiText(label, x + 28, NOTIFY_CHIP_Y + 11, 2);
  tft.fillCircle(x + 137, NOTIFY_CHIP_Y + 11, 4, up ? T_UP : T_DOWN);
}

void drawNotifyRow(int index) {
  const Notification &n = notifyStore.at(index);
  const int y = NOTIFY_LIST_Y + index * NOTIFY_ROW_H;
  const uint16_t bg = n.unread ? T_TILE : T_SURFACE;
  tft.fillRoundRect(8, y, UI_SCREEN_W - 16, NOTIFY_ROW_H - 2, 8, bg);
  tft.drawRoundRect(14, y + 9, 22, 22, 5, T_BORDER);
  drawIcon(tft, n.source == NotifySource::Phone ? Icon::Phone : Icon::Monitor, 18, y + 13, 14, T_INK, bg);
  drawViText(tft, NOTIFY_TEXT_X, y + 3, fitViText(notifyTitle(n), NOTIFY_TEXT_W, true), T_INK, bg, true);
  const String body = settings.hideBody ? String("Content hidden") : n.body;
  drawViText(tft, NOTIFY_TEXT_X, y + 21, fitViText(body, NOTIFY_TEXT_W), T_MUTED, bg);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, bg);
  uiText(agoText(n.receivedMs), UI_SCREEN_W - 14, y + 20, 1);
}

void drawNotifyHideToggle() {
  const uint16_t bg = settings.hideBody ? T_ACCENT : T_CARD;
  tft.fillRect(STATUS_X, 0, UI_SCREEN_W - STATUS_X, UI_HEADER_H - 1, T_BG);
  tft.fillRoundRect(STATUS_X + 2, 4, UI_SCREEN_W - STATUS_X - 8, 24, 6, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(settings.hideBody ? T_ON_ACCENT : T_TEXT, bg);
  uiText(settings.hideBody ? "HIDE: ON" : "HIDE: OFF", (STATUS_X + UI_SCREEN_W - 6) / 2, 16, 2);
}

void drawNotifyV2() {
  drawHeaderV2("NOTIFICATIONS", true);
  drawNotifyHideToggle();
  drawLinkChip(8, "PHONE", Icon::Phone, phoneLinkUp);
  drawLinkChip(163, "PC", Icon::Wifi, pcLinkUp);
  if (notifyStore.size() == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_SURFACE);
    uiText("No notifications yet", UI_SCREEN_W / 2, 150, 2);
  }
  for (size_t i = 0; i < notifyStore.size() && i < NOTIFY_ROWS; ++i) drawNotifyRow(i);
  // Seen now: rows keep their unread look until the next redraw.
  notifyStore.markAllRead();
  drawnNotifyVersion = notifyStore.version();
}

void touchNotifyV2(uint16_t x, uint16_t y) {
  if (y < UI_HEADER_H && x >= STATUS_X) {
    settings.hideBody = !settings.hideBody;
    saveSettings();
    fullRedrawRequested = true;
  }
}

// Banner over the top of any other screen for BANNER_MS; tap it to open the list.
void drawBannerV2() {
  const Notification &n = notifyStore.at(0);
  tft.fillRect(0, 0, UI_SCREEN_W, BANNER_H, T_ALERT_BG);
  tft.drawRect(0, 0, UI_SCREEN_W, BANNER_H, T_ACCENT);
  tft.drawRect(1, 1, UI_SCREEN_W - 2, BANNER_H - 2, T_ACCENT);
  drawIcon(tft, n.source == NotifySource::Phone ? Icon::Phone : Icon::Monitor, 8, 12, 16, T_INK, T_ALERT_BG);
  drawViText(tft, 30, 3, fitViText(notifyTitle(n), 280, true), T_INK, T_ALERT_BG, true);
  const String body = settings.hideBody ? String("Content hidden") : n.body;
  drawViText(tft, 30, 20, fitViText(body, 280), T_MUTED, T_ALERT_BG);
}

// Quiet hours: listed silently, the Home tile's badge still updates.
void uiNotificationQuiet() {
  if (screen == Screen::Notify) fullRedrawRequested = true;
}

void uiNotificationArrived() {
  if (screen == Screen::Notify) {
    fullRedrawRequested = true;
    return;
  }
  bannerActive = true;
  bannerUntil = millis() + BANNER_MS;
  drawBannerV2();
}

// ---------- Placeholders (phase 2) ----------

void drawPlaceholderV2(const char *title) {
  drawHeaderV2(title, true);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiText("Coming in phase 2", UI_SCREEN_W / 2, 136, 2);
}

// ---------- Settings ----------

enum SettingsTab : uint8_t { SET_DISPLAY, SET_NOTIFY, SET_CONNECT, SET_DEVICE, SET_TABS };
uint8_t settingsTab = SET_DISPLAY;
constexpr InfoTabSpec SET_TAB_SPECS[SET_TABS] = {
    {"Display", Icon::Sun, 0xffd60a},
    {"Notify", Icon::Bell, 0xff375f},
    {"Connect", Icon::Wifi, 0x0a84ff},
    {"Device", Icon::Chip, 0x8e8e93},
};
// Five 30 px rows under the tab strip; controls sit in the right part of a row.
constexpr int SET_ROWS = 5;
constexpr int SET_ROW_X = 8, SET_ROW_W = UI_SCREEN_W - 16, SET_ROW_H = 30, SET_ROW_STEP = 34;
constexpr int SET_ROW_Y0 = INFO_Y + 4;  // 67
constexpr int SET_HALF_W = (SET_ROW_W - 6) / 2;
constexpr int SET_RIGHT_X = SET_ROW_X + SET_HALF_W + 6;
constexpr int SET_CTRL_X = 120;
constexpr int SET_CTRL_W = SET_ROW_X + SET_ROW_W - 6 - SET_CTRL_X;
// Quiet hours row: start box, end box, then the switch.
constexpr int QUIET_START_X = 84, QUIET_END_X = 156, QUIET_BOX_W = 56;
const char *const SLEEP_LABELS[SLEEP_OPTIONS] = {"Never", "1 min", "5 min", "15 min"};
String settingsCache[SET_ROWS];

int setRowY(int row) { return SET_ROW_Y0 + row * SET_ROW_STEP; }

void drawSetRow(int row, const char *label, int x = SET_ROW_X, int w = SET_ROW_W) {
  const int y = setRowY(row);
  tft.fillRoundRect(x, y, w, SET_ROW_H, 7, T_CARD_ALT);
  if (!label) return;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_CARD_ALT);
  uiText(label, x + 10, y + SET_ROW_H / 2, 2);
}

// Pill switch whose right edge is `right`.
void drawSwitch(int right, int row, bool on) {
  constexpr int w = 40, h = 20;
  const int x = right - w - 6;
  const int top = setRowY(row) + (SET_ROW_H - h) / 2;
  tft.fillRoundRect(x, top, w, h, h / 2, on ? T_UP : T_LINE);
  tft.fillCircle(on ? x + w - h / 2 : x + h / 2, top + h / 2, h / 2 - 3, T_TEXT);
}

void drawSwitchRow(int row, const char *label, bool on, int x = SET_ROW_X, int w = SET_ROW_W) {
  drawSetRow(row, label, x, w);
  drawSwitch(x + w, row, on);
}

// Segmented control in the controls column; `fill` lights every segment up to `active`.
void drawSegments(int row, const char *const *labels, int n, int active, bool fill) {
  const int y = setRowY(row) + 4;
  for (int i = 0; i < n; ++i) {
    const CellRect c = gridCellIn(i, n, 1, SET_CTRL_X, y, SET_CTRL_W, SET_ROW_H - 8, 3);
    const bool on = fill ? i <= active : i == active;
    const uint16_t bg = on ? (fill ? T_GOLD : T_LINK) : T_LINE;
    tft.fillRoundRect(c.x, c.y, c.w, c.h, 5, bg);
    if (!labels) continue;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(on ? T_TEXT : T_MUTED, bg);
    uiText(labels[i], c.x + c.w / 2, c.y + c.h / 2 + 1, 1);
  }
}

int segmentAt(int x, int n) {
  if (x < SET_CTRL_X) return -1;
  return min(n - 1, (x - SET_CTRL_X) * n / SET_CTRL_W);
}

// Info row: label on the left, value on the right; repainted only when the value changes.
void drawValueRow(int row, const char *label, const String &value, uint16_t color = T_MUTED) {
  if (settingsCache[row] == value) return;
  settingsCache[row] = value;
  drawSetRow(row, label);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(color, T_CARD_ALT);
  uiText(value, SET_ROW_X + SET_ROW_W - 10, setRowY(row) + SET_ROW_H / 2, 2);
}

void drawActionRow(int row, const char *label, uint16_t color, int x = SET_ROW_X, int w = SET_ROW_W) {
  drawSetRow(row, nullptr, x, w);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, T_CARD_ALT);
  uiText(label, x + w / 2, setRowY(row) + SET_ROW_H / 2, 2);
}

String hourText(uint8_t hour) {
  char text[6];
  snprintf(text, sizeof(text), "%02u:00", hour);
  return text;
}

void drawQuietBox(int x, uint8_t hour, bool on) {
  const int y = setRowY(2) + 4;
  tft.fillRoundRect(x, y, QUIET_BOX_W, SET_ROW_H - 8, 5, T_LINE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(on ? T_TEXT : T_MUTED, T_LINE);
  uiText(hourText(hour), x + QUIET_BOX_W / 2, y + (SET_ROW_H - 8) / 2 + 1, 2);
}

void drawAppChip(int i) {
  const CellRect c = gridCellIn(i, 4, 2, SET_ROW_X, setRowY(3), SET_ROW_W, SET_ROW_H * 2 + 4, 4);
  const bool muted = settings.mutedApps & (1u << i);
  const uint16_t bg = muted ? T_BG : T_CARD_ALT;
  tft.fillRoundRect(c.x, c.y, c.w, c.h, 7, bg);
  if (muted) tft.drawRoundRect(c.x, c.y, c.w, c.h, 7, T_LINE);
  tft.fillCircle(c.x + 9, c.y + c.h / 2, 3, muted ? T_DOWN : T_UP);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(muted ? T_MUTED : T_TEXT, bg);
  uiText(NOTIFY_APPS[i], c.x + 17, c.y + c.h / 2, 1);
}

String uptimeText() {
  const uint32_t minutes = millis() / 60000UL;
  if (minutes < 60) return String(minutes) + " min";
  if (minutes < 1440) return String(minutes / 60) + "h " + String(minutes % 60) + "m";
  return String(minutes / 1440) + "d " + String(minutes / 60 % 24) + "h";
}

void drawConnectValues() {
  drawValueRow(0, "Wi-Fi", pcLinkUp ? "Connected  " + String(netRssi()) + " dBm" : String("Not connected"),
               pcLinkUp ? T_UP : T_DOWN);
  drawValueRow(1, "Address", pcLinkUp ? netIp() + ":" + String(netPort()) : String("-"));
  drawValueRow(2, "iPhone", ancsReady() ? "Ready" : (ancsConnected() ? "Pairing..." : "Waiting"),
               ancsReady() ? T_UP : T_MUTED);
  const char *link = !usbLive() ? "Offline" : (serialHostActive() ? "USB" : (wifiLinkActive() ? "Wi-Fi" : "Online"));
  drawValueRow(3, "PC link", link, usbLive() ? T_UP : T_DOWN);
}

void drawDeviceValues() {
  drawValueRow(1, "Firmware", FIRMWARE_VERSION);
  drawValueRow(2, "Uptime", uptimeText());
  drawValueRow(3, "Free RAM", String(ESP.getFreeHeap() / 1024) + " KB");
}

void drawSettingsV2() {
  drawHeaderV2("SETTINGS", true);
  drawTabStrip(SET_TAB_SPECS, SET_TABS, settingsTab);
  tft.fillRect(0, INFO_Y, UI_SCREEN_W, UI_SCREEN_H - INFO_Y, T_CARD);
  for (String &cached : settingsCache) cached = "";
  switch (settingsTab) {
    case SET_DISPLAY:
      drawSetRow(0, "Brightness");
      drawSegments(0, nullptr, BRIGHTNESS_LEVELS, settings.brightness, true);
      drawSetRow(1, "Screen off");
      drawSegments(1, SLEEP_LABELS, SLEEP_OPTIONS, settings.sleepIndex, false);
      drawSwitchRow(2, "Flip 180", settings.rotate180);
      drawSetRow(3, "Standby");
      drawSegments(3, SLEEP_LABELS, SLEEP_OPTIONS, settings.standbyIndex, false);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(T_MUTED, T_CARD);
      uiText("Standby: clock screen after no touch. Tap to go back.", SET_ROW_X + 2, setRowY(4) + 6, 1);
      uiText("Screen off: backlight off; tap to wake.", SET_ROW_X + 2, setRowY(4) + 23, 1);
      break;
    case SET_NOTIFY:
      drawSwitchRow(0, "iPhone", settings.phoneOn, SET_ROW_X, SET_HALF_W);
      drawSwitchRow(0, "PC", settings.pcOn, SET_RIGHT_X, SET_HALF_W);
      drawSwitchRow(1, "Hide content", settings.hideBody);
      drawSwitchRow(2, "Quiet", settings.quietOn);
      drawQuietBox(QUIET_START_X, settings.quietStart, settings.quietOn);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(T_MUTED, T_CARD_ALT);
      uiText("-", (QUIET_START_X + QUIET_BOX_W + QUIET_END_X) / 2, setRowY(2) + SET_ROW_H / 2, 2);
      drawQuietBox(QUIET_END_X, settings.quietEnd, settings.quietOn);
      for (int i = 0; i < NOTIFY_APP_COUNT; ++i) drawAppChip(i);
      break;
    case SET_CONNECT:
      drawConnectValues();
      drawActionRow(4, "Hold: forget iPhone", T_DOWN);
      break;
    case SET_DEVICE:
      drawSwitchRow(0, "LED", settings.ledOn, SET_ROW_X, SET_HALF_W);
      drawSwitchRow(0, "Night off", settings.ledNight, SET_RIGHT_X, SET_HALF_W);
      drawDeviceValues();
      drawActionRow(4, "Hold: restart", T_TEXT, SET_ROW_X, SET_HALF_W);
      drawActionRow(4, "Hold: reset all", T_DOWN, SET_RIGHT_X, SET_HALF_W);
      break;
    default: break;
  }
}

// A bar fills along the bottom of the button while the finger stays on it.
bool holdRect(int x, int row, int w) {
  const int barY = setRowY(row) + SET_ROW_H - 5;
  const uint32_t start = millis();
  int misses = 0;
  while (millis() - start < HOLD_CONFIRM_MS) {
    misses = touchHeld() ? 0 : misses + 1;
    if (misses >= 3) {
      fullRedrawRequested = true;
      return false;
    }
    tft.fillRect(x + 8, barY, (w - 16) * (millis() - start) / HOLD_CONFIRM_MS, 3, T_TEXT);
    delay(20);
  }
  return true;
}

[[noreturn]] void restartWithMessage(const char *line1, const char *line2) {
  tft.fillRect(0, INFO_Y, UI_SCREEN_W, UI_SCREEN_H - INFO_Y, T_CARD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiText(line1, UI_SCREEN_W / 2, 130, 2);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText(line2, UI_SCREEN_W / 2, 154, 1);
  delay(2500);
  ESP.restart();
  while (true) delay(10);
}

void touchSettingsV2(uint16_t x, uint16_t y) {
  const int tab = gridIndexIn(x, y, SET_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
  if (tab >= 0) {
    if (tab != settingsTab) {
      settingsTab = tab;
      fullRedrawRequested = true;
    }
    return;
  }
  if (y < SET_ROW_Y0) return;
  const int row = (y - SET_ROW_Y0) / SET_ROW_STEP;
  if (row >= SET_ROWS || (y - SET_ROW_Y0) % SET_ROW_STEP >= SET_ROW_H) return;
  const bool right = x >= SET_RIGHT_X;
  bool changed = true;
  switch (settingsTab) {
    case SET_DISPLAY:
      if (row == 0 && segmentAt(x, BRIGHTNESS_LEVELS) >= 0) {
        settings.brightness = segmentAt(x, BRIGHTNESS_LEVELS);
        applyBacklight();
      } else if (row == 1 && segmentAt(x, SLEEP_OPTIONS) >= 0) {
        settings.sleepIndex = segmentAt(x, SLEEP_OPTIONS);
        lastActivityMs = millis();
      } else if (row == 2) {
        settings.rotate180 = !settings.rotate180;
        applyRotation();
      } else if (row == 3 && segmentAt(x, SLEEP_OPTIONS) >= 0) {
        settings.standbyIndex = segmentAt(x, SLEEP_OPTIONS);
        lastActivityMs = millis();
      } else {
        changed = false;
      }
      break;
    case SET_NOTIFY:
      if (row == 0) {
        bool &on = right ? settings.pcOn : settings.phoneOn;
        on = !on;
      } else if (row == 1) {
        settings.hideBody = !settings.hideBody;
      } else if (row == 2) {
        if (x >= QUIET_START_X && x < QUIET_START_X + QUIET_BOX_W) {
          settings.quietStart = (settings.quietStart + 1) % 24;
        } else if (x >= QUIET_END_X && x < QUIET_END_X + QUIET_BOX_W) {
          settings.quietEnd = (settings.quietEnd + 1) % 24;
        } else {
          settings.quietOn = !settings.quietOn;
        }
      } else {
        const int app = gridIndexIn(x, y, 4, 2, SET_ROW_X, setRowY(3), SET_ROW_W, SET_ROW_H * 2 + 4, 4);
        if (app >= 0) {
          settings.mutedApps ^= 1u << app;
        } else {
          changed = false;
        }
      }
      break;
    case SET_CONNECT:
      changed = false;
      if (row == 4 && holdRect(SET_ROW_X, 4, SET_ROW_W)) {
        ancsForgetPairing();
        restartWithMessage("iPhone forgotten", "Also tap Forget This Device on the iPhone.");
      }
      break;
    case SET_DEVICE:
      if (row == 0) {
        bool &on = right ? settings.ledNight : settings.ledOn;
        on = !on;
      } else if (row == 4) {
        changed = false;
        if (!right && holdRect(SET_ROW_X, 4, SET_HALF_W)) restartWithMessage("Restarting...", "");
        if (right && holdRect(SET_RIGHT_X, 4, SET_HALF_W)) {
          clearSettings();
          netForget();
          ancsForgetPairing();
          restartWithMessage("Settings reset", "The bridge sets Wi-Fi up again over USB.");
        }
      } else {
        changed = false;
      }
      break;
    default:
      changed = false;
      break;
  }
  if (!changed) return;
  saveSettings();
  fullRedrawRequested = true;
}

// ---------- Standby ----------
// Layout C: picture on the left, stacked clock / date / weather in a darkened right column.
// The picture comes from tools/make_standby_image.py (git-ignored); without it, a gradient.
#if __has_include("standby_image.h")
#include "standby_image.h"
#define DECK_STANDBY_IMAGE 1
#endif

constexpr int STANDBY_COL_X = 200;   // right column, repainted when the text changes
constexpr int STANDBY_TEXT_R = 310;  // right edge of the text
const uint16_t STANDBY_SOFT = rgb565(0xffd2d2);
String drawnStandbyKey;

// Repaints the background of a region (the picture rows, or the fallback gradient).
void drawStandbyBackground(int x, int y, int w, int h) {
#ifdef DECK_STANDBY_IMAGE
  tft.setSwapBytes(true);
  for (int row = y; row < y + h; ++row) {
    tft.pushImage(x, row, w, 1, STANDBY_IMAGE + row * STANDBY_IMAGE_W + x);
  }
  tft.setSwapBytes(false);
#else
  for (int row = y; row < y + h; ++row) {
    tft.drawFastHLine(x, row, w, blend565(rgb565(0x8b0a0a), T_BG, row * 255 / UI_SCREEN_H));
  }
#endif
}

// Background colour under (x, y), so antialiased text blends into the picture.
uint16_t standbyPixel(uint16_t x, uint16_t y) {
#ifdef DECK_STANDBY_IMAGE
  if (x < STANDBY_IMAGE_W && y < STANDBY_IMAGE_H) return pgm_read_word(&STANDBY_IMAGE[y * STANDBY_IMAGE_W + x]);
  return T_BG;
#else
  return blend565(rgb565(0x8b0a0a), T_BG, y * 255 / UI_SCREEN_H);
#endif
}

// Text with a 1 px drop shadow, drawn straight onto the picture.
void drawShadowText(const String &text, int x, int y, uint8_t font, uint16_t color) {
  tft.setCallback(standbyPixel);
  tft.setTextColor(TFT_BLACK, TFT_BLACK);
  uiText(text, x + 1, y + 2, font);
  tft.setTextColor(color, color);
  uiText(text, x, y, font);
  tft.setCallback(nullptr);
}

String standbyKey() {
  return currentClockText() + todayText() + (weather.has ? String(lroundf(weather.temp)) : String("-"));
}

void drawStandbyText() {
  drawStandbyBackground(STANDBY_COL_X, 0, UI_SCREEN_W - STANDBY_COL_X, UI_SCREEN_H);
  const String clock = currentClockText();  // "HH:MM" or "--:--"
  tft.setTextDatum(TR_DATUM);
  drawShadowText(clock.substring(0, 2), STANDBY_TEXT_R, 10, 6, T_TEXT);
  drawShadowText(clock.substring(3, 5), STANDBY_TEXT_R, 64, 6, T_GOLD);

  int d, m, y;
  if (deckDate(d, m, y)) {
    char text[16];
    snprintf(text, sizeof(text), "%s %02d/%02d", WEEKDAY_SHORT[dayOfWeek(d, m, y)], d, m);
    drawShadowText(text, STANDBY_TEXT_R, 126, 2, T_TEXT);
    const LunarDate lunar = solarToLunar(d, m, y);
    snprintf(text, sizeof(text), "%d/%d AL", lunar.day, lunar.month);
    drawShadowText(text, STANDBY_TEXT_R, 144, 2, STANDBY_SOFT);
    const VnHoliday *next = nullptr;
    const int toHoliday = daysToHoliday(d, m, y, &next, 7);
    if (toHoliday == 0) {
      drawShadowText(next->ascii, STANDBY_TEXT_R, 162, 2, T_GOLD);
    } else if (toHoliday > 0) {
      drawShadowText(String(next->ascii) + " " + toHoliday + "d", STANDBY_TEXT_R, 162, 2, STANDBY_SOFT);
    }
  }

  if (weather.has) {
    const String temp = String(lroundf(weather.temp));
    const int tempX = STANDBY_TEXT_R - 8;  // room for the degree sign
    drawShadowText(temp, tempX, 188, 4, T_TEXT);
    tft.drawCircle(STANDBY_TEXT_R - 3, 192, 3, T_TEXT);
    const int iconX = tempX - uiTextWidth(temp, 4) - 30;
    drawWeatherIcon(weather.icon, iconX, 190, 24, rgb565(0x200404));
    drawShadowText(weather.city, STANDBY_TEXT_R, 220, 2, STANDBY_SOFT);
  }
  drawnStandbyKey = standbyKey();
}

void drawStandbyV2() {
  drawStandbyBackground(0, 0, STANDBY_COL_X, UI_SCREEN_H);
  drawStandbyText();
}

// Called every loop: no touch for the chosen time opens the standby screen.
void uiUpdateStandby() {
  const uint32_t timeout = sleepTimeoutMs(settings.standbyIndex);
  if (timeout == 0 || screen == Screen::Standby || millis() - lastActivityMs < timeout) return;
  bannerActive = false;
  uiShow(Screen::Standby);
}

// ---------- ARAM (game helper pushed by ARAM Helper on the PC) ----------

AramData aram;
uint32_t aramLastMs = 0;
bool aramDismissed = false;  // Home pressed during this game: do not pop up again until the next one
Screen aramReturnTo = Screen::Home;

constexpr int ARAM_X = 8, ARAM_W = UI_SCREEN_W - 16;
constexpr int ARAM_CHAMP_Y = 36;
constexpr int ARAM_TOP_LABEL_Y = 58, ARAM_ROW_Y = 70, ARAM_ROW_STEP = 24, ARAM_ROW_H = 22;
constexpr int ARAM_COMBO_LABEL_Y = 146, ARAM_COMBO_Y = 158;
constexpr int ARAM_ENEMY_LABEL_Y = 182, ARAM_ENEMY_Y = 194, ARAM_ENEMY_STEP = 21;
constexpr uint16_t T_BEST_BG = 0x11C5;  // #123a2a

uint16_t aramTierColor(const String &tier) {
  if (tier.startsWith("S")) return T_ACCENT;
  if (tier == "A") return T_GOLD;
  if (tier == "B") return T_ACCENT_2;
  return T_MUTED;
}

void drawAramLabel(int y, const char *label) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, T_BG);
  const int w = uiTextIn(UiFont::Tiny, label, ARAM_X + 2, y);
  tft.drawFastHLine(ARAM_X + 8 + w, y + 5, ARAM_X + ARAM_W - (ARAM_X + 8 + w), T_DIVIDER);
}

void drawAramPick(int i) {
  const AramPick &p = aram.picks[i];
  const int y = ARAM_ROW_Y + i * ARAM_ROW_STEP;
  const uint16_t bg = p.best ? T_BEST_BG : T_CARD;
  tft.fillRoundRect(ARAM_X, y, ARAM_W, ARAM_ROW_H, 6, bg);
  if (p.best) {
    tft.fillCircle(ARAM_X + 11, y + ARAM_ROW_H / 2, 4, T_UP);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, bg);
    uiText(String(i + 1), ARAM_X + 11, y + ARAM_ROW_H / 2, 2);
  }
  drawViText(tft, ARAM_X + 22, y + 3, fitViText(p.name, 176, p.best), p.best ? T_UP : T_TEXT, bg, p.best);
  // Win rate at the right edge, the combo note ("+3 A") just left of it
  const int wrRight = ARAM_X + ARAM_W - 6;
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(p.best ? T_UP : T_TEXT, bg);
  uiText(p.winrate, wrRight, y + ARAM_ROW_H / 2, 2);
  tft.setTextColor(T_GOLD, bg);
  uiText(p.note, wrRight - uiTextWidth(p.winrate, 2) - 8, y + ARAM_ROW_H / 2, 1);
}

void drawAramV2() {
  drawHeaderV2("ARAM", true);
  // Champion, tier, champion win rate
  drawViText(tft, ARAM_X + 2, ARAM_CHAMP_Y, fitViText(aram.champion, 200, true), T_TEXT, T_BG, true);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(aramTierColor(aram.tier), T_BG);
  uiTextIn(UiFont::BodyBold, aram.tier, ARAM_X + 10 + viTextWidth(fitViText(aram.champion, 200, true), true),
           ARAM_CHAMP_Y + 9);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, T_BG);
  uiText(aram.winrate, ARAM_X + ARAM_W - 2, ARAM_CHAMP_Y + 9, 2);

  if (aram.pickCount) {  // augment choice on screen: ranked offer
    drawAramLabel(ARAM_TOP_LABEL_Y, "PICK");
    for (uint8_t i = 0; i < aram.pickCount; ++i) drawAramPick(i);
  } else {               // between choices: core build and augments already taken
    drawAramLabel(ARAM_TOP_LABEL_Y, "BUILD");
    drawViText(tft, ARAM_X + 2, ARAM_ROW_Y, fitViText(aram.build.length() ? aram.build : String("-"), ARAM_W - 4, true),
               T_UP, T_BG, true);
    drawAramLabel(ARAM_ROW_Y + 26, "TAKEN");
    drawViText(tft, ARAM_X + 2, ARAM_ROW_Y + 38, fitViText(aram.owned.length() ? aram.owned : String("-"), ARAM_W - 4),
               T_TEXT, T_BG);
  }

  if (aram.combo.length()) {
    drawAramLabel(ARAM_COMBO_LABEL_Y, "COMBO");
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(aramTierColor(aram.comboTier), T_BG);
    uiTextIn(UiFont::BodyBold, aram.comboTier, ARAM_X + 2, ARAM_COMBO_Y + 9);
    drawViText(tft, ARAM_X + 22, ARAM_COMBO_Y, fitViText(aram.combo, ARAM_W - 70), T_TEXT, T_BG);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(T_LINK, T_BG);
    uiText(aram.comboHave, ARAM_X + ARAM_W - 2, ARAM_COMBO_Y + 9, 1);
  }

  if (aram.counterCount) {
    drawAramLabel(ARAM_ENEMY_LABEL_Y, "VS ENEMY TEAM");
    for (uint8_t i = 0; i < aram.counterCount; ++i) {
      const AramLine &line = aram.counters[i];
      const int y = ARAM_ENEMY_Y + i * ARAM_ENEMY_STEP;
      int x = ARAM_X + 2;
      if (line.title.length()) {
        const String title = fitViText(line.title, 150, true);
        drawViText(tft, x, y, title, T_ACCENT, T_BG, true);
        x += viTextWidth(title, true) + 8;
      }
      drawViText(tft, x, y, fitViText(line.detail, ARAM_X + ARAM_W - x), T_MUTED, T_BG);
    }
  }
}

// Called from netLoop for every valid {"type":"aram"} datagram.
void uiAramPacket(const AramData &next) {
  const bool newGame = next.inGame && !aram.inGame;
  const bool changed = next.seq != aram.seq || next.inGame != aram.inGame;
  aram = next;
  aramLastMs = millis();
  if (!next.inGame) {  // game over: back to where the user was
    aramDismissed = false;
    if (screen == Screen::Aram) uiShow(aramReturnTo);
    return;
  }
  if (newGame) aramDismissed = false;
  noteActivity();  // keep the screen awake while a game runs
  if (screen == Screen::Aram) {
    if (changed) fullRedrawRequested = true;
  } else if (!aramDismissed) {
    aramReturnTo = screen == Screen::Standby ? Screen::Home : screen;
    bannerActive = false;
    uiShow(Screen::Aram);
  }
}

// No packet for ARAM_STALE_MS: the PC app stopped or the game ended without a final packet.
void aramCheckStale() {
  if (!aram.inGame || millis() - aramLastMs <= ARAM_STALE_MS) return;
  aram.inGame = false;
  aramDismissed = false;
  if (screen == Screen::Aram) uiShow(aramReturnTo);
}

// ---------- Claude Code (sessions and permission prompts relayed by the bridge) ----------

constexpr int CL_X = 8, CL_W = UI_SCREEN_W - 16;
constexpr int CL_ROW_Y = 38, CL_ROW_H = 36, CL_ROW_STEP = 39;
constexpr int CL_BTN_Y = 198, CL_BTN_H = 36, CL_BTN_W = (CL_W - 6) / 2;
constexpr int CL_BADGE_W = 64;
// Permission card: project, tool, command, then DENY (tap) and ALLOW (hold) buttons.
constexpr int CL_CARD_Y = 38, CL_CARD_H = 196;
constexpr int CL_ASK_BTN_Y = 182, CL_ASK_BTN_H = 44;
constexpr uint16_t T_RUN = T_ACCENT_2;      // #7fa9ee
constexpr uint16_t T_OPEN_BTN = 0x2B7C;     // #2f6fe4
constexpr uint16_t T_DENY_BG = 0x5124;      // #522420
constexpr uint16_t T_ALLOW_BG = 0x1347;     // #14683a

Screen claudeReturnTo = Screen::Home;
bool claudeAutoOpened = false;  // opened by a permission prompt: go back once it is answered
uint32_t claudeDrawnMinute = 0;
uint32_t claudeAskSinceMs = 0;
// The bridge gives up on a prompt after ask_wait_seconds (25 s by default) and says so; if the
// bridge itself goes away, the card closes after this long instead of staying on screen.
constexpr uint32_t CLAUDE_ASK_MAX_MS = 90000;

uint32_t claudeRowAge(const ClaudeRow &row) { return row.ageSec + (millis() - claudeRxMs) / 1000UL; }

// Cuts text to `w` pixels in `font`, ending in "..." when something was dropped.
String claudeFit(const String &text, UiFont font, int w) {
  if (uiTextWidthIn(font, text) <= w) return text;
  String out = text;
  while (out.length() && uiTextWidthIn(font, out + "...") > w) out.remove(out.length() - 1);
  return out + "...";
}

void drawClaudeBadge(int x, int y, ClaudeRunState state, uint16_t bg) {
  const char *label =
      state == ClaudeRunState::Waiting ? "WAITING" : (state == ClaudeRunState::Running ? "RUNNING" : "DONE");
  tft.setTextDatum(MC_DATUM);
  if (state == ClaudeRunState::Done) {
    tft.drawRoundRect(x, y, CL_BADGE_W, 18, 5, T_TEXT);
    tft.setTextColor(T_TEXT, bg);
  } else {
    const uint16_t fill = state == ClaudeRunState::Waiting ? T_ACCENT : T_RUN;
    tft.fillRoundRect(x, y, CL_BADGE_W, 18, 5, fill);
    tft.setTextColor(T_ON_ACCENT, fill);
  }
  uiTextIn(UiFont::Tiny, label, x + CL_BADGE_W / 2, y + 9);
}

constexpr uint8_t CLAUDE_FAIL_BADGE = 3;  // this many failed tool calls in a row: red badge

void drawClaudeFailBadge(int x, int y, uint8_t fails) {
  tft.fillRoundRect(x, y, CL_BADGE_W, 18, 5, T_BADGE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_BADGE);
  uiTextIn(UiFont::Tiny, String("FAIL x") + fails, x + CL_BADGE_W / 2, y + 9);
}

void drawClaudeRow(int i) {
  const ClaudeRow &row = claude.rows[i];
  const int y = CL_ROW_Y + i * CL_ROW_STEP;
  const bool waiting = row.state == ClaudeRunState::Waiting;
  const uint16_t bg = waiting ? T_ALERT_BG : T_CARD;
  tft.fillRoundRect(CL_X, y, CL_W, CL_ROW_H, 8, bg);
  if (waiting) {
    tft.drawRoundRect(CL_X, y, CL_W, CL_ROW_H, 8, T_ACCENT);
    tft.drawRoundRect(CL_X + 1, y + 1, CL_W - 2, CL_ROW_H - 2, 7, T_ACCENT);
  }
  if (row.fails >= CLAUDE_FAIL_BADGE && row.state == ClaudeRunState::Running) {
    drawClaudeFailBadge(CL_X + 6, y + (CL_ROW_H - 18) / 2, row.fails);
  } else {
    drawClaudeBadge(CL_X + 6, y + (CL_ROW_H - 18) / 2, row.state, bg);
  }
  const int textX = CL_X + CL_BADGE_W + 12;
  const int textW = CL_X + CL_W - 34 - textX;
  drawViText(tft, textX, y + 2, fitViText(row.name, textW, true), T_TEXT, bg, true);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, bg);
  uiTextIn(UiFont::Small, claudeFit(row.detail, UiFont::Small, textW), textX, y + 20);
  tft.setTextDatum(MR_DATUM);
  uiTextIn(UiFont::Small, claudeAgeText(claudeRowAge(row)), CL_X + CL_W - 8, y + CL_ROW_H / 2);
}

void drawClaudeButton(int x, const char *label, Icon icon, uint16_t bg, bool enabled) {
  tft.fillRoundRect(x, CL_BTN_Y, CL_BTN_W, CL_BTN_H, 8, bg);
  const uint16_t fg = enabled ? T_TEXT : T_MUTED;
  const int w = 18 + uiTextWidthIn(UiFont::BodyBold, label);
  const int left = x + (CL_BTN_W - w) / 2;
  drawIcon(tft, icon, left, CL_BTN_Y + (CL_BTN_H - 14) / 2, 14, fg, bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  uiTextIn(UiFont::BodyBold, label, left + 18, CL_BTN_Y + CL_BTN_H / 2);
}

int claudeWaitingRow() {
  for (uint8_t i = 0; i < claude.count; ++i)
    if (claude.rows[i].state == ClaudeRunState::Waiting) return i;
  return -1;
}

bool claudeHasDone() {
  for (uint8_t i = 0; i < claude.count; ++i)
    if (claude.rows[i].state == ClaudeRunState::Done) return true;
  return false;
}

// Splits text into at most two lines of `w` pixels (the second ends in "..." if cut).
void claudeTwoLines(const String &text, int w, String &a, String &b) {
  a = text;
  b = "";
  while (a.length() && uiTextWidthIn(UiFont::Body, a) > w) a.remove(a.length() - 1);
  if (a.length() < text.length()) b = claudeFit(text.substring(a.length()), UiFont::Body, w);
}

constexpr uint16_t T_ALWAYS_BG = 0x0A8D;  // #14506a
constexpr int CL_ASK_LEFT = CL_X + 8, CL_ASK_RIGHT = CL_X + CL_W - 8, CL_ASK_GAP = 6;

int claudeAskButtons() { return claude.askAlways.length() ? 3 : 2; }
int claudeAskButtonW() { return (CL_ASK_RIGHT - CL_ASK_LEFT - (claudeAskButtons() - 1) * CL_ASK_GAP) / claudeAskButtons(); }
int claudeAskButtonX(int i) { return CL_ASK_LEFT + i * (claudeAskButtonW() + CL_ASK_GAP); }

// Button index under x: 0 DENY, 1 ALLOW, 2 ALWAYS (-1 in a gap).
int claudeAskButtonAt(int x) {
  for (int i = 0; i < claudeAskButtons(); ++i)
    if (x >= claudeAskButtonX(i) && x < claudeAskButtonX(i) + claudeAskButtonW()) return i;
  return -1;
}

void drawClaudeAskButton(int i) {
  static const char *const LABELS[3] = {"DENY", "ALLOW", "ALWAYS"};
  static const uint16_t COLORS[3] = {T_DENY_BG, T_ALLOW_BG, T_ALWAYS_BG};
  const int x = claudeAskButtonX(i), w = claudeAskButtonW();
  tft.fillRoundRect(x, CL_ASK_BTN_Y, w, CL_ASK_BTN_H, 8, COLORS[i]);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, COLORS[i]);
  uiTextIn(UiFont::BodyBold, LABELS[i], x + w / 2, CL_ASK_BTN_Y + 15);
  tft.setTextColor(T_MUTED, COLORS[i]);
  uiTextIn(UiFont::Tiny, i == 0 ? "tap" : "hold 1 s", x + w / 2, CL_ASK_BTN_Y + 32);
}

void drawClaudeAskButtons() {
  for (int i = 0; i < claudeAskButtons(); ++i) drawClaudeAskButton(i);
}

void drawClaudeAsk() {
  tft.fillRoundRect(CL_X, CL_CARD_Y, CL_W, CL_CARD_H, 10, T_ALERT_BG);
  tft.drawRoundRect(CL_X, CL_CARD_Y, CL_W, CL_CARD_H, 10, T_ACCENT);
  tft.fillRoundRect(CL_X + 10, CL_CARD_Y + 10, 84, 18, 5, T_ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_ON_ACCENT, T_ACCENT);
  uiTextIn(UiFont::Tiny, "PERMISSION", CL_X + 52, CL_CARD_Y + 19);
  drawViText(tft, CL_X + 10, CL_CARD_Y + 34, fitViText(claude.askName, CL_W - 20, true), T_TEXT, T_ALERT_BG, true);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, T_ALERT_BG);
  uiTextIn(UiFont::Small, claudeFit(String("wants to use ") + claude.askTool, UiFont::Small, CL_W - 20), CL_X + 10,
           CL_CARD_Y + 54);
  // Command (or file / URL) in a dark well, up to two lines.
  const int wellY = CL_CARD_Y + 74;
  const bool rule = claude.askAlways.length() > 0;
  tft.fillRoundRect(CL_X + 10, wellY, CL_W - 20, rule ? 50 : 62, 6, T_BG);
  String a, b;
  claudeTwoLines(claude.askCmd.length() ? claude.askCmd : claude.askTool, CL_W - 36, a, b);
  tft.setTextColor(T_TEXT, T_BG);
  uiTextIn(UiFont::Body, a, CL_X + 18, wellY + (rule ? 5 : 10));
  uiTextIn(UiFont::Body, b, CL_X + 18, wellY + (rule ? 26 : 32));
  if (rule) {
    tft.setTextColor(T_MUTED, T_ALERT_BG);
    uiTextIn(UiFont::Small, claudeFit(String("Always: ") + claude.askAlways, UiFont::Small, CL_W - 20), CL_X + 10,
             wellY + 54);
  }
  drawClaudeAskButtons();
}

// ---------- Claude Code: Usage and Stats tabs (swipe from the session list) ----------

enum ClaudeTab : uint8_t { CLAUDE_SESSIONS, CLAUDE_USAGE, CLAUDE_STATS, CLAUDE_TABS };
uint8_t claudeTab = CLAUDE_SESSIONS;
constexpr uint16_t T_BAR_BG = 0x29A8;  // #2e3442

void drawClaudeTabDots() {
  for (int i = 0; i < CLAUDE_TABS; ++i)
    tft.fillCircle(190 + i * 10, UI_HEADER_H / 2, 3, i == claudeTab ? T_TEXT : T_DIVIDER);
}

void drawClaudeLabel(int x, int y, const char *label, uint16_t bg) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, bg);
  uiTextIn(UiFont::Tiny, label, x, y);
}

String claudeUsd(float usd) { return String("$") + String(usd, usd < 100 ? 2 : 0); }

void drawClaudeUsage() {
  if (!claudeUsage.has) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_SURFACE);
    uiText("Reading Claude Code logs...", UI_SCREEN_W / 2, 130, 2);
    return;
  }
  const ClaudeUsage &u = claudeUsage;
  // Today: estimated cost and token kinds
  tft.fillRoundRect(CL_X, 38, CL_W, 62, 8, T_CARD);
  drawClaudeLabel(CL_X + 10, 44, "TODAY  (est. at API rates)", T_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiTextIn(UiFont::Large, claudeUsd(u.usd), CL_X + 10, 76);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiTextIn(UiFont::Small, String("in ") + claudeTokens(u.input) + "   out " + claudeTokens(u.output),
           CL_X + CL_W - 10, 66);
  uiTextIn(UiFont::Small, String("cache w ") + claudeTokens(u.cacheWrite) + "   r " + claudeTokens(u.cacheRead),
           CL_X + CL_W - 10, 86);

  // Current 5-hour window, compared with the biggest one so far
  tft.fillRoundRect(CL_X, 106, CL_W, 58, 8, T_CARD);
  drawClaudeLabel(CL_X + 10, 112, "5-HOUR WINDOW  (vs your peak)", T_CARD);
  if (u.window) {
    const uint32_t passed = (millis() - claudeUsageRxMs) / 1000UL;
    const uint32_t left = u.windowLeftSec > passed ? u.windowLeftSec - passed : 0;
    const int barW = CL_W - 20;
    const uint16_t fill = u.windowPct >= 90 ? T_DOWN : (u.windowPct >= 70 ? T_ACCENT : T_UP);
    tft.fillRoundRect(CL_X + 10, 128, barW, 8, 4, T_BAR_BG);
    tft.fillRoundRect(CL_X + 10, 128, max(8, barW * min<int>(u.windowPct, 100) / 100), 8, 4, fill);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_TEXT, T_CARD);
    uiTextIn(UiFont::BodyBold, String(u.windowPct) + "%", CL_X + 10, 150);
    tft.setTextColor(T_MUTED, T_CARD);
    uiTextIn(UiFont::Small, claudeUsd(u.windowUsd) + " since " + u.windowStart, CL_X + 56, 150);
    tft.setTextDatum(MR_DATUM);
    uiTextIn(UiFont::Small, String("reset in ") + claudeDuration(left), CL_X + CL_W - 10, 150);
  } else {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_MUTED, T_CARD);
    uiTextIn(UiFont::Small, "No open window - the next message starts one", CL_X + 10, 140);
  }

  // Top projects today
  drawClaudeLabel(CL_X + 2, 170, "TOP TODAY", T_SURFACE);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiTextIn(UiFont::Tiny, String("updated ") + u.time, CL_X + CL_W, 170);
  const float most = u.topCount ? u.top[0].usd : 0;
  for (uint8_t i = 0; i < u.topCount; ++i) {
    const int y = 184 + i * 18;
    const int barMax = 90;
    tft.fillRoundRect(CL_X + 150, y + 5, barMax, 6, 3, T_BAR_BG);
    if (most > 0) tft.fillRoundRect(CL_X + 150, y + 5, max(6, static_cast<int>(barMax * u.top[i].usd / most)), 6, 3, T_RUN);
    drawViText(tft, CL_X + 2, y - 1, fitViText(u.top[i].name, 140), T_TEXT, T_SURFACE);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(T_TEXT, T_SURFACE);
    uiTextIn(UiFont::Small, claudeUsd(u.top[i].usd), CL_X + CL_W, y + 1);
  }
}

void drawClaudeStatTile(int i, const char *label, uint16_t value, uint16_t color) {
  const int w = (CL_W - 12) / 3, h = 30;
  const int x = CL_X + (i % 3) * (w + 6), y = 170 + (i / 3) * (h + 4);
  tft.fillRoundRect(x, y, w, h, 6, T_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, T_CARD);
  uiTextIn(UiFont::BodyBold, String(value), x + 8, y + h / 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiTextIn(UiFont::Tiny, label, x + w - 6, y + h / 2);
}

void drawClaudeStats() {
  // Assistant messages per hour today
  drawClaudeLabel(CL_X + 2, 38, "ACTIVITY TODAY  (Claude replies per hour)", T_SURFACE);
  const int chartX = CL_X + 2, chartY = 54, chartH = 92, slot = (CL_W - 4) / 24;
  const uint16_t peak = claudeUsage.peakHour();
  const int hourNow = clockHour();
  tft.drawFastHLine(chartX, chartY + chartH, slot * 24, T_DIVIDER);
  for (int h = 0; h < 24; ++h) {
    const uint16_t n = claudeUsage.hours[h];
    const int barH = peak ? max<int>(n ? 2 : 0, chartH * n / peak) : 0;
    if (barH) tft.fillRect(chartX + h * slot + 2, chartY + chartH - barH, slot - 3, barH, h == hourNow ? T_ACCENT : T_RUN);
  }
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(T_MUTED, T_SURFACE);
  for (int h = 0; h < 24; h += 6) uiTextIn(UiFont::Tiny, String(h), chartX + h * slot + slot / 2, chartY + chartH + 4);
  if (peak) {
    tft.setTextDatum(TR_DATUM);
    uiTextIn(UiFont::Tiny, String("peak ") + peak, CL_X + CL_W, 38);
  }
  // Daily counts from the bridge
  const ClaudeStats &s = claudeStats;
  drawClaudeStatTile(0, "ASKS", s.asks, T_ACCENT);
  drawClaudeStatTile(1, "ALLOW", s.allowed, T_UP);
  drawClaudeStatTile(2, "ALWAYS", s.always, T_RUN);
  drawClaudeStatTile(3, "DENY", s.denied, T_DOWN);
  drawClaudeStatTile(4, "ON PC", s.onPc, T_TEXT);
  drawClaudeStatTile(5, "DONE", s.done, T_GOLD);
}

void uiClaudeInfoChanged() {
  if (screen == Screen::Claude && !claude.askId && claudeTab != CLAUDE_SESSIONS) fullRedrawRequested = true;
}

void drawClaudeV2() {
  static const char *const TITLES[CLAUDE_TABS] = {"CLAUDE CODE", "CLAUDE USAGE", "CLAUDE STATS"};
  claudeDrawnMinute = millis() / 60000UL;
  if (claude.askId) {
    drawHeaderV2("CLAUDE CODE", true);
    drawClaudeAsk();
    return;
  }
  drawHeaderV2(TITLES[claudeTab], true);
  drawClaudeTabDots();
  if (claudeTab == CLAUDE_USAGE) {
    drawClaudeUsage();
    return;
  }
  if (claudeTab == CLAUDE_STATS) {
    drawClaudeStats();
    return;
  }
  if (claude.count == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_SURFACE);
    uiText(claudeRxMs ? "No Claude Code sessions" : "Waiting for the bridge", UI_SCREEN_W / 2, 100, 2);
    uiTextIn(UiFont::Small, "Hook setup: docs/CLAUDE_CODE.md", UI_SCREEN_W / 2, 124);
  }
  for (uint8_t i = 0; i < claude.count; ++i) drawClaudeRow(i);
  if (claude.total > claude.count) {  // more sessions than rows: count them in the header
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_MUTED, T_BG);
    uiTextIn(UiFont::Small, String("+") + (claude.total - claude.count),
             50 + uiTextWidthIn(UiFont::BodyBold, "CLAUDE CODE"), UI_HEADER_H / 2);
  }
  const bool waiting = claudeWaitingRow() >= 0;
  drawClaudeButton(CL_X, "OPEN WAITING", Icon::ChevronRight, waiting ? T_OPEN_BTN : T_CARD, waiting);
  drawClaudeButton(CL_X + CL_BTN_W + 6, "CLEAR DONE", Icon::Clipboard, T_CARD, claudeHasDone());
}

// ALLOW and ALWAYS need the finger on the button for HOLD_CONFIRM_MS; a bar fills under the label.
bool claudeHoldButton(int i) {
  const int x = claudeAskButtonX(i), w = claudeAskButtonW();
  const uint32_t start = millis();
  int misses = 0;
  while (millis() - start < HOLD_CONFIRM_MS) {
    misses = touchHeld() ? 0 : misses + 1;
    if (misses >= 3) {
      drawClaudeAskButton(i);
      return false;
    }
    tft.fillRect(x + 6, CL_ASK_BTN_Y + CL_ASK_BTN_H - 5, (w - 12) * (millis() - start) / HOLD_CONFIRM_MS, 3, T_TEXT);
    delay(20);
  }
  return true;
}

void claudeAnswered() {
  claude.askId = 0;  // the bridge's next frame confirms it
  if (claudeAutoOpened) {
    claudeAutoOpened = false;
    uiShow(claudeReturnTo);
  } else {
    fullRedrawRequested = true;
  }
}

void claudeCheckStaleAsk() {
  if (!claude.askId || millis() - claudeAskSinceMs <= CLAUDE_ASK_MAX_MS) return;
  claude.askId = 0;
  for (uint8_t i = 0; i < claude.count; ++i)
    if (claude.rows[i].state == ClaudeRunState::Waiting) claude.rows[i].detail = "Answer on PC";
  if (screen == Screen::Claude) claudeAnswered();
}

void touchClaudeV2(uint16_t x, uint16_t y) {
  if (claude.askId) {
    if (y < CL_ASK_BTN_Y || y > CL_ASK_BTN_Y + CL_ASK_BTN_H) return;
    const uint32_t id = claude.askId;
    const int button = claudeAskButtonAt(x);
    if (button == 0) {
      sendClaudeReply(id, false);
      waitForRelease();
      claudeAnswered();
    } else if (button > 0 && claudeHoldButton(button)) {
      sendClaudeReply(id, true, button == 2);
      ledFlashUntil = millis() + 150;
      waitForRelease();
      claudeAnswered();
    } else {
      waitForRelease();
    }
    lastTouchMs = millis();
    return;
  }
  if (claudeTab != CLAUDE_SESSIONS || y < CL_BTN_Y) return;
  if (x < CL_X + CL_BTN_W) {
    const int row = claudeWaitingRow();
    if (row >= 0) sendClaudeOpen(claude.rows[row].name);
  } else if (claudeHasDone()) {
    sendSimpleFrame("claude_clear");
  }
}

// Every "claude" frame. A new permission prompt pops this screen up (like ARAM);
// once it is answered (here or on the PC) the deck goes back to where it was.
void uiClaudeFrame(const ClaudeState &next) {
  const bool newAsk = next.askId != 0 && next.askId != claude.askId;
  const bool askGone = next.askId == 0 && claude.askId != 0;
  claude = next;
  if (newAsk) {
    claudeAskSinceMs = millis();
    noteActivity();
    if (screen != Screen::Claude) {
      claudeReturnTo = screen == Screen::Standby ? Screen::Home : screen;
      claudeAutoOpened = true;
      bannerActive = false;
      uiShow(Screen::Claude);
      return;
    }
  }
  if (askGone && claudeAutoOpened && screen == Screen::Claude) {
    claudeAutoOpened = false;
    uiShow(claudeReturnTo);
    return;
  }
  if (screen == Screen::Claude) fullRedrawRequested = true;
}

// ---------- Router ----------

void uiRenderFullV2() {
  tft.fillScreen(T_SURFACE);
  switch (screen) {
    case Screen::Home: drawHomeV2(); break;
    case Screen::Monitor:
      drawMonitorV2Static();
      drawMonitorV2Dynamic();
      break;
    case Screen::Controls: drawControlsV2(); break;
    case Screen::Info: drawInfoV2(); break;
    case Screen::Claude: drawClaudeV2(); break;
    case Screen::AiTools: drawPlaceholderV2("AI TOOLS"); break;
    case Screen::Standby: drawStandbyV2(); break;
    case Screen::Notify: drawNotifyV2(); break;
    case Screen::Settings: drawSettingsV2(); break;
    case Screen::Aram: drawAramV2(); break;
    case Screen::Break: drawBreakV2(); break;
  }
  drawHeaderStatusV2();
  // A full redraw (e.g. navigating) must not leave an invisible banner that still eats header taps.
  if (bannerActive) drawBannerV2();
}

void uiRenderDynamicV2() {
  aramCheckStale();
  claudeCheckStaleAsk();
  breakTick();
  if (bannerActive && static_cast<int32_t>(bannerUntil - millis()) <= 0) {
    bannerActive = false;
    fullRedrawRequested = true;  // repaint what the banner covered
    return;
  }
  drawHeaderStatusV2();
  if (screen == Screen::Monitor) drawMonitorV2Dynamic();
  // Row ages tick once a minute.
  if (screen == Screen::Claude && !claude.askId && millis() / 60000UL != claudeDrawnMinute) fullRedrawRequested = true;

  // A new day: the calendar's "today" moves.
  static int drawnDay = -1;
  int d, m, y;
  if (deckDate(d, m, y) && d != drawnDay) {
    if (drawnDay >= 0 && screen == Screen::Info && infoTab == TAB_CALENDAR) fullRedrawRequested = true;
    drawnDay = d;
  }
  if (screen == Screen::Settings && settingsTab == SET_CONNECT) drawConnectValues();
  if (screen == Screen::Settings && settingsTab == SET_DEVICE) drawDeviceValues();
  if (screen == Screen::Standby && !bannerActive && standbyKey() != drawnStandbyKey) drawStandbyText();
  if (screen == Screen::Home) {
    // The banner covers the status bar; the full redraw after it repaints both.
    if (!bannerActive && homeChromeKey() != drawnHomeKey) {
      drawHomeStatusBar();
      drawHomeFooter();
      drawnHomeKey = homeChromeKey();
    }
    if (drawnNotifyVersion != notifyStore.version()) {
      drawHomeApp(NOTIFY_APP);
      if (bannerActive) drawBannerV2();  // the top app row runs under the banner
    }
  }
}

void uiHandleTouchV2(uint16_t x, uint16_t y) {
  if (bannerActive && y < BANNER_H) {
    bannerActive = false;
    uiShow(Screen::Notify);
    return;
  }
  if (screen == Screen::Break) {
    touchBreakV2(x, y);
    return;
  }
  if (screen == Screen::Standby) {
    uiShow(Screen::Home);
    waitForRelease();
    return;
  }
  if (screen != Screen::Home && y < UI_HEADER_H && x < HOME_BTN_W) {
    if (screen == Screen::Aram) aramDismissed = true;  // stay home until the next game
    if (screen == Screen::Claude) claudeAutoOpened = false;
    uiShow(Screen::Home);
    return;
  }
  switch (screen) {
    case Screen::Home: touchHomeV2(x, y); break;
    case Screen::Controls: touchControlsV2(x, y); break;
    case Screen::Notify: touchNotifyV2(x, y); break;
    case Screen::Info: touchInfoV2(x, y); break;
    case Screen::Settings: touchSettingsV2(x, y); break;
    case Screen::Claude: touchClaudeV2(x, y); break;
    default: break;
  }
}

// Screenshot helper: opens a screen by name (and tab / page when given).
void uiOpenForShot(const char *name, int tab) {
  static const struct {
    const char *name;
    Screen target;
  } SCREENS[] = {{"home", Screen::Home},       {"monitor", Screen::Monitor}, {"controls", Screen::Controls},
                 {"info", Screen::Info},       {"claude", Screen::Claude},   {"aitools", Screen::AiTools},
                 {"standby", Screen::Standby}, {"notify", Screen::Notify},   {"settings", Screen::Settings},
                 {"aram", Screen::Aram},       {"break", Screen::Break}};
  for (const auto &entry : SCREENS) {
    if (strcmp(entry.name, name) == 0) screen = entry.target;
  }
  if (tab >= 0 && screen == Screen::Info) infoTab = tab % INFO_TABS;
  if (tab >= 0 && screen == Screen::Settings) settingsTab = tab % SET_TABS;
  if (tab >= 0 && screen == Screen::Controls) controlsPage = tab % MACRO_PAGES;
  if (tab >= 0 && screen == Screen::Claude) claudeTab = tab % CLAUDE_TABS;
  if (screen == Screen::Break) {
    breakKind = tab == 1 ? BreakKind::Eye : BreakKind::Stand;
    breakEyeUntil = millis() + EYE_REST_MS;
  }
  bannerActive = false;
  lastActivityMs = millis();
}

// Horizontal swipe: next (+1) or previous (-1) tab / page inside the current app.
void uiSwipeV2(int step) {
  switch (screen) {
    case Screen::Info: infoTab = (infoTab + INFO_TABS + step) % INFO_TABS; break;
    case Screen::Settings: settingsTab = (settingsTab + SET_TABS + step) % SET_TABS; break;
    case Screen::Controls: controlsPage = (controlsPage + MACRO_PAGES + step) % MACRO_PAGES; break;
    case Screen::Claude:
      if (claude.askId) return;
      claudeTab = (claudeTab + CLAUDE_TABS + step) % CLAUDE_TABS;
      break;
    case Screen::Standby: uiShow(Screen::Home); return;
    default: return;
  }
  fullRedrawRequested = true;
}

void uiConfigChanged() {
  if (screen == Screen::Controls) fullRedrawRequested = true;
}

void uiLinksChanged() {
  if (screen == Screen::Notify) fullRedrawRequested = true;
}

void uiInfoFeedChanged(uint8_t tab) {
  if (screen == Screen::Info && infoTab == tab) fullRedrawRequested = true;
}

void uiGoldChanged() {
  if (screen == Screen::Info && infoTab == TAB_GOLD) fullRedrawRequested = true;
}
