#include <Arduino.h>
#include <unity.h>

#include "net_notify.h"

static const String TOKEN = "s3cret-token-for-tests";

void test_valid_payload_is_accepted() {
  String app, from, body;
  const bool ok = parseUdpNotify(
      R"({"token":"s3cret-token-for-tests","app":"Teams","from":"Nguyễn","body":"Họp lúc 3h"})",
      TOKEN, app, from, body);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("Teams", app.c_str());
  TEST_ASSERT_EQUAL_STRING("Nguyễn", from.c_str());
  TEST_ASSERT_EQUAL_STRING("Họp lúc 3h", body.c_str());
}

void test_wrong_token_is_rejected() {
  String app, from, body;
  TEST_ASSERT_FALSE(parseUdpNotify(R"({"token":"nope","app":"A","from":"B","body":"C"})", TOKEN, app, from, body));
}

void test_missing_token_is_rejected() {
  String app, from, body;
  TEST_ASSERT_FALSE(parseUdpNotify(R"({"app":"A","from":"B","body":"C"})", TOKEN, app, from, body));
}

void test_non_json_is_rejected() {
  String app, from, body;
  TEST_ASSERT_FALSE(parseUdpNotify("hello device", TOKEN, app, from, body));
}

void test_oversized_payload_is_rejected() {
  String big = R"({"token":"s3cret-token-for-tests","app":"A","from":"B","body":")";
  for (int i = 0; i < 2048; ++i) big += 'x';
  big += "\"}";
  String app, from, body;
  TEST_ASSERT_FALSE(parseUdpNotify(big.c_str(), TOKEN, app, from, body));
}

void test_unprovisioned_device_rejects_everything() {
  String app, from, body;
  TEST_ASSERT_FALSE(parseUdpNotify(R"({"token":"","app":"A","from":"B","body":"C"})", "", app, from, body));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_valid_payload_is_accepted);
  RUN_TEST(test_wrong_token_is_rejected);
  RUN_TEST(test_missing_token_is_rejected);
  RUN_TEST(test_non_json_is_rejected);
  RUN_TEST(test_oversized_payload_is_rejected);
  RUN_TEST(test_unprovisioned_device_rejects_everything);
  UNITY_END();
}

void loop() {}
