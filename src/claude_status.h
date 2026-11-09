#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Claude Code sessions as the bridge reports them (host/claude_sessions.py):
// {"type":"claude","n":5,"s":[["alpha","w","Allow Bash?",12],...],
//  "ask":{"id":7,"name":"alpha","tool":"Bash","cmd":"npm test"}}
constexpr uint8_t CLAUDE_ROWS = 4;

enum class ClaudeRunState : uint8_t { Waiting, Running, Done };

struct ClaudeRow {
  String name;
  ClaudeRunState state = ClaudeRunState::Done;
  String detail;
  uint32_t ageSec = 0;
};

struct ClaudeState {
  uint8_t count = 0;   // rows sent
  uint8_t total = 0;   // sessions the bridge knows
  ClaudeRow rows[CLAUDE_ROWS];
  uint32_t askId = 0;  // 0 = no permission prompt waiting
  String askName;
  String askTool;
  String askCmd;

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
