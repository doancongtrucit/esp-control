#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "ha_controls_info.h"

static bool parse(const char *json, HaState &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return parseHaFrame(doc.as<JsonVariantConst>(), out);
}

void test_tiles_and_air_conditioner() {
  HaState s;
  TEST_ASSERT_TRUE(parse(R"J({"type":"hactl","items":[
    {"n":"Air Con","i":"ac","k":"climate","on":0,"v":"24C off"},
    {"n":"Fan","i":"fan","k":"toggle","on":1,"v":"ON","c":1},
    {"n":"Swing","i":"bolt","k":"run","on":-1,"v":"Run"}],
    "ac":{"ix":0,"on":false,"t":24,"min":16,"max":30,"mode":"cool","modes":["heat_cool","cool","dry"],
          "fan":"auto","fans":["auto","low","mid","high"]}})J", s));
  TEST_ASSERT_EQUAL_UINT8(3, s.count);
  TEST_ASSERT_EQUAL_STRING("Fan", s.tiles[1].name.c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Fan), static_cast<int>(s.tiles[1].icon));
  TEST_ASSERT_EQUAL(static_cast<int>(HaKind::Run), static_cast<int>(s.tiles[2].kind));
  TEST_ASSERT_EQUAL_INT8(1, s.tiles[1].on);
  TEST_ASSERT_TRUE(s.tiles[1].confirm);
  TEST_ASSERT_FALSE(s.tiles[0].confirm);
  TEST_ASSERT_TRUE(s.ac.has);
  TEST_ASSERT_EQUAL_INT(24, s.ac.temp);
  TEST_ASSERT_EQUAL_UINT8(3, s.ac.modeCount);
  TEST_ASSERT_EQUAL_STRING("dry", s.ac.modes[2].c_str());
  TEST_ASSERT_EQUAL_UINT8(4, s.ac.fanCount);
}

void test_no_air_conditioner() {
  HaState s;
  TEST_ASSERT_TRUE(parse(R"J({"type":"hactl","items":[],"ac":null})J", s));
  TEST_ASSERT_TRUE(s.has);
  TEST_ASSERT_EQUAL_UINT8(0, s.count);
  TEST_ASSERT_FALSE(s.ac.has);
}

void test_mode_labels() {
  TEST_ASSERT_EQUAL_STRING("Auto", haModeLabel("heat_cool").c_str());
  TEST_ASSERT_EQUAL_STRING("Cool", haModeLabel("cool").c_str());
  TEST_ASSERT_EQUAL_STRING("Fan", haModeLabel("fan_only").c_str());
  TEST_ASSERT_EQUAL_STRING("Mid", haModeLabel("mid").c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_tiles_and_air_conditioner);
  RUN_TEST(test_no_air_conditioner);
  RUN_TEST(test_mode_labels);
  UNITY_END();
}

void loop() {}
