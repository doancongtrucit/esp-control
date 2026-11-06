#include "deck_settings.h"

const char *const NOTIFY_APPS[NOTIFY_APP_COUNT] = {"Zalo", "Teams", "Outlook", "Messenger",
                                                   "Messages", "Mail", "Phone", "Other"};

namespace {
constexpr uint8_t BLOB_VERSION = 1;
constexpr uint8_t DUTY[BRIGHTNESS_LEVELS] = {25, 60, 110, 175, 255};
constexpr uint32_t SLEEP_MS[SLEEP_OPTIONS] = {0, 60000UL, 300000UL, 900000UL};

enum Flag : uint8_t {
  F_ROTATE = 1 << 0,
  F_PHONE = 1 << 1,
  F_PC = 1 << 2,
  F_HIDE = 1 << 3,
  F_QUIET = 1 << 4,
  F_LED = 1 << 5,
  F_LED_NIGHT = 1 << 6,
  F_MEETING = 1 << 7,
};
}  // namespace

uint8_t backlightDuty(uint8_t level) {
  return DUTY[level < BRIGHTNESS_LEVELS ? level : BRIGHTNESS_LEVELS - 1];
}

uint32_t sleepTimeoutMs(uint8_t index) { return index < SLEEP_OPTIONS ? SLEEP_MS[index] : 0; }

bool inQuietHours(int hour, uint8_t start, uint8_t end) {
  if (hour < 0 || start == end) return false;
  if (start < end) return hour >= start && hour < end;
  return hour >= start || hour < end;  // window wraps past midnight
}

int notifyAppIndex(const String &app) {
  String name = app;
  name.toLowerCase();
  for (int i = 0; i < NOTIFY_APP_COUNT - 1; ++i) {
    String key = NOTIFY_APPS[i];
    key.toLowerCase();
    if (name.indexOf(key) >= 0) return i;
  }
  return NOTIFY_APP_COUNT - 1;
}

bool notificationAllowed(const DeckSettings &s, NotifySource source, const String &app) {
  if (source == NotifySource::Phone ? !s.phoneOn : !s.pcOn) return false;
  return (s.mutedApps & (1u << notifyAppIndex(app))) == 0;
}

void packSettings(const DeckSettings &s, uint8_t *blob) {
  memset(blob, 0, SETTINGS_BLOB_SIZE);
  blob[0] = BLOB_VERSION;
  blob[1] = s.brightness;
  blob[2] = s.sleepIndex;
  blob[3] = (s.rotate180 ? F_ROTATE : 0) | (s.phoneOn ? F_PHONE : 0) | (s.pcOn ? F_PC : 0) |
            (s.hideBody ? F_HIDE : 0) | (s.quietOn ? F_QUIET : 0) | (s.ledOn ? F_LED : 0) |
            (s.ledNight ? F_LED_NIGHT : 0) | (s.meetingTitles ? F_MEETING : 0);
  blob[4] = s.quietStart;
  blob[5] = s.quietEnd;
  blob[6] = s.mutedApps & 0xFF;
  blob[7] = s.mutedApps >> 8;
}

bool unpackSettings(const uint8_t *blob, size_t size, DeckSettings &out) {
  if (size != SETTINGS_BLOB_SIZE || blob[0] != BLOB_VERSION) return false;
  DeckSettings s;
  s.brightness = blob[1] < BRIGHTNESS_LEVELS ? blob[1] : BRIGHTNESS_LEVELS - 1;
  s.sleepIndex = blob[2] < SLEEP_OPTIONS ? blob[2] : 0;
  const uint8_t f = blob[3];
  s.rotate180 = f & F_ROTATE;
  s.phoneOn = f & F_PHONE;
  s.pcOn = f & F_PC;
  s.hideBody = f & F_HIDE;
  s.quietOn = f & F_QUIET;
  s.ledOn = f & F_LED;
  s.ledNight = f & F_LED_NIGHT;
  s.meetingTitles = f & F_MEETING;
  s.quietStart = blob[4] % 24;
  s.quietEnd = blob[5] % 24;
  s.mutedApps = (blob[6] | (blob[7] << 8)) & ((1u << NOTIFY_APP_COUNT) - 1);
  out = s;
  return true;
}
