// Hardware probe: does the speaker on the board's speaker connector (GPIO26, on-board amplifier) play?
// Run: pio test -e cyd_2usb -f test_speaker_probe   - listen: 3 beeps (low, mid, high), a short tune, twice.
#include <Arduino.h>
#include <unity.h>

constexpr int SPEAKER_PIN = 26;
constexpr int CHANNEL = 0;

// Low volume: a narrow pulse (duty VOLUME/1023) instead of a 50 % square wave, so the amplifier
// draws far less current (full volume browned out the board on a weak USB port).
constexpr uint32_t VOLUME = 40;

static void note(uint32_t hz, uint32_t ms) {
  ledcSetup(CHANNEL, hz, 10);
  ledcAttachPin(SPEAKER_PIN, CHANNEL);
  ledcWrite(CHANNEL, VOLUME);
  delay(ms);
  ledcWrite(CHANNEL, 0);
  delay(40);
}

void test_speaker_plays() {
  TEST_ASSERT_NOT_EQUAL(0, ledcSetup(CHANNEL, 1000, 10));
  for (int round = 0; round < 2; ++round) {
    TEST_MESSAGE(round == 0 ? "beeps 440 / 880 / 1760 Hz" : "again");
    note(440, 400);
    delay(200);
    note(880, 400);
    delay(200);
    note(1760, 400);
    delay(500);
    // a short rising tune (C major arpeggio)
    const uint32_t tune[] = {523, 659, 784, 1047, 784, 1047};
    const uint32_t lens[] = {180, 180, 180, 360, 180, 500};
    for (int i = 0; i < 6; ++i) note(tune[i], lens[i]);
    delay(800);
  }
  ledcDetachPin(SPEAKER_PIN);
  TEST_PASS_MESSAGE("tones sent to GPIO26 - did you hear them?");
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_speaker_plays);
  UNITY_END();
}

void loop() {}
