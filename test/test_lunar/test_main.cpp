#include <Arduino.h>
#include <unity.h>

#include "lunar.h"

static void assertLunar(int d, int m, int y, int ld, int lm, int ly, bool leap) {
  const LunarDate l = solarToLunar(d, m, y);
  char msg[48];
  snprintf(msg, sizeof(msg), "%04d-%02d-%02d -> %d/%d/%d%s", y, m, d, l.day, l.month, l.year, l.leap ? "L" : "");
  TEST_ASSERT_EQUAL_MESSAGE(ld, l.day, msg);
  TEST_ASSERT_EQUAL_MESSAGE(lm, l.month, msg);
  TEST_ASSERT_EQUAL_MESSAGE(ly, l.year, msg);
  TEST_ASSERT_EQUAL_MESSAGE(leap, l.leap, msg);
}

void test_tet_2026() { assertLunar(17, 2, 2026, 1, 1, 2026, false); }
void test_today() { assertLunar(5, 11, 2026, 27, 9, 2026, false); }
void test_first_of_tenth_month() { assertLunar(9, 11, 2026, 1, 10, 2026, false); }
void test_full_moon() { assertLunar(23, 11, 2026, 15, 10, 2026, false); }
void test_leap_sixth_month_2025() { assertLunar(25, 7, 2025, 1, 6, 2025, true); }
void test_regular_sixth_month_2025() { assertLunar(25, 6, 2025, 1, 6, 2025, false); }
void test_before_tet_is_previous_lunar_year() { assertLunar(16, 2, 2026, 29, 12, 2025, false); }

void test_can_chi_and_weekday() {
  TEST_ASSERT_EQUAL_STRING("Binh Ngo", canChiYear(2026));
  TEST_ASSERT_EQUAL_STRING("At Ty", canChiYear(2025));
  TEST_ASSERT_EQUAL(3, dayOfWeek(5, 11, 2026));  // Thursday, Monday = 0
  TEST_ASSERT_EQUAL(6, dayOfWeek(1, 11, 2026));  // Sunday
}

void test_holidays() {
  TEST_ASSERT_EQUAL_STRING("Tet Nguyen Dan", vnHoliday(17, 2, 2026)->ascii);
  TEST_ASSERT_EQUAL_STRING("Giao thua", vnHoliday(16, 2, 2026)->ascii);
  TEST_ASSERT_EQUAL_STRING("Quoc khanh", vnHoliday(2, 9, 2026)->ascii);
  TEST_ASSERT_EQUAL_STRING("Trung Thu", vnHoliday(25, 9, 2026)->ascii);
  TEST_ASSERT_EQUAL_STRING("T\xe1\xba\xbf" "t Trung Thu", vnHoliday(25, 9, 2026)->vi);
  TEST_ASSERT_NULL(vnHoliday(10, 11, 2026));
}

void test_days_to_next_holiday() {
  const VnHoliday *next = nullptr;
  TEST_ASSERT_EQUAL(10, daysToHoliday(10, 11, 2026, &next));
  TEST_ASSERT_EQUAL_STRING("Nha giao Viet Nam", next->ascii);
  TEST_ASSERT_EQUAL(0, daysToHoliday(2, 9, 2026, &next));
  TEST_ASSERT_EQUAL(1, daysToHoliday(31, 12, 2026, &next));  // across the year end
  TEST_ASSERT_EQUAL_STRING("Tet Duong lich", next->ascii);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_tet_2026);
  RUN_TEST(test_today);
  RUN_TEST(test_first_of_tenth_month);
  RUN_TEST(test_full_moon);
  RUN_TEST(test_leap_sixth_month_2025);
  RUN_TEST(test_regular_sixth_month_2025);
  RUN_TEST(test_before_tet_is_previous_lunar_year);
  RUN_TEST(test_can_chi_and_weekday);
  RUN_TEST(test_holidays);
  RUN_TEST(test_days_to_next_holiday);
  UNITY_END();
}

void loop() {}
