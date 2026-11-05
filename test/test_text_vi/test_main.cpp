#include <Arduino.h>
#include <unity.h>

#include "text_vi.h"

void test_keeps_vietnamese_diacritics() {
  TEST_ASSERT_EQUAL_STRING("Đi ngủ đi", sanitizeVi("Đi ngủ đi", 40).c_str());
}

void test_drops_emoji_and_trims() {
  TEST_ASSERT_EQUAL_STRING("Bạn An", sanitizeVi("Bạn An 👩", 40).c_str());
}

void test_collapses_whitespace_and_newlines() {
  TEST_ASSERT_EQUAL_STRING("Nguyễn Văn A", sanitizeVi("Nguyễn  Văn\nA", 40).c_str());
}

void test_cuts_on_character_boundary_with_ellipsis() {
  const String cut = sanitizeVi("Tiếng Việt có dấu", 10);
  TEST_ASSERT_EQUAL_STRING("Tiếng V...", cut.c_str());
  TEST_ASSERT_EQUAL(10u, utf8Length(cut));
}

void test_long_ascii_is_limited_in_characters() {
  String longText;
  for (int i = 0; i < 300; ++i) longText += 'x';
  TEST_ASSERT_EQUAL(40u, utf8Length(sanitizeVi(longText, 40)));
}

void test_invalid_bytes_are_dropped() {
  const char bad[] = {'a', static_cast<char>(0xC3), 'b', static_cast<char>(0xFF), 'c', 0};
  TEST_ASSERT_EQUAL_STRING("abc", sanitizeVi(bad, 40).c_str());
}

void test_font_has_every_vietnamese_letter() {
  // Base letters with their own glyphs plus all five tone marks on a.
  const char *letters[] = {"Đ", "đ", "Ă", "ă", "Â", "â", "Ê", "ê", "Ô", "ô", "Ơ", "ơ", "Ư", "ư",
                           "á", "à", "ả", "ã", "ạ", "ỹ", "Ỵ", "ữ", "ộ"};
  for (const char *letter : letters) {
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, viTextWidth(letter), letter);
  }
}

void test_fit_keeps_short_text() {
  TEST_ASSERT_EQUAL_STRING("Đi ngủ đi", fitViText("Đi ngủ đi", 300).c_str());
}

void test_fit_cuts_to_pixel_width_with_ellipsis() {
  const String fitted = fitViText("Nguyễn Văn A gửi một tin nhắn rất dài cho bạn", 100);
  TEST_ASSERT_LESS_OR_EQUAL(100, viTextWidth(fitted));
  TEST_ASSERT_TRUE(fitted.endsWith("..."));
  TEST_ASSERT_TRUE(fitted.startsWith("Nguy"));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_keeps_vietnamese_diacritics);
  RUN_TEST(test_drops_emoji_and_trims);
  RUN_TEST(test_collapses_whitespace_and_newlines);
  RUN_TEST(test_cuts_on_character_boundary_with_ellipsis);
  RUN_TEST(test_long_ascii_is_limited_in_characters);
  RUN_TEST(test_invalid_bytes_are_dropped);
  RUN_TEST(test_font_has_every_vietnamese_letter);
  RUN_TEST(test_fit_keeps_short_text);
  RUN_TEST(test_fit_cuts_to_pixel_width_with_ellipsis);
  UNITY_END();
}

void loop() {}
