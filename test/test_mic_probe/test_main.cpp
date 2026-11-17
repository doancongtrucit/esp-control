// Hardware probe: is an INMP441 I2S microphone answering on SCK=27, WS=22, SD=35 (L/R to GND)?
// Run: pio test -e cyd_2usb -f test_mic_probe   (talk or clap during the 6 s window)
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <unity.h>

constexpr int PIN_SCK = 27, PIN_WS = 22, PIN_SD = 35;
constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr int RATE = 16000;

struct Stats {
  long count = 0, zeros = 0, ones = 0;
  int32_t minV = INT32_MAX, maxV = INT32_MIN;
  double sum = 0, sumSq = 0;
  void add(int32_t raw) {
    const int32_t v = raw >> 8;  // 24-bit sample, left aligned in 32 bits
    ++count;
    if (raw == 0) ++zeros;
    if (raw == -1 || raw == 0x7FFFFFFF) ++ones;
    minV = min(minV, v);
    maxV = max(maxV, v);
    sum += v;
    sumSq += static_cast<double>(v) * v;
  }
  double mean() const { return count ? sum / count : 0; }
  double rms() const {  // AC part only (the INMP441 has a small DC offset)
    if (!count) return 0;
    const double m = mean();
    return sqrt(max(0.0, sumSq / count - m * m));
  }
  double dbfs() const { return rms() > 0 ? 20 * log10(rms() / 8388608.0) : -200; }
};

Stats left, right;

static void report(const char *name, const Stats &s) {
  char line[160];
  snprintf(line, sizeof(line), "%s: n=%ld zeros=%ld stuck=%ld min=%ld max=%ld mean=%.0f rms=%.0f (%.1f dBFS)", name,
           s.count, s.zeros, s.ones, static_cast<long>(s.minV), static_cast<long>(s.maxV), s.mean(), s.rms(), s.dbfs());
  TEST_MESSAGE(line);
}

void test_mic_answers() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;  // read both slots, see which one carries the mic
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  TEST_ASSERT_EQUAL(ESP_OK, i2s_driver_install(PORT, &cfg, 0, nullptr));
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_SCK;
  pins.ws_io_num = PIN_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_SD;
  TEST_ASSERT_EQUAL(ESP_OK, i2s_set_pin(PORT, &pins));
  delay(300);  // the INMP441 needs ~85 ms after the clock starts

  static int32_t buf[512];
  const uint32_t start = millis();
  int window = 0;
  Stats winL, winR;
  while (millis() - start < 6000) {
    size_t got = 0;
    i2s_read(PORT, buf, sizeof(buf), &got, portMAX_DELAY);
    for (size_t i = 0; i + 1 < got / 4; i += 2) {
      // slot order on the ESP32 is right, left
      right.add(buf[i]);
      left.add(buf[i + 1]);
      winR.add(buf[i]);
      winL.add(buf[i + 1]);
    }
    if (millis() - start >= static_cast<uint32_t>(window + 1) * 1000) {
      char line[120];
      snprintf(line, sizeof(line), "second %d: left %.1f dBFS, right %.1f dBFS", window + 1, winL.dbfs(), winR.dbfs());
      TEST_MESSAGE(line);
      winL = Stats();
      winR = Stats();
      ++window;
    }
  }
  i2s_driver_uninstall(PORT);
  report("LEFT ", left);
  report("RIGHT", right);

  const Stats &mic = left.rms() >= right.rms() ? left : right;
  TEST_ASSERT_TRUE_MESSAGE(mic.count > RATE, "no samples read");
  TEST_ASSERT_TRUE_MESSAGE(mic.zeros < mic.count * 0.95, "data line always 0: SD not connected, or L/R / power wrong");
  TEST_ASSERT_TRUE_MESSAGE(mic.ones < mic.count * 0.95, "data line stuck high: SD floating or mic not powered");
  TEST_ASSERT_TRUE_MESSAGE(mic.maxV != mic.minV, "samples never change");
  TEST_ASSERT_TRUE_MESSAGE(mic.dbfs() < -3, "signal is full scale: looks like noise on a floating pin, not a microphone");
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_mic_answers);
  UNITY_END();
}

void loop() {}
