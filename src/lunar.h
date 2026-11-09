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

// Vietnamese public and popular holidays, solar and lunar (no leap months).
struct VnHoliday {
  const char *vi;     // UTF-8 with diacritics, for drawViText
  const char *ascii;  // for the built-in ASCII fonts
};

// The holiday on this solar date, or nullptr. A lunar holiday wins over a solar one.
const VnHoliday *vnHoliday(int day, int month, int year);

// Days from this date to the next holiday (0 = today), -1 when none within maxDays.
int daysToHoliday(int day, int month, int year, const VnHoliday **next, int maxDays = 400);
