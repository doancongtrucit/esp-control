#include "timer_logic.h"

uint32_t pomoSeconds(PomoPhase phase) {
  switch (phase) {
    case PomoPhase::ShortBreak: return POMO_SHORT_S;
    case PomoPhase::LongBreak: return POMO_LONG_S;
    default: return POMO_FOCUS_S;
  }
}

PomoPhase pomoNext(PomoPhase phase, uint8_t &done) {
  if (phase != PomoPhase::Focus) {
    if (phase == PomoPhase::LongBreak) done = 0;  // a new set of four
    return PomoPhase::Focus;
  }
  done = done + 1;
  return done >= POMO_ROUNDS ? PomoPhase::LongBreak : PomoPhase::ShortBreak;
}

bool alarmDue(bool on, uint8_t hour, uint8_t minute, AlarmRepeat repeat, int nowHour, int nowMinute, int weekday,
              long dayNumber, long &firedKey) {
  if (!on || nowHour != hour || nowMinute != minute) return false;
  if (repeat == AlarmRepeat::Weekdays && weekday > 4) return false;
  const long key = dayNumber * 1440L + nowHour * 60L + nowMinute;
  if (key == firedKey) return false;
  firedKey = key;
  return true;
}

String timerText(uint32_t seconds) {
  char text[12];
  if (seconds >= 3600)
    snprintf(text, sizeof(text), "%lu:%02lu:%02lu", static_cast<unsigned long>(seconds / 3600),
             static_cast<unsigned long>((seconds / 60) % 60), static_cast<unsigned long>(seconds % 60));
  else
    snprintf(text, sizeof(text), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60),
             static_cast<unsigned long>(seconds % 60));
  return text;
}
