#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Time at the PC and the auto-lock state from the bridge (host/activity.py):
// {"type":"activity","on":312,"sess":47,"brk":6,"long":95,"h":[24 minutes],"wk":[7 minutes],
//  "late":false,"rem":"stand"|null,"eye":3,"lk":{"on":true,"near":true|false|null,"rssi":-62|null}}
enum class PhoneNear : int8_t { Unknown = -1, Away = 0, Near = 1 };

struct ActivityInfo {
  bool has = false;
  uint16_t todayMin = 0;
  uint16_t sessionMin = 0;  // 0 = on a break
  uint16_t breaks = 0;
  uint16_t longestMin = 0;
  uint8_t hours[24] = {};   // active minutes per hour today
  uint16_t week[7] = {};    // active minutes, 6 days ago .. today
  bool late = false;
  bool standDue = false;    // stand-up reminder waiting
  uint32_t eyeSeq = 0;      // grows by one at each eye-rest reminder
  bool lockOn = false;
  PhoneNear near = PhoneNear::Unknown;
  bool hasRssi = false;
  int16_t rssi = 0;
};

bool parseActivityFrame(JsonVariantConst doc, ActivityInfo &out);

// "5h 12m", "47m".
String activityDuration(uint16_t minutes);
