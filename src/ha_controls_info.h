#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "icons.h"

// Home screen tiles from the bridge (host/ha_controls.py):
// {"type":"hactl","items":[{"n":"Air Con","i":"ac","k":"climate","on":0,"v":"24C off"},...],
//  "ac":{"ix":0,"on":false,"t":24,"min":16,"max":30,"mode":"cool","modes":["heat_cool","cool","dry"],
//        "fan":"auto","fans":["auto","low","mid","high"]}}
constexpr uint8_t HA_TILES = 12;
constexpr uint8_t HA_MODES = 4;
constexpr uint8_t HA_FANS = 5;

enum class HaKind : uint8_t { Toggle, Run, Climate };

struct HaTile {
  String name;
  Icon icon = Icon::Power;
  HaKind kind = HaKind::Toggle;
  int8_t on = -1;  // 1 on, 0 off, -1 unknown / not a switch
  String value;    // "ON", "OFF", "Run", "24C Cool", "N/A"
  bool confirm = false;  // hold 1 s before it runs (a PC switch)
};

struct HaAc {
  bool has = false;
  uint8_t index = 0;  // tile of the air conditioner
  bool on = false;
  int temp = 0, minTemp = 16, maxTemp = 30;
  String mode;
  String modes[HA_MODES];
  uint8_t modeCount = 0;
  String fan;
  String fans[HA_FANS];
  uint8_t fanCount = 0;
};

struct HaState {
  bool has = false;
  uint8_t count = 0;
  HaTile tiles[HA_TILES];
  HaAc ac;
};

bool parseHaFrame(JsonVariantConst doc, HaState &out);

// Short label of a Home Assistant hvac / fan mode: "heat_cool" -> "Auto", "cool" -> "Cool", "mid" -> "Mid".
String haModeLabel(const String &mode);
