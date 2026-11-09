// The app entry point is left out of on-device unit test builds (test_build_src = yes).
#ifndef PIO_UNIT_TESTING
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <math.h>
#ifdef DECK_CYD_TOUCH
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#endif
#ifdef DECK_UI_V2
#include <base64.h>

#include "ancs.h"
#include "aram.h"
#include "claude_status.h"
#include "color_util.h"
#include "deck_settings.h"
#include "format_util.h"
#include "gesture.h"
#include "icons.h"
#include "lunar.h"
#include "net_notify.h"
#include "notify_store.h"
#include "text_vi.h"
#include "theme.h"
#include "ui_font.h"
#include "ui_layout.h"
#endif

namespace {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
#ifdef DECK_BACKLIGHT_PIN
constexpr int BACKLIGHT_PIN = DECK_BACKLIGHT_PIN;
#else
constexpr int BACKLIGHT_PIN = 27;
#endif
// Landscape: 1 = USB on the right, 3 = turned 180 degrees (USB on the left).
#ifdef DECK_ROTATION
constexpr uint8_t DISPLAY_ROTATION = DECK_ROTATION;
#else
constexpr uint8_t DISPLAY_ROTATION = 1;
#endif
constexpr uint32_t STALE_AFTER_MS = 15000;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 300;
constexpr char PROTOCOL_PREFIX[] = "DD:";
constexpr size_t PROTOCOL_PREFIX_LEN = 3;
constexpr uint8_t PROTOCOL_VERSION = 3;
constexpr char FIRMWARE_VERSION[] = "0.4.0";

constexpr uint16_t C_BG = 0x0841;
constexpr uint16_t C_PANEL = 0x1082;
constexpr uint16_t C_PANEL_2 = 0x18C3;
constexpr uint16_t C_TEXT = 0xFFFF;
constexpr uint16_t C_MUTED = 0xAD55;
constexpr uint16_t C_ACCENT = 0x07FF;
constexpr uint16_t C_GOOD = 0x07E0;
constexpr uint16_t C_WARN = 0xFFE0;
constexpr uint16_t C_BAD = 0xF800;
constexpr uint16_t C_BAR_BG = 0x2945;

TFT_eSPI tft;
Preferences prefs;

#ifdef DECK_CYD_TOUCH
// CYD wiring: XPT2046 on its own VSPI bus (CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36).
constexpr int TOUCH_CLK = 25;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_CS_PIN = 33;
constexpr int TOUCH_IRQ = 36;
// Raw XPT2046 range for this panel in landscape (rotation 1).
constexpr int TOUCH_RAW_X_MIN = 200;
constexpr int TOUCH_RAW_X_MAX = 3700;
constexpr int TOUCH_RAW_Y_MIN = 240;
constexpr int TOUCH_RAW_Y_MAX = 3800;

SPIClass touchSpi(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ);
#endif

#ifdef DECK_CYD_TOUCH
// Phantom-touch filter: a press must be firm enough and hold still for a moment.
constexpr int TOUCH_MIN_Z = 500;
constexpr uint32_t TOUCH_STABLE_MS = 40;
constexpr int TOUCH_MAX_JITTER = 150;  // raw units
#endif

#ifdef DECK_CYD_TOUCH
void mapTouchPoint(const TS_Point &p, uint16_t *x, uint16_t *y) {
  *x = constrain(map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  *y = constrain(map(p.y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
}
#endif

bool readTouch(uint16_t *x, uint16_t *y) {
#ifdef DECK_CYD_TOUCH
  if (!ts.tirqTouched() || !ts.touched()) return false;
  const TS_Point first = ts.getPoint();
  if (first.z < TOUCH_MIN_Z) return false;
  delay(TOUCH_STABLE_MS);
  if (!ts.touched()) return false;
  const TS_Point p = ts.getPoint();
  if (p.z < TOUCH_MIN_Z || abs(p.x - first.x) > TOUCH_MAX_JITTER ||
      abs(p.y - first.y) > TOUCH_MAX_JITTER) {
    return false;
  }
  mapTouchPoint(p, x, y);
  return true;
#else
  return tft.getTouch(x, y);
#endif
}

#ifdef DECK_CYD_TOUCH
// Current finger position without the stability wait (used while tracking a swipe).
bool touchPosition(uint16_t *x, uint16_t *y) {
  if (!ts.touched()) return false;
  const TS_Point p = ts.getPoint();
  if (p.z < TOUCH_MIN_Z) return false;
  mapTouchPoint(p, x, y);
  return true;
}
#endif

// True while a finger is still on the panel (used by hold-to-confirm).
bool touchHeld() {
#ifdef DECK_CYD_TOUCH
  return ts.touched();
#else
  uint16_t x = 0;
  uint16_t y = 0;
  return tft.getTouch(&x, &y);
#endif
}

enum class Page : uint8_t { Monitor, Macros, Info };
Page page = Page::Monitor;

struct Stats {
  float cpu = NAN;
  float ram = NAN;
  float gpu = NAN;
  float vram = NAN;
  float cpuTemp = NAN;
  float gpuTemp = NAN;
  float systemTemp = NAN;
  float diskTemp = NAN;
  float ramUsedGB = NAN;
  float ramTotalGB = NAN;
  float vramUsedGB = NAN;
  float vramTotalGB = NAN;
  float diskUsed = NAN;
  float diskReadMBs = NAN;
  float diskWriteMBs = NAN;
  float netDownMbps = NAN;
  float netUpMbps = NAN;
  uint32_t clockSecondsAtSync = 0;
  uint32_t clockSyncMs = 0;
  uint32_t lastUpdateMs = 0;
  bool clockValid = false;
  bool everUpdated = false;
  int dateDay = 0;  // PC local date from the stats frame; 0 = unknown
  int dateMonth = 0;
  int dateYear = 0;
} stats;

// INFO page data (gold prices, thousand VND per chi), sent by the host bridge
// as {"type":"info","g":{...}}.
constexpr uint8_t GOLD_ROWS = 6;

struct InfoData {
  bool hasGold = false;
  String goldTime;
  String goldName[GOLD_ROWS];
  long goldBuy[GOLD_ROWS] = {0};
  long goldSell[GOLD_ROWS] = {0};
  long goldChange[GOLD_ROWS] = {0};
  uint8_t goldCount = 0;
  float xau = NAN;
  float xauChange = 0;
} info;

constexpr uint8_t MACROS_PER_PAGE = 6;
constexpr uint8_t MACRO_PAGES = 3;
constexpr uint8_t MACRO_COUNT = MACROS_PER_PAGE * MACRO_PAGES;
const char *const MACRO_PAGE_TITLES[MACRO_PAGES] = {"PC TOOLS", "AI & WORK", "GAMING"};
const char *const MACRO_PAGE_TABS[MACRO_PAGES] = {"PC", "AI/WORK", "GAME"};

String macroLabels[MACRO_COUNT] = {
    "TASK MANAGER", "DOWNLOADS", "LOCK PC", "SCREENSHOT", "EDGE", "CONTROL PANEL",
    "CLAUDE", "GEMINI", "CHATGPT", "TEAMS", "OUTLOOK", "ZALO",
    "LEAGUE OF LEGENDS", "DISCORD", "WEB", "", "", ""};
uint8_t macroPage = 0;
// Buttons flagged "hold" by the bridge must be held for HOLD_CONFIRM_MS (e.g. Lock PC).
bool macroHold[MACRO_COUNT] = {false};
constexpr uint32_t HOLD_CONFIRM_MS = 1000;
uint32_t ledFlashUntil = 0;
#ifdef DECK_UI_V2
Icon macroIcon[MACRO_COUNT];  // set from the bridge's config frame; Controls until then
// Controls icon gradients, indexed by the bridge's "color" names (MACRO_COLORS).
struct MacroGradient {
  const char *name;
  uint32_t from;
  uint32_t to;
};
constexpr MacroGradient MACRO_GRADIENTS[] = {
    {"green", 0x30d158, 0x1a9e48}, {"blue", 0x0a84ff, 0x0060df},   {"orange", 0xff9f0a, 0xff6b1a},
    {"purple", 0xbf5af2, 0x8e44ec}, {"pink", 0xff375f, 0xe0244a},  {"teal", 0x64d2ff, 0x2b8fd8},
    {"gray", 0x8e8e93, 0x5c5c63},  {"indigo", 0x5e5ce6, 0x3a37b8}, {"yellow", 0xffd60a, 0xf5a300},
};
int8_t macroColor[MACRO_COUNT];  // index into MACRO_GRADIENTS, -1 = page default

// Info tab feeds from the bridge (coin, stock, weather frames).
struct CoinRow {
  char symbol[6];
  char name[11];
  float price;
  float change;
};
struct CoinData {
  bool has = false;
  uint8_t count = 0;
  CoinRow rows[5];
  char time[6] = "";
  char date[6] = "";
} coins;

struct QuoteRow {
  char name[11];
  float value;
  float change;
};
constexpr uint8_t SPARK_MAX = 40;
struct StockData {
  bool has = false;
  uint8_t indexCount = 0;
  QuoteRow index[3];
  uint8_t itemCount = 0;
  QuoteRow items[4];
  uint8_t sparkCount = 0;
  float spark[SPARK_MAX];
  float open = 0;
  char time[6] = "";
  char date[6] = "";
} stocks;

struct ForecastDay {
  char day[4];
  int8_t hi;
  int8_t lo;
  int8_t rain;  // -1 = unknown
  uint8_t icon;
};
struct WeatherData {
  bool has = false;
  char city[17] = "";
  float temp = NAN;
  int feels = 0;
  int humidity = 0;
  int wind = 0;
  uint8_t icon = 2;
  char desc[16] = "";
  uint8_t dayCount = 0;
  ForecastDay days[4];
  char time[6] = "";
  char date[6] = "";
} weather;

int8_t macroColorFromName(const char *name) {
  if (name == nullptr) return -1;
  for (size_t i = 0; i < sizeof(MACRO_GRADIENTS) / sizeof(MACRO_GRADIENTS[0]); ++i) {
    if (strcmp(MACRO_GRADIENTS[i].name, name) == 0) return static_cast<int8_t>(i);
  }
  return -1;
}
NotifyStore notifyStore;
uint32_t notifyFlashUntil = 0;
#ifdef DECK_UI_V2
ClaudeState claude;          // Claude Code sessions reported by the bridge
uint32_t claudeRxMs = 0;     // when the last "claude" frame arrived (row ages count from it)
ClaudeUsage claudeUsage;     // tokens and estimated cost today ("cusage")
uint32_t claudeUsageRxMs = 0;
ClaudeStats claudeStats;     // daily ask / answer / done counts ("cstats")
#endif
bool phoneLinkUp = false;  // iPhone ANCS connected (Task 5)
bool pcLinkUp = false;     // Wi-Fi notification link up (Task 6)
DeckSettings settings;
bool screenAsleep = false;
uint32_t lastActivityMs = 0;
constexpr uint8_t BACKLIGHT_CHANNEL = 7;
void uiRenderFullV2();
void uiNotificationArrived();
void uiNotificationQuiet();
void uiRenderDynamicV2();
void uiHandleTouchV2(uint16_t x, uint16_t y);
void uiConfigChanged();
void uiGoldChanged();
void uiInfoFeedChanged(uint8_t tab);
void uiLinksChanged();
void uiClaudeFrame(const ClaudeState &next);
void uiClaudeInfoChanged();
#endif
String serialLine;
uint32_t lastTouchMs = 0;
bool fullRedrawRequested = true;
bool statsDirty = true;

float clampPct(float value) {
  if (isnan(value)) return 0.0f;
  if (value < 0.0f) return 0.0f;
  if (value > 100.0f) return 100.0f;
  return value;
}

String fmtValue(float value, unsigned int decimals = 0) {
  if (isnan(value)) return "--";
  return String(value, decimals);
}

uint16_t usageColor(float value) {
  if (isnan(value)) return C_MUTED;
  if (value < 70.0f) return C_GOOD;
  if (value < 90.0f) return C_WARN;
  return C_BAD;
}

bool parseClockText(const char *text, uint32_t &secondsOfDay) {
  if (!text || text[0] < '0' || text[0] > '9' || text[1] < '0' ||
      text[1] > '9' || text[2] != ':' || text[3] < '0' || text[3] > '9' ||
      text[4] < '0' || text[4] > '9') {
    return false;
  }

  const int hour = (text[0] - '0') * 10 + (text[1] - '0');
  const int minute = (text[3] - '0') * 10 + (text[4] - '0');
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;

  secondsOfDay = static_cast<uint32_t>(hour * 3600 + minute * 60);
  return true;
}

String currentClockText() {
  if (!stats.clockValid) return "--:--";

  const uint32_t elapsedSeconds = (millis() - stats.clockSyncMs) / 1000UL;
  const uint32_t secondsOfDay =
      (stats.clockSecondsAtSync + elapsedSeconds) % 86400UL;
  const uint32_t hour = secondsOfDay / 3600UL;
  const uint32_t minute = (secondsOfDay % 3600UL) / 60UL;

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu",
           static_cast<unsigned long>(hour),
           static_cast<unsigned long>(minute));
  return String(buffer);
}

String memoryText(float usedGB, float totalGB) {
  if (isnan(usedGB) || isnan(totalGB) || totalGB <= 0.0f) return "MEM N/A";
  const unsigned int totalDecimals = totalGB >= 10.0f ? 0U : 1U;
  return String(usedGB, 1) + "/" + String(totalGB, totalDecimals) + "G";
}

void drawHeaderStatic(const String &title) {
  tft.fillRect(0, 0, SCREEN_W, 28, C_PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.drawString(title, 8, 14, 2);
}

void drawHeaderDynamic() {
  const bool stale =
      stats.everUpdated && (millis() - stats.lastUpdateMs > STALE_AFTER_MS);
  uint16_t statusColor = C_MUTED;
  if (stats.everUpdated) statusColor = stale ? C_WARN : C_GOOD;

  // USB state has its own fixed area.
  tft.fillRect(168, 0, 62, 28, C_PANEL);
  tft.fillCircle(178, 14, 4, statusColor);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString("USB", 187, 14, 1);

  // Right-align the clock in a wider fixed area. This makes it easier to see
  // on the 320x240 display and avoids overlap with the USB status text.
  tft.fillRect(230, 0, 90, 28, C_PANEL);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(stats.clockValid ? C_TEXT : C_WARN, C_PANEL);
  tft.drawString(currentClockText(), 314, 14, 2);
}

void drawMetricStatic(const char *label, int y) {
  constexpr int barX = 54;
  constexpr int barW = 123;
  constexpr int barH = 13;

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString(label, 8, y + 7, 2);
  tft.fillRoundRect(barX, y, barW, barH, 3, C_BAR_BG);
}

void drawMetricDynamic(float value, const String &auxText, uint16_t auxColor,
                       uint8_t auxFont, int y) {
  constexpr int barX = 54;
  constexpr int barW = 123;
  constexpr int barH = 13;

  tft.fillRoundRect(barX, y, barW, barH, 3, C_BAR_BG);
  const int fillW = static_cast<int>((barW - 2) * clampPct(value) / 100.0f);
  if (fillW > 0) {
    tft.fillRoundRect(barX + 1, y + 1, fillW, barH - 2, 2,
                      usageColor(value));
  }

  tft.fillRect(181, y - 2, 139, 18, C_BG);

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(usageColor(value), C_BG);
  tft.drawString(fmtValue(value) + "%", 228, y + 7, 2);

  tft.setTextColor(auxColor, C_BG);
  tft.drawString(auxText, 314, y + 7, auxFont);
}

void drawInfoBoxStatic(int x, int y, int w, int h, const String &title) {
  tft.fillRoundRect(x, y, w, h, 5, C_PANEL_2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED, C_PANEL_2);
  tft.drawString(title, x + 6, y + 5, 2);
}

void drawInfoBoxDynamic(int x, int y, int w, int h, const String &line1,
                        const String &line2) {
  tft.fillRect(x + 5, y + 20, w - 10, h - 25, C_PANEL_2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TEXT, C_PANEL_2);
  tft.drawString(line1, x + 6, y + 22, 2);
  tft.setTextColor(C_MUTED, C_PANEL_2);
  tft.drawString(line2, x + 6, y + 43, 1);
}

// Footer tabs: MONITOR, one tab per macro page, then INFO.
constexpr int FOOTER_Y = 212;
constexpr int FOOTER_TABS = 1 + MACRO_PAGES + 1;
constexpr int FOOTER_TAB_W = SCREEN_W / FOOTER_TABS;
constexpr int INFO_TAB = FOOTER_TABS - 1;

void drawFooter(Page selected) {
  constexpr int h = 28;

  tft.fillRect(0, FOOTER_Y, SCREEN_W, h, C_PANEL);
  tft.setTextDatum(MC_DATUM);

  for (int tab = 0; tab < FOOTER_TABS; ++tab) {
    bool active;
    const char *label;
    if (tab == 0) {
      active = selected == Page::Monitor;
      label = "MONITOR";
    } else if (tab == INFO_TAB) {
      active = selected == Page::Info;
      label = "INFO";
    } else {
      active = selected == Page::Macros && macroPage == tab - 1;
      label = MACRO_PAGE_TABS[tab - 1];
    }
    const int x = tab * FOOTER_TAB_W;
    tft.fillRect(x, FOOTER_Y, FOOTER_TAB_W, h, active ? C_ACCENT : C_PANEL);
    if (tab > 0) tft.drawFastVLine(x, FOOTER_Y + 4, h - 8, C_BAR_BG);
    tft.setTextColor(active ? TFT_BLACK : C_TEXT, active ? C_ACCENT : C_PANEL);
    tft.drawString(label, x + FOOTER_TAB_W / 2, FOOTER_Y + h / 2, 2);
  }
}

// 14250 -> "14.250" (Vietnamese thousands separator).
String fmtThousands(long value) {
  const bool negative = value < 0;
  String digits = String(negative ? -value : value);
  String out;
  int count = 0;
  for (int i = digits.length() - 1; i >= 0; --i) {
    out = digits[i] + out;
    if (++count % 3 == 0 && i > 0) out = "." + out;
  }
  return negative ? "-" + out : out;
}

String signedThousands(long value) {
  if (value > 0) return "+" + fmtThousands(value);
  return fmtThousands(value);
}

uint16_t changeColor(float value) {
  if (value > 0.0f) return C_GOOD;
  if (value < 0.0f) return C_BAD;
  return C_MUTED;
}

void drawInfoFull() {
  // Column right edges: buy, sell, change.
  constexpr int colBuy = 178;
  constexpr int colSell = 254;
  constexpr int colChange = 314;
  constexpr int headY = 32;
  constexpr int rowY0 = 52;
  constexpr int rowH = 24;

  tft.fillScreen(C_BG);
  drawHeaderStatic("GIA VANG");
  drawHeaderDynamic();

  tft.fillRect(0, headY, SCREEN_W, 18, C_PANEL);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("nghin d/chi", 8, headY + 9, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("MUA", colBuy, headY + 9, 2);
  tft.drawString("BAN", colSell, headY + 9, 2);
  tft.drawString("+/-", colChange, headY + 9, 2);

  if (!info.hasGold) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString("Dang tai gia vang...", SCREEN_W / 2, 120, 2);
    drawFooter(Page::Info);
    return;
  }

  for (int i = 0; i < info.goldCount; ++i) {
    const int y = rowY0 + i * rowH;
    const uint16_t bg = (i % 2 == 0) ? C_BG : C_PANEL_2;
    const int midY = y + rowH / 2;
    tft.fillRect(0, y, SCREEN_W, rowH, bg);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TEXT, bg);
    tft.drawString(info.goldName[i], 8, midY, 2);

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_TEXT, bg);
    tft.drawString(fmtThousands(info.goldBuy[i]), colBuy, midY, 2);
    tft.setTextColor(C_WARN, bg);
    tft.drawString(fmtThousands(info.goldSell[i]), colSell, midY, 2);
    tft.setTextColor(changeColor(info.goldChange[i]), bg);
    tft.drawString(signedThousands(info.goldChange[i]), colChange, midY, 2);
  }

  // World price and update time under the table.
  constexpr int bottomY = 203;
  tft.setTextDatum(ML_DATUM);
  if (!isnan(info.xau)) {
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString("The gioi $" + String(info.xau, 1) + "/oz", 8, bottomY, 1);
    tft.setTextColor(changeColor(info.xauChange), C_BG);
    const String change = (info.xauChange > 0 ? "+" : "") + String(info.xauChange, 1);
    tft.drawString(change, 8 + tft.textWidth("The gioi $" + String(info.xau, 1) + "/oz ", 1), bottomY, 1);
  }
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("Cap nhat " + info.goldTime, colChange, bottomY, 1);

  drawFooter(Page::Info);
}

void drawMonitorDynamic() {
  drawHeaderDynamic();

  String cpuAux = "TEMP N/A";
  uint16_t cpuAuxColor = C_MUTED;
  uint8_t cpuAuxFont = 1;
  if (!isnan(stats.cpuTemp)) {
    cpuAux = String(stats.cpuTemp, 0) + "C";
    cpuAuxColor = C_TEXT;
    cpuAuxFont = 2;
  } else if (!isnan(stats.systemTemp)) {
    cpuAux = "SYS " + String(stats.systemTemp, 0) + "C";
    cpuAuxColor = C_WARN;
  }

  String gpuAux = "TEMP N/A";
  uint16_t gpuAuxColor = C_MUTED;
  uint8_t gpuAuxFont = 1;
  if (!isnan(stats.gpuTemp)) {
    gpuAux = String(stats.gpuTemp, 0) + "C";
    gpuAuxColor = C_TEXT;
    gpuAuxFont = 2;
  }

  drawMetricDynamic(stats.cpu, cpuAux, cpuAuxColor, cpuAuxFont, 38);
  drawMetricDynamic(stats.ram, memoryText(stats.ramUsedGB, stats.ramTotalGB),
                    C_MUTED, 1, 61);
  drawMetricDynamic(stats.gpu, gpuAux, gpuAuxColor, gpuAuxFont, 84);
  drawMetricDynamic(stats.vram,
                    memoryText(stats.vramUsedGB, stats.vramTotalGB), C_MUTED,
                    1, 107);

  const String net1 = "D " + fmtValue(stats.netDownMbps, 1) + " Mb/s";
  const String net2 = "U " + fmtValue(stats.netUpMbps, 1) + " Mb/s";
  drawInfoBoxDynamic(8, 137, 148, 66, net1, net2);

  const String disk1 = "USED " + fmtValue(stats.diskUsed, 0) + "%";
  String disk2 = "R " + fmtValue(stats.diskReadMBs, 1) + " W " +
                 fmtValue(stats.diskWriteMBs, 1);
  if (!isnan(stats.diskTemp)) {
    disk2 += " T" + String(stats.diskTemp, 0) + "C";
  } else {
    disk2 += " MB/s";
  }
  drawInfoBoxDynamic(164, 137, 148, 66, disk1, disk2);
}

void drawMonitorFull() {
  tft.fillScreen(C_BG);
  drawHeaderStatic("PC CONTROL DECK");

  drawMetricStatic("CPU", 38);
  drawMetricStatic("RAM", 61);
  drawMetricStatic("GPU", 84);
  drawMetricStatic("VRAM", 107);

  drawInfoBoxStatic(8, 137, 148, 66, "NETWORK");
  drawInfoBoxStatic(164, 137, 148, 66, "DISK");
  drawFooter(Page::Monitor);
  drawMonitorDynamic();
}

void splitMacroLabel(const String &label, String &line1, String &line2) {
  String normalized = label;
  normalized.trim();
  const int newlineIndex = normalized.indexOf('\n');
  if (newlineIndex >= 0) {
    line1 = normalized.substring(0, newlineIndex);
    line2 = normalized.substring(newlineIndex + 1);
    line1.trim();
    line2.trim();
    return;
  }

  if (normalized.length() <= 11) {
    line1 = normalized;
    line2 = "";
    return;
  }

  int bestSpace = -1;
  int bestDistance = 999;
  const int middle = normalized.length() / 2;
  for (int i = 1; i < static_cast<int>(normalized.length()) - 1; ++i) {
    if (normalized.charAt(i) == ' ') {
      const int distance = abs(i - middle);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestSpace = i;
      }
    }
  }

  if (bestSpace > 0) {
    line1 = normalized.substring(0, bestSpace);
    line2 = normalized.substring(bestSpace + 1);
  } else {
    line1 = normalized.substring(0, 10);
    line2 = normalized.substring(10);
  }
  line1.trim();
  line2.trim();
}

void drawMacroButton(uint8_t id, int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 7, C_PANEL_2);
  tft.drawRoundRect(x, y, w, h, 7, C_ACCENT);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED, C_PANEL_2);
  tft.drawString(String(id), x + 5, y + 4, 1);
  if (macroHold[id - 1]) {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(C_WARN, C_PANEL_2);
    tft.drawString("HOLD", x + w - 5, y + 4, 1);
  }

  String line1;
  String line2;
  splitMacroLabel(macroLabels[id - 1], line1, line2);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_TEXT, C_PANEL_2);
  if (line2.length() == 0) {
    tft.drawString(line1, x + w / 2, y + h / 2 - 2, 2);
  } else {
    tft.drawString(line1, x + w / 2, y + h / 2 - 12, 2);
    tft.drawString(line2, x + w / 2, y + h / 2 + 7, 2);
  }
}

void drawMacrosFull() {
  tft.fillScreen(C_BG);
  drawHeaderStatic(MACRO_PAGE_TITLES[macroPage]);
  drawHeaderDynamic();

  constexpr int gap = 7;
  constexpr int x0 = 8;
  constexpr int y0 = 36;
  constexpr int bw = 96;
  constexpr int bh = 78;

  for (int i = 0; i < MACROS_PER_PAGE; ++i) {
    const uint8_t id = macroPage * MACROS_PER_PAGE + i + 1;
    if (macroLabels[id - 1].length() == 0) continue;
    const int col = i % 3;
    const int row = i / 3;
    drawMacroButton(id, x0 + col * (bw + gap), y0 + row * (bh + gap), bw, bh);
  }
  drawFooter(Page::Macros);
}

void renderFull() {
#ifdef DECK_UI_V2
  uiRenderFullV2();
  fullRedrawRequested = false;
  statsDirty = false;
  return;
#endif
  if (page == Page::Monitor) {
    drawMonitorFull();
  } else if (page == Page::Info) {
    drawInfoFull();
  } else {
    drawMacrosFull();
  }
  fullRedrawRequested = false;
  statsDirty = false;
}

void sendFrame(JsonDocument &doc) {
  Serial.print(PROTOCOL_PREFIX);
  serializeJson(doc, Serial);
  Serial.println();
}

void sendSimpleFrame(const char *type) {
  JsonDocument doc;
  doc["type"] = type;
  sendFrame(doc);
}

void sendClaudeReply(uint32_t id, bool allow, bool always = false) {
  JsonDocument doc;
  doc["type"] = "claude_reply";
  doc["id"] = id;
  doc["ok"] = allow;
  if (always) doc["always"] = true;
  sendFrame(doc);
}

void sendClaudeOpen(const String &name) {
  JsonDocument doc;
  doc["type"] = "claude_open";
  doc["name"] = name;
  sendFrame(doc);
}

void sendMacro(uint8_t id) {
  JsonDocument doc;
  doc["type"] = "macro";
  doc["id"] = id;
  sendFrame(doc);
}

constexpr int MACRO_GAP = 7;
constexpr int MACRO_X0 = 8;
constexpr int MACRO_Y0 = 36;
constexpr int MACRO_BW = 96;
constexpr int MACRO_BH = 78;

void macroButtonOrigin(uint8_t id, int &x, int &y) {
  const int index = (id - 1) % MACROS_PER_PAGE;
  x = MACRO_X0 + (index % 3) * (MACRO_BW + MACRO_GAP);
  y = MACRO_Y0 + (index / 3) * (MACRO_BH + MACRO_GAP);
}

void flashMacro(uint8_t id) {
  int x = 0;
  int y = 0;
  macroButtonOrigin(id, x, y);
  tft.drawRoundRect(x + 1, y + 1, MACRO_BW - 2, MACRO_BH - 2, 7, C_GOOD);
  delay(60);
  drawMacroButton(id, x, y, MACRO_BW, MACRO_BH);
}

// Keep the finger on the button for HOLD_CONFIRM_MS; a progress bar fills meanwhile.
bool holdToConfirm(uint8_t id) {
  int x = 0;
  int y = 0;
  macroButtonOrigin(id, x, y);
  const int barW = MACRO_BW - 12;
  const uint32_t start = millis();
  int misses = 0;
  while (millis() - start < HOLD_CONFIRM_MS) {
    misses = touchHeld() ? 0 : misses + 1;
    if (misses >= 3) {  // released early: cancel
      drawMacroButton(id, x, y, MACRO_BW, MACRO_BH);
      return false;
    }
    const int filled = barW * (millis() - start) / HOLD_CONFIRM_MS;
    tft.fillRect(x + 6, y + MACRO_BH - 10, filled, 4, C_WARN);
    delay(20);
  }
  drawMacroButton(id, x, y, MACRO_BW, MACRO_BH);
  return true;
}

void waitForRelease(uint32_t timeoutMs = 3000) {
  const uint32_t start = millis();
  while (touchHeld() && millis() - start < timeoutMs) delay(10);
}

// Board RGB LED (active low): blue = waiting for PC, green = live data,
// red = data stale, cyan flash = button pressed.
#ifdef DECK_LED_R
constexpr uint8_t LED_PINS[3] = {DECK_LED_R, DECK_LED_G, DECK_LED_B};
constexpr uint8_t LED_CHANNEL0 = 4;
constexpr uint8_t LED_LEVEL = 24;  // dim, out of 255

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t value[3] = {r, g, b};
  for (int i = 0; i < 3; ++i) ledcWrite(LED_CHANNEL0 + i, 255 - value[i]);
}

void setupLed() {
  for (int i = 0; i < 3; ++i) {
    ledcSetup(LED_CHANNEL0 + i, 5000, 8);
    ledcAttachPin(LED_PINS[i], LED_CHANNEL0 + i);
  }
  setLed(0, 0, 0);
}
#endif

#ifdef DECK_UI_V2
// ---------- Settings: NVS blob, backlight, screen sleep, quiet hours ----------
Preferences settingsPrefs;  // own namespace, separate from touch calibration
constexpr char SETTINGS_NS[] = "decksettings";

void saveSettings() {
  uint8_t blob[SETTINGS_BLOB_SIZE];
  packSettings(settings, blob);
  settingsPrefs.begin(SETTINGS_NS, false);
  settingsPrefs.putBytes("blob", blob, sizeof(blob));
  settingsPrefs.end();
}

void loadSettings() {
  settings.rotate180 = DISPLAY_ROTATION == 3;  // until the user picks, follow the build
  if (!settingsPrefs.begin(SETTINGS_NS, true)) return;  // nothing saved yet
  uint8_t blob[SETTINGS_BLOB_SIZE];
  if (settingsPrefs.getBytesLength("blob") == sizeof(blob)) {
    settingsPrefs.getBytes("blob", blob, sizeof(blob));
    unpackSettings(blob, sizeof(blob), settings);
  }
  settingsPrefs.end();
}

void clearSettings() {
  settingsPrefs.begin(SETTINGS_NS, false);
  settingsPrefs.clear();
  settingsPrefs.end();
}

void applyRotation() {
  const uint8_t rotation = settings.rotate180 ? 3 : 1;
  tft.setRotation(rotation);
#ifdef DECK_CYD_TOUCH
  ts.setRotation(rotation);  // touch turns with the picture
#endif
}

void applyBacklight() {
  ledcWrite(BACKLIGHT_CHANNEL, screenAsleep ? 0 : backlightDuty(settings.brightness));
}

void setupBacklight() {
  ledcSetup(BACKLIGHT_CHANNEL, 5000, 8);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
  applyBacklight();
}

// Any touch or alert keeps the screen on; returns true if it was asleep.
bool noteActivity() {
  lastActivityMs = millis();
  if (!screenAsleep) return false;
  screenAsleep = false;
  applyBacklight();
  return true;
}

void updateScreenSleep() {
  const uint32_t timeout = sleepTimeoutMs(settings.sleepIndex);
  if (screenAsleep || timeout == 0 || millis() - lastActivityMs < timeout) return;
  screenAsleep = true;
  applyBacklight();
}

// PC clock hour, or -1 before the bridge has sent the time.
int clockHour() {
  if (!stats.clockValid) return -1;
  const uint32_t seconds = (stats.clockSecondsAtSync + (millis() - stats.clockSyncMs) / 1000UL) % 86400UL;
  return static_cast<int>(seconds / 3600UL);
}

bool quietNow() { return settings.quietOn && inQuietHours(clockHour(), settings.quietStart, settings.quietEnd); }
bool ledNightNow() {
  return settings.ledNight && inQuietHours(clockHour(), settings.quietStart, settings.quietEnd);
}
#endif

void updateStatusLed() {
#ifdef DECK_LED_R
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 100) return;
  lastUpdate = millis();
#ifdef DECK_UI_V2
  if (!settings.ledOn || ledNightNow()) {
    setLed(0, 0, 0);
    return;
  }
  if (static_cast<int32_t>(notifyFlashUntil - millis()) > 0) {
    setLed(0, 0, LED_LEVEL * 6);
    return;
  }
  if (claude.anyWaiting()) {  // a Claude Code session needs you: blink orange
    if ((millis() / 500) % 2) setLed(LED_LEVEL * 6, LED_LEVEL * 2, 0);
    else setLed(LED_LEVEL, LED_LEVEL / 3, 0);
    return;
  }
#endif
  if (static_cast<int32_t>(ledFlashUntil - millis()) > 0) {
    setLed(0, LED_LEVEL * 4, LED_LEVEL * 4);
  } else if (!stats.everUpdated) {
    setLed(0, 0, LED_LEVEL);
  } else if (millis() - stats.lastUpdateMs > STALE_AFTER_MS) {
    setLed(LED_LEVEL, 0, 0);
  } else {
    setLed(0, LED_LEVEL, 0);
  }
#endif
}

#ifdef DECK_UI_V2
#include "ui_v2_screens.h"
#endif

#if defined(DECK_UI_V2) && defined(DECK_CYD_TOUCH)
// Follows the finger from (x0, y0) until it is a tap (at the start point) or a horizontal swipe.
void trackGesture(uint16_t x0, uint16_t y0) {
  const uint32_t start = millis();
  uint16_t x = x0, y = y0;
  int dx = 0, dy = 0, misses = 0;
  while (true) {
    bool released = false;
    if (touchPosition(&x, &y)) {
      misses = 0;
      dx = static_cast<int>(x) - x0;
      dy = static_cast<int>(y) - y0;
    } else if (++misses >= 3) {  // resistive panels drop out for a sample now and then
      released = true;
    }
    const Gesture gesture = classifyGesture(dx, dy, millis() - start, released);
    if (gesture == Gesture::Pending) {
      delay(10);
      continue;
    }
    if (gesture == Gesture::Tap) {
      uiHandleTouchV2(x0, y0);
    } else {
      if (gesture == Gesture::SwipeLeft) uiSwipeV2(1);
      if (gesture == Gesture::SwipeRight) uiSwipeV2(-1);
      waitForRelease();
    }
    lastTouchMs = millis();
    return;
  }
}
#endif

void handleTouch() {
  if (millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) return;

  uint16_t x = 0;
  uint16_t y = 0;
  if (!readTouch(&x, &y)) return;
  lastTouchMs = millis();
#ifdef DECK_UI_V2
  if (noteActivity()) {  // the waking touch only turns the screen on
    waitForRelease();
    lastTouchMs = millis();
    return;
  }
#ifdef DECK_CYD_TOUCH
  trackGesture(x, y);
#else
  uiHandleTouchV2(x, y);
#endif
  return;
#endif

  if (y >= FOOTER_Y) {
    const int tab = min(x / FOOTER_TAB_W, FOOTER_TABS - 1);
    const bool macroTab = tab > 0 && tab < INFO_TAB;
    const Page requested = tab == 0 ? Page::Monitor : (macroTab ? Page::Macros : Page::Info);
    const uint8_t requestedMacroPage = macroTab ? tab - 1 : macroPage;
    if (requested != page || requestedMacroPage != macroPage) {
      page = requested;
      macroPage = requestedMacroPage;
      fullRedrawRequested = true;
    }
    return;
  }

  if (page != Page::Macros || y < 36 || y >= 199) return;

  for (int i = 0; i < MACROS_PER_PAGE; ++i) {
    const uint8_t id = macroPage * MACROS_PER_PAGE + i + 1;
    int bx = 0;
    int by = 0;
    macroButtonOrigin(id, bx, by);
    if (x >= bx && x < bx + MACRO_BW && y >= by && y < by + MACRO_BH) {
      if (macroLabels[id - 1].length() == 0) return;
      if (macroHold[id - 1]) {
        const bool confirmed = holdToConfirm(id);
        waitForRelease();
        lastTouchMs = millis();
        if (!confirmed) return;
      }
      ledFlashUntil = millis() + 150;
      flashMacro(id);
      sendMacro(id);
      return;
    }
  }
}

float readFloat(JsonVariantConst value, float fallback = NAN) {
  if (value.isNull()) return fallback;
  return value.as<float>();
}

void syncClock(JsonDocument &doc) {
  int y = 0, m = 0, d = 0;
  const char *date = doc["date"] | "";
  if (sscanf(date, "%4d-%2d-%2d", &y, &m, &d) == 3 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
    stats.dateYear = y;
    stats.dateMonth = m;
    stats.dateDay = d;
  }

  if (!doc["clock_seconds"].isNull()) {
    const int32_t seconds = doc["clock_seconds"].as<int32_t>();
    if (seconds >= 0 && seconds < 86400) {
      stats.clockSecondsAtSync = static_cast<uint32_t>(seconds);
      stats.clockSyncMs = millis();
      stats.clockValid = true;
      return;
    }
  }

  const char *clockText = doc["clock"] | nullptr;
  uint32_t secondsOfDay = 0;
  if (parseClockText(clockText, secondsOfDay)) {
    stats.clockSecondsAtSync = secondsOfDay;
    stats.clockSyncMs = millis();
    stats.clockValid = true;
  }
}

void handleStats(JsonDocument &doc) {
  stats.cpu = readFloat(doc["cpu"]);
  stats.ram = readFloat(doc["ram"]);
  stats.gpu = readFloat(doc["gpu"]);
  stats.vram = readFloat(doc["vram"]);
  stats.cpuTemp = readFloat(doc["cpu_temp"]);
  stats.gpuTemp = readFloat(doc["gpu_temp"]);
  stats.systemTemp = readFloat(doc["system_temp"]);
  stats.diskTemp = readFloat(doc["disk_temp"]);
  stats.ramUsedGB = readFloat(doc["ram_used_gb"]);
  stats.ramTotalGB = readFloat(doc["ram_total_gb"]);
  stats.vramUsedGB = readFloat(doc["vram_used_gb"]);
  stats.vramTotalGB = readFloat(doc["vram_total_gb"]);
  stats.diskUsed = readFloat(doc["disk_used"]);
  stats.diskReadMBs = readFloat(doc["disk_read_mbs"]);
  stats.diskWriteMBs = readFloat(doc["disk_write_mbs"]);
  stats.netDownMbps = readFloat(doc["net_down_mbps"]);
  stats.netUpMbps = readFloat(doc["net_up_mbps"]);
  syncClock(doc);

  stats.lastUpdateMs = millis();
  stats.everUpdated = true;
  statsDirty = true;
}

void handleConfig(JsonDocument &doc) {
  JsonArrayConst macros = doc["macros"].as<JsonArrayConst>();
  bool labelsChanged = false;
  bool nextHold[MACRO_COUNT] = {false};

  for (JsonObjectConst item : macros) {
    const int id = item["id"] | 0;
    const char *label = item["label"] | "";
    if (id < 1 || id > MACRO_COUNT || !label || label[0] == '\0') continue;
    nextHold[id - 1] = item["hold"] | false;
#ifdef DECK_UI_V2
    const Icon icon = iconFromName(item["icon"] | static_cast<const char *>(nullptr));
    if (macroIcon[id - 1] != icon) {
      macroIcon[id - 1] = icon;
      labelsChanged = true;
    }
    const int8_t color = macroColorFromName(item["color"] | static_cast<const char *>(nullptr));
    if (macroColor[id - 1] != color) {
      macroColor[id - 1] = color;
      labelsChanged = true;
    }
#endif

    String next(label);
    if (next.length() > 22) next = next.substring(0, 22);
    if (macroLabels[id - 1] != next) {
      macroLabels[id - 1] = next;
      labelsChanged = true;
    }
  }

  for (int i = 0; i < MACRO_COUNT; ++i) {
    if (macroHold[i] != nextHold[i]) {
      macroHold[i] = nextHold[i];
      labelsChanged = true;
    }
  }

#ifdef DECK_UI_V2
  if (labelsChanged) uiConfigChanged();
#else
  if (labelsChanged && page == Page::Macros) {
    fullRedrawRequested = true;
  }
#endif
}

void handleInfo(JsonDocument &doc) {
  JsonObjectConst gold = doc["g"].as<JsonObjectConst>();
  if (!gold.isNull()) {
    info.hasGold = true;
    info.goldTime = gold["time"] | "";
    info.goldCount = 0;
    for (JsonObjectConst item : gold["items"].as<JsonArrayConst>()) {
      if (info.goldCount >= GOLD_ROWS) break;
      const uint8_t i = info.goldCount++;
      info.goldName[i] = item["n"] | "";
      info.goldBuy[i] = item["b"] | 0L;
      info.goldSell[i] = item["s"] | 0L;
      info.goldChange[i] = item["c"] | 0L;
    }
    info.xau = readFloat(gold["xau"]);
    info.xauChange = readFloat(gold["xauc"], 0.0f);
  }

#ifdef DECK_UI_V2
  uiGoldChanged();
#else
  if (page == Page::Info) fullRedrawRequested = true;
#endif
}

#ifdef DECK_UI_V2
// Adds a notification from any source and alerts the user (banner + blue LED).
// Muted sources/apps are dropped; in quiet hours it is stored without banner, LED or wake.
void addNotification(NotifySource source, const String &app, const String &from, const String &body) {
  if (!notificationAllowed(settings, source, app)) return;
  notifyStore.add(source, app, from, body, millis());
  if (quietNow()) {
    uiNotificationQuiet();
    return;
  }
  notifyFlashUntil = millis() + 300;
  noteActivity();
  uiNotificationArrived();
}

void copyText(char *dest, size_t size, JsonVariantConst value) {
  strlcpy(dest, value | "", size);
}

void handleCoin(JsonDocument &doc) {
  coins.count = 0;
  for (JsonObjectConst item : doc["items"].as<JsonArrayConst>()) {
    if (coins.count >= 5) break;
    CoinRow &row = coins.rows[coins.count++];
    copyText(row.symbol, sizeof(row.symbol), item["s"]);
    copyText(row.name, sizeof(row.name), item["n"]);
    row.price = readFloat(item["p"], 0.0f);
    row.change = readFloat(item["c"], 0.0f);
  }
  copyText(coins.time, sizeof(coins.time), doc["time"]);
  copyText(coins.date, sizeof(coins.date), doc["date"]);
  coins.has = true;
  uiInfoFeedChanged(1);
}

uint8_t readQuotes(JsonArrayConst list, QuoteRow *rows, uint8_t capacity, const char *valueKey) {
  uint8_t count = 0;
  for (JsonObjectConst item : list) {
    if (count >= capacity) break;
    QuoteRow &row = rows[count++];
    copyText(row.name, sizeof(row.name), item["n"]);
    row.value = readFloat(item[valueKey], 0.0f);
    row.change = readFloat(item["c"], 0.0f);
  }
  return count;
}

void handleStock(JsonDocument &doc) {
  stocks.indexCount = readQuotes(doc["index"].as<JsonArrayConst>(), stocks.index, 3, "v");
  stocks.itemCount = readQuotes(doc["items"].as<JsonArrayConst>(), stocks.items, 4, "p");
  stocks.sparkCount = 0;
  for (JsonVariantConst point : doc["spark"].as<JsonArrayConst>()) {
    if (stocks.sparkCount >= SPARK_MAX) break;
    stocks.spark[stocks.sparkCount++] = point.as<float>();
  }
  stocks.open = readFloat(doc["open"], 0.0f);
  copyText(stocks.time, sizeof(stocks.time), doc["time"]);
  copyText(stocks.date, sizeof(stocks.date), doc["date"]);
  stocks.has = true;
  uiInfoFeedChanged(2);
}

void handleWeather(JsonDocument &doc) {
  copyText(weather.city, sizeof(weather.city), doc["city"]);
  weather.temp = readFloat(doc["t"]);
  weather.feels = doc["f"] | 0;
  weather.humidity = doc["h"] | 0;
  weather.wind = doc["wind"] | 0;
  weather.icon = doc["icon"] | 2;
  copyText(weather.desc, sizeof(weather.desc), doc["desc"]);
  weather.dayCount = 0;
  for (JsonObjectConst item : doc["days"].as<JsonArrayConst>()) {
    if (weather.dayCount >= 4) break;
    ForecastDay &day = weather.days[weather.dayCount++];
    copyText(day.day, sizeof(day.day), item["d"]);
    day.hi = item["hi"] | 0;
    day.lo = item["lo"] | 0;
    day.rain = item["rain"] | -1;
    day.icon = item["icon"] | 2;
  }
  copyText(weather.time, sizeof(weather.time), doc["time"]);
  copyText(weather.date, sizeof(weather.date), doc["date"]);
  weather.has = true;
  uiInfoFeedChanged(3);
}

void handleClaude(JsonDocument &doc) {
  ClaudeState next;
  if (!parseClaudeFrame(doc.as<JsonVariantConst>(), next)) return;
  claudeRxMs = millis();
  uiClaudeFrame(next);
}

void handleClaudeUsage(JsonDocument &doc) {
  if (!parseClaudeUsage(doc.as<JsonVariantConst>(), claudeUsage)) return;
  claudeUsageRxMs = millis();
  uiClaudeInfoChanged();
}

void handleClaudeStats(JsonDocument &doc) {
  if (parseClaudeStats(doc.as<JsonVariantConst>(), claudeStats)) uiClaudeInfoChanged();
}

void handleNotify(JsonDocument &doc) {
  const char *src = doc["src"] | "pc";
  addNotification(strcmp(src, "phone") == 0 ? NotifySource::Phone : NotifySource::Pc,
                  doc["app"] | "", doc["from"] | "", doc["body"] | "");
}

void onPhoneNotification(const String &app, const String &title, const String &message) {
  addNotification(NotifySource::Phone, app, title, message);
}

// Mirrors the iPhone link into the UI; reports it (with free heap) to the bridge log.
void updatePhoneLink() {
  if (phoneLinkUp == ancsReady()) return;
  phoneLinkUp = ancsReady();
  uiLinksChanged();
  JsonDocument doc;
  doc["type"] = "ancs";
  doc["ready"] = phoneLinkUp;
  doc["heap"] = ESP.getFreeHeap();
  sendFrame(doc);
}

void onPcNotification(const String &app, const String &from, const String &body) {
  addNotification(NotifySource::Pc, app, from, body);
}

void sendWifiStatus() {
  JsonDocument doc;
  doc["type"] = "wifi_status";
  doc["connected"] = netConnected();
  doc["ip"] = netIp();
  doc["port"] = netPort();
  sendFrame(doc);
}

// Wi-Fi credentials and the UDP token come from the bridge's local config.
void handleWifi(JsonDocument &doc) {
  WifiSettings settings;
  settings.ssid = doc["ssid"] | "";
  settings.pass = doc["pass"] | "";
  settings.token = doc["token"] | "";
  settings.port = doc["port"] | 4210;
  netConfigure(settings);
  sendWifiStatus();
}

// Dev aid: {"type":"shot","screen":"info","tab":2} opens that screen and streams it back,
// one base64 row of big-endian RGB565 per line (tools/deck_screenshot.py turns it into a PNG).
void handleShot(JsonDocument &doc) {
  uiOpenForShot(doc["screen"] | "", doc["tab"] | -1);
  renderFull();
  static uint16_t row[SCREEN_W];
  uint8_t bytes[SCREEN_W * 2];
  for (int y = 0; y < SCREEN_H; ++y) {
    tft.readRect(0, y, SCREEN_W, 1, row);
    for (int x = 0; x < SCREEN_W; ++x) {  // readRect() returns byte-swapped RGB565
      bytes[x * 2] = row[x] & 0xFF;
      bytes[x * 2 + 1] = row[x] >> 8;
    }
    Serial.print(PROTOCOL_PREFIX);
    Serial.print("{\"type\":\"shot_row\",\"y\":");
    Serial.print(y);
    Serial.print(",\"d\":\"");
    Serial.print(base64::encode(bytes, sizeof(bytes)));
    Serial.println("\"}");
  }
  fullRedrawRequested = true;
}

void updatePcLink() {
  if (pcLinkUp == netConnected()) return;
  pcLinkUp = netConnected();
  uiLinksChanged();
  sendWifiStatus();
}
#endif

void processSerialLine(const String &line) {
  bool framed = false;
  String payload;
  if (line.startsWith(PROTOCOL_PREFIX)) {
    if (line.length() <= PROTOCOL_PREFIX_LEN) return;
    payload = line.substring(PROTOCOL_PREFIX_LEN);
    framed = true;
  } else if (line.startsWith("{")) {
    payload = line;
  } else {
    return;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    if (framed) {
      JsonDocument errorDoc;
      errorDoc["type"] = "error";
      errorDoc["code"] = "invalid_json";
      errorDoc["detail"] = error.c_str();
      sendFrame(errorDoc);
    }
    return;
  }

  const char *type = doc["type"] | "";
  if (strcmp(type, "stats") == 0) {
    handleStats(doc);
  } else if (strcmp(type, "config") == 0) {
    handleConfig(doc);
  } else if (strcmp(type, "info") == 0) {
    handleInfo(doc);
#ifdef DECK_UI_V2
  } else if (strcmp(type, "coin") == 0) {
    handleCoin(doc);
  } else if (strcmp(type, "stock") == 0) {
    handleStock(doc);
  } else if (strcmp(type, "weather") == 0) {
    handleWeather(doc);
  } else if (strcmp(type, "notify") == 0) {
    handleNotify(doc);
  } else if (strcmp(type, "claude") == 0) {
    handleClaude(doc);
  } else if (strcmp(type, "cusage") == 0) {
    handleClaudeUsage(doc);
  } else if (strcmp(type, "cstats") == 0) {
    handleClaudeStats(doc);
  } else if (strcmp(type, "wifi") == 0) {
    handleWifi(doc);
  } else if (strcmp(type, "shot") == 0) {
    handleShot(doc);
#endif
  } else if (strcmp(type, "ping") == 0) {
    sendSimpleFrame("pong");
  } else if (strcmp(type, "recalibrate") == 0) {
    prefs.remove("touchcal");
    sendSimpleFrame("rebooting");
    delay(100);
    ESP.restart();
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n') {
      serialLine.trim();
      processSerialLine(serialLine);
      serialLine = "";
    } else if (c != '\r') {
      if (serialLine.length() < 2048) {
        serialLine += c;
      } else {
        serialLine = "";
      }
    }
  }
}

void setupTouchCalibration() {
  prefs.begin("deskdeck", false);
#ifdef DECK_CYD_TOUCH
  // Fixed raw-to-screen mapping instead of TFT_eSPI's shared-bus calibration.
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS_PIN);
  ts.begin(touchSpi);
  ts.setRotation(DISPLAY_ROTATION);  // touch turns with the picture
#else
  uint16_t calData[5] = {0};
  const size_t length = prefs.getBytesLength("touchcal");

  if (length == sizeof(calData)) {
    prefs.getBytes("touchcal", calData, sizeof(calData));
    tft.setTouch(calData);
    return;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FIRST START", SCREEN_W / 2, 28, 4);
  tft.drawString("Touch calibration", SCREEN_W / 2, 66, 2);
  tft.drawString("Use the stylus and touch", SCREEN_W / 2, 92, 2);
  tft.drawString("the marked corners.", SCREEN_W / 2, 114, 2);
  delay(1200);

  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
  tft.setTouch(calData);
  prefs.putBytes("touchcal", calData, sizeof(calData));
#endif
}
}  // namespace

void setup() {
  // Feed frames are up to ~1.5 KB; the default 256 B RX buffer overflows during long redraws.
  Serial.setRxBufferSize(2048);
  Serial.begin(SERIAL_BAUD);
  serialLine.reserve(2048);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
#ifdef DECK_LED_R
  setupLed();
#endif

#ifdef DECK_UI_V2
  for (Icon &icon : macroIcon) icon = Icon::Controls;
  for (int8_t &color : macroColor) color = -1;
#endif
#ifdef DECK_UI_V2
  loadSettings();
#endif
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  setupTouchCalibration();
#ifdef DECK_UI_V2
  applyRotation();
  uiFontBegin(tft);
  setupBacklight();  // after tft.init(), which drives TFT_BL as a plain output
  lastActivityMs = millis();
#endif
  renderFull();

  JsonDocument hello;
  hello["type"] = "hello";
  hello["device"] = "ESP32 PC Control Deck";
  hello["firmware"] = FIRMWARE_VERSION;
  hello["protocol"] = PROTOCOL_VERSION;
  sendFrame(hello);
#ifdef DECK_UI_V2
  ancsBegin("ESP32 Deck", onPhoneNotification);
  netBegin(onPcNotification, uiAramPacket);
#endif
}

void loop() {
  pollSerial();
  handleTouch();
  updateStatusLed();
#ifdef DECK_UI_V2
  ancsLoop();
  updatePhoneLink();
  netLoop();
  updatePcLink();
  updateScreenSleep();
  uiUpdateStandby();
#endif

  if (fullRedrawRequested) {
    renderFull();
  } else if (statsDirty) {
#ifdef DECK_UI_V2
    uiRenderDynamicV2();
#else
    if (page == Page::Monitor) {
      drawMonitorDynamic();
    } else {
      drawHeaderDynamic();
    }
#endif
    statsDirty = false;
  }

  static uint32_t lastHeaderRefresh = 0;
  if (millis() - lastHeaderRefresh >= 1000) {
    lastHeaderRefresh = millis();
#ifdef DECK_UI_V2
    uiRenderDynamicV2();
#else
    drawHeaderDynamic();
#endif
  }

  delay(8);
}
#endif  // PIO_UNIT_TESTING
