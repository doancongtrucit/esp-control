#include <Arduino.h>
#include <unity.h>

#include "color_util.h"

void test_rgb565_matches_palette() {
  TEST_ASSERT_EQUAL_HEX16(0xFC47, rgb565(0xff8a3d));
  TEST_ASSERT_EQUAL_HEX16(0x0862, rgb565(0x0d0f14));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(0xffffff));
}

void test_blend_endpoints_and_middle() {
  const uint16_t a = rgb565(0x30d158);
  const uint16_t b = rgb565(0x1a9e48);
  TEST_ASSERT_EQUAL_HEX16(a, blend565(a, b, 0));
  TEST_ASSERT_EQUAL_HEX16(b, blend565(a, b, 255));
  const uint16_t mid = blend565(0x0000, 0xFFFF, 128);
  TEST_ASSERT_TRUE((mid >> 11) >= 14 && (mid >> 11) <= 17);           // red ~half of 31
  TEST_ASSERT_TRUE(((mid >> 5) & 0x3F) >= 30 && ((mid >> 5) & 0x3F) <= 33);  // green ~half of 63
}

void test_rounded_row_inset_shape() {
  TEST_ASSERT_GREATER_THAN(0, roundedRowInset(0, 48, 13));
  TEST_ASSERT_LESS_OR_EQUAL(13, roundedRowInset(0, 48, 13));
  TEST_ASSERT_EQUAL(0, roundedRowInset(24, 48, 13));
  TEST_ASSERT_EQUAL(roundedRowInset(0, 48, 13), roundedRowInset(47, 48, 13));
  for (int row = 0; row < 48; ++row) {
    const int inset = roundedRowInset(row, 48, 13);
    TEST_ASSERT_TRUE(inset >= 0 && inset <= 13);
  }
  TEST_ASSERT_GREATER_OR_EQUAL(roundedRowInset(1, 48, 13), roundedRowInset(0, 48, 13));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_rgb565_matches_palette);
  RUN_TEST(test_blend_endpoints_and_middle);
  RUN_TEST(test_rounded_row_inset_shape);
  UNITY_END();
}

void loop() {}
