#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Petrolimex retail fuel prices from the bridge (host/fuel_prices.py):
// {"type":"fuel","z":1,"t":"20:05","items":[["E10 RON95 M5",22650,-500],...]}
// Prices in VND per litre; the third number is the change since the previous price set.
constexpr uint8_t FUEL_ROWS = 6;

struct FuelInfo {
  bool has = false;
  uint8_t zone = 1;
  String time;
  uint8_t count = 0;
  String name[FUEL_ROWS];
  long price[FUEL_ROWS] = {};
  long change[FUEL_ROWS] = {};
};

bool parseFuelFrame(JsonVariantConst doc, FuelInfo &out);

// 22650 -> "22.650" (Vietnamese thousands separator).
String fuelPriceText(long vnd);
