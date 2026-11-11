#include "ha_controls_info.h"

bool parseHaFrame(JsonVariantConst doc, HaState &out) {
  if (strcmp(doc["type"] | "", "hactl") != 0) return false;
  HaState next;
  next.has = true;
  JsonArrayConst items = doc["items"];
  for (JsonVariantConst item : items) {
    if (next.count >= HA_TILES) break;
    HaTile &t = next.tiles[next.count++];
    t.name = item["n"] | "";
    t.icon = iconFromName(item["i"] | "power");
    const char *kind = item["k"] | "toggle";
    t.kind = strcmp(kind, "climate") == 0 ? HaKind::Climate : (strcmp(kind, "run") == 0 ? HaKind::Run : HaKind::Toggle);
    t.on = item["on"] | -1;
    t.value = item["v"] | "";
    t.confirm = (item["c"] | 0) != 0;
  }
  JsonVariantConst ac = doc["ac"];
  if (ac.is<JsonObjectConst>()) {
    HaAc &a = next.ac;
    a.has = true;
    a.index = ac["ix"] | 0;
    a.on = ac["on"] | false;
    a.temp = ac["t"] | 0;
    a.minTemp = ac["min"] | 16;
    a.maxTemp = ac["max"] | 30;
    a.mode = ac["mode"] | "";
    for (JsonVariantConst m : ac["modes"].as<JsonArrayConst>())
      if (a.modeCount < HA_MODES) a.modes[a.modeCount++] = m.as<const char *>();
    a.fan = ac["fan"] | "";
    for (JsonVariantConst f : ac["fans"].as<JsonArrayConst>())
      if (a.fanCount < HA_FANS) a.fans[a.fanCount++] = f.as<const char *>();
  }
  out = next;
  return true;
}

String haModeLabel(const String &mode) {
  if (mode == "heat_cool" || mode == "auto") return "Auto";
  if (mode == "fan_only") return "Fan";
  if (mode.length() == 0) return "";
  String label = mode;
  label.replace("_", " ");
  label.setCharAt(0, toupper(label[0]));
  return label;
}
