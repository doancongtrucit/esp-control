#include <Arduino.h>
#include <unity.h>

#include "notify_store.h"
#include "text_vi.h"

void test_keeps_newest_ten_in_order() {
  NotifyStore store;
  for (int i = 0; i < 15; ++i) {
    store.add(NotifySource::Phone, "App", "From", "n" + String(i), 1000 + i);
  }
  TEST_ASSERT_EQUAL(10u, store.size());
  TEST_ASSERT_EQUAL_STRING("n14", store.at(0).body.c_str());
  TEST_ASSERT_EQUAL_STRING("n5", store.at(9).body.c_str());
  TEST_ASSERT_EQUAL(10u, store.unreadCount());
}

void test_mark_all_read_clears_unread_and_bumps_version() {
  NotifyStore store;
  store.add(NotifySource::Pc, "Teams", "A", "hello", 1);
  const uint32_t before = store.version();
  store.markAllRead();
  TEST_ASSERT_EQUAL(0u, store.unreadCount());
  TEST_ASSERT_GREATER_THAN(before, store.version());
}

void test_long_vietnamese_body_is_sanitized_and_cut() {
  NotifyStore store;
  String body;
  for (int i = 0; i < 30; ++i) body += "Tiếng Việt 👩 ";
  store.add(NotifySource::Phone, "Messenger", "Mẹ", body, 1);
  const Notification &n = store.at(0);
  TEST_ASSERT_LESS_OR_EQUAL(48u, utf8Length(n.body));
  TEST_ASSERT_EQUAL(-1, n.body.indexOf("\xF0"));  // no 4-byte (emoji) sequences
  TEST_ASSERT_EQUAL_STRING("Mẹ", n.from.c_str());
  TEST_ASSERT_TRUE(n.unread);
  TEST_ASSERT_EQUAL(NotifySource::Phone, n.source);
}

void test_field_limits() {
  NotifyStore store;
  String longSender;
  for (int i = 0; i < 60; ++i) longSender += 'f';
  store.add(NotifySource::Pc, "An application name", longSender, "b", 1);
  TEST_ASSERT_LESS_OR_EQUAL(12u, utf8Length(store.at(0).app));
  TEST_ASSERT_LESS_OR_EQUAL(24u, utf8Length(store.at(0).from));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_keeps_newest_ten_in_order);
  RUN_TEST(test_mark_all_read_clears_unread_and_bumps_version);
  RUN_TEST(test_long_vietnamese_body_is_sanitized_and_cut);
  RUN_TEST(test_field_limits);
  UNITY_END();
}

void loop() {}
