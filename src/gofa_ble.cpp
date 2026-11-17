#ifdef DECK_UI_V2
#include "gofa_ble.h"

#include <NimBLEDevice.h>

#include <mutex>

#include "ancs.h"

// Turn icon ids the deck's HUD screen (drawHudV2 in ui_v2_screens.h) draws.
enum { kTurnStraight = 0, kTurnLeft = 1, kTurnRight = 2, kTurnSlightLeft = 3, kTurnSlightRight = 4,
       kTurnUTurn = 5, kTurnRoundabout = 6 };

namespace {

// GOFA service and characteristics (hud.platform.gofa.vn/docs).
const NimBLEUUID SVC_GOFA((uint16_t)0x2A60);
const NimBLEUUID CHR_SIGN((uint16_t)0x3030);
const NimBLEUUID CHR_SPEED((uint16_t)0x3031);
const NimBLEUUID CHR_SOUND((uint16_t)0x3032);
const NimBLEUUID CHR_OVER((uint16_t)0x3033);
const NimBLEUUID CHR_NAV((uint16_t)0x3038);
const NimBLEUUID CHR_TIME((uint16_t)0x3039);

const NimBLEUUID SVC_DEVINFO((uint16_t)0x180A);
const NimBLEUUID CHR_FW((uint16_t)0x2A26);
const NimBLEUUID CHR_HW((uint16_t)0x2A27);
const NimBLEUUID CHR_MODEL((uint16_t)0x2A24);
const NimBLEUUID CHR_ERR((uint16_t)0x2A3F);

std::mutex mux;  // BLE writes land on the NimBLE task
hud::NavState nav;
bool hasData = false;
uint32_t lastFrameMs = 0;
bool demo = false;

// ---- minimal protobuf reader (varint / len-delimited only) ----
struct PbReader {
  const uint8_t *p, *end;
  bool ok = true;
  uint64_t varint() {
    uint64_t v = 0; int shift = 0;
    while (p < end && shift < 64) {
      uint8_t b = *p++;
      v |= uint64_t(b & 0x7F) << shift;
      if (!(b & 0x80)) return v;
      shift += 7;
    }
    ok = false; return v;
  }
};

// Calls fn(field, wiretype, reader) for each top-level field. For wiretype 2
// the reader is positioned at the bytes and `len` gives their length.
template <typename F>
void pbWalk(const uint8_t *data, size_t n, F fn) {
  PbReader r;
  r.p = data;
  r.end = data + n;
  while (r.p < r.end && r.ok) {
    uint64_t key = r.varint();
    if (!r.ok) break;
    uint32_t field = key >> 3;
    uint32_t wt = key & 7;
    if (wt == 0) {
      uint64_t v = r.varint();
      fn(field, wt, v, (const uint8_t *)nullptr, (size_t)0);
    } else if (wt == 2) {
      uint64_t len = r.varint();
      if (!r.ok || r.p + len > r.end) break;
      fn(field, wt, (uint64_t)0, r.p, (size_t)len);
      r.p += len;
    } else if (wt == 5) {
      if (r.p + 4 > r.end) break;
      r.p += 4;
    } else if (wt == 1) {
      if (r.p + 8 > r.end) break;
      r.p += 8;
    } else {
      break;  // unknown wire type
    }
  }
}

int signIdToSpeedLimit(uint32_t id) {
  switch (id) {
    case 3: return 30; case 4: return 40; case 5: return 50; case 6: return 60;
    case 7: return 70; case 8: return 80; case 9: return 90; case 10: return 100;
    case 11: return 120; default: return -1;
  }
}
bool signIdIsCamera(uint32_t id) { return id == 32 || id == 33 || id == 34 || id == 36; }

int maneuverToIcon(uint32_t m) {
  switch (m) {
    case 1: return kTurnStraight;
    case 2: return kTurnUTurn;
    case 3: case 7: return kTurnLeft;
    case 4: case 8: return kTurnRight;
    case 5: return kTurnSlightLeft;
    case 6: return kTurnSlightRight;
    case 9: return kTurnRoundabout;
    default: return -1;
  }
}

// Decode one write into nav (under mux). Returns true if something changed.
void applyGofa(uint16_t chr, const uint8_t *data, size_t n) {
  if (chr == 0x3031) {  // SPEED: field1 = speed
    pbWalk(data, n, [&](uint32_t f, uint32_t wt, uint64_t v, const uint8_t *, size_t) {
      if (f == 1 && wt == 0) nav.speedKmh = (int)v;
    });
  } else if (chr == 0x3030) {  // SIGN: f1=current id, f2=id2,f3=dist2, f4=id3,f5=dist3
    uint32_t id1 = 0, id2 = 0, d2 = 0, id3 = 0, d3 = 0;
    pbWalk(data, n, [&](uint32_t f, uint32_t wt, uint64_t v, const uint8_t *, size_t) {
      if (wt != 0) return;
      if (f == 1) id1 = v; else if (f == 2) id2 = v; else if (f == 3) d2 = v;
      else if (f == 4) id3 = v; else if (f == 5) d3 = v;
    });
    nav.speedLimit = signIdToSpeedLimit(id1);  // -1 clears when no limit sign
    // Sort every sign slot into camera / upcoming-limit / other-warning. id1 is
    // the current sign (no distance); id2/id3 are upcoming signs with distance.
    int cam = -1, warn = -1, warnDist = -1, limAhead = -1, limAheadDist = -1;
    auto sort = [&](uint32_t id, int dist) {
      if (id == 0) return;
      const int lim = signIdToSpeedLimit(id);
      if (signIdIsCamera(id)) { if (cam < 0) cam = dist; }
      else if (lim > 0) {  // a speed-limit sign ahead (with distance)
        if (dist >= 0 && limAhead < 0) { limAhead = lim; limAheadDist = dist; }
      } else {  // anything else -> generic warning
        if (warn < 0) { warn = (int)id; warnDist = dist; }
      }
    };
    sort(id1, -1);          // current sign: "now"
    sort(id2, (int)d2);     // upcoming
    sort(id3, (int)d3);
    nav.cameraDistM = cam;
    nav.warnSignId = warn;
    nav.warnSignDistM = warnDist;
    nav.limitAheadKmh = limAhead;
    nav.limitAheadDistM = limAheadDist;
  } else if (chr == 0x3038) {  // NAV
    uint32_t state = 0, maneuver = 0, dist = 0; std::string road, next;
    pbWalk(data, n, [&](uint32_t f, uint32_t wt, uint64_t v, const uint8_t *b, size_t len) {
      if (f == 1 && wt == 0) state = v;
      else if (f == 2 && wt == 2) road.assign((const char *)b, len);   // current road
      else if (f == 3 && wt == 2) next.assign((const char *)b, len);   // road to turn onto
      else if (f == 4 && wt == 0) maneuver = v;
      else if (f == 5 && wt == 0) dist = v;
    });
    if (state == 2) {
      nav.turnIcon = maneuverToIcon(maneuver);
      nav.turnDistM = (int)dist;
      nav.street = road;
      nav.nextStreet = next;
    } else {
      nav.turnIcon = -1; nav.turnDistM = -1; nav.street.clear(); nav.nextStreet.clear();
    }
  }
  // SOUND/OVER/TIME: logged only for now.
}

class GofaWrite : public NimBLECharacteristicCallbacks {
 public:
  explicit GofaWrite(uint16_t id) : id_(id) {}
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    NimBLEAttValue v = c->getValue();
    Serial.printf("GOFA 0x%04X %s\n", id_, hud::toHex(v.data(), v.size()).c_str());
    std::lock_guard<std::mutex> lock(mux);
    applyGofa(id_, v.data(), v.size());
    hasData = true;
    lastFrameMs = millis();
  }
 private:
  uint16_t id_;
};

void addWrite(NimBLEService *svc, const NimBLEUUID &uuid, uint16_t id) {
  NimBLECharacteristic *c =
      svc->createCharacteristic(uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  c->setCallbacks(new GofaWrite(id));
}

class DisRead : public NimBLECharacteristicCallbacks {
 public:
  explicit DisRead(uint16_t id) : id_(id) {}
  void onRead(NimBLECharacteristic *, NimBLEConnInfo &) override {
    Serial.printf("GOFA read 0x%04X\n", id_);
  }
 private:
  uint16_t id_;
};

void addRead(NimBLEService *svc, const NimBLEUUID &uuid, uint16_t id, const char *value) {
  NimBLECharacteristic *c = svc->createCharacteristic(uuid, NIMBLE_PROPERTY::READ);
  c->setValue((const uint8_t *)value, strlen(value));
  c->setCallbacks(new DisRead(id));
}

void addReadByte(NimBLEService *svc, const NimBLEUUID &uuid, uint16_t id, uint8_t value) {
  NimBLECharacteristic *c = svc->createCharacteristic(uuid, NIMBLE_PROPERTY::READ);
  c->setValue(&value, 1);
  c->setCallbacks(new DisRead(id));
}

}  // namespace

void gofaBleAttach() {
  NimBLEServer *server = NimBLEDevice::getServer();
  if (!server) return;
  NimBLEService *gofa = server->createService(SVC_GOFA);
  addWrite(gofa, CHR_SIGN, 0x3030);
  addWrite(gofa, CHR_SPEED, 0x3031);
  addWrite(gofa, CHR_SOUND, 0x3032);
  addWrite(gofa, CHR_OVER, 0x3033);
  addWrite(gofa, CHR_NAV, 0x3038);
  addWrite(gofa, CHR_TIME, 0x3039);
  gofa->start();

  NimBLEService *info = server->createService(SVC_DEVINFO);
  addRead(info, CHR_FW, 0x2A26, "1.0.0");
  addRead(info, CHR_HW, 0x2A27, "ESP32-2432S028");
  addRead(info, CHR_MODEL, 0x2A24, "EVCarHUD 2.8");
  addReadByte(info, CHR_ERR, 0x2A3F, 0x00);  // error code 0 = no error (byte, not "0")
  info->start();
}

void gofaBleLoop() {
  if (demo) {
    const uint32_t t = (millis() / 100) % 300;
    nav.speedKmh = 20 + int(25 + 25 * sinf(t * 2 * PI / 300.0f));
    nav.speedLimit = t < 150 ? 50 : 60;
    nav.cameraDistM = int(300 - t); if (nav.cameraDistM < 0) nav.cameraDistM = -1;
    nav.turnDistM = (int(300 - t) / 10) * 10;
    const uint32_t phase = (millis() / 12000) % 3;
    nav.turnIcon = phase == 0 ? kTurnLeft : phase == 1 ? kTurnRight : kTurnRoundabout;
    nav.street = phase == 1 ? "Nguyễn Tuân" : "Lê Văn Lương";
    nav.nextStreet = phase == 0 ? "Phan Đăng Lưu" : phase == 1 ? "Hoàng Đạo Thúy" : "Ngã 6 Nguyễn Trãi";
    nav.warnSignId = t < 150 ? 18 : -1;        // 18 = toll, to preview the warning slot
    nav.warnSignDistM = t < 150 ? int(300 - t) : -1;
    nav.weather.clear();
    hasData = true; lastFrameMs = millis();
  }
}

const hud::NavState &gofaNav() { return nav; }
bool gofaHasData() { return hasData; }
uint32_t gofaLastFrameMs() { return lastFrameMs; }

void gofaSetDemo(bool on) {
  demo = on;
  if (!on) { nav = hud::NavState{}; hasData = false; lastFrameMs = 0; }
}
bool gofaDemo() { return demo; }

#endif  // DECK_UI_V2
