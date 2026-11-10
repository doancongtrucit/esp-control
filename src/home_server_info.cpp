#include "home_server_info.h"

namespace {
float number(JsonVariantConst v) { return v.is<float>() || v.is<int>() ? v.as<float>() : NAN; }
}  // namespace

bool parseHomeServerFrame(JsonVariantConst doc, HomeServerInfo &out) {
  if (strcmp(doc["type"] | "", "hserver") != 0) return false;
  HomeServerInfo next;
  next.has = true;
  next.online = doc["on"] | false;
  next.name = doc["name"] | "Home Server";
  next.error = doc["err"] | "";
  next.version = doc["ver"] | "";
  next.time = doc["t"] | "";
  next.latencyMs = doc["ms"] | -1;
  next.cpu = number(doc["cpu"]);
  next.ram = number(doc["ram"]);
  next.disk = number(doc["disk"]);
  next.temp = number(doc["temp"]);
  next.load = number(doc["load"]);
  next.downMbps = number(doc["dn"]);
  next.upMbps = number(doc["up"]);
  next.battery = number(doc["bat"]);
  next.uptimeSec = doc["upt"].is<int>() ? doc["upt"].as<int32_t>() : -1;
  next.updates = doc["upd"] | 0;
  out = next;
  return true;
}

String uptimeText(int32_t seconds) {
  if (seconds < 0) return "--";
  const int32_t minutes = seconds / 60, hours = minutes / 60, days = hours / 24;
  if (days > 0) return String(days) + "d " + String(hours % 24) + "h";
  if (hours > 0) return String(hours) + "h " + String(minutes % 60) + "m";
  return String(minutes) + "m";
}
