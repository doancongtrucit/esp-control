#ifdef DECK_UI_V2
#include "ancs.h"

#include <NimBLEDevice.h>

#include "ancs_parse.h"

namespace {
const NimBLEUUID ANCS_SERVICE("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
const NimBLEUUID NOTIFICATION_SOURCE("9FBF120D-6301-42D9-8C58-25E699A21DBD");
const NimBLEUUID CONTROL_POINT("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
const NimBLEUUID DATA_SOURCE("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

constexpr uint8_t EVENT_ADDED = 0;
constexpr uint8_t FLAG_PRE_EXISTING = 0x04;
constexpr uint32_t REPLY_TIMEOUT_MS = 3000;
constexpr uint32_t DISCOVERY_DELAY_MS = 1000;
constexpr uint8_t TITLE_MAX = 32;
constexpr uint8_t MESSAGE_MAX = 64;

AncsHandler handler = nullptr;
const char *advertisedName = "";
volatile bool connected = false;
volatile bool encrypted = false;
volatile bool discoveryPending = false;
volatile uint16_t connHandle = 0xFFFF;
volatile uint32_t connectedAtMs = 0;
NimBLERemoteCharacteristic *controlPoint = nullptr;
bool ready = false;

// BLE callbacks run in the NimBLE host task: they only copy bytes under this lock.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
constexpr int UID_QUEUE = 8;
uint32_t uidQueue[UID_QUEUE];
int uidHead = 0;
int uidTail = 0;
uint8_t staging[512];
size_t stagingLen = 0;

AncsReply reply;
bool awaitingReply = false;
// iOS may refuse the subscriptions until the user taps "Allow" on first pairing.
RetryTimer discoveryRetry(4000, 8);
uint32_t requestedAtMs = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &info) override {
    connHandle = info.getConnHandle();
    connected = true;
    encrypted = false;
    connectedAtMs = millis();
    NimBLEDevice::startSecurity(info.getConnHandle());
  }

  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override {
    connected = false;
    encrypted = false;
    discoveryPending = false;
    NimBLEDevice::startAdvertising();
  }

  void onAuthenticationComplete(NimBLEConnInfo &info) override {
    encrypted = info.isEncrypted();
    discoveryPending = encrypted;
  }
};

void onNotificationSource(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  if (length < 8 || data[0] != EVENT_ADDED || (data[1] & FLAG_PRE_EXISTING)) return;
  const uint32_t uid = data[4] | (data[5] << 8) | (data[6] << 16) | (static_cast<uint32_t>(data[7]) << 24);
  portENTER_CRITICAL(&lock);
  const int next = (uidHead + 1) % UID_QUEUE;
  if (next != uidTail) {  // full: drop the newest
    uidQueue[uidHead] = uid;
    uidHead = next;
  }
  portEXIT_CRITICAL(&lock);
}

void onDataSource(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  portENTER_CRITICAL(&lock);
  const size_t n = min(length, sizeof(staging) - stagingLen);
  memcpy(staging + stagingLen, data, n);
  stagingLen += n;
  portEXIT_CRITICAL(&lock);
}

void startAdvertising() {
  NimBLEAdvertisementData adv;
  adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  // List of 128-bit Service Solicitation UUIDs: ANCS, little-endian.
  const uint8_t solicitation[] = {17, 0x15, 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
                                  0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79};
  adv.addData(solicitation, sizeof(solicitation));
  NimBLEAdvertisementData scan;
  scan.setName(advertisedName);
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setAdvertisementData(adv);
  advertising->setScanResponseData(scan);
  advertising->start();
}

// Returns false when ANCS is not available yet; the caller retries.
bool discover() {
  ready = false;
  controlPoint = nullptr;
  NimBLEServer *server = NimBLEDevice::getServer();
  NimBLEConnInfo info = server->getPeerInfoByHandle(connHandle);
  NimBLEClient *client = server->getClient(info);
  if (!client) return false;
  NimBLERemoteService *service = client->getService(ANCS_SERVICE);
  if (!service) return false;
  NimBLERemoteCharacteristic *source = service->getCharacteristic(NOTIFICATION_SOURCE);
  NimBLERemoteCharacteristic *dataSource = service->getCharacteristic(DATA_SOURCE);
  NimBLERemoteCharacteristic *control = service->getCharacteristic(CONTROL_POINT);
  if (!source || !dataSource || !control) return false;
  if (!dataSource->subscribe(true, onDataSource) || !source->subscribe(true, onNotificationSource)) return false;
  controlPoint = control;
  ready = true;
  return true;
}

void requestAttributes(uint32_t uid) {
  const uint8_t cmd[] = {0x00,
                         static_cast<uint8_t>(uid), static_cast<uint8_t>(uid >> 8),
                         static_cast<uint8_t>(uid >> 16), static_cast<uint8_t>(uid >> 24),
                         0x00,                     // AppIdentifier
                         0x01, TITLE_MAX, 0x00,    // Title
                         0x03, MESSAGE_MAX, 0x00};  // Message
  portENTER_CRITICAL(&lock);
  stagingLen = 0;
  portEXIT_CRITICAL(&lock);
  reply.begin(uid);
  awaitingReply = controlPoint->writeValue(cmd, sizeof(cmd), true);
  requestedAtMs = millis();
}
}  // namespace

void ancsBegin(const char *deviceName, AncsHandler onNotification) {
  handler = onNotification;
  advertisedName = deviceName;
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setSecurityAuth(true, false, true);  // bond, no MITM, secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  startAdvertising();
}

void ancsLoop() {
  if (!connected) {
    ready = false;
    controlPoint = nullptr;
    awaitingReply = false;
    discoveryRetry.reset();
    return;
  }
  if (discoveryPending && encrypted && millis() - connectedAtMs > DISCOVERY_DELAY_MS &&
      discoveryRetry.due(millis())) {
    if (discover()) {
      discoveryPending = false;
      discoveryRetry.reset();
    } else {
      discoveryRetry.failed(millis());
    }
  }
  if (!ready) return;

  if (awaitingReply) {
    uint8_t chunk[sizeof(staging)];
    portENTER_CRITICAL(&lock);
    const size_t n = stagingLen;
    memcpy(chunk, staging, n);
    stagingLen = 0;
    portEXIT_CRITICAL(&lock);
    if (n) reply.feed(chunk, n);
    if (reply.complete()) {
      awaitingReply = false;
      if (handler) handler(ancsAppName(reply.app()), reply.title(), reply.message());
    } else if (millis() - requestedAtMs > REPLY_TIMEOUT_MS) {
      awaitingReply = false;  // give up on this one, take the next
    }
    return;
  }

  portENTER_CRITICAL(&lock);
  const bool pending = uidTail != uidHead;
  const uint32_t uid = pending ? uidQueue[uidTail] : 0;
  if (pending) uidTail = (uidTail + 1) % UID_QUEUE;
  portEXIT_CRITICAL(&lock);
  if (pending) requestAttributes(uid);
}

bool ancsConnected() { return connected && encrypted; }
bool ancsReady() { return ready; }
void ancsForgetPairing() { NimBLEDevice::deleteAllBonds(); }

#endif  // DECK_UI_V2
