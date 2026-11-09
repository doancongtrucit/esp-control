#include "claude_status.h"

namespace {
ClaudeRunState stateFrom(const char *code) {
  if (strcmp(code, "w") == 0) return ClaudeRunState::Waiting;
  if (strcmp(code, "r") == 0) return ClaudeRunState::Running;
  return ClaudeRunState::Done;
}
}  // namespace

bool parseClaudeFrame(JsonVariantConst doc, ClaudeState &out) {
  if (strcmp(doc["type"] | "", "claude") != 0) return false;
  ClaudeState s;
  JsonArrayConst rows = doc["s"].as<JsonArrayConst>();
  for (JsonArrayConst row : rows) {
    if (s.count >= CLAUDE_ROWS) break;
    ClaudeRow &r = s.rows[s.count++];
    r.name = row[0] | "";
    r.state = stateFrom(row[1] | "d");
    r.detail = row[2] | "";
    r.ageSec = row[3] | 0;
    r.fails = row[4] | 0;
  }
  s.total = doc["n"] | static_cast<uint8_t>(rows.size());
  JsonVariantConst ask = doc["ask"];
  if (!ask.isNull()) {
    s.askId = ask["id"] | 0;
    s.askName = ask["name"] | "";
    s.askTool = ask["tool"] | "";
    s.askCmd = ask["cmd"] | "";
    s.askAlways = ask["always"] | "";
  }
  out = s;
  return true;
}

String claudeAgeText(uint32_t seconds) {
  if (seconds < 60) return "now";
  if (seconds < 3600) return String(seconds / 60) + "m";
  return String(seconds / 3600) + "h";
}

bool parseClaudeUsage(JsonVariantConst doc, ClaudeUsage &out) {
  if (strcmp(doc["type"] | "", "cusage") != 0) return false;
  ClaudeUsage u;
  u.has = true;
  u.time = doc["t"] | "";
  JsonVariantConst day = doc["day"];
  u.input = day["in"] | 0;
  u.output = day["out"] | 0;
  u.cacheWrite = day["cw"] | 0;
  u.cacheRead = day["cr"] | 0;
  u.usd = day["usd"] | 0.0f;
  for (JsonArrayConst row : doc["top"].as<JsonArrayConst>()) {
    if (u.topCount >= CLAUDE_TOP) break;
    u.top[u.topCount].name = row[0] | "";
    u.top[u.topCount++].usd = row[1] | 0.0f;
  }
  JsonVariantConst blk = doc["blk"];
  if (blk.is<JsonObjectConst>()) {
    u.window = true;
    u.windowUsd = blk["usd"] | 0.0f;
    u.windowPct = blk["pct"] | 0;
    u.windowLeftSec = blk["left"] | 0;
    u.windowStart = blk["start"] | "";
  }
  uint8_t h = 0;
  for (JsonVariantConst n : doc["h"].as<JsonArrayConst>()) {
    if (h >= 24) break;
    u.hours[h++] = n | 0;
  }
  out = u;
  return true;
}

bool parseClaudeStats(JsonVariantConst doc, ClaudeStats &out) {
  if (strcmp(doc["type"] | "", "cstats") != 0) return false;
  out.has = true;
  out.asks = doc["ask"] | 0;
  out.allowed = doc["ok"] | 0;
  out.always = doc["al"] | 0;
  out.denied = doc["no"] | 0;
  out.onPc = doc["pc"] | 0;
  out.done = doc["done"] | 0;
  return true;
}

String claudeTokens(uint32_t tokens) {
  static const struct {
    uint32_t unit;
    char suffix;
  } SCALES[] = {{1000000000UL, 'B'}, {1000000UL, 'M'}, {1000UL, 'K'}};
  for (const auto &s : SCALES) {
    if (tokens < s.unit) continue;
    const float value = static_cast<float>(tokens) / s.unit;
    return (value < 10 ? String(value, 1) : String(static_cast<uint32_t>(value + 0.5f))) + s.suffix;
  }
  return String(tokens);
}

String claudeDuration(uint32_t seconds) {
  const uint32_t minutes = seconds / 60;
  if (minutes < 60) return String(minutes) + "m";
  return String(minutes / 60) + "h " + String(minutes % 60) + "m";
}
