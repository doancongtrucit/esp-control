#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "home_server_info.h"

static bool parse(const char *json, HomeServerInfo &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return parseHomeServerFrame(doc.as<JsonVariantConst>(), out);
}

void test_online_frame() {
  HomeServerInfo h;
  TEST_ASSERT_TRUE(parse(R"J({"type":"hserver","name":"Laptop","on":true,"ms":23,"ver":"2026.11.1",
    "cpu":12.5,"ram":43.2,"disk":61,"temp":48,"load":0.42,"dn":4.0,"up":1.0,"upt":273600,"upd":2,
    "bat":null,"t":"12:00"})J", h));
  TEST_ASSERT_TRUE(h.online);
  TEST_ASSERT_EQUAL_STRING("Laptop", h.name.c_str());
  TEST_ASSERT_EQUAL_STRING("2026.11.1", h.version.c_str());
  TEST_ASSERT_EQUAL_INT(23, h.latencyMs);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 12.5, h.cpu);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 61.0, h.disk);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.42, h.load);
  TEST_ASSERT_EQUAL_INT32(273600, h.uptimeSec);
  TEST_ASSERT_EQUAL_UINT16(2, h.updates);
  TEST_ASSERT_TRUE(isnan(h.battery));
}

void test_offline_frame() {
  HomeServerInfo h;
  TEST_ASSERT_TRUE(parse(R"J({"type":"hserver","name":"Laptop","on":false,"err":"token","t":"12:00"})J", h));
  TEST_ASSERT_FALSE(h.online);
  TEST_ASSERT_EQUAL_STRING("token", h.error.c_str());
  TEST_ASSERT_TRUE(isnan(h.cpu));
  TEST_ASSERT_EQUAL_INT32(-1, h.uptimeSec);
}

void test_other_frames_are_ignored() {
  HomeServerInfo h;
  TEST_ASSERT_FALSE(parse(R"J({"type":"stats"})J", h));
  TEST_ASSERT_FALSE(h.has);
}

void test_uptime_text() {
  TEST_ASSERT_EQUAL_STRING("3d 4h", uptimeText(273600).c_str());
  TEST_ASSERT_EQUAL_STRING("5h 12m", uptimeText(18720).c_str());
  TEST_ASSERT_EQUAL_STRING("42m", uptimeText(2520).c_str());
  TEST_ASSERT_EQUAL_STRING("--", uptimeText(-1).c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_online_frame);
  RUN_TEST(test_offline_frame);
  RUN_TEST(test_other_frames_are_ignored);
  RUN_TEST(test_uptime_text);
  UNITY_END();
}

void loop() {}
