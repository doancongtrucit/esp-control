#pragma once

#include <Arduino.h>

#include "notify_store.h"

// User settings kept in NVS as one small versioned blob.
constexpr uint8_t BRIGHTNESS_LEVELS = 5;
constexpr uint8_t SLEEP_OPTIONS = 4;  // never, 1, 5, 15 minutes
constexpr uint8_t NOTIFY_APP_COUNT = 8;
// Apps that can be muted one by one; the last entry catches every other app.
extern const char *const NOTIFY_APPS[NOTIFY_APP_COUNT];

struct DeckSettings {
  uint8_t brightness = BRIGHTNESS_LEVELS - 1;
  uint8_t sleepIndex = 0;
  bool rotate180 = true;
  bool phoneOn = true;
  bool pcOn = true;
  bool hideBody = false;
  bool quietOn = false;
  uint8_t quietStart = 22;  // hour, inclusive
  uint8_t quietEnd = 7;     // hour, exclusive
  uint16_t mutedApps = 0;   // bit i mutes NOTIFY_APPS[i]
  bool ledOn = true;
  bool ledNight = true;     // LED off during quiet hours
  bool meetingTitles = false;
};

constexpr size_t SETTINGS_BLOB_SIZE = 16;

uint8_t backlightDuty(uint8_t level);
uint32_t sleepTimeoutMs(uint8_t index);
// hour < 0 means the clock is unknown: never quiet.
bool inQuietHours(int hour, uint8_t start, uint8_t end);
int notifyAppIndex(const String &app);
bool notificationAllowed(const DeckSettings &s, NotifySource source, const String &app);
void packSettings(const DeckSettings &s, uint8_t *blob);
// False (and `out` untouched) for a blob of the wrong size or version.
bool unpackSettings(const uint8_t *blob, size_t size, DeckSettings &out);
