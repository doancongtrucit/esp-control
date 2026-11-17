// Hardware probe: is the flicker the display/power, or the software redrawing?
// A: static picture, backlight 100 % - nothing is redrawn.
// B: same static picture while the speaker draws current (power stress).
// C: backlight dimmed with PWM (30 %).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_system.h>
#include <unity.h>

TFT_eSPI tft;
constexpr int SPEAKER_PIN = 26;

static void screen(const char *phase, const char *hint, uint16_t accent) {
  tft.fillScreen(TFT_BLACK);
  const uint16_t bars[6] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_YELLOW, TFT_CYAN};
  for (int i = 0; i < 6; ++i) tft.fillRect(i * 53, 0, 53, 90, bars[i]);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(accent, TFT_BLACK);
  tft.drawString(phase, 160, 130, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(hint, 160, 170, 2);
  tft.drawString("Watch: does it flicker?", 160, 200, 2);
}

void test_display_phases() {
  char line[96];
  snprintf(line, sizeof(line), "reset reason before this run: %d (1=power-on 3=sw 9=brownout)",
           static_cast<int>(esp_reset_reason()));
  TEST_MESSAGE(line);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(DECK_ROTATION);

  screen("A: STATIC", "nothing is redrawn, backlight 100%", TFT_GREEN);
  TEST_MESSAGE("phase A: static, 12 s");
  delay(12000);

  screen("B: POWER LOAD", "speaker on, screen still static", TFT_ORANGE);
  TEST_MESSAGE("phase B: speaker load, 10 s");
  ledcSetup(0, 1000, 10);
  ledcAttachPin(SPEAKER_PIN, 0);
  ledcWrite(0, 40);  // the "High" volume the app allows
  delay(10000);
  ledcWrite(0, 0);
  ledcDetachPin(SPEAKER_PIN);

  screen("C: DIMMED 30%", "backlight PWM at 30%", TFT_CYAN);
  TEST_MESSAGE("phase C: backlight 30 %, 8 s");
  ledcSetup(7, 5000, 8);
  ledcAttachPin(TFT_BL, 7);
  ledcWrite(7, 77);
  delay(8000);
  ledcWrite(7, 255);
  TEST_PASS_MESSAGE("all phases shown without a reset");
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_display_phases);
  UNITY_END();
}

void loop() {}
