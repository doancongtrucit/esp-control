#include "sound.h"

#include <string.h>

namespace {
// Note frequencies (Hz)
constexpr uint16_t C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880, B5 = 988;
constexpr uint16_t C6 = 1047, E6 = 1319, G6 = 1568, A4 = 440, G4 = 392, E4 = 330, C4 = 262;

constexpr Tone CLICK[] = {{2400, 12}};
constexpr Tone HOLD[] = {{C6, 50}, {0, 30}, {G6, 70}};
constexpr Tone TIMER_DONE[] = {{G5, 160}, {0, 60}, {G5, 160}, {0, 60}, {C6, 360}, {0, 500},
                               {G5, 160}, {0, 60}, {G5, 160}, {0, 60}, {C6, 360}};
constexpr Tone ALARM[] = {{A5, 120}, {0, 80}, {A5, 120}, {0, 80}, {A5, 120}, {0, 80}, {A5, 120}, {0, 700}};
constexpr Tone FOCUS_DONE[] = {{E5, 180}, {G5, 180}, {C6, 420}};
constexpr Tone BREAK_DONE[] = {{C6, 180}, {G5, 180}, {E5, 180}, {C6, 420}};
constexpr Tone CLAUDE_ASK[] = {{E6, 90}, {0, 50}, {E6, 90}};
constexpr Tone CLAUDE_DONE[] = {{G5, 90}, {C6, 160}};
constexpr Tone CLAUDE_FAIL[] = {{E4, 220}, {0, 60}, {C4, 320}};
constexpr Tone NOTIFY_PHONE[] = {{B5, 70}, {0, 30}, {E6, 120}};
constexpr Tone NOTIFY_PC[] = {{A5, 110}};
constexpr Tone REMINDER[] = {{C5, 200}, {E5, 200}, {G5, 360}};
constexpr Tone ALERT[] = {{A5, 160}, {0, 80}, {F5, 160}, {0, 80}, {A5, 160}, {0, 80}, {F5, 300}};
constexpr Tone CHIME[] = {{G5, 140}, {E6, 260}};
constexpr Tone DOORBELL[] = {{E6, 380}, {C6, 600}};
constexpr Tone TEST[] = {{A4, 200}, {0, 60}, {A5, 200}, {0, 60}, {C6, 300}};

struct Entry {
  Sound sound;
  const Tone *tones;
  uint8_t count;
  bool repeat;
  SoundGroup group;
  const char *name;
};

#define SND(id, arr, rep, grp, nm) {Sound::id, arr, static_cast<uint8_t>(sizeof(arr) / sizeof(arr[0])), rep, SoundGroup::grp, nm}
constexpr Entry SOUNDS[] = {
    SND(Click, CLICK, false, Click, "click"),
    SND(Hold, HOLD, false, Click, "hold"),
    SND(TimerDone, TIMER_DONE, false, Timer, "timer"),
    SND(Alarm, ALARM, true, Timer, "alarm"),
    SND(FocusDone, FOCUS_DONE, false, Timer, "focus"),
    SND(BreakDone, BREAK_DONE, false, Timer, "break"),
    SND(ClaudeAsk, CLAUDE_ASK, false, Claude, "claude_ask"),
    SND(ClaudeDone, CLAUDE_DONE, false, Claude, "claude_done"),
    SND(ClaudeFail, CLAUDE_FAIL, false, Claude, "claude_fail"),
    SND(NotifyPhone, NOTIFY_PHONE, false, Notify, "phone"),
    SND(NotifyPc, NOTIFY_PC, false, Notify, "pc"),
    SND(Reminder, REMINDER, false, Reminder, "reminder"),
    SND(Alert, ALERT, false, Alert, "alert"),
    SND(Chime, CHIME, false, Alert, "chime"),
    SND(Doorbell, DOORBELL, false, Alert, "doorbell"),
    SND(Test, TEST, false, Alert, "test"),
};
#undef SND

const Entry *find(Sound s) {
  for (const Entry &e : SOUNDS)
    if (e.sound == s) return &e;
  return nullptr;
}
}  // namespace

const Tone *soundTones(Sound s, uint8_t &count, bool &repeat) {
  const Entry *e = find(s);
  count = e ? e->count : 0;
  repeat = e ? e->repeat : false;
  return e ? e->tones : nullptr;
}

SoundGroup soundGroup(Sound s) {
  const Entry *e = find(s);
  return e ? e->group : SoundGroup::Alert;
}

Sound soundFromName(const char *name) {
  if (!name) return Sound::None;
  for (const Entry &e : SOUNDS)
    if (strcmp(e.name, name) == 0) return e.sound;
  return Sound::None;
}

int soundToneAt(const Tone *tones, uint8_t count, bool repeat, uint32_t elapsed, uint32_t &round) {
  round = 0;
  if (!tones || count == 0) return -1;
  uint32_t total = 0;
  for (uint8_t i = 0; i < count; ++i) total += tones[i].ms;
  if (total == 0) return -1;
  if (elapsed >= total) {
    if (!repeat) return -1;
    round = elapsed / total;
    elapsed %= total;
  }
  for (uint8_t i = 0; i < count; ++i) {
    if (elapsed < tones[i].ms) return i;
    elapsed -= tones[i].ms;
  }
  return -1;
}

uint16_t soundDuty(uint8_t volume) {
  // Out of 1023. 40 was measured safe on a USB hub port; louder needs a stronger supply.
  static const uint16_t DUTY[SOUND_VOLUMES] = {0, 10, 22, 40};
  return DUTY[volume < SOUND_VOLUMES ? volume : SOUND_VOLUMES - 1];
}

bool soundAllowed(Sound s, uint8_t volume, uint8_t groupMask, bool quietNow) {
  if (s == Sound::None || volume == 0) return false;
  const SoundGroup group = soundGroup(s);
  if (!(groupMask & (1u << static_cast<uint8_t>(group)))) return false;
  if (quietNow && group != SoundGroup::Timer) return false;
  return true;
}

// ---------------- hardware ----------------
#ifndef PIO_UNIT_TESTING
#include <driver/i2s.h>

namespace {
constexpr int SPEAKER_PIN = 26;
constexpr uint8_t TONE_CHANNEL = 0;  // its LEDC timer is not shared with the LED or backlight
const Tone *playTones = nullptr;
uint8_t playCount = 0;
bool playRepeat = false;
uint8_t playVolume = 0;
uint32_t playStartMs = 0;
int playIndex = -1;
uint32_t playRound = 0;
bool toneAttached = false;
// speech
const uint8_t *pcmData = nullptr;
size_t pcmLen = 0, pcmPos = 0;
uint8_t pcmVolume = 0;
bool pcmOn = false;

void toneOff() {
  if (toneAttached) {
    ledcWrite(TONE_CHANNEL, 0);
    ledcDetachPin(SPEAKER_PIN);
    toneAttached = false;
  }
}

void toneOn(uint16_t hz, uint8_t volume) {
  if (hz == 0) {
    if (toneAttached) ledcWrite(TONE_CHANNEL, 0);
    return;
  }
  ledcSetup(TONE_CHANNEL, hz, 10);
  ledcAttachPin(SPEAKER_PIN, TONE_CHANNEL);
  toneAttached = true;
  ledcWrite(TONE_CHANNEL, soundDuty(volume));
}

void pcmStop() {
  if (!pcmOn) return;
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_0);
  pcmOn = false;
  pcmData = nullptr;
}
}  // namespace

void soundBegin() {
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);
}

void soundStop() {
  playTones = nullptr;
  playIndex = -1;
  toneOff();
  pcmStop();
}

void soundPlay(Sound s, uint8_t volume) {
  uint8_t count;
  bool repeat;
  const Tone *tones = soundTones(s, count, repeat);
  if (!tones || volume == 0) return;
  soundStop();
  playTones = tones;
  playCount = count;
  playRepeat = repeat;
  playVolume = volume;
  playStartMs = millis();
  playIndex = -2;  // forces the first tone out in soundLoop
  playRound = 0;
  soundLoop();
}

bool soundPlaying() { return playTones != nullptr || pcmOn; }

bool soundPlayPcm(const uint8_t *data, size_t len, uint16_t rate, uint8_t volume) {
  if (!data || len == 0 || volume == 0) return false;
  soundStop();
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  cfg.sample_rate = rate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_set_pin(I2S_NUM_0, nullptr);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);  // GPIO26 only (GPIO25 is not ours)
  pcmData = data;
  pcmLen = len;
  pcmPos = 0;
  pcmVolume = volume;
  pcmOn = true;
  soundLoop();
  return true;
}

void soundLoop() {
  if (pcmOn) {
    // Feed the DMA without blocking: one stereo frame per sample, the DAC takes the high byte.
    static const uint8_t GAIN[SOUND_VOLUMES] = {0, 64, 128, 200};  // /256
    uint16_t frames[128];
    while (pcmPos < pcmLen) {
      const size_t n = min<size_t>(64, pcmLen - pcmPos);
      for (size_t i = 0; i < n; ++i) {
        const int centred = static_cast<int>(pcmData[pcmPos + i]) - 128;
        const int v = 128 + centred * GAIN[pcmVolume] / 256;
        const uint16_t sample = static_cast<uint16_t>(constrain(v, 0, 255)) << 8;
        frames[i * 2] = sample;
        frames[i * 2 + 1] = sample;
      }
      size_t written = 0;
      i2s_write(I2S_NUM_0, frames, n * 4, &written, 0);
      if (written < n * 4) {
        pcmPos += written / 4;
        return;  // DMA full: continue next loop
      }
      pcmPos += n;
    }
    static uint32_t drainedAt = 0;
    if (drainedAt == 0) drainedAt = millis();
    if (millis() - drainedAt > 250) {  // let the last DMA buffers play out
      drainedAt = 0;
      pcmStop();
    }
    return;
  }
  if (!playTones) return;
  uint32_t round = 0;
  const int index = soundToneAt(playTones, playCount, playRepeat, millis() - playStartMs, round);
  if (index < 0) {
    soundStop();
    return;
  }
  if (index != playIndex || round != playRound) {
    // An alarm grows louder each time it starts over, up to the top volume.
    const uint8_t volume = playRepeat ? min<uint8_t>(SOUND_VOLUMES - 1, playVolume + round / 3) : playVolume;
    toneOn(playTones[index].hz, volume);
    playIndex = index;
    playRound = round;
  }
}
#endif
