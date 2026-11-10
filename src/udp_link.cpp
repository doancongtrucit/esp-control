#include "udp_link.h"

#include <mbedtls/md.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void linkMac(const String &token, const char *body, size_t len, char out[33]) {
  uint8_t digest[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), reinterpret_cast<const uint8_t *>(token.c_str()),
                  token.length(), reinterpret_cast<const uint8_t *>(body), len, digest);
  static const char HEX_DIGITS[] = "0123456789abcdef";
  for (int i = 0; i < 16; ++i) {
    out[i * 2] = HEX_DIGITS[digest[i] >> 4];
    out[i * 2 + 1] = HEX_DIGITS[digest[i] & 0x0F];
  }
  out[32] = '\0';
}

size_t linkSeal(const String &token, uint32_t ts, uint32_t seq, uint8_t part, uint8_t parts, const char *chunk,
                size_t chunkLen, char *out, size_t cap) {
  char head[48];
  const int headLen = snprintf(head, sizeof(head), "%lu %lu %u %u ", static_cast<unsigned long>(ts),
                               static_cast<unsigned long>(seq), part, parts);
  const size_t bodyLen = headLen + chunkLen;
  const size_t total = 4 + 32 + 1 + bodyLen;
  if (headLen <= 0 || total > cap) return 0;
  char *body = out + 37;
  memcpy(body, head, headLen);
  memcpy(body + headLen, chunk, chunkLen);
  char mac[33];
  linkMac(token, body, bodyLen, mac);
  memcpy(out, "DK1 ", 4);
  memcpy(out + 4, mac, 32);
  out[36] = ' ';
  return total;
}

namespace {
// Reads a decimal field ending in a space; advances p.
bool field(const char *&p, const char *end, uint32_t &value) {
  if (p >= end || *p < '0' || *p > '9') return false;
  uint64_t v = 0;
  while (p < end && *p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    if (v > 0xFFFFFFFFULL) return false;
    ++p;
  }
  if (p >= end || *p != ' ') return false;
  ++p;
  value = static_cast<uint32_t>(v);
  return true;
}
}  // namespace

bool linkOpen(const String &token, const char *data, size_t len, LinkEnvelope &out) {
  if (token.length() == 0 || len < 38 || memcmp(data, "DK1 ", 4) != 0 || data[36] != ' ') return false;
  const char *body = data + 37;
  const size_t bodyLen = len - 37;
  char mac[33];
  linkMac(token, body, bodyLen, mac);
  uint8_t diff = 0;  // constant time compare
  for (int i = 0; i < 32; ++i) diff |= static_cast<uint8_t>(mac[i] ^ data[4 + i]);
  if (diff) return false;
  const char *p = body, *end = body + bodyLen;
  uint32_t ts, seq, part, parts;
  if (!field(p, end, ts) || !field(p, end, seq) || !field(p, end, part) || !field(p, end, parts)) return false;
  if (parts == 0 || parts > LINK_MAX_PARTS || part >= parts) return false;
  out.ts = ts;
  out.seq = seq;
  out.part = part;
  out.parts = parts;
  out.chunk = p;
  out.chunkLen = end - p;
  return true;
}

bool LinkReassembler::feed(const LinkEnvelope &env, uint32_t nowMs, String &frame) {
  if (env.parts == 1) {
    frame = String();
    frame.concat(env.chunk, env.chunkLen);
    return true;
  }
  if (env.ts != ts_ || env.seq != seq_ || env.parts != parts_ || nowMs - startedMs_ > 3000) {
    ts_ = env.ts;
    seq_ = env.seq;
    parts_ = env.parts;
    have_ = 0;
    startedMs_ = nowMs;
    for (auto &c : chunks_) c = String();
  }
  const uint8_t bit = 1u << env.part;
  if (!(have_ & bit)) {
    chunks_[env.part] = String();
    chunks_[env.part].concat(env.chunk, env.chunkLen);
    have_ |= bit;
  }
  if (have_ != (1u << parts_) - 1) return false;
  frame = String();
  for (uint8_t i = 0; i < parts_; ++i) frame += chunks_[i];
  parts_ = 0;  // done: a repeat of the last part starts over
  for (auto &c : chunks_) c = String();
  return frame.length() <= LINK_FRAME_MAX;
}

bool LinkReplayGuard::fresh(uint32_t ts, uint32_t now) const {
  const uint32_t d = ts > now ? ts - now : now - ts;
  return d <= LINK_WINDOW_S;
}

bool LinkReplayGuard::seen(uint32_t ts, uint32_t seq) const {
  for (int i = 0; i < SIZE; ++i)
    if (ts_[i] == ts && seq_[i] == seq && ts != 0) return true;
  return false;
}

void LinkReplayGuard::remember(uint32_t ts, uint32_t seq) {
  ts_[next_] = ts;
  seq_[next_] = seq;
  next_ = (next_ + 1) % SIZE;
}
