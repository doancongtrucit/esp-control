#include "activity_info.h"

bool parseActivityFrame(JsonVariantConst doc, ActivityInfo &out) {
  if (strcmp(doc["type"] | "", "activity") != 0) return false;
  ActivityInfo next;
  next.has = true;
  next.todayMin = doc["on"] | 0;
  next.sessionMin = doc["sess"] | 0;
  next.breaks = doc["brk"] | 0;
  next.longestMin = doc["long"] | 0;
  JsonArrayConst hours = doc["h"];
  for (int i = 0; i < 24 && i < static_cast<int>(hours.size()); ++i) next.hours[i] = min(60, hours[i] | 0);
  JsonArrayConst week = doc["wk"];
  for (int i = 0; i < 7 && i < static_cast<int>(week.size()); ++i) next.week[i] = week[i] | 0;
  next.late = doc["late"] | false;
  next.standDue = strcmp(doc["rem"] | "", "stand") == 0;
  next.eyeSeq = doc["eye"] | 0;
  JsonVariantConst lk = doc["lk"];
  next.lockOn = lk["on"] | false;
  if (lk["near"].is<bool>()) next.near = lk["near"].as<bool>() ? PhoneNear::Near : PhoneNear::Away;
  if (lk["rssi"].is<int>()) {
    next.hasRssi = true;
    next.rssi = lk["rssi"].as<int>();
  }
  out = next;
  return true;
}

String activityDuration(uint16_t minutes) {
  if (minutes < 60) return String(minutes) + "m";
  return String(minutes / 60) + "h " + String(minutes % 60) + "m";
}
