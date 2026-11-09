#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Claude Code sessions as the bridge reports them (host/claude_sessions.py):
// {"type":"claude","n":5,"s":[["alpha","w","Allow Bash?",12],...],
//  "ask":{"id":7,"name":"alpha","tool":"Bash","cmd":"npm test","always":"Bash(npm test:*)"}}
// Rows: name, state (w/r/d), detail, age in seconds, failures in a row (optional).
constexpr uint8_t CLAUDE_ROWS = 4;

enum class ClaudeRunState : uint8_t { Waiting, Running, Done };

struct ClaudeRow {
  String name;
  ClaudeRunState state = ClaudeRunState::Done;
  String detail;
  uint32_t ageSec = 0;
  uint8_t fails = 0;  // tool calls failed in a row
};

struct ClaudeState {
  uint8_t count = 0;   // rows sent
  uint8_t total = 0;   // sessions the bridge knows
  ClaudeRow rows[CLAUDE_ROWS];
  uint32_t askId = 0;  // 0 = no permission prompt waiting
  String askName;
  String askTool;
  String askCmd;
  String askAlways;  // Claude Code's "don't ask again" rule, empty if none was offered

  bool anyWaiting() const {
    for (uint8_t i = 0; i < count; ++i)
      if (rows[i].state == ClaudeRunState::Waiting) return true;
    return askId != 0;
  }
};

// Pure: false unless the document is a "claude" frame; replaces `out` on success.
bool parseClaudeFrame(JsonVariantConst doc, ClaudeState &out);

// "now", "5m", "2h" for a row's age.
String claudeAgeText(uint32_t seconds);

// Token usage today and the current 5-hour window (host/claude_usage.py, "cusage" frame).
constexpr uint8_t CLAUDE_TOP = 3;

struct ClaudeProjectCost {
  String name;
  float usd = 0;
};

struct ClaudeUsage {
  bool has = false;
  String time;  // PC clock when computed
  uint32_t input = 0, output = 0, cacheWrite = 0, cacheRead = 0;
  float usd = 0;  // estimated at API rates
  uint8_t topCount = 0;
  ClaudeProjectCost top[CLAUDE_TOP];
  bool window = false;  // a 5-hour window is open
  float windowUsd = 0;
  uint8_t windowPct = 0;       // of the biggest window seen
  uint32_t windowLeftSec = 0;  // at send time
  String windowStart;
  uint16_t hours[24] = {};  // assistant messages per hour today

  uint16_t peakHour() const {
    uint16_t peak = 0;
    for (uint16_t h : hours) peak = h > peak ? h : peak;
    return peak;
  }
};

// Daily counts kept by the bridge ("cstats" frame).
struct ClaudeStats {
  bool has = false;
  uint16_t asks = 0, allowed = 0, always = 0, denied = 0, onPc = 0, done = 0;
};

bool parseClaudeUsage(JsonVariantConst doc, ClaudeUsage &out);
bool parseClaudeStats(JsonVariantConst doc, ClaudeStats &out);

// "950", "2.2K", "1.3M", "457M", "3.1B".
String claudeTokens(uint32_t tokens);
// "3h 50m", "12m".
String claudeDuration(uint32_t seconds);
