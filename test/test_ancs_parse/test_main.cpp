#include <Arduino.h>
#include <unity.h>

#include "ancs_parse.h"

// Data Source reply: CommandID 0, NotificationUID, then (AttributeID, len16, data)*.
static size_t buildReply(uint8_t *out, const char *app, const char *title, const char *message) {
  size_t n = 0;
  out[n++] = 0x00;
  out[n++] = 0x2A; out[n++] = 0; out[n++] = 0; out[n++] = 0;  // UID 42
  const char *values[3] = {app, title, message};
  const uint8_t ids[3] = {0, 1, 3};
  for (int i = 0; i < 3; ++i) {
    const size_t len = strlen(values[i]);
    out[n++] = ids[i];
    out[n++] = len & 0xFF;
    out[n++] = len >> 8;
    memcpy(out + n, values[i], len);
    n += len;
  }
  return n;
}

void test_app_names_for_known_bundles() {
  TEST_ASSERT_EQUAL_STRING("Messages", ancsAppName("com.apple.MobileSMS").c_str());
  TEST_ASSERT_EQUAL_STRING("Zalo", ancsAppName("com.vng.zalo").c_str());
  TEST_ASSERT_EQUAL_STRING("Teams", ancsAppName("com.microsoft.skype.teams").c_str());
}

void test_unknown_bundle_uses_last_segment() {
  TEST_ASSERT_EQUAL_STRING("youtube", ancsAppName("com.google.ios.youtube").c_str());
  TEST_ASSERT_EQUAL_STRING("Single", ancsAppName("Single").c_str());
}

void test_reply_in_one_packet() {
  uint8_t buf[128];
  const size_t n = buildReply(buf, "com.vng.zalo", "Mẹ", "Đi ngủ đi");
  AncsReply reply;
  reply.begin(42);
  reply.feed(buf, n);
  TEST_ASSERT_TRUE(reply.complete());
  TEST_ASSERT_EQUAL_STRING("com.vng.zalo", reply.app().c_str());
  TEST_ASSERT_EQUAL_STRING("Mẹ", reply.title().c_str());
  TEST_ASSERT_EQUAL_STRING("Đi ngủ đi", reply.message().c_str());
}

void test_reply_split_across_packets() {
  uint8_t buf[128];
  const size_t n = buildReply(buf, "com.apple.MobileSMS", "Contact", "Hello there");
  AncsReply reply;
  reply.begin(42);
  reply.feed(buf, 9);
  TEST_ASSERT_FALSE(reply.complete());
  reply.feed(buf + 9, 15);
  TEST_ASSERT_FALSE(reply.complete());
  reply.feed(buf + 24, n - 24);
  TEST_ASSERT_TRUE(reply.complete());
  TEST_ASSERT_EQUAL_STRING("Hello there", reply.message().c_str());
}

void test_reply_for_another_uid_is_ignored() {
  uint8_t buf[128];
  const size_t n = buildReply(buf, "a", "b", "c");
  AncsReply reply;
  reply.begin(7);
  reply.feed(buf, n);
  TEST_ASSERT_FALSE(reply.complete());
}

void test_retry_waits_for_backoff_after_failure() {
  RetryTimer retry(4000, 5);
  TEST_ASSERT_TRUE(retry.due(0));  // first attempt right away
  retry.failed(1000);
  TEST_ASSERT_FALSE(retry.due(4999));
  TEST_ASSERT_TRUE(retry.due(5000));
}

void test_retry_gives_up_after_max_attempts_until_reset() {
  RetryTimer retry(10, 3);
  for (int i = 0; i < 3; ++i) retry.failed(i * 100);
  TEST_ASSERT_FALSE(retry.due(100000));
  retry.reset();
  TEST_ASSERT_TRUE(retry.due(0));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_app_names_for_known_bundles);
  RUN_TEST(test_unknown_bundle_uses_last_segment);
  RUN_TEST(test_reply_in_one_packet);
  RUN_TEST(test_reply_split_across_packets);
  RUN_TEST(test_reply_for_another_uid_is_ignored);
  RUN_TEST(test_retry_waits_for_backoff_after_failure);
  RUN_TEST(test_retry_gives_up_after_max_attempts_until_reset);
  UNITY_END();
}

void loop() {}
