#pragma once

#include <Arduino.h>

// Signed UDP datagrams between the deck and the bridge (host/udp_link.py):
//   "DK1 " + mac + " " + body,  body = "<ts> <seq> <part> <parts> " + chunk
//   mac = first 32 hex chars of HMAC-SHA256(token, body)
// ts is Unix time (0 = no clock yet); a frame longer than LINK_CHUNK is split
// into parts sharing ts and seq.
constexpr size_t LINK_CHUNK = 1200;
constexpr size_t LINK_DATAGRAM_MAX = LINK_CHUNK + 64;
constexpr size_t LINK_FRAME_MAX = 4096;
constexpr uint8_t LINK_MAX_PARTS = 4;
constexpr uint32_t LINK_WINDOW_S = 120;

struct LinkEnvelope {
  uint32_t ts = 0;
  uint32_t seq = 0;
  uint8_t part = 0;
  uint8_t parts = 1;
  const char *chunk = nullptr;  // points into the datagram
  size_t chunkLen = 0;
};

// 32 lowercase hex chars + NUL.
void linkMac(const String &token, const char *body, size_t len, char out[33]);

// Writes one datagram into out (cap bytes); returns its length, 0 when it does not fit.
size_t linkSeal(const String &token, uint32_t ts, uint32_t seq, uint8_t part, uint8_t parts, const char *chunk,
                size_t chunkLen, char *out, size_t cap);

// True only for a well-formed datagram with a valid MAC.
bool linkOpen(const String &token, const char *data, size_t len, LinkEnvelope &out);

// Joins the parts of one frame (one frame in flight at a time; a new seq restarts).
class LinkReassembler {
 public:
  // True when `frame` holds a complete frame.
  bool feed(const LinkEnvelope &env, uint32_t nowMs, String &frame);

 private:
  uint32_t ts_ = 0, seq_ = 0, startedMs_ = 0;
  uint8_t parts_ = 0, have_ = 0;
  String chunks_[LINK_MAX_PARTS];
};

// Accepts each (ts, seq) once and only within LINK_WINDOW_S of our clock.
class LinkReplayGuard {
 public:
  bool fresh(uint32_t ts, uint32_t now) const;
  bool seen(uint32_t ts, uint32_t seq) const;
  void remember(uint32_t ts, uint32_t seq);

 private:
  static constexpr int SIZE = 48;
  uint32_t ts_[SIZE] = {};
  uint32_t seq_[SIZE] = {};
  int next_ = 0;
};
