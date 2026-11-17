#include "hud_protocol.h"

#include <cstring>

namespace hud {

namespace {

uint32_t readU32(const uint8_t *p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

void writeU32(std::vector<uint8_t> &v, uint32_t x) {
  v.push_back(x >> 24);
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x);
}

}  // namespace

std::vector<uint8_t> buildFrame(uint32_t timestamp, uint8_t type, const uint8_t *payload, size_t payloadLen,
                                TrailerFn trailerFn) {
  std::vector<uint8_t> f(kMagic, kMagic + 4);
  writeU32(f, uint32_t(4 + 1 + payloadLen + kTrailerLen));
  writeU32(f, timestamp);
  f.push_back(type);
  f.insert(f.end(), payload, payload + payloadLen);
  uint8_t trailer[kTrailerLen] = {};
  if (trailerFn) trailerFn(f.data(), f.size(), trailer);
  f.insert(f.end(), trailer, trailer + kTrailerLen);
  return f;
}

size_t Parser::feed(const uint8_t *data, size_t len) {
  buf_.insert(buf_.end(), data, data + len);
  for (;;) {
    // Find the magic.
    size_t start = 0;
    while (start + 4 <= buf_.size() && memcmp(buf_.data() + start, kMagic, 4) != 0) start++;
    if (start + 4 > buf_.size()) {
      // Keep the last 3 bytes in case a magic is split across writes.
      size_t keep = buf_.size() < 3 ? buf_.size() : 3;
      dropped_ += buf_.size() - keep;
      buf_.erase(buf_.begin(), buf_.end() - keep);
      break;
    }
    if (start) {
      dropped_ += start;
      buf_.erase(buf_.begin(), buf_.begin() + start);
    }
    if (buf_.size() < 8) break;
    uint32_t n = readU32(buf_.data() + 4);
    if (n < 4 + 1 + kTrailerLen || n + 8 > kMaxFrameLen) {
      // Not a real frame: skip this magic and search again.
      dropped_ += 1;
      buf_.erase(buf_.begin());
      continue;
    }
    if (buf_.size() < n + 8) break;
    Frame f;
    f.timestamp = readU32(buf_.data() + 8);
    f.type = buf_[12];
    f.payload.assign(buf_.begin() + kHeaderLen, buf_.begin() + 8 + n - kTrailerLen);
    memcpy(f.trailer, buf_.data() + 8 + n - kTrailerLen, kTrailerLen);
    frames_.push_back(std::move(f));
    buf_.erase(buf_.begin(), buf_.begin() + 8 + n);
  }
  return frames_.size();
}

bool applyFrame(const Frame &f, NavState &s) {
  const auto &p = f.payload;
  switch (f.type) {
    case kAlertGuess: {
      // Guess: kind 0x10 = camera distance (m), 0x12 = speed limit (km/h).
      if (p.size() < 3) return false;
      int value = (p[1] << 8) | p[2];
      if (p[0] == 0x10 && s.cameraDistM != value) { s.cameraDistM = value; return true; }
      if (p[0] == 0x12 && s.speedLimit != value) { s.speedLimit = value; return true; }
      return false;
    }
    case kWeather: {
      // fe 00000001 01 ffffffff <len u8> <utf8 text> ...
      if (p.size() < 11) return false;
      size_t n = p[10];
      if (11 + n > p.size()) return false;
      std::string text(p.begin() + 11, p.begin() + 11 + n);
      if (text == s.weather) return false;
      s.weather = text;
      return true;
    }
    default:
      return false;
  }
}

std::string toHex(const uint8_t *data, size_t len) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  out.reserve(len * 3);
  for (size_t i = 0; i < len; i++) {
    if (i) out += ' ';
    out += digits[data[i] >> 4];
    out += digits[data[i] & 15];
  }
  return out;
}

}  // namespace hud
