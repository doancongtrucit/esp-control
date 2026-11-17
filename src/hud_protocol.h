#pragma once

// VIETMAP HUD frame codec, worked out from a public Bluetooth capture of
// VIETMAP LIVE (Android) talking to a VIETMAP H1V over SPP.
//
//   A5 5A 37 C3 | len (u32 BE) | timestamp (u32 BE, unix) | type (u8) | payload | trailer (8 B)
//
// len counts everything after the len field: 4 + 1 + payload + 8.
// The trailer is deterministic per frame but is not a plain MD5/SHA/CRC32 of
// the frame; it is probably keyed. Until the algorithm is known, outgoing
// frames carry kTrailerUnknown and incoming trailers are not checked.
//
// Pure C++ (no Arduino) so the native unit tests can run on the PC.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hud {

constexpr uint8_t kMagic[4] = {0xA5, 0x5A, 0x37, 0xC3};
constexpr size_t kHeaderLen = 13;   // magic + len + timestamp + type
constexpr size_t kTrailerLen = 8;
constexpr size_t kMaxFrameLen = 1024;

// Frame types seen in the capture. Names ending in Guess are not confirmed.
enum Type : uint8_t {
  kUnknown01 = 0x01,      // payload 01 86 A0 00, sent about once a second
  kUnknown02 = 0x02,      // payload FF FF, once a second
  kTimeSync = 0x0A,       // payload = unix time (u32 BE)
  kUnknown0B = 0x0B,      // payload 02 30
  kDeviceInfo = 0x0E,     // HUD -> app: "MODEL:H1V,HW:...,FW:...,PROTOCOL:2.2.2,..."
  kUnknown13 = 0x13,      // 21 01 13 / 01 02 00 01 / 01 02 FF 01
  kAlertGuess = 0x33,     // [kind u8][value u16 BE]: 10 01 2C, 12 00 46
  kWeather = 0x40,        // contains a UTF-8 line such as "25° mây đen u ám"
};

struct Frame {
  uint32_t timestamp = 0;
  uint8_t type = 0;
  std::vector<uint8_t> payload;
  uint8_t trailer[kTrailerLen] = {};
};

// Builds one frame. The trailer is filled by trailerFn when given, otherwise zeros.
using TrailerFn = void (*)(const uint8_t *frameWithoutTrailer, size_t len, uint8_t out[kTrailerLen]);
std::vector<uint8_t> buildFrame(uint32_t timestamp, uint8_t type, const uint8_t *payload, size_t payloadLen,
                                TrailerFn trailerFn = nullptr);

// Stream parser: BLE writes / SPP reads can split or join frames, so bytes are
// fed in and whole frames come out. Garbage before a magic is skipped.
class Parser {
 public:
  // Appends bytes; returns the number of complete frames now waiting in frames().
  size_t feed(const uint8_t *data, size_t len);
  std::vector<Frame> &frames() { return frames_; }
  size_t droppedBytes() const { return dropped_; }

 private:
  std::vector<uint8_t> buf_;
  std::vector<Frame> frames_;
  size_t dropped_ = 0;
};

// What the display needs. -1 / empty means "not known".
struct NavState {
  int speedKmh = -1;
  int speedLimit = -1;
  int cameraDistM = -1;
  int turnDistM = -1;
  int turnIcon = -1;         // see hud_ui.h for the icon ids
  std::string street;        // road currently driven on
  std::string nextStreet;    // road the upcoming maneuver turns onto
  int warnSignId = -1;       // non-speed, non-camera sign to show (-1 = none); raw GOFA sign id
  int warnSignDistM = -1;    // distance to warnSignId (-1 = "now"/unknown)
  int limitAheadKmh = -1;    // an upcoming speed-limit sign (-1 = none)
  int limitAheadDistM = -1;  // distance to limitAheadKmh
  std::string weather;
};

// Applies one frame to the state. Returns true when something visible changed.
bool applyFrame(const Frame &f, NavState &s);

// Hex dump helper for logs: "a5 5a 37 c3 ...".
std::string toHex(const uint8_t *data, size_t len);

}  // namespace hud
