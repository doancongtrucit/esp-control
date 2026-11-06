#include <Arduino.h>
#include <unity.h>

#include "format_util.h"

void test_grouped_numbers() {
  TEST_ASSERT_EQUAL_STRING("76,032", fmtGrouped(76032, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("1,740.73", fmtGrouped(1740.73, 2).c_str());
  TEST_ASSERT_EQUAL_STRING("999", fmtGrouped(999, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("-1,234.5", fmtGrouped(-1234.5, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("1,000,000", fmtGrouped(999999.6, 0).c_str());
}

void test_coin_prices_pick_precision() {
  TEST_ASSERT_EQUAL_STRING("$76,032", coinPriceText(76032.0).c_str());
  TEST_ASSERT_EQUAL_STRING("$692.97", coinPriceText(692.97).c_str());
  TEST_ASSERT_EQUAL_STRING("$0.6234", coinPriceText(0.6234).c_str());
}

void test_stock_price_from_thousand_vnd() {
  TEST_ASSERT_EQUAL_STRING("57,500", stockPriceText(57.5).c_str());
  TEST_ASSERT_EQUAL_STRING("21,550", stockPriceText(21.55).c_str());
}

void test_signed_percent() {
  TEST_ASSERT_EQUAL_STRING("+0.44%", signedPercent(0.44).c_str());
  TEST_ASSERT_EQUAL_STRING("-5.11%", signedPercent(-5.11).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00%", signedPercent(0).c_str());
}

void test_small_negative_values_keep_their_sign() {
  TEST_ASSERT_EQUAL_STRING("-0.44%", signedPercent(-0.44).c_str());
  TEST_ASSERT_EQUAL_STRING("-0.5", fmtGrouped(-0.5, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00%", signedPercent(-0.004).c_str());  // rounds to zero: no sign
}

void test_updated_text_shows_date_only_when_not_today() {
  TEST_ASSERT_EQUAL_STRING("09:44", updatedText("06/11", "09:44", "06/11").c_str());
  TEST_ASSERT_EQUAL_STRING("05/11 17:02", updatedText("05/11", "17:02", "06/11").c_str());
  TEST_ASSERT_EQUAL_STRING("09:44", updatedText("", "09:44", "06/11").c_str());   // old bridge: no date
  TEST_ASSERT_EQUAL_STRING("05/11 17:02", updatedText("05/11", "17:02", "").c_str());  // today unknown
  TEST_ASSERT_EQUAL_STRING("--", updatedText("", "", "06/11").c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_grouped_numbers);
  RUN_TEST(test_coin_prices_pick_precision);
  RUN_TEST(test_stock_price_from_thousand_vnd);
  RUN_TEST(test_signed_percent);
  RUN_TEST(test_small_negative_values_keep_their_sign);
  RUN_TEST(test_updated_text_shows_date_only_when_not_today);
  UNITY_END();
}

void loop() {}
