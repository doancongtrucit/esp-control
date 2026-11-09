#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "activity_info.h"

static bool parse(const char *json, ActivityInfo &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return parseActivityFrame(doc.as<JsonVariantConst>(), out);
}

void test_full_frame() {
  ActivityInfo a;
  TEST_ASSERT_TRUE(parse(R"J({"type":"activity","on":312,"sess":47,"brk":6,"long":95,
    "h":[0,0,0,0,0,0,0,0,0,55,60,12,0,0,0,0,0,0,0,0,0,0,0,0],"wk":[410,380,0,0,455,390,312],
    "late":true,"rem":"stand","eye":3,"lk":{"on":true,"near":false,"rssi":-91}})J", a));
  TEST_ASSERT_TRUE(a.has);
  TEST_ASSERT_EQUAL_UINT16(312, a.todayMin);
  TEST_ASSERT_EQUAL_UINT16(47, a.sessionMin);
  TEST_ASSERT_EQUAL_UINT16(6, a.breaks);
  TEST_ASSERT_EQUAL_UINT16(95, a.longestMin);
  TEST_ASSERT_EQUAL_UINT8(60, a.hours[10]);
  TEST_ASSERT_EQUAL_UINT16(455, a.week[4]);
  TEST_ASSERT_TRUE(a.late);
  TEST_ASSERT_TRUE(a.standDue);
  TEST_ASSERT_EQUAL_UINT32(3, a.eyeSeq);
  TEST_ASSERT_TRUE(a.lockOn);
  TEST_ASSERT_EQUAL(static_cast<int>(PhoneNear::Away), static_cast<int>(a.near));
  TEST_ASSERT_TRUE(a.hasRssi);
  TEST_ASSERT_EQUAL_INT16(-91, a.rssi);
}

void test_nulls_mean_unknown() {
  ActivityInfo a;
  TEST_ASSERT_TRUE(parse(R"J({"type":"activity","on":0,"rem":null,"lk":{"on":false,"near":null,"rssi":null}})J", a));
  TEST_ASSERT_FALSE(a.standDue);
  TEST_ASSERT_EQUAL(static_cast<int>(PhoneNear::Unknown), static_cast<int>(a.near));
  TEST_ASSERT_FALSE(a.hasRssi);
}

void test_other_frames_are_ignored() {
  ActivityInfo a;
  TEST_ASSERT_FALSE(parse(R"J({"type":"claude"})J", a));
  TEST_ASSERT_FALSE(a.has);
}

void test_duration() {
  TEST_ASSERT_EQUAL_STRING("47m", activityDuration(47).c_str());
  TEST_ASSERT_EQUAL_STRING("5h 12m", activityDuration(312).c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_full_frame);
  RUN_TEST(test_nulls_mean_unknown);
  RUN_TEST(test_other_frames_are_ignored);
  RUN_TEST(test_duration);
  UNITY_END();
}

void loop() {}
