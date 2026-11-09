#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "claude_status.h"

static bool parse(const char *json, ClaudeState &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return parseClaudeFrame(doc.as<JsonVariantConst>(), out);
}

void test_rows_and_ask_are_parsed() {
  ClaudeState s;
  TEST_ASSERT_TRUE(parse(R"({"type":"claude","n":5,
    "s":[["alpha","w","Allow Bash?",12],["beta","r","Edit: main.cpp",480],["gamma","d","Task complete",1500]],
    "ask":{"id":7,"name":"alpha","tool":"Bash","cmd":"npm test"}})", s));
  TEST_ASSERT_EQUAL_UINT8(3, s.count);
  TEST_ASSERT_EQUAL_UINT8(5, s.total);
  TEST_ASSERT_EQUAL_STRING("alpha", s.rows[0].name.c_str());
  TEST_ASSERT_EQUAL(ClaudeRunState::Waiting, s.rows[0].state);
  TEST_ASSERT_EQUAL(ClaudeRunState::Running, s.rows[1].state);
  TEST_ASSERT_EQUAL(ClaudeRunState::Done, s.rows[2].state);
  TEST_ASSERT_EQUAL_STRING("Edit: main.cpp", s.rows[1].detail.c_str());
  TEST_ASSERT_EQUAL_UINT32(480, s.rows[1].ageSec);
  TEST_ASSERT_EQUAL_UINT32(7, s.askId);
  TEST_ASSERT_EQUAL_STRING("Bash", s.askTool.c_str());
  TEST_ASSERT_EQUAL_STRING("npm test", s.askCmd.c_str());
  TEST_ASSERT_TRUE(s.anyWaiting());
}

void test_no_ask_means_id_zero() {
  ClaudeState s;
  s.askId = 9;  // a previous ask must be cleared
  TEST_ASSERT_TRUE(parse(R"({"type":"claude","n":1,"s":[["beta","r","Working",3]]})", s));
  TEST_ASSERT_EQUAL_UINT32(0, s.askId);
  TEST_ASSERT_FALSE(s.anyWaiting());
}

void test_extra_rows_are_dropped() {
  ClaudeState s;
  TEST_ASSERT_TRUE(parse(R"({"type":"claude","s":[["a","r","",1],["b","r","",1],["c","r","",1],["d","r","",1],["e","r","",1]]})", s));
  TEST_ASSERT_EQUAL_UINT8(CLAUDE_ROWS, s.count);
  TEST_ASSERT_EQUAL_UINT8(5, s.total);  // no "n": fall back to what was sent
}

void test_unknown_state_is_done_and_wrong_type_is_rejected() {
  ClaudeState s;
  TEST_ASSERT_TRUE(parse(R"({"type":"claude","s":[["a","?","",1]]})", s));
  TEST_ASSERT_EQUAL(ClaudeRunState::Done, s.rows[0].state);
  TEST_ASSERT_FALSE(parse(R"({"type":"stats"})", s));
}

void test_age_text() {
  TEST_ASSERT_EQUAL_STRING("now", claudeAgeText(30).c_str());
  TEST_ASSERT_EQUAL_STRING("2m", claudeAgeText(150).c_str());
  TEST_ASSERT_EQUAL_STRING("1h", claudeAgeText(3700).c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_rows_and_ask_are_parsed);
  RUN_TEST(test_no_ask_means_id_zero);
  RUN_TEST(test_extra_rows_are_dropped);
  RUN_TEST(test_unknown_state_is_done_and_wrong_type_is_rejected);
  RUN_TEST(test_age_text);
  UNITY_END();
}

void loop() {}
