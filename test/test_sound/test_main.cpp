#include <Arduino.h>
#include <unity.h>

#include "sound.h"

void test_tone_at_steps_through_a_sound() {
  const Tone tones[] = {{440, 100}, {0, 50}, {880, 200}};
  uint32_t round = 0;
  TEST_ASSERT_EQUAL_INT(0, soundToneAt(tones, 3, false, 0, round));
  TEST_ASSERT_EQUAL_INT(0, soundToneAt(tones, 3, false, 99, round));
  TEST_ASSERT_EQUAL_INT(1, soundToneAt(tones, 3, false, 100, round));
  TEST_ASSERT_EQUAL_INT(2, soundToneAt(tones, 3, false, 349, round));
  TEST_ASSERT_EQUAL_INT(-1, soundToneAt(tones, 3, false, 350, round));
}

void test_repeating_sound_counts_rounds() {
  const Tone tones[] = {{440, 100}, {0, 100}};
  uint32_t round = 0;
  TEST_ASSERT_EQUAL_INT(1, soundToneAt(tones, 2, true, 550, round));  // 3rd round, 150 ms in
  TEST_ASSERT_EQUAL_UINT32(2, round);
}

void test_every_sound_has_tones_and_a_name() {
  const char *names[] = {"chime", "doorbell", "alert", "timer", "alarm", "claude_ask", "phone", "test"};
  for (const char *n : names) {
    const Sound s = soundFromName(n);
    TEST_ASSERT_TRUE_MESSAGE(s != Sound::None, n);
    uint8_t count = 0;
    bool repeat = false;
    TEST_ASSERT_NOT_NULL(soundTones(s, count, repeat));
    TEST_ASSERT_TRUE(count > 0);
  }
  TEST_ASSERT_TRUE(soundFromName("nope") == Sound::None);
  uint8_t count;
  bool repeat;
  soundTones(Sound::Alarm, count, repeat);
  TEST_ASSERT_TRUE(repeat);
}

void test_volume_is_capped_and_zero_is_silent() {
  TEST_ASSERT_EQUAL_UINT16(0, soundDuty(0));
  TEST_ASSERT_TRUE(soundDuty(1) < soundDuty(2));
  TEST_ASSERT_TRUE(soundDuty(3) <= 48);  // measured safe on a weak USB port
  TEST_ASSERT_EQUAL_UINT16(soundDuty(3), soundDuty(9));
}

void test_allowed_rules() {
  const uint8_t all = 0xFF;
  TEST_ASSERT_TRUE(soundAllowed(Sound::ClaudeAsk, 2, all, false));
  TEST_ASSERT_FALSE(soundAllowed(Sound::ClaudeAsk, 0, all, false));                 // volume off
  TEST_ASSERT_FALSE(soundAllowed(Sound::ClaudeAsk, 2, all, true));                  // quiet hours
  TEST_ASSERT_TRUE(soundAllowed(Sound::Alarm, 2, all, true));                       // alarms ring anyway
  const uint8_t noClaude = all & ~(1u << static_cast<uint8_t>(SoundGroup::Claude));
  TEST_ASSERT_FALSE(soundAllowed(Sound::ClaudeAsk, 2, noClaude, false));            // group off
  TEST_ASSERT_TRUE(soundAllowed(Sound::NotifyPc, 2, noClaude, false));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_tone_at_steps_through_a_sound);
  RUN_TEST(test_repeating_sound_counts_rounds);
  RUN_TEST(test_every_sound_has_tones_and_a_name);
  RUN_TEST(test_volume_is_capped_and_zero_is_silent);
  RUN_TEST(test_allowed_rules);
  UNITY_END();
}

void loop() {}
