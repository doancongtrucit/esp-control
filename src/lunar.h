#pragma once

// Vietnamese lunar calendar (Ho Ngoc Duc's algorithm, astronomical new moons
// and solar terms, default time zone UTC+7).
struct LunarDate {
  int day;
  int month;
  int year;
  bool leap;  // leap (nhuan) month
};

LunarDate solarToLunar(int day, int month, int year, int tzHours = 7);

// Can-chi name of a lunar year in ASCII, e.g. 2026 -> "Binh Ngo".
const char *canChiYear(int lunarYear);

// 0 = Monday ... 6 = Sunday.
int dayOfWeek(int day, int month, int year);
