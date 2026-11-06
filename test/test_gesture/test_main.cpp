#include <Arduino.h>
#include <unity.h>

#include "gesture.h"

void test_quick_tap_is_a_tap_on_release() {
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::Pending), static_cast<int>(classifyGesture(2, 1, 60, false)));
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::Tap), static_cast<int>(classifyGesture(3, -2, 90, true)));
}

void test_still_finger_becomes_a_tap_while_held() {
  // Keeps hold-to-confirm working: the tap fires while the finger is still down.
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::Tap), static_cast<int>(classifyGesture(4, 3, GESTURE_HOLD_MS, false)));
}

void test_horizontal_drag_is_a_swipe() {
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::SwipeLeft), static_cast<int>(classifyGesture(-70, 8, 150, false)));
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::SwipeRight), static_cast<int>(classifyGesture(65, -10, 180, true)));
}

void test_short_or_vertical_drag_is_ignored() {
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::None), static_cast<int>(classifyGesture(30, 5, 200, true)));    // too short
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::None), static_cast<int>(classifyGesture(60, 50, 200, true)));   // too diagonal
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::Pending), static_cast<int>(classifyGesture(25, 0, 150, false)));  // still moving
}

void test_slow_wander_ends_without_action() {
  TEST_ASSERT_EQUAL(static_cast<int>(Gesture::None), static_cast<int>(classifyGesture(30, 30, GESTURE_MAX_MS, false)));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_quick_tap_is_a_tap_on_release);
  RUN_TEST(test_still_finger_becomes_a_tap_while_held);
  RUN_TEST(test_horizontal_drag_is_a_swipe);
  RUN_TEST(test_short_or_vertical_drag_is_ignored);
  RUN_TEST(test_slow_wander_ends_without_action);
  UNITY_END();
}

void loop() {}
