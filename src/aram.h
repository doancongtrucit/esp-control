#pragma once

#include <Arduino.h>

// ARAM Helper (PC app) pushes a compact game summary over the Wi-Fi UDP channel
// (same port and token as PC notifications):
// {"token":"...","type":"aram","seq":7,"ig":1,"ch":"Yasuo","tr":"S+","wr":"58.3%",
//  "of":[["name","71.0%","+3 A",1],...],      offered augments, best first, 1 = recommended
//  "cb":["S","A + B + C","1/3"],               best combo with an offered augment
//  "ct":[["item","reason"],...],               up to 2 items against the enemy team
//  "bd":"core item > core item","ow":"augments already taken"}
// The PC sends only ASCII and Vietnamese letters, so drawViText can draw every field.
constexpr uint8_t ARAM_PICKS = 3;
constexpr uint8_t ARAM_COUNTERS = 2;
constexpr uint32_t ARAM_STALE_MS = 15000;  // no packet for this long: the game is over

struct AramPick {
  String name;
  String winrate;
  String note;  // e.g. "+3 A" (combo bonus and tier)
  bool best = false;
};

struct AramLine {
  String title;
  String detail;
};

struct AramData {
  bool inGame = false;
  uint32_t seq = 0;
  String champion;
  String tier;
  String winrate;
  uint8_t pickCount = 0;
  AramPick picks[ARAM_PICKS];
  String comboTier;
  String combo;
  String comboHave;
  uint8_t counterCount = 0;
  AramLine counters[ARAM_COUNTERS];
  String build;
  String owned;
};

// Pure: true only for valid JSON within UDP_NOTIFY_MAX bytes with "type":"aram"
// and a token that matches a non-empty expected token.
bool parseUdpAram(const char *payload, const String &token, AramData &out);
