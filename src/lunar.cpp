#include "lunar.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace {
constexpr double PI_D = 3.14159265358979323846;

long julianDay(int dd, int mm, int yy) {
  const long a = (14 - mm) / 12;
  const long y = yy + 4800 - a;
  const long m = mm + 12 * a - 3;
  long jd = dd + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
  if (jd < 2299161) jd = dd + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
  return jd;
}

double newMoon(long k) {
  const double T = k / 1236.85;
  const double T2 = T * T;
  const double T3 = T2 * T;
  const double dr = PI_D / 180;
  double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
  jd1 += 0.00033 * sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);
  const double M = 359.2242 + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3;
  const double Mpr = 306.0253 + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3;
  const double F = 21.2964 + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3;
  double c1 = (0.1734 - 0.000393 * T) * sin(M * dr) + 0.0021 * sin(2 * dr * M);
  c1 = c1 - 0.4068 * sin(Mpr * dr) + 0.0161 * sin(dr * 2 * Mpr);
  c1 = c1 - 0.0004 * sin(dr * 3 * Mpr);
  c1 = c1 + 0.0104 * sin(dr * 2 * F) - 0.0051 * sin(dr * (M + Mpr));
  c1 = c1 - 0.0074 * sin(dr * (M - Mpr)) + 0.0004 * sin(dr * (2 * F + M));
  c1 = c1 - 0.0004 * sin(dr * (2 * F - M)) - 0.0006 * sin(dr * (2 * F + Mpr));
  c1 = c1 + 0.0010 * sin(dr * (2 * F - Mpr)) + 0.0005 * sin(dr * (2 * Mpr + M));
  const double deltaT = T < -11
                            ? 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3
                            : -0.000278 + 0.000265 * T + 0.000262 * T2;
  return jd1 + c1 - deltaT;
}

long newMoonDay(long k, int tz) { return static_cast<long>(newMoon(k) + 0.5 + tz / 24.0); }

int sunLongitude(long jdn, int tz) {
  const double T = (jdn - 2451545.5 - tz / 24.0) / 36525;
  const double T2 = T * T;
  const double dr = PI_D / 180;
  const double M = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
  const double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
  double DL = (1.914600 - 0.004817 * T - 0.000014 * T2) * sin(dr * M);
  DL = DL + (0.019993 - 0.000101 * T) * sin(dr * 2 * M) + 0.000290 * sin(dr * 3 * M);
  double L = (L0 + DL) * dr;
  L = L - PI_D * 2 * static_cast<long>(L / (PI_D * 2));
  return static_cast<int>(L / PI_D * 6);
}

// New moon day that starts lunar month 11 of the given year.
long lunarMonth11(int yy, int tz) {
  const long off = julianDay(31, 12, yy) - 2415021;
  const long k = static_cast<long>(off / 29.530588853);
  long nm = newMoonDay(k, tz);
  if (sunLongitude(nm, tz) >= 9) nm = newMoonDay(k - 1, tz);
  return nm;
}

int leapMonthOffset(long a11, int tz) {
  const long k = static_cast<long>((a11 - 2415021.076998695) / 29.530588853 + 0.5);
  int i = 1;
  int arc = sunLongitude(newMoonDay(k + i, tz), tz);
  int last;
  do {
    last = arc;
    ++i;
    arc = sunLongitude(newMoonDay(k + i, tz), tz);
  } while (arc != last && i < 14);
  return i - 1;
}

const char *const CAN[10] = {"Giap", "At", "Binh", "Dinh", "Mau", "Ky", "Canh", "Tan", "Nham", "Quy"};
const char *const CHI[12] = {"Ty", "Suu", "Dan", "Mao", "Thin", "Ty", "Ngo", "Mui", "Than", "Dau", "Tuat", "Hoi"};
}  // namespace

LunarDate solarToLunar(int dd, int mm, int yy, int tz) {
  const long dayNumber = julianDay(dd, mm, yy);
  const long k = static_cast<long>((dayNumber - 2415021.076998695) / 29.530588853);
  long monthStart = newMoonDay(k + 1, tz);
  if (monthStart > dayNumber) monthStart = newMoonDay(k, tz);
  long a11 = lunarMonth11(yy, tz);
  long b11 = a11;
  int lunarYear;
  if (a11 >= monthStart) {
    lunarYear = yy;
    a11 = lunarMonth11(yy - 1, tz);
  } else {
    lunarYear = yy + 1;
    b11 = lunarMonth11(yy + 1, tz);
  }
  const int lunarDay = static_cast<int>(dayNumber - monthStart + 1);
  const int diff = static_cast<int>((monthStart - a11) / 29);
  bool leap = false;
  int lunarMonth = diff + 11;
  if (b11 - a11 > 365) {
    const int leapDiff = leapMonthOffset(a11, tz);
    if (diff >= leapDiff) {
      lunarMonth = diff + 10;
      if (diff == leapDiff) leap = true;
    }
  }
  if (lunarMonth > 12) lunarMonth -= 12;
  if (lunarMonth >= 11 && diff < 4) lunarYear -= 1;
  return {lunarDay, lunarMonth, lunarYear, leap};
}

const char *canChiYear(int lunarYear) {
  static char name[16];
  snprintf(name, sizeof(name), "%s %s", CAN[(lunarYear + 6) % 10], CHI[(lunarYear + 8) % 12]);
  return name;
}

int dayOfWeek(int day, int month, int year) { return static_cast<int>(julianDay(day, month, year) % 7); }

namespace {
struct DatedHoliday {
  uint8_t day, month;
  VnHoliday name;
};

const DatedHoliday SOLAR_HOLIDAYS[] = {
    {1, 1, {"Tết Dương lịch", "Tet Duong lich"}},
    {14, 2, {"Valentine", "Valentine"}},
    {8, 3, {"Quốc tế Phụ nữ", "Quoc te Phu nu"}},
    {30, 4, {"Giải phóng miền Nam", "Giai phong mien Nam"}},
    {1, 5, {"Quốc tế Lao động", "Quoc te Lao dong"}},
    {1, 6, {"Quốc tế Thiếu nhi", "Quoc te Thieu nhi"}},
    {2, 9, {"Quốc khánh", "Quoc khanh"}},
    {20, 10, {"Phụ nữ Việt Nam", "Phu nu Viet Nam"}},
    {20, 11, {"Nhà giáo Việt Nam", "Nha giao Viet Nam"}},
    {25, 12, {"Giáng sinh", "Giang sinh"}},
};

const DatedHoliday LUNAR_HOLIDAYS[] = {
    {1, 1, {"Tết Nguyên Đán", "Tet Nguyen Dan"}},
    {2, 1, {"Mùng 2 Tết", "Mung 2 Tet"}},
    {3, 1, {"Mùng 3 Tết", "Mung 3 Tet"}},
    {15, 1, {"Rằm tháng Giêng", "Ram thang Gieng"}},
    {10, 3, {"Giỗ Tổ Hùng Vương", "Gio To Hung Vuong"}},
    {15, 4, {"Lễ Phật Đản", "Le Phat Dan"}},
    {5, 5, {"Tết Đoan Ngọ", "Tet Doan Ngo"}},
    {15, 7, {"Lễ Vu Lan", "Le Vu Lan"}},
    {15, 8, {"Tết Trung Thu", "Trung Thu"}},
    {23, 12, {"Ông Công Ông Táo", "Ong Cong Ong Tao"}},
};

const VnHoliday NEW_YEARS_EVE = {"Giao thừa", "Giao thua"};

int monthLength(int month, int year) {
  static const int DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return month == 2 && leapYear ? 29 : DAYS[month - 1];
}

void nextDay(int &day, int &month, int &year) {
  if (++day <= monthLength(month, year)) return;
  day = 1;
  if (++month <= 12) return;
  month = 1;
  ++year;
}
}  // namespace

const VnHoliday *vnHoliday(int day, int month, int year) {
  const LunarDate l = solarToLunar(day, month, year);
  if (!l.leap) {
    for (const auto &h : LUNAR_HOLIDAYS)
      if (h.day == l.day && h.month == l.month) return &h.name;
    if (l.month == 12) {  // the last day of the lunar year
      int d = day, m = month, y = year;
      nextDay(d, m, y);
      const LunarDate tomorrow = solarToLunar(d, m, y);
      if (tomorrow.day == 1 && tomorrow.month == 1) return &NEW_YEARS_EVE;
    }
  }
  for (const auto &h : SOLAR_HOLIDAYS)
    if (h.day == day && h.month == month) return &h.name;
  return nullptr;
}

int daysToHoliday(int day, int month, int year, const VnHoliday **next, int maxDays) {
  for (int i = 0; i <= maxDays; ++i) {
    const VnHoliday *h = vnHoliday(day, month, year);
    if (h) {
      if (next) *next = h;
      return i;
    }
    nextDay(day, month, year);
  }
  return -1;
}
