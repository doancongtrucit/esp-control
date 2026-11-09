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
  }
  s.total = doc["n"] | static_cast<uint8_t>(rows.size());
  JsonVariantConst ask = doc["ask"];
  if (!ask.isNull()) {
    s.askId = ask["id"] | 0;
    s.askName = ask["name"] | "";
    s.askTool = ask["tool"] | "";
    s.askCmd = ask["cmd"] | "";
  }
  out = s;
  return true;
}

String claudeAgeText(uint32_t seconds) {
  if (seconds < 60) return "now";
  if (seconds < 3600) return String(seconds / 60) + "m";
  return String(seconds / 3600) + "h";
}
