#include <Arduino.h>
#include <unity.h>

#include "timer_logic.h"

void test_pomodoro_cycle() {
  uint8_t done = 0;
  PomoPhase p = PomoPhase::Focus;
  p = pomoNext(p, done);
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::ShortBreak), static_cast<int>(p));
  TEST_ASSERT_EQUAL_UINT8(1, done);
  for (int i = 0; i < 3; ++i) {
    p = pomoNext(p, done);  // focus
    p = pomoNext(p, done);  // break
  }
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::LongBreak), static_cast<int>(p));
  TEST_ASSERT_EQUAL_UINT8(4, done);
  p = pomoNext(p, done);
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Focus), static_cast<int>(p));
  TEST_ASSERT_EQUAL_UINT8(0, done);
  TEST_ASSERT_EQUAL_UINT32(25 * 60, pomoSeconds(PomoPhase::Focus));
  TEST_ASSERT_EQUAL_UINT32(15 * 60, pomoSeconds(PomoPhase::LongBreak));
}

void test_alarm_rings_once_per_minute() {
  long fired = -1;
  TEST_ASSERT_TRUE(alarmDue(true, 7, 0, AlarmRepeat::Daily, 7, 0, 2, 100, fired));
  TEST_ASSERT_FALSE(alarmDue(true, 7, 0, AlarmRepeat::Daily, 7, 0, 2, 100, fired));
  TEST_ASSERT_TRUE(alarmDue(true, 7, 0, AlarmRepeat::Daily, 7, 0, 3, 101, fired));
}

void test_alarm_rules() {
  long fired = -1;
  TEST_ASSERT_FALSE(alarmDue(false, 7, 0, AlarmRepeat::Daily, 7, 0, 2, 100, fired));
  TEST_ASSERT_FALSE(alarmDue(true, 7, 0, AlarmRepeat::Daily, 7, 1, 2, 100, fired));
  TEST_ASSERT_FALSE(alarmDue(true, 7, 0, AlarmRepeat::Weekdays, 7, 0, 5, 100, fired));  // Saturday
  TEST_ASSERT_TRUE(alarmDue(true, 7, 0, AlarmRepeat::Weekdays, 7, 0, 4, 100, fired));   // Friday
}

void test_timer_text() {
  TEST_ASSERT_EQUAL_STRING("05:00", timerText(300).c_str());
  TEST_ASSERT_EQUAL_STRING("00:09", timerText(9).c_str());
  TEST_ASSERT_EQUAL_STRING("1:05:00", timerText(3900).c_str());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_pomodoro_cycle);
  RUN_TEST(test_alarm_rings_once_per_minute);
  RUN_TEST(test_alarm_rules);
  RUN_TEST(test_timer_text);
  UNITY_END();
}

void loop() {}
