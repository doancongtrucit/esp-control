#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "fuel_info.h"

static bool parse(const char *json, FuelInfo &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return parseFuelFrame(doc.as<JsonVariantConst>(), out);
}

void test_frame() {
  FuelInfo f;
  TEST_ASSERT_TRUE(parse(R"J({"type":"fuel","z":1,"t":"20:05","items":[
    ["E10 RON95 M5",22650,-500],["E5 RON92 M2",20890,0],["Dầu hỏa",28600,310]]})J", f));
  TEST_ASSERT_EQUAL_UINT8(3, f.count);
  TEST_ASSERT_EQUAL_STRING("E10 RON95 M5", f.name[0].c_str());
  TEST_ASSERT_EQUAL_INT32(22650, f.price[0]);
  TEST_ASSERT_EQUAL_INT32(-500, f.change[0]);
  TEST_ASSERT_EQUAL_STRING("20:05", f.time.c_str());
}

void test_other_frames_are_ignored() {
  FuelInfo f;
  TEST_ASSERT_FALSE(parse(R"J({"type":"info"})J", f));
  TEST_ASSERT_FALSE(f.has);
}

void test_price_text() {
  TEST_ASSERT_EQUAL_STRING("22.650", fuelPriceText(22650).c_str());
  TEST_ASSERT_EQUAL_STRING("-500", fuelPriceText(-500).c_str());
  TEST_ASSERT_EQUAL_STRING("1.234.567", fuelPriceText(1234567).c_str());
  TEST_ASSERT_EQUAL_STRING("0", fuelPriceText(0).c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_frame);
  RUN_TEST(test_other_frames_are_ignored);
  RUN_TEST(test_price_text);
  UNITY_END();
}

void loop() {}
