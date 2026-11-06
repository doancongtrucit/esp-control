// UI v2 (Home grid design). Included into main.cpp's anonymous namespace so the
// screens share the deck state (stats, info, macros, notifyStore) without
// exporting it. Only built with DECK_UI_V2.
#pragma once

enum class Screen : uint8_t { Home, Monitor, Controls, Info, Claude, AiTools, Standby, Notify, Settings };
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
    tft.drawString(title, homeButton ? 44 : 10, UI_HEADER_H / 2, 2);
  }
}

// USB link dot and PC clock, refreshed every second.
void drawHeaderStatusV2() {
  // The banner covers the header; the Notifications header uses this area for HIDE.
  if (bannerActive || screen == Screen::Notify || screen == Screen::Home || screen == Screen::Standby) return;
  const bool stale = stats.everUpdated && millis() - stats.lastUpdateMs > STALE_AFTER_MS;
  const uint16_t dot = !stats.everUpdated ? T_DIVIDER : (stale ? T_DOWN : T_UP);
  tft.fillRect(STATUS_X, 0, UI_SCREEN_W - STATUS_X, UI_HEADER_H - 1, T_BG);
  tft.fillCircle(STATUS_X + 30, UI_HEADER_H / 2, 3, dot);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  tft.drawString(currentClockText(), UI_SCREEN_W - 10, UI_HEADER_H / 2, 2);
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
  tft.drawString(date, 12, STATUS_BAR_H / 2, 2);

  // Right to left: USB box, Bluetooth, Wi-Fi, clock.
  const uint16_t usb = usbLive() ? T_TEXT : T_MUTED;
  tft.drawRoundRect(284, 7, 28, 13, 4, usb);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(usb, T_BG);
  tft.drawString("USB", 298, 14, 1);
  drawIcon(tft, Icon::Bluetooth, 266, 6, 14, phoneLinkUp ? T_TEXT : T_MUTED, T_BG);
  drawIcon(tft, Icon::Wifi, 246, 6, 14, pcLinkUp ? T_TEXT : T_MUTED, T_BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  tft.drawString(currentClockText(), 240, STATUS_BAR_H / 2, 2);
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
  tft.drawString(app.label, c.x + c.w / 2, iy + APP_ICON + 14, 2);

  if (index == NOTIFY_APP) {
    const size_t unread = notifyStore.unreadCount();
    if (unread > 0) {
      const String count = unread > 9 ? String("9+") : String(unread);
      tft.fillCircle(ix + APP_ICON - 2, iy + 2, 10, T_BG);
      tft.fillCircle(ix + APP_ICON - 2, iy + 2, 8, T_BADGE);
      tft.setTextColor(0xFFFF, T_BADGE);
      tft.drawString(count, ix + APP_ICON - 2, iy + 3, 1);
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
  for (const char *label : labels) total += 10 + tft.textWidth(label, 2) + 12;
  int x = (UI_SCREEN_W - total + 12) / 2;
  const int midY = HOME_FOOTER_Y + 12;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_BG);
  for (int i = 0; i < 3; ++i) {
    tft.fillCircle(x + 3, midY, 3, up[i] ? T_UP : T_DOWN);
    tft.drawString(labels[i], x + 10, midY, 2);
    x += 10 + tft.textWidth(labels[i], 2) + 12;
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
constexpr int MON_BAR_W = 150;
constexpr int MON_BOX_Y = 156;
constexpr int MON_BOX_H = 74;

String tempText(float celsius) { return isnan(celsius) ? String("N/A") : fmtValue(celsius) + "C"; }

void drawMonitorV2Static() {
  drawHeaderV2("MONITOR", true);
  const char *names[4] = {"CPU", "RAM", "GPU", "VRAM"};
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_SURFACE);
  for (int i = 0; i < 4; ++i) tft.drawString(names[i], 10, MON_ROW_Y[i] + 8, 2);
  const char *boxes[2] = {"NETWORK", "DISK C:"};
  for (int i = 0; i < 2; ++i) {
    const int x = 10 + i * 153;
    tft.fillRoundRect(x, MON_BOX_Y, 147, MON_BOX_H, 8, T_TILE);
    tft.setTextColor(T_MUTED, T_TILE);
    tft.drawString(boxes[i], x + 8, MON_BOX_Y + 12, 1);
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
  tft.drawString(fmtValue(pct) + "%", 248, y + 8, 2);
  tft.setTextColor(T_MUTED, T_SURFACE);
  tft.drawString(aux, UI_SCREEN_W - 8, y + 8, 2);
}

void drawMonitorBox(int index, const String &value, const String &detail) {
  const int x = 10 + index * 153;
  tft.fillRect(x + 6, MON_BOX_Y + 22, 135, 46, T_TILE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_TILE);
  tft.drawString(value, x + 8, MON_BOX_Y + 32, 2);
  tft.setTextColor(T_MUTED, T_TILE);
  tft.drawString(detail, x + 8, MON_BOX_Y + 54, 2);
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
    tft.drawString(line1, c.x + c.w / 2, iy + CONTROL_ICON + 11, 2);
    tft.drawString(line2, c.x + c.w / 2, iy + CONTROL_ICON + 26, 2);
  } else {
    tft.drawString(line1, c.x + c.w / 2, iy + CONTROL_ICON + 14, 2);
  }
  if (macroHold[id - 1]) {
    tft.fillRoundRect(ix + CONTROL_ICON - 22, iy - 6, 34, 14, 7, T_BG);
    tft.fillRoundRect(ix + CONTROL_ICON - 20, iy - 4, 30, 10, 5, T_ACCENT);
    tft.setTextColor(T_ON_ACCENT, T_ACCENT);
    tft.drawString("HOLD", ix + CONTROL_ICON - 5, iy + 1, 1);
  }
}

void drawControlsV2() {
  drawHeaderV2(nullptr, true);
  drawIcon(tft, Icon::ChevronLeft, PREV_BTN_X + 10, 8, 16, T_TEXT, T_BG);
  drawIcon(tft, Icon::ChevronRight, NEXT_BTN_X + 10, 8, 16, T_TEXT, T_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  tft.drawString(CONTROLS_TITLES[controlsPage], (PREV_BTN_X + ARROW_BTN_W + NEXT_BTN_X) / 2, UI_HEADER_H / 2, 2);
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

enum InfoTab : uint8_t { TAB_GOLD, TAB_COIN, TAB_STOCK, TAB_WEATHER, TAB_CALENDAR, INFO_TABS };
uint8_t infoTab = TAB_GOLD;

struct InfoTabSpec {
  const char *label;
  Icon icon;
  uint32_t line;  // underline color of the active tab
};
constexpr InfoTabSpec INFO_TAB_SPECS[INFO_TABS] = {
    {"Gold", Icon::Gold, 0xffd60a},   {"Coin", Icon::Coin, 0xff9f0a},          {"Stock", Icon::Trend, 0x30d158},
    {"Weather", Icon::Cloud, 0x64d2ff}, {"Calendar", Icon::Calendar, 0xff375f},
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
    tft.drawString(specs[i].label, c.x + c.w / 2, c.y + 21, 1);
  }
}

void drawInfoTabs() { drawTabStrip(INFO_TAB_SPECS, INFO_TABS, infoTab); }

void drawInfoEmpty(const char *message) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString(message, UI_SCREEN_W / 2, (INFO_Y + UI_SCREEN_H) / 2, 2);
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
  tft.drawString("x1000 VND / chi", 10, headY + 9, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("BUY", colBuy, headY + 9, 1);
  tft.drawString("SELL", colSell, headY + 9, 1);
  tft.drawString("+/-", colChange, headY + 9, 1);

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
    tft.drawString(info.goldName[i], 10, mid, 2);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(fmtPrice(info.goldBuy[i]), colBuy, mid, 2);
    tft.setTextColor(T_GOLD, bg);
    tft.drawString(fmtPrice(info.goldSell[i]), colSell, mid, 2);
    const long change = info.goldChange[i];
    tft.setTextColor(change > 0 ? T_UP : (change < 0 ? T_DOWN : T_MUTED), bg);
    tft.drawString(change > 0 ? "+" + fmtPrice(change) : fmtPrice(change), colChange, mid, 2);
  }

  constexpr int footY = 226;
  tft.setTextDatum(ML_DATUM);
  if (!isnan(info.xau)) {
    const String world = "World $" + String(info.xau, 1) + "/oz ";
    tft.setTextColor(T_MUTED, T_CARD);
    tft.drawString(world, 10, footY, 2);
    tft.setTextColor(info.xauChange > 0 ? T_UP : (info.xauChange < 0 ? T_DOWN : T_MUTED), T_CARD);
    tft.drawString((info.xauChange > 0 ? "+" : "") + String(info.xauChange, 1), 10 + tft.textWidth(world, 2), footY, 2);
  }
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString("Updated " + info.goldTime, colChange, footY, 2);
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
  tft.drawString(WEEKDAY_LONG[dayOfWeek(day, month, year)], cx, cardY + 14, 1);
  tft.setTextColor(T_TEXT, T_CARD_ALT);
  char text[24];
  snprintf(text, sizeof(text), "%02d", day);
  tft.drawString(text, cx, cardY + 40, 4);
  tft.setTextColor(T_MUTED, T_CARD_ALT);
  snprintf(text, sizeof(text), "%s %d", MONTH_SHORT[month - 1], year);
  tft.drawString(text, cx, cardY + 64, 2);
  tft.drawFastHLine(cardX + 16, cardY + 78, cardW - 32, T_DIVIDER);
  tft.drawString(today.leap ? "LUNAR (LEAP)" : "LUNAR", cx, cardY + 90, 1);
  tft.setTextColor(T_ACCENT, T_CARD_ALT);
  snprintf(text, sizeof(text), "%d/%d", today.day, today.month);
  tft.drawString(text, cx, cardY + 110, 4);
  tft.setTextColor(T_MUTED, T_CARD_ALT);
  tft.drawString(canChiYear(today.year), cx, cardY + 134, 2);

  // Right: Monday-first month grid with the lunar day under each solar day.
  constexpr int gridX = 108, gridW = 204, headY = INFO_Y + 6, weekY = INFO_Y + 20, rowsY = INFO_Y + 31;
  constexpr int rowH = (UI_SCREEN_H - 2 - rowsY) / 6;
  const int colW = gridW / 7;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  snprintf(text, sizeof(text), "%s %d", MONTH_LONG[month - 1], year);
  tft.drawString(text, gridX + 2, headY, 1);
  const char *const WEEK_INITIALS = "MTWTFSS";
  tft.setTextDatum(MC_DATUM);
  for (int c = 0; c < 7; ++c) {
    tft.setTextColor(c == 6 ? T_DOWN : T_MUTED, T_CARD);
    tft.drawString(String(WEEK_INITIALS[c]), gridX + c * colW + colW / 2, weekY, 1);
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
    tft.setTextColor(isToday ? 0xFFFF : (cell % 7 == 6 ? T_DOWN : T_TEXT), bg);
    tft.drawString(String(d), x + colW / 2, y + 7, 2);
    const bool special = l.day == 1 || l.day == 15;
    snprintf(text, sizeof(text), l.day == 1 ? "%d/%d" : "%d", l.day, l.month);
    tft.setTextColor(isToday ? 0xFFFF : (special ? T_ACCENT : T_MUTED), bg);
    tft.drawString(text, x + colW / 2, y + rowH - 5, 1);
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
  tft.drawString(left, 10, footY, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("Updated " + updated, UI_SCREEN_W - 10, footY, 1);
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
    tft.drawString(String(row.symbol[0]), 22, mid + 1, 2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_TEXT, T_CARD);
    tft.drawString(row.symbol, 40, mid, 2);
    tft.setTextColor(T_MUTED, T_CARD);
    tft.drawString(row.name, 40 + tft.textWidth(row.symbol, 2) + 6, mid + 1, 1);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(T_TEXT, T_CARD);
    tft.drawString(coinPriceText(row.price), 248, mid, 2);
    tft.setTextColor(changeColorFor(row.change), T_CARD);
    tft.drawString(signedPercent(row.change), UI_SCREEN_W - 10, mid, 2);
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
  tft.drawString(name, 10, mid, 2);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString(kind, 104, mid + 1, 1);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  tft.drawString(value, 248, mid, 2);
  tft.setTextColor(changeColorFor(change), T_CARD);
  tft.drawString(signedPercent(change), UI_SCREEN_W - 10, mid, 2);
}

void drawStockTab() {
  if (!stocks.has || stocks.indexCount == 0) {
    drawInfoEmpty("No data yet");
    return;
  }
  const QuoteRow &main = stocks.index[0];
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString(main.name, 10, INFO_Y + 6, 1);
  tft.setTextColor(T_TEXT, T_CARD);
  tft.drawString(fmtGrouped(main.value, 2), 10, INFO_Y + 17, 4);
  tft.setTextColor(changeColorFor(main.change), T_CARD);
  tft.drawString(signedPercent(main.change), 10, INFO_Y + 44, 2);
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
  tft.drawString(temp, 62, INFO_Y + 12, 4);
  const int tempW = tft.textWidth(temp, 4);
  tft.drawCircle(66 + tempW, INFO_Y + 15, 3, T_TEXT);  // degree sign
  tft.drawString(weather.desc, 128, INFO_Y + 8, 2);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString(String(weather.city) + " - feels " + String(weather.feels) + "C", 128, INFO_Y + 26, 1);
  tft.drawString("Humidity " + String(weather.humidity) + "%  Wind " + String(weather.wind) + " km/h", 128, INFO_Y + 40, 1);

  const CellRect box = weatherDaysBox();
  for (int i = 0; i < weather.dayCount; ++i) {
    const CellRect c = gridCellIn(i, 4, 1, box.x, box.y, box.w, box.h, 5);
    const ForecastDay &day = weather.days[i];
    tft.fillRoundRect(c.x, c.y, c.w, c.h, 8, T_CARD_ALT);
    const int cx = c.x + c.w / 2;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_CARD_ALT);
    tft.drawString(day.day, cx, c.y + 12, 2);
    drawWeatherIcon(day.icon, cx - 14, c.y + 24, 28, T_CARD_ALT);
    tft.setTextColor(T_TEXT, T_CARD_ALT);
    tft.drawString(String(day.hi) + "/" + String(day.lo), cx, c.y + 66, 2);
    if (day.rain >= 0) {
      tft.setTextColor(rgb565(0x64d2ff), T_CARD_ALT);
      tft.drawString(String(day.rain) + "%", cx, c.y + 86, 2);
    }
  }
  drawInfoFooter("Open-Meteo", weather.date, weather.time);
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
    default: break;
  }
}

void touchInfoV2(uint16_t x, uint16_t y) {
  const int tab = gridIndexIn(x, y, INFO_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
  if (tab >= 0 && tab != infoTab) {
    infoTab = tab;
    fullRedrawRequested = true;
  }
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
  tft.drawString(label, x + 28, NOTIFY_CHIP_Y + 11, 2);
  tft.fillCircle(x + 137, NOTIFY_CHIP_Y + 11, 4, up ? T_UP : T_DOWN);
}

void drawNotifyRow(int index) {
  const Notification &n = notifyStore.at(index);
  const int y = NOTIFY_LIST_Y + index * NOTIFY_ROW_H;
  const uint16_t bg = n.unread ? T_TILE : T_SURFACE;
  tft.fillRoundRect(8, y, UI_SCREEN_W - 16, NOTIFY_ROW_H - 2, 8, bg);
  tft.drawRoundRect(14, y + 9, 22, 22, 5, T_BORDER);
  drawIcon(tft, n.source == NotifySource::Phone ? Icon::Phone : Icon::Monitor, 18, y + 13, 14, T_INK, bg);
  drawViText(tft, NOTIFY_TEXT_X, y + 3, fitViText(notifyTitle(n), NOTIFY_TEXT_W), T_INK, bg);
  const String body = settings.hideBody ? String("Content hidden") : n.body;
  drawViText(tft, NOTIFY_TEXT_X, y + 21, fitViText(body, NOTIFY_TEXT_W), T_MUTED, bg);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, bg);
  tft.drawString(agoText(n.receivedMs), UI_SCREEN_W - 14, y + 20, 1);
}

void drawNotifyHideToggle() {
  const uint16_t bg = settings.hideBody ? T_ACCENT : T_CARD;
  tft.fillRect(STATUS_X, 0, UI_SCREEN_W - STATUS_X, UI_HEADER_H - 1, T_BG);
  tft.fillRoundRect(STATUS_X + 2, 4, UI_SCREEN_W - STATUS_X - 8, 24, 6, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(settings.hideBody ? T_ON_ACCENT : T_TEXT, bg);
  tft.drawString(settings.hideBody ? "HIDE: ON" : "HIDE: OFF", (STATUS_X + UI_SCREEN_W - 6) / 2, 16, 2);
}

void drawNotifyV2() {
  drawHeaderV2("NOTIFICATIONS", true);
  drawNotifyHideToggle();
  drawLinkChip(8, "PHONE", Icon::Phone, phoneLinkUp);
  drawLinkChip(163, "PC", Icon::Wifi, pcLinkUp);
  if (notifyStore.size() == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_SURFACE);
    tft.drawString("No notifications yet", UI_SCREEN_W / 2, 150, 2);
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
  drawViText(tft, 30, 3, fitViText(notifyTitle(n), 280), T_INK, T_ALERT_BG);
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
  tft.drawString("Coming in phase 2", UI_SCREEN_W / 2, 136, 2);
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
  tft.drawString(label, x + 10, y + SET_ROW_H / 2, 2);
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
    tft.drawString(labels[i], c.x + c.w / 2, c.y + c.h / 2 + 1, 1);
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
  tft.drawString(value, SET_ROW_X + SET_ROW_W - 10, setRowY(row) + SET_ROW_H / 2, 2);
}

void drawActionRow(int row, const char *label, uint16_t color, int x = SET_ROW_X, int w = SET_ROW_W) {
  drawSetRow(row, nullptr, x, w);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, T_CARD_ALT);
  tft.drawString(label, x + w / 2, setRowY(row) + SET_ROW_H / 2, 2);
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
  tft.drawString(hourText(hour), x + QUIET_BOX_W / 2, y + (SET_ROW_H - 8) / 2 + 1, 2);
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
  tft.drawString(NOTIFY_APPS[i], c.x + 17, c.y + c.h / 2, 1);
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
  drawValueRow(3, "PC (USB)", usbLive() ? "Online" : "Offline", usbLive() ? T_UP : T_DOWN);
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
      tft.drawString("Standby: clock screen after no touch. Tap to go back.", SET_ROW_X + 2, setRowY(4) + 8, 1);
      tft.drawString("Screen off: backlight off; tap to wake.", SET_ROW_X + 2, setRowY(4) + 20, 1);
      break;
    case SET_NOTIFY:
      drawSwitchRow(0, "iPhone", settings.phoneOn, SET_ROW_X, SET_HALF_W);
      drawSwitchRow(0, "PC", settings.pcOn, SET_RIGHT_X, SET_HALF_W);
      drawSwitchRow(1, "Hide content", settings.hideBody);
      drawSwitchRow(2, "Quiet", settings.quietOn);
      drawQuietBox(QUIET_START_X, settings.quietStart, settings.quietOn);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(T_MUTED, T_CARD_ALT);
      tft.drawString("-", (QUIET_START_X + QUIET_BOX_W + QUIET_END_X) / 2, setRowY(2) + SET_ROW_H / 2, 2);
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
  tft.drawString(line1, UI_SCREEN_W / 2, 130, 2);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString(line2, UI_SCREEN_W / 2, 154, 1);
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

// Text with a 1 px drop shadow, drawn straight onto the picture.
void drawShadowText(const String &text, int x, int y, uint8_t font, uint16_t color) {
  tft.setTextColor(TFT_BLACK);
  tft.drawString(text, x + 1, y + 2, font);
  tft.setTextColor(color);
  tft.drawString(text, x, y, font);
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
  }

  if (weather.has) {
    const String temp = String(lroundf(weather.temp));
    const int tempX = STANDBY_TEXT_R - 8;  // room for the degree sign
    drawShadowText(temp, tempX, 188, 4, T_TEXT);
    tft.drawCircle(STANDBY_TEXT_R - 3, 192, 3, T_TEXT);
    const int iconX = tempX - tft.textWidth(temp, 4) - 30;
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
    case Screen::Claude: drawPlaceholderV2("CLAUDE CODE"); break;
    case Screen::AiTools: drawPlaceholderV2("AI TOOLS"); break;
    case Screen::Standby: drawStandbyV2(); break;
    case Screen::Notify: drawNotifyV2(); break;
    case Screen::Settings: drawSettingsV2(); break;
  }
  drawHeaderStatusV2();
  // A full redraw (e.g. navigating) must not leave an invisible banner that still eats header taps.
  if (bannerActive) drawBannerV2();
}

void uiRenderDynamicV2() {
  if (bannerActive && static_cast<int32_t>(bannerUntil - millis()) <= 0) {
    bannerActive = false;
    fullRedrawRequested = true;  // repaint what the banner covered
    return;
  }
  drawHeaderStatusV2();
  if (screen == Screen::Monitor) drawMonitorV2Dynamic();
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
  if (screen == Screen::Standby) {
    uiShow(Screen::Home);
    waitForRelease();
    return;
  }
  if (screen != Screen::Home && y < UI_HEADER_H && x < HOME_BTN_W) {
    uiShow(Screen::Home);
    return;
  }
  switch (screen) {
    case Screen::Home: touchHomeV2(x, y); break;
    case Screen::Controls: touchControlsV2(x, y); break;
    case Screen::Notify: touchNotifyV2(x, y); break;
    case Screen::Info: touchInfoV2(x, y); break;
    case Screen::Settings: touchSettingsV2(x, y); break;
    default: break;
  }
}

// Horizontal swipe: next (+1) or previous (-1) tab / page inside the current app.
void uiSwipeV2(int step) {
  switch (screen) {
    case Screen::Info: infoTab = (infoTab + INFO_TABS + step) % INFO_TABS; break;
    case Screen::Settings: settingsTab = (settingsTab + SET_TABS + step) % SET_TABS; break;
    case Screen::Controls: controlsPage = (controlsPage + MACRO_PAGES + step) % MACRO_PAGES; break;
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
