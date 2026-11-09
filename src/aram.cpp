#include "aram.h"

#include <ArduinoJson.h>

#include "net_notify.h"

bool parseUdpAram(const char *payload, const String &token, AramData &out) {
  if (token.length() == 0 || payload == nullptr || strlen(payload) > UDP_NOTIFY_MAX) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  const char *sent = doc["token"] | "";
  if (token != sent) return false;
  if (strcmp(doc["type"] | "", "aram") != 0) return false;

  AramData d;
  d.inGame = (doc["ig"] | 0) != 0;
  d.seq = doc["seq"] | 0;
  d.champion = doc["ch"] | "";
  d.tier = doc["tr"] | "";
  d.winrate = doc["wr"] | "";
  for (JsonArrayConst row : doc["of"].as<JsonArrayConst>()) {
    if (d.pickCount >= ARAM_PICKS) break;
    AramPick &p = d.picks[d.pickCount++];
    p.name = row[0] | "";
    p.winrate = row[1] | "";
    p.note = row[2] | "";
    p.best = (row[3] | 0) != 0;
  }
  JsonArrayConst combo = doc["cb"].as<JsonArrayConst>();
  if (combo.size() >= 2) {
    d.comboTier = combo[0] | "";
    d.combo = combo[1] | "";
    d.comboHave = combo[2] | "";
  }
  for (JsonArrayConst row : doc["ct"].as<JsonArrayConst>()) {
    if (d.counterCount >= ARAM_COUNTERS) break;
    AramLine &line = d.counters[d.counterCount++];
    line.title = row[0] | "";
    line.detail = row[1] | "";
  }
  d.build = doc["bd"] | "";
  d.owned = doc["ow"] | "";
  out = d;
  return true;
}
