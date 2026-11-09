#pragma once

#include <Arduino.h>

// PC notifications over Wi-Fi. The bridge provisions Wi-Fi credentials and a
// shared token over USB ({"type":"wifi",...}); they are kept in Preferences,
// never compiled in. Each UDP datagram is
// {"token":"...","app":"...","from":"...","body":"..."}.
struct WifiSettings {
  String ssid;
  String pass;
  String token;
  uint16_t port = 4210;
};

constexpr size_t UDP_NOTIFY_MAX = 1024;

// Pure: true only for valid JSON within UDP_NOTIFY_MAX bytes whose token
// matches a non-empty expected token.
bool parseUdpNotify(const char *payload, const String &token, String &app, String &from, String &body);

using NetHandler = void (*)(const String &app, const String &from, const String &body);
struct AramData;
using AramHandler = void (*)(const AramData &data);

// onAram receives {"type":"aram"} packets (ARAM Helper on the PC); every other
// valid datagram is a notification.
void netBegin(NetHandler handler, AramHandler onAram = nullptr);  // loads saved settings
void netConfigure(const WifiSettings &settings);  // saves and reconnects when changed
void netLoop();  // never blocks; retries every 30 s while down
bool netConnected();
String netIp();
uint16_t netPort();
uint32_t netRejectedCount();
int netRssi();     // dBm, 0 when not connected
void netForget();  // erases saved Wi-Fi credentials and token
