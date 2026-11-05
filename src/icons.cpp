#include "icons.h"

#include <TFT_eSPI.h>
#include <strings.h>

namespace {
struct NamedIcon {
  const char *name;
  Icon icon;
};

constexpr NamedIcon ICON_NAMES[] = {
    {"monitor", Icon::Monitor},   {"controls", Icon::Controls},     {"gold", Icon::Gold},
    {"sparkle", Icon::Sparkle},   {"clipboard", Icon::Clipboard},   {"chip", Icon::Chip},
    {"bell", Icon::Bell},         {"gear", Icon::Gear},             {"plus", Icon::Plus},
    {"activity", Icon::Activity}, {"download", Icon::Download},     {"lock", Icon::Lock},
    {"screenshot", Icon::Screenshot}, {"globe", Icon::Globe},       {"sliders", Icon::Sliders},
    {"gem", Icon::Gem},           {"chat", Icon::Chat},             {"users", Icon::Users},
    {"mail", Icon::Mail},         {"bubble", Icon::Bubble},         {"gamepad", Icon::Gamepad},
    {"headset", Icon::Headset},   {"bars", Icon::Bars},             {"voldown", Icon::VolDown},
    {"mute", Icon::Mute},         {"volup", Icon::VolUp},           {"phone", Icon::Phone},
    {"wifi", Icon::Wifi},         {"home", Icon::Home},             {"bluetooth", Icon::Bluetooth},
    {"pie", Icon::Pie},           {"coin", Icon::Coin},             {"trend", Icon::Trend},
    {"cloud", Icon::Cloud},       {"calendar", Icon::Calendar},     {"sun", Icon::Sun},
    {"rain", Icon::Rain},         {"storm", Icon::Storm},           {"fog", Icon::Fog},
};

// Maps the 24-unit design grid onto the requested box.
class Pen {
 public:
  Pen(TFT_eSPI &tft, int x, int y, int size, uint16_t fg, uint16_t bg)
      : tft_(tft), x_(x), y_(y), size_(size), fg_(fg), bg_(bg) {}

  float px(float v) const { return x_ + v * size_ / 24.0f; }
  float py(float v) const { return y_ + v * size_ / 24.0f; }
  int r(float v) const { return static_cast<int>(v * size_ / 24.0f + 0.5f); }

  void line(float x1, float y1, float x2, float y2) {
    tft_.drawWideLine(px(x1), py(y1), px(x2), py(y2), stroke(), fg_, bg_);
  }
  void poly(const float *pts, int n, bool closed) {
    for (int i = 0; i + 1 < n; ++i) line(pts[2 * i], pts[2 * i + 1], pts[2 * i + 2], pts[2 * i + 3]);
    if (closed) line(pts[2 * n - 2], pts[2 * n - 1], pts[0], pts[1]);
  }
  void rect(float x, float y, float w, float h) {
    const float pts[] = {x, y, x + w, y, x + w, y + h, x, y + h};
    poly(pts, 4, true);
  }
  void roundRect(float x, float y, float w, float h, float rad) {
    for (int k = 0; k < 2; ++k) {
      tft_.drawRoundRect(px(x) + k, py(y) + k, r(w) - 2 * k, r(h) - 2 * k, r(rad), fg_);
    }
  }
  void circle(float cx, float cy, float rad) {
    for (int k = 0; k < 2; ++k) tft_.drawCircle(px(cx), py(cy), r(rad) - k, fg_);
  }
  // Quarter arcs: 0x1 top-left, 0x2 top-right, 0x4 bottom-right, 0x8 bottom-left.
  void arc(float cx, float cy, float rad, uint8_t corners) {
    for (int k = 0; k < 2; ++k) tft_.drawCircleHelper(px(cx), py(cy), r(rad) - k, corners, fg_);
  }
  void ellipse(float cx, float cy, float rx, float ry) {
    tft_.drawEllipse(px(cx), py(cy), r(rx), r(ry), fg_);
  }
  void dot(float cx, float cy, float rad) { tft_.fillCircle(px(cx), py(cy), r(rad), fg_); }

 private:
  float stroke() const { return size_ >= 16 ? 2.0f : 1.5f; }

  TFT_eSPI &tft_;
  int x_;
  int y_;
  int size_;
  uint16_t fg_;
  uint16_t bg_;
};

void speaker(Pen &p) {
  const float pts[] = {4, 9, 8, 9, 13, 5, 13, 19, 8, 15, 4, 15};
  p.poly(pts, 6, true);
}

void cloudShape(Pen &p) {
  p.arc(9, 14, 4, 0x1 | 0x8);
  p.arc(14, 11, 5.5f, 0x1 | 0x2);
  p.arc(18, 15, 3.5f, 0x2 | 0x4);
  p.line(9, 18, 18, 18.5f);
}
}  // namespace

Icon iconFromName(const char *name) {
  if (name == nullptr) return Icon::Controls;
  for (const NamedIcon &entry : ICON_NAMES) {
    if (strcasecmp(entry.name, name) == 0) return entry.icon;
  }
  return Icon::Controls;
}

void drawIcon(TFT_eSPI &tft, Icon icon, int x, int y, int size, uint16_t fg, uint16_t bg) {
  Pen p(tft, x, y, size, fg, bg);
  switch (icon) {
    case Icon::Monitor: {
      const float axes[] = {3, 3, 3, 21, 21, 21};
      const float trend[] = {7, 15, 11, 10, 14, 13, 19, 6};
      p.poly(axes, 3, false);
      p.poly(trend, 4, false);
      break;
    }
    case Icon::Controls:
      p.rect(3, 3, 7, 7);
      p.rect(14, 3, 7, 7);
      p.rect(3, 14, 7, 7);
      p.rect(14, 14, 7, 7);
      break;
    case Icon::Gold:
      p.ellipse(12, 6, 7, 3);
      p.ellipse(12, 12, 7, 3);
      p.ellipse(12, 18, 7, 3);
      p.line(5, 6, 5, 18);
      p.line(19, 6, 19, 18);
      break;
    case Icon::Sparkle: {
      const float star[] = {12, 3, 14.2f, 9.8f, 21, 12, 14.2f, 14.2f, 12, 21, 9.8f, 14.2f, 3, 12, 9.8f, 9.8f};
      p.poly(star, 8, true);
      break;
    }
    case Icon::Clipboard:
      p.rect(5, 4, 14, 17);
      p.rect(9, 2, 6, 3);
      p.line(9, 11, 15, 11);
      p.line(9, 15, 13, 15);
      break;
    case Icon::Chip:
      p.rect(6, 6, 12, 12);
      p.line(9, 2, 9, 6);
      p.line(15, 2, 15, 6);
      p.line(9, 18, 9, 22);
      p.line(15, 18, 15, 22);
      p.line(2, 9, 6, 9);
      p.line(2, 15, 6, 15);
      p.line(18, 9, 22, 9);
      p.line(18, 15, 22, 15);
      break;
    case Icon::Bell:
      p.arc(12, 11, 6, 0x3);
      p.line(6, 11, 6, 16);
      p.line(18, 11, 18, 16);
      p.line(6, 16, 4, 18);
      p.line(18, 16, 20, 18);
      p.line(4, 18, 20, 18);
      p.line(10, 21, 14, 21);
      break;
    case Icon::Gear:
      p.circle(12, 12, 3.5f);
      p.line(12, 2, 12, 5);
      p.line(12, 19, 12, 22);
      p.line(2, 12, 5, 12);
      p.line(19, 12, 22, 12);
      p.line(4.9f, 4.9f, 7, 7);
      p.line(17, 17, 19.1f, 19.1f);
      p.line(4.9f, 19.1f, 7, 17);
      p.line(17, 7, 19.1f, 4.9f);
      break;
    case Icon::Plus:
      p.line(12, 5, 12, 19);
      p.line(5, 12, 19, 12);
      break;
    case Icon::Activity: {
      const float pulse[] = {3, 12, 7, 12, 10, 4, 14, 20, 17, 12, 21, 12};
      p.poly(pulse, 6, false);
      break;
    }
    case Icon::Download:
      p.line(12, 3, 12, 15);
      p.line(7, 10, 12, 15);
      p.line(17, 10, 12, 15);
      p.line(4, 20, 20, 20);
      break;
    case Icon::Lock:
      p.rect(6, 11, 12, 10);
      p.arc(12, 7.5f, 4, 0x3);
      p.line(8, 7.5f, 8, 11);
      p.line(16, 7.5f, 16, 11);
      break;
    case Icon::Screenshot:
      p.line(4, 8, 4, 4);
      p.line(4, 4, 8, 4);
      p.line(16, 4, 20, 4);
      p.line(20, 4, 20, 8);
      p.line(20, 16, 20, 20);
      p.line(20, 20, 16, 20);
      p.line(8, 20, 4, 20);
      p.line(4, 20, 4, 16);
      p.line(9, 12, 15, 12);
      p.line(12, 9, 12, 15);
      break;
    case Icon::Globe:
      p.circle(12, 12, 9);
      p.line(3, 12, 21, 12);
      p.ellipse(12, 12, 4, 9);
      break;
    case Icon::Sliders:
      p.line(4, 6, 20, 6);
      p.line(4, 12, 20, 12);
      p.line(4, 18, 20, 18);
      p.dot(15, 6, 2.2f);
      p.dot(9, 12, 2.2f);
      p.dot(17, 18, 2.2f);
      break;
    case Icon::Gem: {
      const float star[] = {12, 2, 14, 10, 22, 12, 14, 14, 12, 22, 10, 14, 2, 12, 10, 10};
      p.poly(star, 8, true);
      break;
    }
    case Icon::Chat: {
      const float bubble[] = {4, 5, 20, 5, 20, 16, 9, 16, 4, 20};
      p.poly(bubble, 5, true);
      break;
    }
    case Icon::Users:
      p.circle(9, 8, 3);
      p.arc(9, 20, 6, 0x3);
      p.circle(16.5f, 8.5f, 2.5f);
      p.arc(17, 20, 4.5f, 0x2);
      break;
    case Icon::Mail:
      p.rect(3, 6, 18, 12);
      p.line(3, 7, 12, 13);
      p.line(12, 13, 21, 7);
      break;
    case Icon::Bubble:
      p.circle(12, 11.5f, 8);
      p.line(16.5f, 18, 20, 21);
      p.line(20, 21, 19.5f, 16.5f);
      break;
    case Icon::Gamepad:
      p.roundRect(2.5f, 8, 19, 10, 5);
      p.line(8, 11, 8, 15);
      p.line(6, 13, 10, 13);
      p.dot(16, 12, 1.2f);
      p.dot(18, 14.5f, 1.2f);
      break;
    case Icon::Headset:
      p.arc(12, 13, 8, 0x3);
      p.line(4, 13, 4, 15);
      p.line(20, 13, 20, 15);
      p.rect(4, 15, 3, 5);
      p.rect(17, 15, 3, 5);
      break;
    case Icon::Bars:
      p.line(5, 20, 5, 11);
      p.line(11, 20, 11, 4);
      p.line(17, 20, 17, 13);
      p.line(2, 20.5f, 22, 20.5f);
      break;
    case Icon::VolDown:
      speaker(p);
      p.line(16, 12, 21, 12);
      break;
    case Icon::Mute:
      speaker(p);
      p.line(16, 9, 21, 15);
      p.line(21, 9, 16, 15);
      break;
    case Icon::VolUp:
      speaker(p);
      p.line(16, 12, 21, 12);
      p.line(18.5f, 9.5f, 18.5f, 14.5f);
      break;
    case Icon::Phone:
      p.roundRect(7, 2, 10, 20, 2);
      p.line(11, 18, 13, 18);
      break;
    case Icon::Wifi:
      p.arc(12, 20, 15, 0x3);
      p.arc(12, 20, 10, 0x3);
      p.arc(12, 20, 5, 0x3);
      p.dot(12, 19.5f, 1.4f);
      break;
    case Icon::Home: {
      const float roof[] = {3, 11, 12, 4, 21, 11};
      const float walls[] = {5, 10, 5, 20, 19, 20, 19, 10};
      p.poly(roof, 3, false);
      p.poly(walls, 4, false);
      break;
    }
    case Icon::ChevronLeft: {
      const float chevron[] = {15, 5, 8, 12, 15, 19};
      p.poly(chevron, 3, false);
      break;
    }
    case Icon::ChevronRight: {
      const float chevron[] = {9, 5, 16, 12, 9, 19};
      p.poly(chevron, 3, false);
      break;
    }
    case Icon::Bluetooth: {
      const float rune[] = {7, 7, 17, 17, 12, 22, 12, 2, 17, 7, 7, 17};
      p.poly(rune, 6, false);
      break;
    }
    case Icon::Pie:
      p.circle(12, 12, 9);
      p.line(12, 12, 12, 3);
      p.line(12, 12, 18.4f, 18.4f);
      break;
    case Icon::Coin:
      p.circle(12, 12, 9);
      p.line(10, 7, 10, 17);
      p.line(10, 8, 13.5f, 8);
      p.line(10, 12, 14, 12);
      p.line(10, 16, 14, 16);
      p.arc(13.5f, 10, 2, 0x2 | 0x4);
      p.arc(14, 14, 2, 0x2 | 0x4);
      break;
    case Icon::Trend: {
      const float trend[] = {3, 17, 9, 11, 13, 15, 21, 7};
      const float head[] = {15, 7, 21, 7, 21, 13};
      p.poly(trend, 4, false);
      p.poly(head, 3, false);
      break;
    }
    case Icon::Cloud:
      cloudShape(p);
      break;
    case Icon::Calendar:
      p.rect(4, 6, 16, 15);
      p.line(4, 10, 20, 10);
      p.line(8, 3, 8, 7);
      p.line(16, 3, 16, 7);
      break;
    case Icon::Sun:
      p.circle(12, 12, 4);
      p.line(12, 2, 12, 5);
      p.line(12, 19, 12, 22);
      p.line(2, 12, 5, 12);
      p.line(19, 12, 22, 12);
      p.line(5, 5, 7, 7);
      p.line(17, 17, 19, 19);
      p.line(5, 19, 7, 17);
      p.line(17, 7, 19, 5);
      break;
    case Icon::Rain:
      cloudShape(p);
      p.line(9, 21, 10, 19.5f);
      p.line(13, 21, 14, 19.5f);
      p.line(17, 21, 18, 19.5f);
      break;
    case Icon::Storm: {
      cloudShape(p);
      const float bolt[] = {13, 18, 11, 21, 14, 21, 12, 24};
      p.poly(bolt, 4, false);
      break;
    }
    case Icon::Fog:
      p.line(4, 9, 20, 9);
      p.line(3, 13, 21, 13);
      p.line(5, 17, 19, 17);
      break;
  }
}
