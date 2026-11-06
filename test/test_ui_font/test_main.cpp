#include <Arduino.h>
#include <TFT_eSPI.h>
#include <unity.h>

#include "ui_font.h"

TFT_eSPI tft;  // never initialised: fonts only need the metrics, not the panel

void test_legacy_sizes_map_to_noto_fonts() {
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Small), static_cast<int>(uiFontForSize(1)));
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Body), static_cast<int>(uiFontForSize(2)));
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Large), static_cast<int>(uiFontForSize(4)));
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Huge), static_cast<int>(uiFontForSize(6)));
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Huge), static_cast<int>(uiFontForSize(7)));
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Body), static_cast<int>(uiFontForSize(3)));
}

void test_body_font_has_every_vietnamese_letter() {
  uiUseFont(UiFont::Body);
  TEST_ASSERT_EQUAL(static_cast<int>(UiFont::Body), static_cast<int>(uiCurrentFont()));
  const uint16_t letters[] = {0x0110, 0x1EC5, 0x01B0, 0x01A1, 0x1EE7, 0x00E0, 0x1EF9, 0x00B0};
  for (uint16_t cp : letters) {
    uint16_t index = 0;
    TEST_ASSERT_TRUE_MESSAGE(tft.getUnicodeIndex(cp, &index), String(cp, HEX).c_str());
  }
  TEST_ASSERT_INT_WITHIN(6, 20, tft.fontHeight());
}

void test_widths_come_from_the_selected_font() {
  const int body = uiTextWidth("Coin", 2);
  const int small = uiTextWidth("Coin", 1);
  const int large = uiTextWidth("Coin", 4);
  const int tiny = uiTextWidthIn(UiFont::Tiny, "Coin");
  TEST_ASSERT_GREATER_THAN(0, tiny);
  TEST_ASSERT_GREATER_THAN(tiny, small);
  TEST_ASSERT_GREATER_THAN(small, body);
  TEST_ASSERT_GREATER_THAN(body, large);
  TEST_ASSERT_INT_WITHIN(25, 60, uiTextWidth("10", 6));  // two 52 px digits
}

void test_switching_fonts_does_not_leak() {
  uiUseFont(UiFont::Small);
  const uint32_t before = ESP.getFreeHeap();
  for (int i = 0; i < 40; ++i) {
    uiUseFont(UiFont::Body);
    uiUseFont(UiFont::Large);
  }
  uiUseFont(UiFont::Small);
  TEST_ASSERT_UINT32_WITHIN(64, before, ESP.getFreeHeap());
}

void setup() {
  delay(2000);
  uiFontBegin(tft);
  UNITY_BEGIN();
  RUN_TEST(test_legacy_sizes_map_to_noto_fonts);
  RUN_TEST(test_body_font_has_every_vietnamese_letter);
  RUN_TEST(test_widths_come_from_the_selected_font);
  RUN_TEST(test_switching_fonts_does_not_leak);
  UNITY_END();
}

void loop() {}
