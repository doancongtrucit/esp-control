// UI v2 (Home grid design). Included into main.cpp's anonymous namespace so the
// screens share the deck state (stats, info, macros, notifyStore) without
// exporting it. Only built with DECK_UI_V2.
#pragma once

enum class Screen : uint8_t { Home, Monitor, Controls, Info, Claude, AiTools, AiGpu, Notify, Settings };
Screen screen = Screen::Home;
uint8_t controlsPage = 0;
uint32_t drawnNotifyVersion = 0;
bool hideNotifyText = false;
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
    {"AI GPU", Icon::Chip, Screen::AiGpu, 0x64d2ff, 0x2b8fd8},
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
  if (bannerActive || screen == Screen::Notify || screen == Screen::Home) return;
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

void drawHomeV2() {
  drawHomeStatusBar();
  for (int i = 0; i < 8; ++i) drawHomeApp(i);
  drawHomeFooter();
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

void drawInfoTabs() {
  tft.fillRect(0, TAB_Y, UI_SCREEN_W, TAB_H, T_BG);
  for (int i = 0; i < INFO_TABS; ++i) {
    const CellRect c = gridCellIn(i, INFO_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
    const bool active = i == infoTab;
    const uint16_t bg = active ? T_CARD : T_BG;
    const uint16_t fg = active ? T_TEXT : T_MUTED;
    tft.fillRoundRect(c.x, c.y, c.w, c.h + 4, 6, bg);  // +4: square bottom joins the content
    if (active) tft.fillRect(c.x, c.y + c.h - 2, c.w, 2, rgb565(INFO_TAB_SPECS[i].line));
    drawIcon(tft, INFO_TAB_SPECS[i].icon, c.x + (c.w - 12) / 2, c.y + 2, 12, fg, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(INFO_TAB_SPECS[i].label, c.x + c.w / 2, c.y + 21, 1);
  }
}

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

void drawInfoFooter(const String &left, const char *updated) {
  constexpr int footY = UI_INFO_FOOTER_Y;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  tft.drawString(left, 10, footY, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String("Updated ") + updated, UI_SCREEN_W - 10, footY, 1);
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
  drawInfoFooter("USD - 24h change", coins.time);
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
  drawInfoFooter("VPS - HOSE / HNX", stocks.time);
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
  drawInfoFooter("Open-Meteo", weather.time);
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
  const String body = hideNotifyText ? String("Content hidden") : n.body;
  drawViText(tft, NOTIFY_TEXT_X, y + 21, fitViText(body, NOTIFY_TEXT_W), T_MUTED, bg);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, bg);
  tft.drawString(agoText(n.receivedMs), UI_SCREEN_W - 14, y + 20, 1);
}

void drawNotifyHideToggle() {
  const uint16_t bg = hideNotifyText ? T_ACCENT : T_CARD;
  tft.fillRect(STATUS_X, 0, UI_SCREEN_W - STATUS_X, UI_HEADER_H - 1, T_BG);
  tft.fillRoundRect(STATUS_X + 2, 4, UI_SCREEN_W - STATUS_X - 8, 24, 6, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(hideNotifyText ? T_ON_ACCENT : T_TEXT, bg);
  tft.drawString(hideNotifyText ? "HIDE: ON" : "HIDE: OFF", (STATUS_X + UI_SCREEN_W - 6) / 2, 16, 2);
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
    hideNotifyText = !hideNotifyText;
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
  const String body = hideNotifyText ? String("Content hidden") : n.body;
  drawViText(tft, 30, 20, fitViText(body, 280), T_MUTED, T_ALERT_BG);
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
    case Screen::AiGpu: drawPlaceholderV2("AI GPU"); break;
    case Screen::Notify: drawNotifyV2(); break;
    case Screen::Settings: drawPlaceholderV2("SETTINGS"); break;
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
  if (screen == Screen::Home) {
    if (!bannerActive) drawHomeStatusBar();  // the banner covers the status bar
    drawHomeFooter();
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
  if (screen != Screen::Home && y < UI_HEADER_H && x < HOME_BTN_W) {
    uiShow(Screen::Home);
    return;
  }
  switch (screen) {
    case Screen::Home: touchHomeV2(x, y); break;
    case Screen::Controls: touchControlsV2(x, y); break;
    case Screen::Notify: touchNotifyV2(x, y); break;
    case Screen::Info: touchInfoV2(x, y); break;
    default: break;
  }
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
