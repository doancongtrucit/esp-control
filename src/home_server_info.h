#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Home server metrics from the bridge (host/home_server.py, read from Home Assistant):
// {"type":"hserver","name":"Home Server","on":true,"ms":23,"ver":"2026.11.1","cpu":12.5,"ram":43.2,
//  "disk":61,"temp":48,"load":0.42,"dn":4.0,"up":1.0,"upt":273600,"upd":2,"bat":null,"t":"12:00"}
// or {"type":"hserver","name":"...","on":false,"err":"offline"|"token"|"error","t":"12:00"}.
// Unknown numbers are NAN.
struct HomeServerInfo {
  bool has = false;
  bool online = false;
  String name;
  String error;    // offline / token / error
  String version;  // Home Assistant version
  String time;     // bridge time of the reading, "HH:MM"
  int latencyMs = -1;
  float cpu = NAN, ram = NAN, disk = NAN, temp = NAN, load = NAN;
  float downMbps = NAN, upMbps = NAN, battery = NAN;
  int32_t uptimeSec = -1;
  uint16_t updates = 0;
};

bool parseHomeServerFrame(JsonVariantConst doc, HomeServerInfo &out);

// "3d 4h", "5h 12m", "42m"; "--" when unknown.
String uptimeText(int32_t seconds);
