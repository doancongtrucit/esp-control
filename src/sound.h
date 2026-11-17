#pragma once

#include <Arduino.h>

// Speaker on the board's speaker connector: GPIO26 into the on-board amplifier.
// Tones are LEDC pulses (narrow duty = lower volume, so a weak USB port does not
// brown out); speech is 8-bit PCM through the ESP32's built-in DAC.

enum class Sound : uint8_t {
  None,
  Click,       // touch feedback
  Hold,        // a hold-to-confirm button fired
  TimerDone,   // countdown finished
  Alarm,       // alarm clock: repeats and grows louder until dismissed
  FocusDone,   // Pomodoro: focus over, take a break
  BreakDone,   // Pomodoro: break over
  ClaudeAsk,   // Claude Code waits for a permission answer
  ClaudeDone,  // a Claude Code task finished
  ClaudeFail,  // a session failed 3 times in a row
  NotifyPhone,
  NotifyPc,
  Reminder,    // stand up / rest your eyes
  Alert,       // something needs attention (home server offline, ...)
  Chime,       // gentle generic notice (Home Assistant, Hermes)
  Doorbell,
  Test,
};

// What a sound is for, so each group can be switched off on its own.
enum class SoundGroup : uint8_t { Timer, Claude, Notify, Reminder, Click, Alert };

struct Tone {
  uint16_t hz;  // 0 = silence
  uint16_t ms;
};

constexpr uint8_t SOUND_VOLUMES = 4;  // 0 off, 1 low, 2 medium, 3 high (capped)

// ---- pure (unit tested) ----
const Tone *soundTones(Sound s, uint8_t &count, bool &repeat);
SoundGroup soundGroup(Sound s);
Sound soundFromName(const char *name);  // "chime", "doorbell", "alert", ... ; None if unknown
// Index of the tone playing `elapsed` ms into the sound, or -1 when it has ended.
// `round` is how many times a repeating sound has started over.
int soundToneAt(const Tone *tones, uint8_t count, bool repeat, uint32_t elapsed, uint32_t &round);
uint16_t soundDuty(uint8_t volume);  // LEDC duty out of 1023
// Allowed now? Timers and alarms ring through quiet hours; the rest stay silent.
bool soundAllowed(Sound s, uint8_t volume, uint8_t groupMask, bool quietNow);

// ---- hardware ----
void soundBegin();
void soundPlay(Sound s, uint8_t volume);
void soundStop();
bool soundPlaying();
void soundLoop();  // call every loop: steps through tones, feeds speech
// Plays 8-bit unsigned mono PCM; the buffer must stay valid until soundPlaying() is false.
bool soundPlayPcm(const uint8_t *data, size_t len, uint16_t rate, uint8_t volume);
