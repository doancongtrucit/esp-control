#include <Arduino.h>
#include <unity.h>

#include "icons.h"
#include "ui_layout.h"

void test_icon_names_map_case_insensitively() {
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Lock), static_cast<int>(iconFromName("lock")));
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::VolUp), static_cast<int>(iconFromName("VolUp")));
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Gamepad), static_cast<int>(iconFromName("gamepad")));
}

void test_unknown_icon_falls_back_to_controls() {
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Controls), static_cast<int>(iconFromName("nope")));
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Controls), static_cast<int>(iconFromName(nullptr)));
}

void test_home_grid_cells() {
  const CellRect first = gridCell(0, 3, 3, 32);
  TEST_ASSERT_EQUAL(8, first.x);
  TEST_ASSERT_EQUAL(40, first.y);
  TEST_ASSERT_EQUAL(97, first.w);
  TEST_ASSERT_EQUAL(60, first.h);
  const CellRect last = gridCell(8, 3, 3, 32);
  TEST_ASSERT_EQUAL(214, last.x);
  TEST_ASSERT_EQUAL(172, last.y);
}

void test_controls_grid_cells_are_equal_size() {
  const CellRect a = gridCell(0, 3, 2, 32);
  const CellRect f = gridCell(5, 3, 2, 32);
  TEST_ASSERT_EQUAL(97, a.w);
  TEST_ASSERT_EQUAL(93, a.h);
  TEST_ASSERT_EQUAL(a.w, f.w);
  TEST_ASSERT_EQUAL(a.h, f.h);
  TEST_ASSERT_EQUAL(214, f.x);
  TEST_ASSERT_EQUAL(139, f.y);
}

void test_hit_testing_ignores_padding_and_gaps() {
  TEST_ASSERT_EQUAL(0, gridIndexAt(60, 70, 3, 3, 32));
  TEST_ASSERT_EQUAL(8, gridIndexAt(300, 230, 3, 3, 32));
  TEST_ASSERT_EQUAL(-1, gridIndexAt(5, 100, 3, 3, 32));    // left padding
  TEST_ASSERT_EQUAL(-1, gridIndexAt(107, 70, 3, 3, 32));   // gap between columns
  TEST_ASSERT_EQUAL(-1, gridIndexAt(100, 20, 3, 3, 32));   // header
}

void test_launcher_grid_inside_a_box() {
  const CellRect first = gridCellIn(0, 4, 2, 8, 30, 304, 184, 0);
  TEST_ASSERT_EQUAL(8, first.x);
  TEST_ASSERT_EQUAL(30, first.y);
  TEST_ASSERT_EQUAL(76, first.w);
  TEST_ASSERT_EQUAL(92, first.h);
  const CellRect last = gridCellIn(7, 4, 2, 8, 30, 304, 184, 0);
  TEST_ASSERT_EQUAL(236, last.x);
  TEST_ASSERT_EQUAL(122, last.y);
  TEST_ASSERT_EQUAL(7, gridIndexIn(300, 200, 4, 2, 8, 30, 304, 184, 0));
  TEST_ASSERT_EQUAL(-1, gridIndexIn(5, 100, 4, 2, 8, 30, 304, 184, 0));
  TEST_ASSERT_EQUAL(-1, gridIndexIn(100, 220, 4, 2, 8, 30, 304, 184, 0));  // footer
}

void test_new_icon_names() {
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Bluetooth), static_cast<int>(iconFromName("bluetooth")));
  TEST_ASSERT_EQUAL(static_cast<int>(Icon::Pie), static_cast<int>(iconFromName("pie")));
}

void test_info_and_weather_icon_names() {
  const char *names[] = {"coin", "trend", "cloud", "calendar", "sun", "rain", "storm", "fog"};
  const Icon icons[] = {Icon::Coin, Icon::Trend, Icon::Cloud, Icon::Calendar,
                        Icon::Sun, Icon::Rain, Icon::Storm, Icon::Fog};
  for (int i = 0; i < 8; ++i) {
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(icons[i]), static_cast<int>(iconFromName(names[i])), names[i]);
  }
}

void test_info_tab_strip_cells() {
  const CellRect first = gridCellIn(0, 5, 1, 6, 33, 308, 30, 3);
  const CellRect last = gridCellIn(4, 5, 1, 6, 33, 308, 30, 3);
  TEST_ASSERT_EQUAL(59, first.w);
  TEST_ASSERT_EQUAL(first.w, last.w);
  TEST_ASSERT_EQUAL(4, gridIndexIn(300, 50, 5, 1, 6, 33, 308, 30, 3));
}

void test_weather_days_box_clears_the_footer() {
  const CellRect box = weatherDaysBox();
  TEST_ASSERT_GREATER_OR_EQUAL(UI_INFO_TOP + 52, box.y);              // below the current-conditions text
  TEST_ASSERT_LESS_OR_EQUAL(UI_INFO_FOOTER_Y - 6, box.y + box.h);    // above the "Updated" line
  TEST_ASSERT_GREATER_OR_EQUAL(96, box.h);                           // room for day, icon, hi/lo, rain
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_icon_names_map_case_insensitively);
  RUN_TEST(test_unknown_icon_falls_back_to_controls);
  RUN_TEST(test_home_grid_cells);
  RUN_TEST(test_controls_grid_cells_are_equal_size);
  RUN_TEST(test_hit_testing_ignores_padding_and_gaps);
  RUN_TEST(test_launcher_grid_inside_a_box);
  RUN_TEST(test_new_icon_names);
  RUN_TEST(test_info_and_weather_icon_names);
  RUN_TEST(test_info_tab_strip_cells);
  RUN_TEST(test_weather_days_box_clears_the_footer);
  UNITY_END();
}

void loop() {}
