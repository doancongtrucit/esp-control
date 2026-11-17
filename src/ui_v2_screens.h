// UI v2 (Home grid design). Included into main.cpp's anonymous namespace so the
// screens share the deck state (stats, info, macros, notifyStore) without
// exporting it. Only built with DECK_UI_V2.
#pragma once

enum class Screen : uint8_t { Home, Monitor, Controls, Info, Claude, AiTools, Standby, Notify, Settings, Aram, Break, HomeCtl, Timer, Ring, Hud, HudSettings };
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

constexpr int HOME_PAGES = 2;
uint8_t homePage = 0;
constexpr HomeApp HOME_APPS[8 * HOME_PAGES] = {
    {"Monitor", Icon::Monitor, Screen::Monitor, 0x30d158, 0x1a9e48},
    {"Controls", Icon::Controls, Screen::Controls, 0x0a84ff, 0x0060df},
    {"Info", Icon::Pie, Screen::Info, 0xffd60a, 0xf5a300},
    {"Claude", Icon::Sparkle, Screen::Claude, 0xff9f0a, 0xff6b1a},
    {"Home", Icon::Home, Screen::HomeCtl, 0x63e6be, 0x12b886},
    {"Standby", Icon::Clock, Screen::Standby, 0x64d2ff, 0x2b8fd8},
    {"Notify", Icon::Bell, Screen::Notify, 0xff375f, 0xe0244a},
    {"Settings", Icon::Gear, Screen::Settings, 0x8e8e93, 0x5c5c63},
    // page 2
    {"Timer", Icon::Timer, Screen::Timer, 0xff8787, 0xe03131},
    {"Brief", Icon::Sun, Screen::Home, 0xffd43b, 0xf08c00},  // an action: the bridge speaks the morning brief
    {"HUD", Icon::Globe, Screen::Hud, 0x0a84ff, 0x0040a0},  // VIETMAP LIVE navigation HUD over BLE
    {"", Icon::Plus, Screen::Home, 0, 0},
    {"", Icon::Plus, Screen::Home, 0, 0}, {"", Icon::Plus, Screen::Home, 0, 0}, {"", Icon::Plus, Screen::Home, 0, 0},
    {"", Icon::Plus, Screen::Home, 0, 0},
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

// The HUD screen and its settings share the GOFA BLE link; everything else drops it.
inline bool isHudContext(Screen s) { return s == Screen::Hud || s == Screen::HudSettings; }

void uiShow(Screen target) {
  // The HUD advertises a GOFA identity (no ANCS) so the GOFA app can connect;
  // leaving the HUD context restores iPhone notifications. HUD settings keep it.
  if (isHudContext(target) && !isHudContext(screen)) ancsSetGofaMode(true);
  else if (!isHudContext(target) && isHudContext(screen)) ancsSetGofaMode(false);
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
      screen == Screen::Break || screen == Screen::Ring || isHudContext(screen))
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
  for (int i = 0; i < HOME_PAGES; ++i)  // launcher page dots; swipe to change
    tft.fillCircle(150 + i * 12, STATUS_BAR_H / 2, 3, i == homePage ? T_TEXT : T_DIVIDER);
}

void drawHomeApp(int slot) {
  const CellRect c = gridCellIn(slot, 4, 2, HOME_GRID_X, HOME_GRID_Y, HOME_GRID_W, HOME_GRID_H, 0);
  const int index = homePage * 8 + slot;
  const HomeApp &app = HOME_APPS[index];
  const int ix = c.x + (c.w - APP_ICON) / 2;
  const int iy = c.y + 12;
  tft.fillRect(c.x, c.y, c.w, c.h, T_BG);
  if (app.label[0] == '\0') return;  // an empty slot
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
  const int slot = gridIndexIn(x, y, 4, 2, HOME_GRID_X, HOME_GRID_Y, HOME_GRID_W, HOME_GRID_H, 0);
  if (slot < 0) return;
  const HomeApp &app = HOME_APPS[homePage * 8 + slot];
  if (strcmp(app.label, "Brief") == 0) {
    JsonDocument doc;
    doc["type"] = "brief";
    sendFrame(doc);
    ledFlashUntil = millis() + 300;
    return;
  }
  if (app.label[0] != '\0') uiShow(app.target);
}

// ---------- Monitor ----------

constexpr int MON_ROW_Y[4] = {42, 70, 98, 126};
constexpr int MON_BAR_X = 56;
constexpr int MON_BAR_W = 126;
constexpr int MON_BOX_Y = 156;
constexpr int MON_BOX_H = 74;

String tempText(float celsius) { return isnan(celsius) ? String("N/A") : fmtValue(celsius) + "C"; }

enum MonitorTab : uint8_t { MON_PC, MON_SERVER, MON_TABS };
uint8_t monitorTab = MON_PC;

void drawMonitorTabDots() {
  for (int i = 0; i < MON_TABS; ++i)
    tft.fillCircle(196 + i * 10, UI_HEADER_H / 2, 3, i == monitorTab ? T_TEXT : T_DIVIDER);
}

void drawServerStatic() {
  drawHeaderV2("HOME SERVER", true);
  drawMonitorTabDots();
  const char *names[4] = {"CPU", "RAM", "DISK", "TEMP"};
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_SURFACE);
  for (int i = 0; i < 4; ++i) uiText(names[i], 10, MON_ROW_Y[i] + 8, 2);
  const char *boxes[2] = {"NETWORK", "HOME ASSISTANT"};
  for (int i = 0; i < 2; ++i) {
    const int x = 10 + i * 153;
    tft.fillRoundRect(x, MON_BOX_Y, 147, MON_BOX_H, 8, T_TILE);
    tft.setTextColor(T_MUTED, T_TILE);
    uiText(boxes[i], x + 8, MON_BOX_Y + 12, 1);
  }
}

void drawMonitorV2Static() {
  if (monitorTab == MON_SERVER) {
    drawServerStatic();
    return;
  }
  drawHeaderV2("MONITOR", true);
  drawMonitorTabDots();
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

// What each Monitor row and box shows now, so a new frame repaints only what changed (no flicker).
int monDrawnFill[4] = {-1, -1, -1, -1};  // -1: bar track not painted yet
uint16_t monDrawnColor[4] = {};
String monDrawnText[6];

void monitorForgetDrawn() {
  for (int &f : monDrawnFill) f = -1;
  for (String &t : monDrawnText) t = "";
}

void drawMonitorRow(int row, float pct, const String &aux, const String &valueText = String()) {
  const int y = MON_ROW_Y[row];
  const int fill = static_cast<int>((MON_BAR_W - 2) * clampPct(pct) / 100.0f);
  const uint16_t color = pct >= 80.0f ? T_ACCENT : T_ACCENT_2;
  if (monDrawnFill[row] < 0) {
    tft.fillRoundRect(MON_BAR_X, y + 3, MON_BAR_W, 10, 5, T_TILE);
    monDrawnFill[row] = 0;
  }
  if (fill != monDrawnFill[row] || color != monDrawnColor[row]) {
    // Paint the bar and the empty track side by side instead of erasing the whole bar first.
    const int rest = MON_BAR_W - 2 - fill;
    if (fill > 0) {
      tft.fillRect(MON_BAR_X + 1 + max(0, fill - 4), y + 4, min(fill, 4), 8, T_TILE);  // clean rounded end
      tft.fillRoundRect(MON_BAR_X + 1, y + 4, fill, 8, 4, color);
    }
    if (rest > 0) tft.fillRect(MON_BAR_X + 1 + fill, y + 4, rest, 8, T_TILE);
    monDrawnFill[row] = fill;
    monDrawnColor[row] = color;
  }
  const String text = (valueText.length() ? valueText : fmtValue(pct) + "%") + "|" + aux;
  if (text == monDrawnText[row]) return;
  monDrawnText[row] = text;
  tft.fillRect(MON_BAR_X + MON_BAR_W + 4, y - 2, UI_SCREEN_W - MON_BAR_X - MON_BAR_W - 4, 20, T_SURFACE);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_INK, T_SURFACE);
  uiText(valueText.length() ? valueText : fmtValue(pct) + "%", 226, y + 8, 2);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiText(aux, UI_SCREEN_W - 8, y + 8, 2);
}

void drawMonitorBox(int index, const String &value, const String &detail) {
  const int x = 10 + index * 153;
  const String key = value + "|" + detail;
  if (key == monDrawnText[4 + index]) return;
  monDrawnText[4 + index] = key;
  tft.fillRect(x + 6, MON_BOX_Y + 22, 135, 46, T_TILE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_INK, T_TILE);
  uiText(value, x + 8, MON_BOX_Y + 32, 2);
  tft.setTextColor(T_MUTED, T_TILE);
  uiText(detail, x + 8, MON_BOX_Y + 54, 2);
}

void drawServerStatus(const String &value, uint16_t color, const String &detail) {
  const int x = 10 + 153;
  monDrawnText[5] = "";  // box 1 now shows the server status, not what drawMonitorBox painted
  tft.fillRect(x + 6, MON_BOX_Y + 22, 135, 46, T_TILE);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, T_TILE);
  uiText(value, x + 8, MON_BOX_Y + 32, 2);
  tft.setTextColor(T_MUTED, T_TILE);
  uiText(detail, x + 8, MON_BOX_Y + 54, 2);
}

void drawServerDynamic() {
  const HomeServerInfo &h = homeServer;
  const bool live = h.has && h.online;
  const float cpu = live ? h.cpu : NAN, ram = live ? h.ram : NAN, disk = live ? h.disk : NAN;
  const float temp = live ? h.temp : NAN;
  drawMonitorRow(0, cpu, isnan(h.load) || !live ? String("") : "load " + String(h.load, 2));
  drawMonitorRow(1, ram, isnan(h.battery) || !live ? String("") : "bat " + fmtValue(h.battery) + "%");
  drawMonitorRow(2, disk, "");
  drawMonitorRow(3, isnan(temp) ? NAN : temp, "", isnan(temp) ? String("--C") : fmtValue(temp) + "C");
  drawMonitorBox(0, "DN " + fmtValue(live ? h.downMbps : NAN, 1) + " Mb/s",
                 "UP " + fmtValue(live ? h.upMbps : NAN, 1) + " Mb/s");
  if (!h.has) {
    drawServerStatus("NOT SET UP", T_MUTED, "home_server in config");
  } else if (!h.online) {
    const String why = h.error == "token" ? "token rejected" : (h.error == "offline" ? "no reply" : "API error");
    drawServerStatus("OFFLINE", T_DOWN, why + " " + h.time);
  } else {
    String detail = "up " + uptimeText(h.uptimeSec);
    if (h.updates) detail += "  " + String(h.updates) + (h.updates == 1 ? " update" : " updates");
    drawServerStatus("ONLINE  " + String(h.latencyMs) + " ms", T_UP, detail);
  }
}

void drawMonitorV2Dynamic() {
  if (monitorTab == MON_SERVER) {
    drawServerDynamic();
    return;
  }
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
    {"Prices", Icon::Gold, 0xffd60a},   {"Coin", Icon::Coin, 0xff9f0a},          {"Stock", Icon::Trend, 0x30d158},
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

// Gold (thousand VND per chi) on top, Petrolimex fuel (VND per litre) below.
void drawGoldTab() {
  constexpr int colBuy = 180;
  constexpr int colSell = 250;
  constexpr int colChange = 312;
  constexpr int headY = INFO_Y;
  const bool withFuel = fuel.has && fuel.count > 0;
  const int goldRows = withFuel ? min<int>(info.goldCount, 3) : info.goldCount;
  const int rowH = withFuel ? 19 : 21;
  const int rowY0 = headY + 16;

  tft.setTextColor(T_MUTED, T_CARD);
  tft.setTextDatum(ML_DATUM);
  uiText(withFuel ? "GOLD  x1000 VND/chi" : "x1000 VND / chi", 10, headY + 8, 1);
  tft.setTextDatum(MR_DATUM);
  uiText("BUY", colBuy, headY + 8, 1);
  uiText("SELL", colSell, headY + 8, 1);
  uiText("+/-", colChange, headY + 8, 1);

  if (!info.hasGold && !withFuel) {
    drawInfoEmpty("No data yet");
    return;
  }

  for (int i = 0; i < goldRows; ++i) {
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

  if (withFuel) {
    const int fuelHead = rowY0 + goldRows * rowH + 4;
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(T_MUTED, T_CARD);
    uiText("FUEL  VND/litre  zone " + String(fuel.zone), 10, fuelHead + 6, 1);
    tft.setTextDatum(MR_DATUM);
    uiText("PRICE", colSell, fuelHead + 6, 1);
    uiText("+/-", colChange, fuelHead + 6, 1);
    constexpr int fuelRowH = 17;
    const int fuelY0 = fuelHead + 13;
    const int fit = (UI_SCREEN_H - 15 - fuelY0) / fuelRowH;
    for (int i = 0; i < min<int>(fuel.count, fit); ++i) {
      const int y = fuelY0 + i * fuelRowH;
      const uint16_t bg = (i % 2 == 0) ? T_CARD_ALT : T_CARD;
      tft.fillRect(0, y, UI_SCREEN_W, fuelRowH, bg);
      drawViText(tft, 10, y + 1, fitViText(fuel.name[i], 150), T_TEXT, bg);
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(T_TEXT, bg);
      uiTextIn(UiFont::Small, fuelPriceText(fuel.price[i]), colSell, y + fuelRowH / 2);
      const long change = fuel.change[i];
      tft.setTextColor(change > 0 ? T_UP : (change < 0 ? T_DOWN : T_MUTED), bg);
      uiTextIn(UiFont::Small, change > 0 ? "+" + fuelPriceText(change) : fuelPriceText(change), colChange,
               y + fuelRowH / 2);
    }
  }

  const int footY = withFuel ? UI_SCREEN_H - 7 : 226;
  const uint8_t footFont = withFuel ? 1 : 2;
  tft.setTextDatum(ML_DATUM);
  if (!isnan(info.xau)) {
    const String world = "World $" + String(info.xau, 1) + "/oz ";
    tft.setTextColor(T_MUTED, T_CARD);
    uiText(world, 10, footY, footFont);
    tft.setTextColor(info.xauChange > 0 ? T_UP : (info.xauChange < 0 ? T_DOWN : T_MUTED), T_CARD);
    uiText((info.xauChange > 0 ? "+" : "") + String(info.xauChange, 1), 10 + uiTextWidth(world, footFont), footY,
           footFont);
  }
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiText("Updated " + (info.goldTime.length() ? info.goldTime : fuel.time), colChange, footY, footFont);
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
  playSound(Sound::Reminder);
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

enum SettingsTab : uint8_t { SET_DISPLAY, SET_NOTIFY, SET_SOUND, SET_CONNECT, SET_DEVICE, SET_TABS };
uint8_t settingsTab = SET_DISPLAY;
constexpr InfoTabSpec SET_TAB_SPECS[SET_TABS] = {
    {"Display", Icon::Sun, 0xffd60a},
    {"Notify", Icon::Bell, 0xff375f},
    {"Sound", Icon::VolUp, 0x30d158},
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
    case SET_SOUND: {
      static const char *const VOLUME_LABELS[SOUND_VOLUMES] = {"Off", "Low", "Mid", "High"};
      drawSetRow(0, "Volume");
      drawSegments(0, VOLUME_LABELS, SOUND_VOLUMES, soundSettings.volume, false);
      drawSwitchRow(1, "Timers", soundGroupOn(SoundGroup::Timer), SET_ROW_X, SET_HALF_W);
      drawSwitchRow(1, "Claude", soundGroupOn(SoundGroup::Claude), SET_RIGHT_X, SET_HALF_W);
      drawSwitchRow(2, "Notify", soundGroupOn(SoundGroup::Notify), SET_ROW_X, SET_HALF_W);
      drawSwitchRow(2, "Remind", soundGroupOn(SoundGroup::Reminder), SET_RIGHT_X, SET_HALF_W);
      drawSwitchRow(3, "Alerts", soundGroupOn(SoundGroup::Alert), SET_ROW_X, SET_HALF_W);
      drawSwitchRow(3, "Clicks", soundGroupOn(SoundGroup::Click), SET_RIGHT_X, SET_HALF_W);
      drawActionRow(4, "Tap: test sound", T_TEXT, SET_ROW_X, SET_HALF_W);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(T_MUTED, T_CARD);
      uiText("Quiet hours mute all", SET_RIGHT_X + 4, setRowY(4) + 9, 1);
      uiText("but timers and alarms.", SET_RIGHT_X + 4, setRowY(4) + 23, 1);
      break;
    }
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
    case SET_SOUND: {
      changed = false;
      if (row == 0 && segmentAt(x, SOUND_VOLUMES) >= 0) {
        soundSettings.volume = segmentAt(x, SOUND_VOLUMES);
        soundPlay(Sound::Test, soundSettings.volume);  // a preview at the new volume
      } else if (row >= 1 && row <= 3) {
        static const SoundGroup GROUPS[3][2] = {{SoundGroup::Timer, SoundGroup::Claude},
                                                {SoundGroup::Notify, SoundGroup::Reminder},
                                                {SoundGroup::Alert, SoundGroup::Click}};
        soundSettings.groups ^= 1u << static_cast<uint8_t>(GROUPS[row - 1][right ? 1 : 0]);
      } else if (row == 4 && !right) {
        soundPlay(Sound::Test, soundSettings.volume);
        waitForRelease();
        return;
      } else {
        return;
      }
      saveSoundSettings();
      fullRedrawRequested = true;
      waitForRelease();
      return;
    }
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
  if (newAsk) {
    playSound(Sound::ClaudeAsk);
  } else {
    for (uint8_t i = 0; i < next.count; ++i) {
      for (uint8_t j = 0; j < claude.count; ++j) {
        if (next.rows[i].name != claude.rows[j].name) continue;
        if (claude.rows[j].state == ClaudeRunState::Running && next.rows[i].state == ClaudeRunState::Done)
          playSound(Sound::ClaudeDone);
        else if (claude.rows[j].fails < CLAUDE_FAIL_BADGE && next.rows[i].fails >= CLAUDE_FAIL_BADGE)
          playSound(Sound::ClaudeFail);
      }
    }
  }
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

// ---------- Home: Home Assistant device tiles and the air conditioner card ----------

constexpr uint8_t HA_PER_PAGE = 6;
uint8_t haPage = 0;
bool haAcOpen = false;
constexpr uint16_t T_HA_ON = 0x1B47;  // #1a6a3a, a tile whose device is on

uint8_t haPages() { return max<uint8_t>(1, (haState.count + HA_PER_PAGE - 1) / HA_PER_PAGE); }

void drawHaTile(int slot) {
  const CellRect c = gridCell(slot, 3, 2, UI_HEADER_H);
  const int index = haPage * HA_PER_PAGE + slot;
  tft.fillRect(c.x, c.y, c.w, c.h, T_SURFACE);
  if (index >= haState.count) return;
  const HaTile &t = haState.tiles[index];
  const bool on = t.on == 1;
  const uint16_t bg = on ? T_HA_ON : T_CARD;
  tft.fillRoundRect(c.x + 3, c.y + 3, c.w - 6, c.h - 6, 10, bg);
  drawIcon(tft, t.icon, c.x + 12, c.y + 12, 26, on ? T_TEXT : T_MUTED, bg);
  if (t.kind == HaKind::Climate) {  // "open" hint
    drawIcon(tft, Icon::ChevronRight, c.x + c.w - 24, c.y + 16, 14, T_MUTED, bg);
  } else if (t.confirm) {  // hold to confirm
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(T_ACCENT, bg);
    uiTextIn(UiFont::Tiny, "HOLD", c.x + c.w - 12, c.y + 12);
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(T_TEXT, bg);
  uiTextIn(UiFont::BodyBold, claudeFit(t.name, UiFont::BodyBold, c.w - 22), c.x + 11, c.y + c.h - 44);
  const uint16_t valueColor = t.value == "N/A" ? T_DOWN : (on ? T_UP : (t.kind == HaKind::Run ? T_ACCENT_2 : T_MUTED));
  tft.setTextColor(valueColor, bg);
  uiTextIn(UiFont::Small, claudeFit(t.value, UiFont::Small, c.w - 22), c.x + 11, c.y + c.h - 24);
}

void drawHaPageDots() {
  const uint8_t pages = haPages();
  if (pages < 2) return;
  for (int i = 0; i < pages; ++i) tft.fillCircle(150 + i * 10, UI_HEADER_H / 2, 3, i == haPage ? T_TEXT : T_DIVIDER);
}

// ---- air conditioner card ----
constexpr int AC_TOP = UI_HEADER_H + 6;
constexpr int AC_BTN = 44;
constexpr int AC_MINUS_X = 8, AC_PLUS_X = 152, AC_TEMP_CX = (AC_MINUS_X + 44 + AC_PLUS_X) / 2;
constexpr int AC_POWER_X = 206, AC_POWER_W = UI_SCREEN_W - 10 - AC_POWER_X;
constexpr int AC_ROW_H = 30;
constexpr int AC_MODE_Y = 124, AC_FAN_Y = 180;
constexpr int AC_SEG_X = 62, AC_SEG_W = UI_SCREEN_W - 10 - AC_SEG_X;

void drawAcSegments(int y, const String *items, uint8_t count, const String &active, bool enabled) {
  for (uint8_t i = 0; i < count; ++i) {
    const CellRect c = gridCellIn(i, count, 1, AC_SEG_X, y, AC_SEG_W, AC_ROW_H, 4);
    const bool on = items[i] == active;
    const uint16_t bg = on ? (enabled ? T_LINK : T_DIVIDER) : T_CARD;
    tft.fillRoundRect(c.x, c.y, c.w, c.h, 8, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(on ? T_TEXT : T_MUTED, bg);
    uiTextIn(UiFont::Small, haModeLabel(items[i]), c.x + c.w / 2, c.y + c.h / 2);
  }
}

void drawAcTemp() {
  const HaAc &a = haState.ac;
  tft.fillRect(AC_MINUS_X + AC_BTN + 2, AC_TOP, AC_PLUS_X - AC_MINUS_X - AC_BTN - 4, 80, T_SURFACE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(a.on ? T_TEXT : T_MUTED, T_SURFACE);
  const String temp = String(a.temp);
  const int half = uiTextWidth(temp, 6) / 2;
  uiText(temp, AC_TEMP_CX - 4, AC_TOP + 36, 6);
  tft.drawCircle(AC_TEMP_CX - 4 + half + 6, AC_TOP + 18, 4, a.on ? T_TEXT : T_MUTED);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiTextIn(UiFont::Tiny, "TARGET", AC_TEMP_CX, AC_TOP + 70);
}

void drawAcCard() {
  const HaAc &a = haState.ac;
  const String name = a.index < haState.count ? haState.tiles[a.index].name : String("Air Con");
  drawHeaderV2(name.c_str(), true);
  // - / + around the target temperature
  for (int i = 0; i < 2; ++i) {
    const int x = i ? AC_PLUS_X : AC_MINUS_X;
    tft.fillRoundRect(x, AC_TOP + 14, AC_BTN, AC_BTN, 10, T_CARD);
    tft.fillRect(x + 13, AC_TOP + 14 + AC_BTN / 2 - 2, AC_BTN - 26, 4, T_TEXT);
    if (i) tft.fillRect(x + AC_BTN / 2 - 2, AC_TOP + 27, 4, AC_BTN - 26, T_TEXT);
  }
  drawAcTemp();
  // power
  const uint16_t pbg = a.on ? T_UP : T_CARD;
  tft.fillRoundRect(AC_POWER_X, AC_TOP + 8, AC_POWER_W, 56, 12, pbg);
  drawIcon(tft, Icon::Power, AC_POWER_X + 12, AC_TOP + 24, 24, a.on ? T_BG : T_TEXT, pbg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(a.on ? T_BG : T_TEXT, pbg);
  uiTextIn(UiFont::BodyBold, a.on ? "ON" : "OFF", AC_POWER_X + 46, AC_TOP + 36);
  // mode and fan
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiTextIn(UiFont::Tiny, "MODE", 12, AC_MODE_Y + AC_ROW_H / 2);
  uiTextIn(UiFont::Tiny, "FAN", 12, AC_FAN_Y + AC_ROW_H / 2);
  drawAcSegments(AC_MODE_Y, a.modes, a.modeCount, a.mode, a.on);
  drawAcSegments(AC_FAN_Y, a.fans, a.fanCount, a.fan, a.on);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_MUTED, T_SURFACE);
  uiTextIn(UiFont::Tiny, "Infrared: the deck shows what was sent", UI_SCREEN_W / 2, 226);
}

void drawHaV2() {
  if (haAcOpen && haState.ac.has) {
    drawAcCard();
    return;
  }
  haAcOpen = false;
  drawHeaderV2("HOME", true);
  drawHaPageDots();
  if (!haState.has) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(T_MUTED, T_SURFACE);
    uiText("Waiting for Home Assistant", UI_SCREEN_W / 2, 110, 2);
    uiTextIn(UiFont::Small, "Setup: docs/HOME_CONTROLS.md", UI_SCREEN_W / 2, 134);
    return;
  }
  if (haPage >= haPages()) haPage = 0;
  for (int slot = 0; slot < HA_PER_PAGE; ++slot) drawHaTile(slot);
}

void touchAcCard(uint16_t x, uint16_t y) {
  HaAc &a = haState.ac;
  JsonDocument doc;
  doc["type"] = "ha_ac";
  if (y >= AC_TOP + 14 && y <= AC_TOP + 14 + AC_BTN && (x < AC_MINUS_X + AC_BTN || (x >= AC_PLUS_X && x < AC_PLUS_X + AC_BTN))) {
    const int d = x >= AC_PLUS_X ? 1 : -1;
    const int next = constrain(a.temp + d, a.minTemp, a.maxTemp);
    if (next == a.temp) return;
    a.temp = next;
    doc["op"] = "temp";
    doc["d"] = d;
    sendFrame(doc);
    drawAcTemp();
    delay(120);  // a short repeat guard while the finger stays down
    return;
  }
  if (x >= AC_POWER_X && y >= AC_TOP + 8 && y <= AC_TOP + 64) {
    a.on = !a.on;
    doc["op"] = "power";
    sendFrame(doc);
    waitForRelease();
    fullRedrawRequested = true;
    return;
  }
  const int rowY = y >= AC_MODE_Y && y < AC_MODE_Y + AC_ROW_H ? AC_MODE_Y : (y >= AC_FAN_Y && y < AC_FAN_Y + AC_ROW_H ? AC_FAN_Y : -1);
  if (rowY < 0 || x < AC_SEG_X) return;
  const bool mode = rowY == AC_MODE_Y;
  const uint8_t count = mode ? a.modeCount : a.fanCount;
  const int i = gridIndexIn(x, y, count, 1, AC_SEG_X, rowY, AC_SEG_W, AC_ROW_H, 4);
  if (i < 0) return;
  const String choice = mode ? a.modes[i] : a.fans[i];
  if (mode) {
    a.mode = choice;
    a.on = true;  // choosing a mode turns it on
  } else {
    a.fan = choice;
  }
  doc["op"] = mode ? "mode" : "fan";
  doc["m"] = choice;
  sendFrame(doc);
  waitForRelease();
  fullRedrawRequested = true;
}

// Hold to confirm: a bar fills along the tile's bottom edge for HOLD_CONFIRM_MS.
bool haHoldTile(int slot) {
  const CellRect c = gridCell(slot, 3, 2, UI_HEADER_H);
  const uint32_t start = millis();
  int misses = 0;
  while (millis() - start < HOLD_CONFIRM_MS) {
    misses = touchHeld() ? 0 : misses + 1;
    if (misses >= 3) {
      drawHaTile(slot);
      return false;
    }
    tft.fillRect(c.x + 12, c.y + c.h - 9, (c.w - 24) * (millis() - start) / HOLD_CONFIRM_MS, 3, T_TEXT);
    delay(20);
  }
  return true;
}

void touchHaV2(uint16_t x, uint16_t y) {
  if (haAcOpen) {
    touchAcCard(x, y);
    return;
  }
  const int slot = gridIndexAt(x, y, 3, 2, UI_HEADER_H);
  const int index = haPage * HA_PER_PAGE + slot;
  if (slot < 0 || index >= haState.count) return;
  HaTile &t = haState.tiles[index];
  if (t.kind == HaKind::Climate && haState.ac.has) {
    haAcOpen = true;
    waitForRelease();
    fullRedrawRequested = true;
    return;
  }
  if (t.confirm && !haHoldTile(slot)) return;  // let go early: nothing happens
  JsonDocument doc;
  doc["type"] = "ha_press";
  doc["i"] = index;
  sendFrame(doc);
  ledFlashUntil = millis() + 150;
  if (t.kind == HaKind::Toggle && t.on >= 0) {  // show it at once; the bridge confirms
    t.on = t.on ? 0 : 1;
    t.value = t.on ? "ON" : "OFF";
  }
  drawHaTile(slot);
  waitForRelease();
}

void uiHaChanged() {
  if (screen == Screen::HomeCtl) fullRedrawRequested = true;
}

// ---------- Timer app: countdown, Pomodoro, alarm (runs on the deck, no PC needed) ----------

enum TimerTab : uint8_t { TM_TIMER, TM_POMO, TM_ALARM, TM_TABS };
uint8_t timerTab = TM_TIMER;
constexpr InfoTabSpec TIMER_TAB_SPECS[TM_TABS] = {
    {"Timer", Icon::Timer, 0xff9f0a}, {"Pomodoro", Icon::Activity, 0xff375f}, {"Alarm", Icon::Bell, 0x0a84ff}};

// Countdown
uint32_t cdTotalS = 5 * 60, cdLeftS = 5 * 60, cdEndMs = 0;
bool cdRunning = false;
// Pomodoro
PomoPhase pomoPhase = PomoPhase::Focus;
uint8_t pomoDone = 0;
uint32_t pomoLeftS = POMO_FOCUS_S, pomoEndMs = 0;
bool pomoRunning = false;
// Alarm (kept in flash)
bool alarmOn = false;
uint8_t alarmH = 7, alarmM = 0;
AlarmRepeat alarmRepeat = AlarmRepeat::Daily;
long alarmFiredKey = -1;
bool alarmLoaded = false;
// Ringing
enum class RingKind : uint8_t { Countdown, Pomo, Alarm };
RingKind ringKind = RingKind::Countdown;
Screen ringReturnTo = Screen::Home;
uint32_t ringSinceMs = 0;
int timerDrawnSecond = -1;
constexpr uint32_t RING_MAX_MS = 3 * 60 * 1000UL;

constexpr int TM_BIG_Y = 104;
constexpr uint32_t CD_PRESETS[6] = {1, 3, 5, 10, 15, 30};

uint32_t secondsLeft(bool running, uint32_t endMs, uint32_t leftS) {
  if (!running) return leftS;
  const int32_t ms = static_cast<int32_t>(endMs - millis());
  return ms <= 0 ? 0 : (static_cast<uint32_t>(ms) + 999) / 1000;
}

bool timerRinging() { return screen == Screen::Ring; }

void loadAlarm() {
  if (alarmLoaded) return;
  alarmLoaded = true;
  alarmOn = prefs.getBool("alOn", false);
  alarmH = prefs.getUChar("alH", 7);
  alarmM = prefs.getUChar("alM", 0);
  alarmRepeat = static_cast<AlarmRepeat>(prefs.getUChar("alRep", 1));
}

void saveAlarm() {
  prefs.putBool("alOn", alarmOn);
  prefs.putUChar("alH", alarmH);
  prefs.putUChar("alM", alarmM);
  prefs.putUChar("alRep", static_cast<uint8_t>(alarmRepeat));
}

void drawTimerButton(const CellRect &c, const String &label, uint16_t bg, uint16_t fg = T_TEXT) {
  tft.fillRoundRect(c.x, c.y, c.w, c.h, 10, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  uiTextIn(UiFont::BodyBold, label, c.x + c.w / 2, c.y + c.h / 2);
}

CellRect timerButtonCell(int i, int n) { return gridCellIn(i, n, 1, 8, 188, UI_SCREEN_W - 16, 44, 6); }

// The big mm:ss and its progress bar; repainted every second while running.
void drawTimerBig() {
  uint32_t left = 0, total = 1;
  uint16_t color = T_TEXT;
  if (timerTab == TM_TIMER) {
    left = secondsLeft(cdRunning, cdEndMs, cdLeftS);
    total = cdTotalS;
    color = cdRunning ? T_ACCENT : T_TEXT;
  } else if (timerTab == TM_POMO) {
    left = secondsLeft(pomoRunning, pomoEndMs, pomoLeftS);
    total = pomoSeconds(pomoPhase);
    color = pomoPhase == PomoPhase::Focus ? T_ACCENT : T_UP;
    if (!pomoRunning) color = T_TEXT;
  } else {
    return;
  }
  tft.fillRect(20, TM_BIG_Y - 23, UI_SCREEN_W - 40, 51, T_CARD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, T_CARD);
  uiText(timerText(left), UI_SCREEN_W / 2, TM_BIG_Y, 6);
  const int barW = UI_SCREEN_W - 80;
  const int done = total ? barW * (total - min(left, total)) / total : 0;
  tft.fillRoundRect(40, TM_BIG_Y + 34, barW, 6, 3, T_LINE);
  if (done > 0) tft.fillRoundRect(40, TM_BIG_Y + 34, max(6, done), 6, 3, color);
  timerDrawnSecond = static_cast<int>(left);
}

void drawCountdownTab() {
  drawTimerBig();
  for (int i = 0; i < 6; ++i) {
    const CellRect c = gridCellIn(i, 6, 1, 8, 150, UI_SCREEN_W - 16, 28, 4);
    const bool on = cdTotalS == CD_PRESETS[i] * 60;
    tft.fillRoundRect(c.x, c.y, c.w, c.h, 8, on ? T_LINK : T_CARD_ALT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(on ? T_TEXT : T_MUTED, on ? T_LINK : T_CARD_ALT);
    uiTextIn(UiFont::Small, String(CD_PRESETS[i]) + "m", c.x + c.w / 2, c.y + c.h / 2);
  }
  drawTimerButton(timerButtonCell(0, 4), "-1m", T_CARD_ALT);
  drawTimerButton(timerButtonCell(1, 4), cdRunning ? "PAUSE" : "START", cdRunning ? T_ACCENT : T_UP, T_BG);
  drawTimerButton(timerButtonCell(2, 4), "+1m", T_CARD_ALT);
  drawTimerButton(timerButtonCell(3, 4), "RESET", T_CARD_ALT);
}

void drawPomoTab() {
  const char *phase = pomoPhase == PomoPhase::Focus ? "FOCUS" : (pomoPhase == PomoPhase::ShortBreak ? "BREAK" : "LONG BREAK");
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(pomoPhase == PomoPhase::Focus ? T_ACCENT : T_UP, T_CARD);
  uiTextIn(UiFont::Small, phase, 14, INFO_Y + 9);
  for (int i = 0; i < POMO_ROUNDS; ++i) {  // finished focus rounds
    const int x = UI_SCREEN_W - 20 - (POMO_ROUNDS - 1 - i) * 14;
    if (i < pomoDone) tft.fillCircle(x, INFO_Y + 9, 4, T_ACCENT);
    else tft.drawCircle(x, INFO_Y + 9, 4, T_MUTED);
  }
  drawTimerBig();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_MUTED, T_CARD);
  uiTextIn(UiFont::Small, "25 min focus  |  5 min break  |  15 min after 4", UI_SCREEN_W / 2, 164);
  drawTimerButton(timerButtonCell(0, 3), pomoRunning ? "PAUSE" : "START", pomoRunning ? T_ACCENT : T_UP, T_BG);
  drawTimerButton(timerButtonCell(1, 3), "SKIP", T_CARD_ALT);
  drawTimerButton(timerButtonCell(2, 3), "RESET", T_CARD_ALT);
}

constexpr int AL_SWITCH_Y = INFO_Y + 4;
constexpr int AL_BIG_Y = 124;
constexpr int AL_ADJ_Y = 154;
constexpr int AL_REP_Y = 192;

void drawAlarmTab() {
  loadAlarm();
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiTextIn(UiFont::BodyBold, "ALARM", 14, AL_SWITCH_Y + 12);
  tft.setTextColor(T_MUTED, T_CARD);
  uiTextIn(UiFont::Small, alarmOn ? "rings on the deck" : "off", 80, AL_SWITCH_Y + 12);
  const int sx = UI_SCREEN_W - 58, sy = AL_SWITCH_Y + 2;
  tft.fillRoundRect(sx, sy, 44, 22, 11, alarmOn ? T_UP : T_LINE);
  tft.fillCircle(alarmOn ? sx + 33 : sx + 11, sy + 11, 8, T_TEXT);
  char text[6];
  snprintf(text, sizeof(text), "%02u:%02u", alarmH, alarmM);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(alarmOn ? T_TEXT : T_MUTED, T_CARD);
  uiText(text, UI_SCREEN_W / 2, AL_BIG_Y, 6);
  static const char *const ADJ[4] = {"H -", "H +", "M -", "M +"};
  for (int i = 0; i < 4; ++i) drawTimerButton(gridCellIn(i, 4, 1, 8, AL_ADJ_Y, UI_SCREEN_W - 16, 30, 6), ADJ[i], T_CARD_ALT);
  static const char *const REP[3] = {"Once", "Daily", "Mon-Fri"};
  for (int i = 0; i < 3; ++i) {
    const CellRect c = gridCellIn(i, 3, 1, 8, AL_REP_Y, UI_SCREEN_W - 16, 40, 6);
    const bool on = static_cast<int>(alarmRepeat) == i;
    drawTimerButton(c, REP[i], on ? T_LINK : T_CARD_ALT, on ? T_TEXT : T_MUTED);
  }
}

void drawTimerV2() {
  drawHeaderV2("TIMER", true);
  drawTabStrip(TIMER_TAB_SPECS, TM_TABS, timerTab);
  tft.fillRect(0, INFO_Y, UI_SCREEN_W, UI_SCREEN_H - INFO_Y, T_CARD);
  if (timerTab == TM_TIMER) drawCountdownTab();
  else if (timerTab == TM_POMO) drawPomoTab();
  else drawAlarmTab();
}

void startCountdown() {
  if (cdLeftS == 0) cdLeftS = cdTotalS;
  cdEndMs = millis() + cdLeftS * 1000UL;
  cdRunning = true;
}

void touchTimerV2(uint16_t x, uint16_t y) {
  const int tab = gridIndexIn(x, y, TM_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
  if (tab >= 0) {
    if (tab != timerTab) {
      timerTab = tab;
      fullRedrawRequested = true;
    }
    return;
  }
  if (timerTab == TM_TIMER) {
    if (y >= 150 && y < 178) {  // presets
      const int i = gridIndexIn(x, y, 6, 1, 8, 150, UI_SCREEN_W - 16, 28, 4);
      if (i < 0) return;
      cdTotalS = cdLeftS = CD_PRESETS[i] * 60;
      cdRunning = false;
    } else if (y >= 188) {
      const int b = gridIndexIn(x, y, 4, 1, 8, 188, UI_SCREEN_W - 16, 44, 6);
      const uint32_t left = secondsLeft(cdRunning, cdEndMs, cdLeftS);
      if (b == 0 || b == 2) {  // -1 min / +1 min, also while running
        const int32_t next = static_cast<int32_t>(left) + (b == 2 ? 60 : -60);
        cdLeftS = static_cast<uint32_t>(constrain(next, 0, 24 * 3600 - 1));
        if (cdLeftS > cdTotalS) cdTotalS = cdLeftS;
        if (cdRunning) cdEndMs = millis() + cdLeftS * 1000UL;
        if (cdLeftS == 0) cdRunning = false;
      } else if (b == 1) {
        if (cdRunning) {
          cdLeftS = left;
          cdRunning = false;
        } else {
          startCountdown();
        }
      } else if (b == 3) {
        cdRunning = false;
        cdLeftS = cdTotalS;
      }
    } else {
      return;
    }
  } else if (timerTab == TM_POMO) {
    if (y < 188) return;
    const int b = gridIndexIn(x, y, 3, 1, 8, 188, UI_SCREEN_W - 16, 44, 6);
    if (b == 0) {
      if (pomoRunning) {
        pomoLeftS = secondsLeft(true, pomoEndMs, 0);
        pomoRunning = false;
      } else {
        pomoEndMs = millis() + pomoLeftS * 1000UL;
        pomoRunning = true;
      }
    } else if (b == 1) {
      pomoPhase = pomoNext(pomoPhase, pomoDone);
      pomoLeftS = pomoSeconds(pomoPhase);
      if (pomoRunning) pomoEndMs = millis() + pomoLeftS * 1000UL;
    } else if (b == 2) {
      pomoRunning = false;
      pomoPhase = PomoPhase::Focus;
      pomoDone = 0;
      pomoLeftS = POMO_FOCUS_S;
    }
  } else {
    loadAlarm();
    if (y >= AL_SWITCH_Y && y < AL_SWITCH_Y + 28 && x > UI_SCREEN_W - 80) {
      alarmOn = !alarmOn;
    } else if (y >= AL_ADJ_Y && y < AL_ADJ_Y + 30) {
      const int b = gridIndexIn(x, y, 4, 1, 8, AL_ADJ_Y, UI_SCREEN_W - 16, 30, 6);
      if (b == 0) alarmH = (alarmH + 23) % 24;
      if (b == 1) alarmH = (alarmH + 1) % 24;
      if (b == 2) alarmM = (alarmM + 55) % 60;
      if (b == 3) alarmM = (alarmM + 5) % 60;
      if (b >= 0) alarmOn = true;
    } else if (y >= AL_REP_Y && y < AL_REP_Y + 40) {
      const int b = gridIndexIn(x, y, 3, 1, 8, AL_REP_Y, UI_SCREEN_W - 16, 40, 6);
      if (b < 0) return;
      alarmRepeat = static_cast<AlarmRepeat>(b);
    } else {
      return;
    }
    saveAlarm();
    alarmFiredKey = -1;
  }
  ledFlashUntil = millis() + 120;
  fullRedrawRequested = true;
  waitForRelease();
}

// ---- ringing ----

void ring(RingKind kind) {
  if (kind == RingKind::Alarm) playSound(Sound::Alarm);
  else if (kind == RingKind::Pomo) playSound(pomoPhase == PomoPhase::Focus ? Sound::BreakDone : Sound::FocusDone);
  else playSound(Sound::TimerDone);
  ringKind = kind;
  if (screen != Screen::Ring) ringReturnTo = screen == Screen::Standby ? Screen::Home : screen;
  ringSinceMs = millis();
  bannerActive = false;
  noteActivity();
  uiShow(Screen::Ring);
}

void drawRingV2() {
  tft.fillScreen(T_BG);
  String title = "TIME'S UP", detail = timerText(cdTotalS) + " timer", left = "+5 MIN", right = "DONE";
  if (ringKind == RingKind::Pomo) {
    const bool focusNext = pomoPhase == PomoPhase::Focus;
    title = focusNext ? "BREAK OVER" : "FOCUS DONE";
    detail = focusNext ? "Ready for the next focus?" : String("Take a ") + (pomoSeconds(pomoPhase) / 60) + " min break";
    left = "STOP";
    right = focusNext ? "START FOCUS" : "START BREAK";
  } else if (ringKind == RingKind::Alarm) {
    char text[16];
    snprintf(text, sizeof(text), "%02u:%02u", alarmH, alarmM);
    title = "ALARM";
    detail = text;
    left = "SNOOZE 5";
  }
  drawIcon(tft, Icon::Bell, UI_SCREEN_W / 2 - 22, 18, 44, T_GOLD, T_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(T_TEXT, T_BG);
  uiText(title, UI_SCREEN_W / 2, 98, 4);
  tft.setTextColor(T_MUTED, T_BG);
  uiTextIn(UiFont::Body, detail, UI_SCREEN_W / 2, 134);
  drawTimerButton(gridCellIn(0, 2, 1, 16, 172, UI_SCREEN_W - 32, 50, 10), left, T_CARD);
  drawTimerButton(gridCellIn(1, 2, 1, 16, 172, UI_SCREEN_W - 32, 50, 10), right, T_UP, T_BG);
}

void endRing() {
  soundStop();
  uiShow(ringReturnTo);
}

void touchRingV2(uint16_t x, uint16_t y) {
  if (y < 172) return;
  const bool leftButton = x < UI_SCREEN_W / 2;
  if (ringKind == RingKind::Pomo) {
    if (!leftButton) {  // start the phase that is now due
      pomoLeftS = pomoSeconds(pomoPhase);
      pomoEndMs = millis() + pomoLeftS * 1000UL;
      pomoRunning = true;
    }
  } else if (leftButton) {  // +5 min / snooze: a 5 minute countdown
    cdTotalS = cdLeftS = 5 * 60;
    startCountdown();
  }
  waitForRelease();
  endRing();
}

// Every loop: counts down, rings, checks the alarm, repaints the big time.
void timerTick() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs < 200) return;
  lastMs = millis();
  loadAlarm();
  if (cdRunning && secondsLeft(true, cdEndMs, 0) == 0) {
    cdRunning = false;
    cdLeftS = cdTotalS;
    ring(RingKind::Countdown);
  }
  if (pomoRunning && secondsLeft(true, pomoEndMs, 0) == 0) {
    pomoRunning = false;
    pomoPhase = pomoNext(pomoPhase, pomoDone);
    pomoLeftS = pomoSeconds(pomoPhase);
    ring(RingKind::Pomo);
  }
  int d, m, y;
  const int hour = clockHour();
  if (alarmOn && hour >= 0 && deckDate(d, m, y)) {
    const uint32_t secs = (stats.clockSecondsAtSync + (millis() - stats.clockSyncMs) / 1000UL) % 86400UL;
    if (alarmDue(alarmOn, alarmH, alarmM, alarmRepeat, hour, (secs / 60) % 60, dayOfWeek(d, m, y),
                 y * 372L + m * 31L + d, alarmFiredKey)) {
      if (alarmRepeat == AlarmRepeat::Once) {
        alarmOn = false;
        saveAlarm();
      }
      ring(RingKind::Alarm);
    }
  }
  if (screen == Screen::Ring && millis() - ringSinceMs > RING_MAX_MS) endRing();
  if (screen == Screen::Timer && (timerTab == TM_TIMER || timerTab == TM_POMO)) {
    const bool running = timerTab == TM_TIMER ? cdRunning : pomoRunning;
    const uint32_t left = timerTab == TM_TIMER ? secondsLeft(cdRunning, cdEndMs, cdLeftS)
                                               : secondsLeft(pomoRunning, pomoEndMs, pomoLeftS);
    if (running && static_cast<int>(left) != timerDrawnSecond) drawTimerBig();
  }
}

// ---------- VIETMAP HUD ----------
// Navigation dashboard fed by the HUD BLE peripheral (hud_ble): speed gauge,
// speed-limit sign, next turn and camera distance, street name. Partial redraws
// keep it from flickering; the top-left 40x32 stays the home button.

constexpr int HUD_GCX = 76, HUD_GCY = 132, HUD_GR = 60, HUD_GW = 8, HUD_GMAX = 120;
constexpr int HUD_CARD_X = 150, HUD_CARD_Y = 42, HUD_CARD_W = 162, HUD_CARD_H = 78;  // turn card
constexpr int HUD_SIGN_CX = 184, HUD_SIGN_CY = 161, HUD_SIGN_R = 27;                 // current limit roundel
constexpr int HUD_CHIP_X = 222, HUD_CHIP_Y = 130, HUD_CHIP_W = 90, HUD_CHIP_H = 62;  // camera + upcoming-limit list
constexpr int HUD_WARN_Y = 206;   // warning pill: bottom strip (GOFA sends no weather)
constexpr uint16_t HUD_RED = 0xF800;
constexpr uint16_t HUD_AMBER = 0xFD20;

struct HudShown {
  bool valid = false;
  int speed = -2, limit = -2, cam = -2, turnDist = -2, turnIcon = -2;
  int warnId = -2, warnDist = -2, limAhead = -2, limAheadDist = -2;
  bool over = false, stale = true;
  String street, nextStreet, weather;
} hudShown;

String hudDist(int m) {
  if (m < 0) return "--";
  if (m >= 1000) return String(m / 1000.0f, 1) + " km";
  return String(m) + " m";
}

void hudDrawGauge(int speed, int limit, bool over) {
  const uint16_t col = over ? HUD_RED : T_UP;
  const int clamped = speed < 0 ? 0 : (speed > HUD_GMAX ? HUD_GMAX : speed);
  const int a0 = 40, a1 = 320;
  const int aEnd = a0 + (a1 - a0) * clamped / HUD_GMAX;
  tft.drawSmoothArc(HUD_GCX, HUD_GCY, HUD_GR, HUD_GR - HUD_GW, a0, a1, T_DIVIDER, T_BG, true);
  if (aEnd > a0 + 1) tft.drawSmoothArc(HUD_GCX, HUD_GCY, HUD_GR, HUD_GR - HUD_GW, a0, aEnd, col, T_BG, true);
  tft.fillCircle(HUD_GCX, HUD_GCY, HUD_GR - HUD_GW - 1, T_BG);  // clear inside the ring only
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(over ? HUD_RED : T_TEXT, T_BG);
  uiTextIn(UiFont::Huge, speed < 0 ? String("--") : String(speed), HUD_GCX, HUD_GCY - 3);
  tft.setTextColor(T_MUTED, T_BG);
  uiTextIn(UiFont::Small, "km/h", HUD_GCX, HUD_GCY + 32);
}

void hudDrawArrow(int icon, int cx, int cy, uint16_t c, uint16_t bg = T_BG) {
  switch (icon) {
    case 1:   // left
    case 2: { // right
      const int d = icon == 1 ? -1 : 1;
      tft.fillRect(cx - 5, cy - 2, 10, 26, c);                                // vertical shaft
      tft.fillRect(d < 0 ? cx - 16 : cx - 5, cy - 7, 21, 10, c);              // horizontal shaft
      tft.fillTriangle(cx + d * 16, cy - 16, cx + d * 16, cy + 8, cx + d * 28, cy - 4, c);  // head
      break;
    }
    case 3:   // slight left
    case 4: { // slight right
      const int d = icon == 3 ? -1 : 1;
      tft.fillRect(cx - 5, cy, 10, 24, c);
      tft.drawWideLine(cx, cy + 2, cx + d * 14, cy - 12, 10, c, bg);
      tft.fillTriangle(cx + d * 4, cy - 20, cx + d * 24, cy - 24, cx + d * 20, cy - 4, c);
      break;
    }
    case 5:  // u-turn
      tft.fillRect(cx + 7, cy - 6, 10, 30, c);
      tft.drawSmoothArc(cx, cy - 6, 16, 7, 90, 270, c, bg, false);
      tft.fillRect(cx - 16, cy - 6, 10, 11, c);
      tft.fillTriangle(cx - 23, cy + 5, cx - 1, cy + 5, cx - 12, cy + 20, c);
      break;
    case 6:  // roundabout: a ring the stem enters, exiting up-right with an arrowhead
      tft.drawSmoothArc(cx - 2, cy - 2, 14, 9, 0, 360, c, bg, false);
      tft.fillRect(cx - 5, cy + 11, 10, 13, c);              // entry stem (bottom)
      tft.fillRect(cx + 6, cy - 18, 9, 13, c);               // exit stem (up-right)
      tft.fillTriangle(cx + 1, cy - 18, cx + 19, cy - 18, cx + 10, cy - 29, c);  // arrowhead
      break;
    default:  // straight
      tft.fillRect(cx - 5, cy - 6, 10, 30, c);
      tft.fillTriangle(cx - 14, cy - 6, cx + 14, cy - 6, cx, cy - 24, c);
      break;
  }
}

void hudDrawTurn(int icon, int dist, const String &nextRoad) {
  tft.fillRect(HUD_CARD_X, HUD_CARD_Y, HUD_CARD_W, HUD_CARD_H, T_BG);
  if (icon < 0 && dist < 0) return;
  tft.fillRoundRect(HUD_CARD_X, HUD_CARD_Y, HUD_CARD_W, HUD_CARD_H, 10, T_CARD);  // rounded card
  hudDrawArrow(icon < 0 ? 0 : icon, HUD_CARD_X + 30, HUD_CARD_Y + 26, T_TEXT, T_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(T_TEXT, T_CARD);
  uiTextIn(UiFont::Large, hudDist(dist), HUD_CARD_X + 70, HUD_CARD_Y + 24);
  if (nextRoad.length()) {  // the road the maneuver turns onto
    const String fit = fitViText(nextRoad, HUD_CARD_W - 22, false);
    drawViText(tft, HUD_CARD_X + 12, HUD_CARD_Y + 58, fit, T_ACCENT, T_CARD, false);
  }
}

// Short label for a non-speed, non-camera GOFA sign id (ids from the GOFA table;
// unknown ids print as "Biển <id>" so they can be identified from real drives).
String hudWarnLabel(int id) {
  if (id == 12) return "Vào khu dân cư";
  if (id == 13) return "Ra khu dân cư";
  if (id == 14) return "Cấm vượt";
  if (id == 15) return "Hết cấm vượt";
  if (id == 17) return "Điểm dừng nghỉ";
  if (id == 18) return "Trạm thu phí";
  if (id == 21 || id == 23) return "Cấm rẽ trái";
  if (id == 22 || id == 24) return "Cấm rẽ phải";
  if (id == 25 || id == 26) return "Cấm quay đầu";
  if (id == 27) return "Cấm đi thẳng";
  return "Biển " + String(id);
}

// Category pictogram for a GOFA warning sign, ~18px, centred at (cx, cy).
// Prohibition signs (cấm) use a red ring; info signs use colour c (amber).
void hudSignGlyph(int id, int cx, int cy, uint16_t c, uint16_t bg) {
  if (id == 12 || id == 13) {              // residential: house
    tft.fillTriangle(cx - 9, cy - 1, cx + 9, cy - 1, cx, cy - 9, c);
    tft.fillRect(cx - 6, cy - 1, 12, 9, c);
    tft.fillRect(cx - 2, cy + 3, 4, 5, bg);           // door
  } else if (id == 17) {                   // rest area: cup + steam
    tft.fillRoundRect(cx - 7, cy - 2, 11, 10, 2, c);
    tft.fillRect(cx + 4, cy - 1, 4, 2, c);            // handle top
    tft.fillRect(cx + 6, cy - 1, 2, 6, c);            // handle side
    tft.fillRect(cx + 4, cy + 3, 4, 2, c);            // handle bottom
    tft.fillRect(cx - 4, cy - 8, 2, 4, c);            // steam
    tft.fillRect(cx, cy - 9, 2, 4, c);
    tft.fillRect(cx + 3, cy - 8, 2, 4, c);
  } else if (id == 18) {                   // toll: barrier gate
    tft.fillRect(cx - 8, cy - 7, 3, 15, c);           // post
    tft.fillRect(cx - 5, cy - 4, 16, 5, c);           // arm
    tft.fillRect(cx - 1, cy - 4, 2, 5, bg);           // stripe
    tft.fillRect(cx + 4, cy - 4, 2, 5, bg);           // stripe
  } else if (id == 14 || id == 15) {       // no overtaking: red ring + two cars
    tft.fillSmoothCircle(cx, cy, 9, HUD_RED, bg);
    tft.fillSmoothCircle(cx, cy, 6, bg, HUD_RED);
    tft.fillRect(cx - 3, cy - 3, 2, 6, HUD_RED);
    tft.fillRect(cx + 1, cy - 3, 2, 6, T_TEXT);
  } else if (id >= 21 && id <= 27) {       // no-turn family: red prohibition ring + slash
    tft.fillSmoothCircle(cx, cy, 9, HUD_RED, bg);
    tft.fillSmoothCircle(cx, cy, 6, bg, HUD_RED);
    tft.drawWideLine(cx - 6, cy + 6, cx + 6, cy - 6, 3, HUD_RED, bg);          // slash
  } else {                                 // generic warning triangle
    tft.fillTriangle(cx - 9, cy + 7, cx + 9, cy + 7, cx, cy - 8, c);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(bg, c);
    uiTextIn(UiFont::Small, "!", cx, cy + 2);
  }
}

void hudDrawWarn(int id, int dist) {
  tft.fillRect(0, HUD_WARN_Y - 2, UI_SCREEN_W, UI_SCREEN_H - (HUD_WARN_Y - 2), T_BG);
  if (id < 0) return;
  const bool near = dist >= 0 && dist <= 150;
  const uint16_t bg = near ? T_ALERT_BG : T_CARD;   // red-tinted pill when close
  const uint16_t c = near ? HUD_RED : HUD_AMBER;
  const int px = 10, pw = UI_SCREEN_W - 20, ph = 26, py = HUD_WARN_Y;
  tft.fillRoundRect(px, py, pw, ph, 13, bg);
  // Centre the whole group (triangle badge + label + distance) in the pill.
  const String label = hudWarnLabel(id);
  const String distStr = dist >= 0 ? hudDist(dist) : String();
  const int wl = viTextWidth(label, false);
  const int gap = 12;                               // space before the distance
  const int wd = distStr.length() ? viTextWidth(distStr, false) : 0;
  const int tri = 24;                               // triangle badge + its gap
  const int total = tri + wl + (distStr.length() ? gap + wd : 0);
  int gx = px + (pw - total) / 2;
  if (gx < px + 6) gx = px + 6;
  const int ty = py + ph / 2;
  hudSignGlyph(id, gx + 9, ty, c, bg);                                   // category pictogram
  drawViText(tft, gx + tri, py + 6, label, c, bg, false);                // label (amber / red)
  if (distStr.length())
    drawViText(tft, gx + tri + wl + gap, py + 6, distStr, T_TEXT, bg, false);  // distance (white)
}

void hudDrawSign(int limit) {
  tft.fillRect(HUD_SIGN_CX - HUD_SIGN_R - 2, HUD_SIGN_CY - HUD_SIGN_R - 2, 2 * HUD_SIGN_R + 4, 2 * HUD_SIGN_R + 4,
               T_BG);
  if (limit <= 0) return;
  tft.fillSmoothCircle(HUD_SIGN_CX, HUD_SIGN_CY, HUD_SIGN_R, HUD_RED, T_BG);
  tft.fillSmoothCircle(HUD_SIGN_CX, HUD_SIGN_CY, HUD_SIGN_R - 6, TFT_WHITE, HUD_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  uiTextIn(limit >= 100 ? UiFont::BodyBold : UiFont::Large, String(limit), HUD_SIGN_CX, HUD_SIGN_CY + 2);
}

// Clean speed-camera glyph (body + lens + top hump) for the chip list.
void hudCameraIcon(int x, int y, uint16_t c, uint16_t bg) {
  tft.fillRect(x + 4, y, 7, 4, c);                  // top hump
  tft.fillRoundRect(x, y + 3, 20, 13, 3, c);        // body
  tft.fillSmoothCircle(x + 7, y + 9, 3, bg, c);     // lens hole
  tft.fillSmoothCircle(x + 7, y + 9, 1, c, bg);     // iris
  // lightning mark -> speed-enforcement ("phạt nguội") camera, knocked out of the body
  tft.fillTriangle(x + 16, y + 4, x + 12, y + 10, x + 16, y + 9, bg);
  tft.fillTriangle(x + 15, y + 14, x + 18, y + 8, x + 14, y + 9, bg);
}

// Small proper speed-limit roundel (red ring + white + number) for the chip list.
void hudMiniSign(int cx, int cy, int limit, uint16_t bg) {
  tft.fillSmoothCircle(cx, cy, 11, HUD_RED, bg);
  tft.fillSmoothCircle(cx, cy, 7, TFT_WHITE, HUD_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  uiTextIn(UiFont::Small, String(limit), cx, cy + 1);
}

// Upcoming-events list (option A): camera distance and/or an upcoming speed-limit
// sign, each as [icon] .... [distance] with the distances right-aligned. One row
// is centred; two rows split with a hairline divider. Red icon when close (<=100 m).
void hudDrawChips(int camDist, int limKmh, int limDist) {
  tft.fillRect(HUD_CHIP_X, HUD_CHIP_Y, HUD_CHIP_W, HUD_CHIP_H, T_BG);
  const bool hasCam = camDist >= 0, hasLim = limKmh > 0;
  if (!hasCam && !hasLim) return;
  tft.fillRoundRect(HUD_CHIP_X, HUD_CHIP_Y, HUD_CHIP_W, HUD_CHIP_H, 9, T_CARD_ALT);
  const int rx = HUD_CHIP_X + HUD_CHIP_W - 8;  // right edge for distances
  auto camRow = [&](int cy) {
    const uint16_t ic = camDist <= 100 ? HUD_RED : T_MUTED;
    hudCameraIcon(HUD_CHIP_X + 10, cy - 8, ic, T_CARD_ALT);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(T_TEXT, T_CARD_ALT);
    uiTextIn(UiFont::BodyBold, hudDist(camDist), rx, cy);
  };
  auto limRow = [&](int cy) {
    hudMiniSign(HUD_CHIP_X + 21, cy, limKmh, T_CARD_ALT);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(T_TEXT, T_CARD_ALT);
    uiTextIn(UiFont::BodyBold, hudDist(limDist), rx, cy);
  };
  if (hasCam && hasLim) {
    camRow(HUD_CHIP_Y + 17);
    tft.drawFastHLine(HUD_CHIP_X + 8, HUD_CHIP_Y + 31, HUD_CHIP_W - 16, T_DIVIDER);
    limRow(HUD_CHIP_Y + 45);
  } else if (hasCam) {
    camRow(HUD_CHIP_Y + HUD_CHIP_H / 2);
  } else {
    limRow(HUD_CHIP_Y + HUD_CHIP_H / 2);
  }
}

void hudDrawTop(const String &street, bool connected, bool stale) {
  tft.fillRect(0, 0, UI_SCREEN_W, UI_HEADER_H, T_BG);
  // Back chevron (top-left 40x32 is the global home button).
  tft.setTextColor(T_MUTED, T_BG);
  tft.setTextDatum(ML_DATUM);
  uiTextIn(UiFont::Body, "<", 12, UI_HEADER_H / 2);
  // Settings gear (top-right) opens HUD Settings.
  drawIcon(tft, Icon::Gear, UI_SCREEN_W - 28, UI_HEADER_H / 2 - 9, 18, T_MUTED, T_BG);
  String title = street;
  uint16_t col = T_TEXT;
  if (title.length() == 0) {
    title = connected ? (stale ? "Chờ dữ liệu..." : "GOFA") : "Chờ kết nối app";
    col = T_MUTED;
  }
  const String fit = fitViText(title, 210, true);  // between the chevron and the gear
  drawViText(tft, 166 - viTextWidth(fit, true) / 2, 7, fit, col, T_BG, true);
  tft.drawFastHLine(10, UI_HEADER_H - 1, UI_SCREEN_W - 20, T_DIVIDER);  // hairline under header
}

void hudDrawWeather(const String &weather) {
  tft.fillRect(0, UI_SCREEN_H - 18, UI_SCREEN_W, 18, T_BG);
  if (weather.length() == 0) return;
  const String w = fitViText(weather, 180, false);
  drawViText(tft, (UI_SCREEN_W - viTextWidth(w, false)) / 2, UI_SCREEN_H - 17, w, T_MUTED, T_BG, false);
}

void drawHudV2() {
  const hud::NavState &s = gofaNav();
  const bool connected = gofaHasData() && millis() - gofaLastFrameMs() < 10000UL;
  const bool stale = !gofaHasData() || millis() - gofaLastFrameMs() > 10000UL;
  const bool over = s.speedLimit > 0 && s.speedKmh > s.speedLimit;  // red gauge: strict
  const String street(s.street.c_str());
  // Road shown by the arrow: the one being turned onto, or the current road.
  const String arrowRoad((hudSettings.nextRoad ? s.nextStreet : s.street).c_str());
  const String weather(s.weather.c_str());
  // Options that hide elements.
  const int warnId = hudSettings.showWarn ? s.warnSignId : -1;
  const int camDist = hudSettings.showCam ? s.cameraDistM : -1;
  const int limAhead = hudSettings.signDist ? s.limitAheadKmh : -1;
  const bool all = !hudShown.valid;

  if (all || street != hudShown.street || connected != !hudShown.stale /*force*/ || stale != hudShown.stale)
    hudDrawTop(street, connected, stale);
  if (all || s.speedKmh != hudShown.speed || over != hudShown.over) hudDrawGauge(s.speedKmh, s.speedLimit, over);
  if (all || s.turnIcon != hudShown.turnIcon || s.turnDistM != hudShown.turnDist || arrowRoad != hudShown.nextStreet)
    hudDrawTurn(s.turnIcon, s.turnDistM, arrowRoad);
  if (all || s.speedLimit != hudShown.limit) hudDrawSign(s.speedLimit);
  if (all || camDist != hudShown.cam || limAhead != hudShown.limAhead ||
      s.limitAheadDistM != hudShown.limAheadDist)
    hudDrawChips(camDist, limAhead, s.limitAheadDistM);
  if (all || warnId != hudShown.warnId || s.warnSignDistM != hudShown.warnDist)
    hudDrawWarn(warnId, s.warnSignDistM);
  // The bottom strip shows the warning sign when present, otherwise weather.
  if ((all || weather != hudShown.weather) && warnId < 0) hudDrawWeather(weather);

  // Audible over-speed warning, honouring the beep toggle and tolerance: beep on
  // crossing the threshold, repeat every 3 s while over. Fresh data only.
  const bool beepOver = hudSettings.overBeep && connected && s.speedLimit > 0 &&
                        s.speedKmh > s.speedLimit + (int)hudOverTolKmh();
  static uint32_t hudOverBeepMs = 0;
  static bool prevBeepOver = false;
  if (beepOver && (!prevBeepOver || millis() - hudOverBeepMs >= 3000)) {
    playSound(Sound::Alert);
    hudOverBeepMs = millis();
  }
  prevBeepOver = beepOver;

  hudShown.valid = true;
  hudShown.speed = s.speedKmh;
  hudShown.limit = s.speedLimit;
  hudShown.cam = camDist;
  hudShown.turnDist = s.turnDistM;
  hudShown.turnIcon = s.turnIcon;
  hudShown.warnId = warnId;
  hudShown.warnDist = s.warnSignDistM;
  hudShown.limAhead = limAhead;
  hudShown.limAheadDist = s.limitAheadDistM;
  hudShown.over = over;
  hudShown.stale = stale;
  hudShown.street = street;
  hudShown.nextStreet = arrowRoad;
  hudShown.weather = weather;
}

void hudForceRedraw() { hudShown.valid = false; }

// ===== HUD Settings screen =====
enum HudSetTab : uint8_t { HS_NAV, HS_DEV, HS_TABS };
uint8_t hudSettingsTab = HS_NAV;
constexpr InfoTabSpec HUD_SET_TAB_SPECS[HS_TABS] = {
    {"Dẫn đường", Icon::Globe, 0x0a84ff},
    {"Thiết bị", Icon::Sliders, 0x8e8e93},
};

// Tab strip with Vietnamese labels (drawTabStrip's uiText drops diacritics).
void drawHudTabStrip(int activeTab) {
  tft.fillRect(0, TAB_Y, UI_SCREEN_W, TAB_H, T_BG);
  for (int i = 0; i < HS_TABS; ++i) {
    const CellRect c = gridCellIn(i, HS_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
    const bool active = i == activeTab;
    const uint16_t bg = active ? T_CARD : T_BG;
    const uint16_t fg = active ? T_TEXT : T_MUTED;
    tft.fillRoundRect(c.x, c.y, c.w, c.h + 4, 6, bg);
    if (active) tft.fillRect(c.x, c.y + c.h - 2, c.w, 2, rgb565(HUD_SET_TAB_SPECS[i].line));
    drawIcon(tft, HUD_SET_TAB_SPECS[i].icon, c.x + 24, c.y + 8, 14, fg, bg);
    drawViText(tft, c.x + 44, c.y + 10, HUD_SET_TAB_SPECS[i].label, fg, bg, false);
  }
}

// Settings row with a Vietnamese label (drawViText over an empty row box).
void hudSetRowVi(int row, const char *viLabel, int x = SET_ROW_X, int w = SET_ROW_W) {
  drawSetRow(row, nullptr, x, w);
  drawViText(tft, x + 10, setRowY(row) + 8, viLabel, T_TEXT, T_CARD_ALT, false);
}
void hudSwitchRowVi(int row, const char *viLabel, bool on, int x = SET_ROW_X, int w = SET_ROW_W) {
  hudSetRowVi(row, viLabel, x, w);
  drawSwitch(x + w, row, on);
}
// Centred Vietnamese action-row label.
void hudActionRowVi(int row, const char *viLabel, uint16_t color) {
  drawSetRow(row, nullptr);
  drawViText(tft, (UI_SCREEN_W - viTextWidth(viLabel, false)) / 2, setRowY(row) + 8, viLabel, color, T_CARD_ALT, false);
}

void drawHudSettingsV2() {
  drawHeaderV2("HUD", true);
  drawHudTabStrip(hudSettingsTab);
  tft.fillRect(0, INFO_Y, UI_SCREEN_W, UI_SCREEN_H - INFO_Y, T_CARD);
  for (String &cached : settingsCache) cached = "";
  static const char *const TOL_LABELS[3] = {"+0", "+5", "+10"};
  if (hudSettingsTab == HS_NAV) {
    hudSwitchRowVi(0, "Cảnh báo vượt tốc độ", hudSettings.overBeep);
    hudSetRowVi(1, "Ngưỡng vượt (km/h)");
    drawSegments(1, TOL_LABELS, 3, hudSettings.overTol, false);
    hudSwitchRowVi(2, "Biển báo", hudSettings.showWarn, SET_ROW_X, SET_HALF_W);
    hudSwitchRowVi(2, "Camera", hudSettings.showCam, SET_RIGHT_X, SET_HALF_W);
    hudSwitchRowVi(3, "Kh.cách", hudSettings.signDist, SET_ROW_X, SET_HALF_W);
    hudSwitchRowVi(3, "Đường kế", hudSettings.nextRoad, SET_RIGHT_X, SET_HALF_W);
    drawViText(tft, SET_ROW_X + 2, setRowY(4) + 8, "Khoảng cách: báo trước khi tới biển giới hạn.", T_MUTED, T_CARD, false);
  } else {  // HS_DEV
    hudSetRowVi(0, "Độ sáng");
    drawSegments(0, nullptr, BRIGHTNESS_LEVELS, settings.brightness, true);
    drawViText(tft, SET_ROW_X + 2, setRowY(1) + 8, "Quên iPhone: xoá bond ANCS để app GOFA giữ kết nối.", T_MUTED, T_CARD, false);
    drawSetRow(2, nullptr);
    drawViText(tft, SET_ROW_X + 10, setRowY(2) + 8, "Thiết bị", T_MUTED, T_CARD_ALT, false);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(T_TEXT, T_CARD_ALT);
    uiText(ancsGofaName(), SET_ROW_X + SET_ROW_W - 10, setRowY(2) + SET_ROW_H / 2, 2);  // ASCII, renders fine
    hudActionRowVi(4, "Giữ: Quên iPhone", T_DOWN);
  }
}

void touchHudSettingsV2(uint16_t x, uint16_t y) {
  const int tab = gridIndexIn(x, y, HS_TABS, 1, TAB_X, TAB_Y, TAB_W, TAB_H, TAB_GAP);
  if (tab >= 0) {
    if (tab != hudSettingsTab) { hudSettingsTab = tab; fullRedrawRequested = true; }
    return;
  }
  if (y < SET_ROW_Y0) return;
  const int row = (y - SET_ROW_Y0) / SET_ROW_STEP;
  if (row >= SET_ROWS || (y - SET_ROW_Y0) % SET_ROW_STEP >= SET_ROW_H) return;
  const bool right = x >= SET_RIGHT_X;
  bool changed = true, saveHud = true;
  if (hudSettingsTab == HS_NAV) {
    if (row == 0) hudSettings.overBeep = !hudSettings.overBeep;
    else if (row == 1 && segmentAt(x, 3) >= 0) hudSettings.overTol = segmentAt(x, 3);
    else if (row == 2) { bool &b = right ? hudSettings.showCam : hudSettings.showWarn; b = !b; }
    else if (row == 3) { bool &b = right ? hudSettings.nextRoad : hudSettings.signDist; b = !b; }
    else changed = false;
  } else {  // HS_DEV
    saveHud = false;
    if (row == 0 && segmentAt(x, BRIGHTNESS_LEVELS) >= 0) {
      settings.brightness = segmentAt(x, BRIGHTNESS_LEVELS);
      applyBacklight();
      saveSettings();
    } else if (row == 4) {
      if (holdRect(SET_ROW_X, 4, SET_ROW_W)) {
        ancsForgetPairing();
        restartWithMessage("iPhone forgotten", "Restarting...");
      }
      changed = false;
    } else {
      changed = false;
    }
  }
  if (changed && saveHud) saveHudSettings();
  if (changed) fullRedrawRequested = true;
}

void touchHudV2(uint16_t x, uint16_t y) {
  if (y < UI_HEADER_H && x >= UI_SCREEN_W - 40) uiShow(Screen::HudSettings);  // gear
}

// ---------- Router ----------

void uiRenderFullV2() {
  tft.fillScreen(T_SURFACE);
  switch (screen) {
    case Screen::Home: drawHomeV2(); break;
    case Screen::Monitor:
      monitorForgetDrawn();
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
    case Screen::HomeCtl: drawHaV2(); break;
    case Screen::Timer: drawTimerV2(); break;
    case Screen::Ring: drawRingV2(); break;
    case Screen::Hud: hudForceRedraw(); drawHudV2(); break;
    case Screen::HudSettings: drawHudSettingsV2(); break;
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
  if (screen == Screen::Hud) drawHudV2();
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
    if (drawnNotifyVersion != notifyStore.version() && homePage == 0) {
      drawHomeApp(NOTIFY_APP);
      if (bannerActive) drawBannerV2();  // the top app row runs under the banner
    }
  }
}

void uiHandleTouchV2(uint16_t x, uint16_t y) {
  playSound(Sound::Click);
  if (bannerActive && y < BANNER_H) {
    bannerActive = false;
    uiShow(Screen::Notify);
    return;
  }
  if (screen == Screen::Break) {
    touchBreakV2(x, y);
    return;
  }
  if (screen == Screen::Ring) {
    touchRingV2(x, y);
    return;
  }
  if (screen == Screen::Standby) {
    uiShow(Screen::Home);
    waitForRelease();
    return;
  }
  if (screen == Screen::HomeCtl && haAcOpen && y < UI_HEADER_H && x < HOME_BTN_W) {
    haAcOpen = false;  // the air conditioner card goes back to the tiles
    waitForRelease();
    fullRedrawRequested = true;
    return;
  }
  // HUD settings: the top-left button goes back to the HUD, not home.
  if (screen == Screen::HudSettings && y < UI_HEADER_H && x < HOME_BTN_W) {
    uiShow(Screen::Hud);
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
    case Screen::HomeCtl: touchHaV2(x, y); break;
    case Screen::Timer: touchTimerV2(x, y); break;
    case Screen::Hud: touchHudV2(x, y); break;
    case Screen::HudSettings: touchHudSettingsV2(x, y); break;
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
                 {"aram", Screen::Aram},       {"break", Screen::Break},
                 {"ha", Screen::HomeCtl},      {"timer", Screen::Timer},
                 {"ring", Screen::Ring},       {"hud", Screen::Hud},
                 {"hudset", Screen::HudSettings}};
  for (const auto &entry : SCREENS) {
    if (strcmp(entry.name, name) == 0) screen = entry.target;
  }
  if (tab >= 0 && screen == Screen::HudSettings) hudSettingsTab = tab % HS_TABS;
  if (tab >= 0 && screen == Screen::Info) infoTab = tab % INFO_TABS;
  if (tab >= 0 && screen == Screen::Monitor) monitorTab = tab % MON_TABS;
  if (tab >= 0 && screen == Screen::Timer) timerTab = tab % TM_TABS;
  if (tab >= 0 && screen == Screen::Home) homePage = tab % HOME_PAGES;
  if (screen == Screen::Ring) {
    ringKind = static_cast<RingKind>(tab < 0 ? 0 : tab % 3);
    ringReturnTo = Screen::Home;
    ringSinceMs = millis();
  }
  if (screen == Screen::HomeCtl) {  // tab 9 opens the air conditioner card, else a page
    haAcOpen = tab == 9;
    if (tab >= 0 && tab != 9) haPage = tab;
  }
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
    case Screen::Monitor: monitorTab = (monitorTab + MON_TABS + step) % MON_TABS; break;
    case Screen::Timer: timerTab = (timerTab + TM_TABS + step) % TM_TABS; break;
    case Screen::Home: homePage = (homePage + HOME_PAGES + step) % HOME_PAGES; break;
    case Screen::HomeCtl:
      if (haAcOpen || haPages() < 2) return;
      haPage = (haPage + haPages() + step) % haPages();
      break;
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

bool homeServerWasOnline = false;

void uiHomeServerChanged() {
  if (homeServerWasOnline && homeServer.has && !homeServer.online) playSound(Sound::Alert);
  homeServerWasOnline = homeServer.has && homeServer.online;
  if (screen == Screen::Monitor && monitorTab == MON_SERVER) drawServerDynamic();
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
