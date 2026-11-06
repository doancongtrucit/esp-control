#include <Arduino.h>
#include <unity.h>

#include "deck_settings.h"

void test_defaults() {
  const DeckSettings s;
  TEST_ASSERT_EQUAL(4, s.brightness);
  TEST_ASSERT_EQUAL(0, s.sleepIndex);  // never
  TEST_ASSERT_TRUE(s.phoneOn);
  TEST_ASSERT_TRUE(s.pcOn);
  TEST_ASSERT_FALSE(s.quietOn);
  TEST_ASSERT_EQUAL(22, s.quietStart);
  TEST_ASSERT_EQUAL(7, s.quietEnd);
  TEST_ASSERT_EQUAL(0, s.mutedApps);
  TEST_ASSERT_TRUE(s.ledOn);
}

void test_backlight_duty_rises_and_never_goes_dark() {
  TEST_ASSERT_GREATER_THAN(0, backlightDuty(0));
  for (uint8_t level = 1; level < BRIGHTNESS_LEVELS; ++level) {
    TEST_ASSERT_GREATER_THAN(backlightDuty(level - 1), backlightDuty(level));
  }
  TEST_ASSERT_EQUAL(255, backlightDuty(BRIGHTNESS_LEVELS - 1));
  TEST_ASSERT_EQUAL(255, backlightDuty(200));  // out of range clamps to full
}

void test_sleep_timeouts() {
  TEST_ASSERT_EQUAL_UINT32(0, sleepTimeoutMs(0));
  TEST_ASSERT_EQUAL_UINT32(60000UL, sleepTimeoutMs(1));
  TEST_ASSERT_EQUAL_UINT32(300000UL, sleepTimeoutMs(2));
  TEST_ASSERT_EQUAL_UINT32(900000UL, sleepTimeoutMs(3));
  TEST_ASSERT_EQUAL_UINT32(0, sleepTimeoutMs(9));
}

void test_quiet_hours_wrap_past_midnight() {
  TEST_ASSERT_TRUE(inQuietHours(22, 22, 7));
  TEST_ASSERT_TRUE(inQuietHours(3, 22, 7));
  TEST_ASSERT_FALSE(inQuietHours(7, 22, 7));
  TEST_ASSERT_FALSE(inQuietHours(12, 22, 7));
  TEST_ASSERT_TRUE(inQuietHours(13, 12, 14));   // same-day window
  TEST_ASSERT_FALSE(inQuietHours(14, 12, 14));
  TEST_ASSERT_FALSE(inQuietHours(5, 6, 6));     // empty window
  TEST_ASSERT_FALSE(inQuietHours(-1, 22, 7));   // clock unknown
}

void test_app_index_matches_names_case_insensitively() {
  TEST_ASSERT_EQUAL(0, notifyAppIndex("Zalo"));
  TEST_ASSERT_EQUAL(1, notifyAppIndex("microsoft teams"));
  TEST_ASSERT_EQUAL(2, notifyAppIndex("Outlook"));
  TEST_ASSERT_EQUAL(NOTIFY_APP_COUNT - 1, notifyAppIndex("Windows"));  // Other
}

void test_notification_filter() {
  DeckSettings s;
  TEST_ASSERT_TRUE(notificationAllowed(s, NotifySource::Phone, "Zalo"));
  s.phoneOn = false;
  TEST_ASSERT_FALSE(notificationAllowed(s, NotifySource::Phone, "Zalo"));
  TEST_ASSERT_TRUE(notificationAllowed(s, NotifySource::Pc, "Zalo"));
  s.mutedApps = 1u << notifyAppIndex("Zalo");
  TEST_ASSERT_FALSE(notificationAllowed(s, NotifySource::Pc, "Zalo"));
  TEST_ASSERT_TRUE(notificationAllowed(s, NotifySource::Pc, "Teams"));
}

void test_round_trip_and_rejects_bad_blobs() {
  DeckSettings s;
  s.brightness = 2;
  s.sleepIndex = 3;
  s.rotate180 = false;
  s.hideBody = true;
  s.quietOn = true;
  s.quietStart = 23;
  s.quietEnd = 6;
  s.mutedApps = 0x05;
  s.ledOn = false;
  uint8_t blob[SETTINGS_BLOB_SIZE];
  packSettings(s, blob);
  DeckSettings back;
  TEST_ASSERT_TRUE(unpackSettings(blob, sizeof(blob), back));
  TEST_ASSERT_EQUAL(2, back.brightness);
  TEST_ASSERT_EQUAL(3, back.sleepIndex);
  TEST_ASSERT_FALSE(back.rotate180);
  TEST_ASSERT_TRUE(back.hideBody);
  TEST_ASSERT_TRUE(back.quietOn);
  TEST_ASSERT_EQUAL(23, back.quietStart);
  TEST_ASSERT_EQUAL(6, back.quietEnd);
  TEST_ASSERT_EQUAL(0x05, back.mutedApps);
  TEST_ASSERT_FALSE(back.ledOn);

  DeckSettings untouched;
  TEST_ASSERT_FALSE(unpackSettings(blob, sizeof(blob) - 1, untouched));  // wrong size
  blob[0] ^= 0xFF;                                                        // wrong version
  TEST_ASSERT_FALSE(unpackSettings(blob, sizeof(blob), untouched));
  TEST_ASSERT_EQUAL(4, untouched.brightness);
}

void test_unpack_clamps_out_of_range_values() {
  DeckSettings s;
  uint8_t blob[SETTINGS_BLOB_SIZE];
  packSettings(s, blob);
  blob[1] = 99;  // brightness
  blob[2] = 99;  // sleep index
  DeckSettings back;
  TEST_ASSERT_TRUE(unpackSettings(blob, sizeof(blob), back));
  TEST_ASSERT_EQUAL(BRIGHTNESS_LEVELS - 1, back.brightness);
  TEST_ASSERT_EQUAL(0, back.sleepIndex);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_backlight_duty_rises_and_never_goes_dark);
  RUN_TEST(test_sleep_timeouts);
  RUN_TEST(test_quiet_hours_wrap_past_midnight);
  RUN_TEST(test_app_index_matches_names_case_insensitively);
  RUN_TEST(test_notification_filter);
  RUN_TEST(test_round_trip_and_rejects_bad_blobs);
  RUN_TEST(test_unpack_clamps_out_of_range_values);
  UNITY_END();
}

void loop() {}
