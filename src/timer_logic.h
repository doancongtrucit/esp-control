#pragma once

#include <Arduino.h>

// Pomodoro: 25 min focus, 5 min break, a 15 min long break after every 4th focus.
enum class PomoPhase : uint8_t { Focus, ShortBreak, LongBreak };

constexpr uint32_t POMO_FOCUS_S = 25 * 60;
constexpr uint32_t POMO_SHORT_S = 5 * 60;
constexpr uint32_t POMO_LONG_S = 15 * 60;
constexpr uint8_t POMO_ROUNDS = 4;

uint32_t pomoSeconds(PomoPhase phase);

// After `phase` ends: the next phase. `done` counts finished focus rounds (1..4), updated in place.
PomoPhase pomoNext(PomoPhase phase, uint8_t &done);

// Alarm repeat: once (turns itself off), every day, Monday to Friday.
enum class AlarmRepeat : uint8_t { Once, Daily, Weekdays };

// True when the alarm should ring at this minute. weekday: 0 = Monday ... 6 = Sunday.
// `firedKey` remembers the last minute it rang (day * 1440 + minute) so it rings once per minute.
bool alarmDue(bool on, uint8_t hour, uint8_t minute, AlarmRepeat repeat, int nowHour, int nowMinute, int weekday,
              long dayNumber, long &firedKey);

// "05:00", "1:05:00" for an hour or more.
String timerText(uint32_t seconds);
