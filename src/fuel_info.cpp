#include "fuel_info.h"

bool parseFuelFrame(JsonVariantConst doc, FuelInfo &out) {
  if (strcmp(doc["type"] | "", "fuel") != 0) return false;
  FuelInfo next;
  next.has = true;
  next.zone = doc["z"] | 1;
  next.time = doc["t"] | "";
  for (JsonVariantConst row : doc["items"].as<JsonArrayConst>()) {
    if (next.count >= FUEL_ROWS) break;
    next.name[next.count] = row[0] | "";
    next.price[next.count] = row[1] | 0L;
    next.change[next.count] = row[2] | 0L;
    ++next.count;
  }
  out = next;
  return true;
}

String fuelPriceText(long vnd) {
  const bool negative = vnd < 0;
  unsigned long v = negative ? -vnd : vnd;
  String digits = String(v);
  String out;
  for (int i = 0; i < static_cast<int>(digits.length()); ++i) {
    if (i && (digits.length() - i) % 3 == 0) out += '.';
    out += digits[i];
  }
  return negative ? "-" + out : out;
}
