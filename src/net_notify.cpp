#include "net_notify.h"

#include <ArduinoJson.h>

bool parseUdpNotify(const char *payload, const String &token, String &app, String &from, String &body) {
  if (token.length() == 0 || payload == nullptr || strlen(payload) > UDP_NOTIFY_MAX) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  const char *sent = doc["token"] | "";
  if (token != sent) return false;
  app = doc["app"] | "";
  from = doc["from"] | "";
  body = doc["body"] | "";
  return true;
}

#ifdef DECK_UI_V2
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_coexist.h>

#include "aram.h"

namespace {
constexpr uint32_t RETRY_MS = 30000;
constexpr char PREFS_NAMESPACE[] = "deckwifi";

WifiSettings settings;
NetHandler handler = nullptr;
AramHandler aramHandler = nullptr;
WiFiUDP udp;
bool udpOpen = false;
bool attempted = false;
uint32_t lastAttemptMs = 0;
uint32_t rejected = 0;
LinkHandler linkHandler = nullptr;
bool ntpStarted = false;
constexpr size_t UDP_READ_MAX = 1500;

void load() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true);
  settings.ssid = prefs.getString("ssid", "");
  settings.pass = prefs.getString("pass", "");
  settings.token = prefs.getString("token", "");
  settings.port = prefs.getUShort("port", 4210);
  prefs.end();
}

void save() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putString("ssid", settings.ssid);
  prefs.putString("pass", settings.pass);
  prefs.putString("token", settings.token);
  prefs.putUShort("port", settings.port);
  prefs.end();
}

void handlePlainPacket(const char *buffer);

void connect() {
  attempted = true;
  lastAttemptMs = millis();
  WiFi.disconnect();
  WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());  // returns at once
}
}  // namespace

void netBegin(NetHandler onNotify, AramHandler onAram) {
  handler = onNotify;
  aramHandler = onAram;
  load();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);  // required while BLE is running
  // Wi-Fi and BLE share one radio; favour Wi-Fi so the PC link stays responsive (ANCS needs little air time).
  esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
  // netLoop owns retries (every RETRY_MS); the core's auto-reconnect would
  // retry a wrong password back to back and starve BLE.
  WiFi.setAutoReconnect(false);
}

void netConfigure(const WifiSettings &next) {
  if (next.ssid == settings.ssid && next.pass == settings.pass && next.token == settings.token &&
      next.port == settings.port) {
    return;
  }
  settings = next;
  save();
  if (udpOpen) {
    udp.stop();
    udpOpen = false;
  }
  if (settings.ssid.length()) connect();
}

void netLoop() {
  if (settings.ssid.length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (udpOpen) {
      udp.stop();
      udpOpen = false;
    }
    if (!attempted || millis() - lastAttemptMs > RETRY_MS) connect();
    return;
  }
  if (!udpOpen) udpOpen = udp.begin(settings.port);
  if (!ntpStarted) {  // UTC; the bridge sends the UTC offset, and the clock runs without the PC
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
  }

  for (int packets = 0; packets < 8; ++packets) {  // a long frame arrives as a few datagrams
    const int size = udp.parsePacket();
    if (size <= 0) return;
    if (static_cast<size_t>(size) > UDP_READ_MAX) {
      udp.flush();
      ++rejected;
      continue;
    }
    static char buffer[UDP_READ_MAX + 1];
    const int n = udp.read(buffer, UDP_READ_MAX);
    buffer[max(n, 0)] = '\0';
    if (n >= 4 && memcmp(buffer, "DK1 ", 4) == 0) {
      if (linkHandler) linkHandler(buffer, n, udp.remoteIP(), udp.remotePort());
      continue;
    }
    if (static_cast<size_t>(n) > UDP_NOTIFY_MAX) {
      ++rejected;
      continue;
    }
    handlePlainPacket(buffer);
  }
}

namespace {
void handlePlainPacket(const char *buffer) {
  // ARAM packets first: the notification parser accepts any JSON with the right token.
  if (aramHandler) {
    AramData aram;
    if (parseUdpAram(buffer, settings.token, aram)) {
      aramHandler(aram);
      return;
    }
  }
  String app, from, body;
  if (!parseUdpNotify(buffer, settings.token, app, from, body)) {
    ++rejected;
    return;
  }
  if (handler) handler(app, from, body);
}
}  // namespace

bool netConnected() { return settings.ssid.length() && WiFi.status() == WL_CONNECTED; }
String netIp() { return netConnected() ? WiFi.localIP().toString() : String(); }
uint16_t netPort() { return settings.port; }
uint32_t netRejectedCount() { return rejected; }
int netRssi() { return netConnected() ? WiFi.RSSI() : 0; }

void netSetLinkHandler(LinkHandler h) { linkHandler = h; }

bool netSendTo(const IPAddress &ip, uint16_t port, const char *data, size_t len) {
  if (!udpOpen || !netConnected()) return false;
  if (!udp.beginPacket(ip, port)) return false;
  udp.write(reinterpret_cast<const uint8_t *>(data), len);
  return udp.endPacket();
}

const String &netToken() { return settings.token; }

void netForget() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

#endif  // DECK_UI_V2
