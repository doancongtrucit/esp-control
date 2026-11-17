#ifdef DECK_UI_V2
#include "hud_ble.h"

#include <NimBLEDevice.h>

#include <mutex>

#include "ancs.h"

namespace {

// Confirmed from the decompiled app vn.vietmap.live 3.4.5 (class Da/i.java):
// service fff0, notify fff1 (HUD -> app), write fff2 (app -> HUD).
const char *SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb";
const char *TX_CHAR_UUID = "0000fff1-0000-1000-8000-00805f9b34fb";
const char *RX_CHAR_UUID = "0000fff2-0000-1000-8000-00805f9b34fb";

// Declared by the stock H1V in the capture; the app reads the model from this.
const char *DEVICE_INFO =
    "MODEL:H1V,HW:1.6.4,FW:1.3.6,PROTOCOL:2.2.2, OBDV:,COMPILE:Feb 27 2021-12:13:56";

NimBLECharacteristic *txChar = nullptr;

std::mutex mux;  // the BLE write callback runs on the NimBLE task, loop() on another
hud::Parser parser;
std::vector<hud::Frame> pending;

hud::NavState nav;      // owned by loop()/the UI thread
bool hasData = false;
uint32_t lastFrameMs = 0;
bool demo = false;

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    NimBLEAttValue v = c->getValue();
    std::lock_guard<std::mutex> lock(mux);
    parser.feed(v.data(), v.size());
    for (auto &f : parser.frames()) pending.push_back(std::move(f));
    parser.frames().clear();
  }
};

void sendDeviceInfo() {
  if (!txChar) return;
  std::vector<uint8_t> payload(DEVICE_INFO, DEVICE_INFO + strlen(DEVICE_INFO) + 1);
  auto frame = hud::buildFrame(uint32_t(time(nullptr)), hud::kDeviceInfo, payload.data(), payload.size());
  txChar->setValue(frame.data(), frame.size());
  txChar->notify();
}

}  // namespace

void hudBleAttach() {
  NimBLEServer *server = NimBLEDevice::getServer();
  if (!server) return;  // ancsBegin() must run first
  NimBLEService *svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic *rx =
      svc->createCharacteristic(RX_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  txChar = svc->createCharacteristic(TX_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  svc->start();
  // Make the VIETMAP scan find us: advertise fff0 alongside the ANCS solicitation.
  ancsAdvertiseService16(0xFFF0);
}

void hudBleLoop() {
  if (demo) {
    const uint32_t t = (millis() / 100) % 300;  // 30 s loop, tenths of a second
    nav.speedKmh = 20 + int(25 + 25 * sinf(t * 2 * PI / 300.0f));
    nav.speedLimit = t < 150 ? 50 : 60;
    nav.cameraDistM = int(300 - t);
    if (nav.cameraDistM < 0) nav.cameraDistM = -1;
    nav.turnDistM = (int(300 - t) / 10) * 10;
    nav.turnIcon = (millis() / 15000) % 2 ? 1 : 2;  // left / right
    nav.street = (millis() / 15000) % 2 ? "Le Van Luong" : "Nguyen Tuan";
    nav.weather = "25 do, nhieu may";
    lastFrameMs = millis();
    hasData = true;
    return;
  }
  std::vector<hud::Frame> frames;
  {
    std::lock_guard<std::mutex> lock(mux);
    frames.swap(pending);
  }
  for (const auto &f : frames) {
    lastFrameMs = millis();
    hasData = true;
    if (f.type == hud::kDeviceInfo || f.type == hud::kUnknown01) {
      // The app polls for HUD info early in the handshake; answer with H1V.
    }
    hud::applyFrame(f, nav);
  }
}

const hud::NavState &hudNav() { return nav; }
bool hudHasData() { return hasData; }
uint32_t hudLastFrameMs() { return lastFrameMs; }

void hudSetDemo(bool on) {
  demo = on;
  if (!on) {
    nav = hud::NavState{};
    hasData = false;
    lastFrameMs = 0;
  }
}
bool hudDemo() { return demo; }

#endif  // DECK_UI_V2
