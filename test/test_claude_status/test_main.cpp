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
  TEST_ASSERT_TRUE(parse(R"J({"type":"claude","n":5,
    "s":[["alpha","w","Allow Bash?",12],["beta","r","Edit: main.cpp",480,3],["gamma","d","Task complete",1500]],
    "ask":{"id":7,"name":"alpha","tool":"Bash","cmd":"npm test","always":"Bash(npm test:*)"}})J", s));
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
  TEST_ASSERT_EQUAL_STRING("Bash(npm test:*)", s.askAlways.c_str());
  TEST_ASSERT_EQUAL_UINT8(0, s.rows[0].fails);  // a row without the fifth field
  TEST_ASSERT_EQUAL_UINT8(3, s.rows[1].fails);
  TEST_ASSERT_TRUE(s.anyWaiting());
}

void test_no_ask_means_id_zero() {
  ClaudeState s;
  s.askId = 9;  // a previous ask must be cleared
  TEST_ASSERT_TRUE(parse(R"({"type":"claude","n":1,"s":[["beta","r","Working",3]]})", s));
  TEST_ASSERT_EQUAL_UINT32(0, s.askId);
  TEST_ASSERT_EQUAL_STRING("", s.askAlways.c_str());
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

void test_usage_frame_is_parsed() {
  JsonDocument doc;
  deserializeJson(doc, R"J({"type":"cusage","t":"22:09",
    "day":{"in":2188,"out":1261074,"cw":3007850,"cr":456942508,"usd":140.68},
    "top":[["alpha",101.67],["beta",33.02]],
    "blk":{"usd":35.78,"pct":59,"left":13845,"start":"21:00"},
    "h":[0,0,0,0,0,0,0,0,0,79,67,129,74,99,112,85,78,57,31,0,0,157,46,0]})J");
  ClaudeUsage u;
  TEST_ASSERT_TRUE(parseClaudeUsage(doc.as<JsonVariantConst>(), u));
  TEST_ASSERT_TRUE(u.has);
  TEST_ASSERT_EQUAL_UINT32(456942508, u.cacheRead);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 140.68, u.usd);
  TEST_ASSERT_EQUAL_UINT8(2, u.topCount);
  TEST_ASSERT_EQUAL_STRING("beta", u.top[1].name.c_str());
  TEST_ASSERT_TRUE(u.window);
  TEST_ASSERT_EQUAL_UINT8(59, u.windowPct);
  TEST_ASSERT_EQUAL_UINT32(13845, u.windowLeftSec);
  TEST_ASSERT_EQUAL_STRING("21:00", u.windowStart.c_str());
  TEST_ASSERT_EQUAL_UINT16(157, u.hours[21]);
  TEST_ASSERT_EQUAL_UINT16(157, u.peakHour());
  TEST_ASSERT_EQUAL_STRING("22:09", u.time.c_str());
}

void test_usage_without_window() {
  JsonDocument doc;
  deserializeJson(doc, R"({"type":"cusage","day":{"usd":0},"top":[],"blk":null,"h":[]})");
  ClaudeUsage u;
  TEST_ASSERT_TRUE(parseClaudeUsage(doc.as<JsonVariantConst>(), u));
  TEST_ASSERT_FALSE(u.window);
  TEST_ASSERT_EQUAL_UINT16(0, u.peakHour());
}

void test_stats_frame_is_parsed() {
  JsonDocument doc;
  deserializeJson(doc, R"({"type":"cstats","ask":7,"ok":3,"al":1,"no":2,"pc":1,"done":12})");
  ClaudeStats st;
  TEST_ASSERT_TRUE(parseClaudeStats(doc.as<JsonVariantConst>(), st));
  TEST_ASSERT_EQUAL_UINT16(7, st.asks);
  TEST_ASSERT_EQUAL_UINT16(3, st.allowed);
  TEST_ASSERT_EQUAL_UINT16(1, st.always);
  TEST_ASSERT_EQUAL_UINT16(2, st.denied);
  TEST_ASSERT_EQUAL_UINT16(1, st.onPc);
  TEST_ASSERT_EQUAL_UINT16(12, st.done);
  TEST_ASSERT_FALSE(parseClaudeStats(doc["nope"].as<JsonVariantConst>(), st));
}

void test_token_and_duration_text() {
  TEST_ASSERT_EQUAL_STRING("950", claudeTokens(950).c_str());
  TEST_ASSERT_EQUAL_STRING("2.2K", claudeTokens(2188).c_str());
  TEST_ASSERT_EQUAL_STRING("1.3M", claudeTokens(1261074).c_str());
  TEST_ASSERT_EQUAL_STRING("457M", claudeTokens(456942508).c_str());
  TEST_ASSERT_EQUAL_STRING("3.1B", claudeTokens(3100000000UL).c_str());
  TEST_ASSERT_EQUAL_STRING("3h 50m", claudeDuration(13845).c_str());
  TEST_ASSERT_EQUAL_STRING("12m", claudeDuration(750).c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_rows_and_ask_are_parsed);
  RUN_TEST(test_no_ask_means_id_zero);
  RUN_TEST(test_extra_rows_are_dropped);
  RUN_TEST(test_unknown_state_is_done_and_wrong_type_is_rejected);
  RUN_TEST(test_age_text);
  RUN_TEST(test_usage_frame_is_parsed);
  RUN_TEST(test_usage_without_window);
  RUN_TEST(test_stats_frame_is_parsed);
  RUN_TEST(test_token_and_duration_text);
  UNITY_END();
}

void loop() {}
