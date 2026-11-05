#include "color_util.h"

#include <TFT_eSPI.h>
#include <math.h>

uint16_t rgb565(uint32_t rgb) {
  return ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
}

uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  const int ar = a >> 11, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = b >> 11, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (br - ar) * t / 255;
  const int g = ag + (bg - ag) * t / 255;
  const int bl = ab + (bb - ab) * t / 255;
  return (r << 11) | (g << 5) | bl;
}

int roundedRowInset(int row, int height, int radius) {
  float dy;
  if (row < radius) {
    dy = radius - row - 0.5f;
  } else if (row >= height - radius) {
    dy = row - (height - radius) + 0.5f;
  } else {
    return 0;
  }
  const float inset = radius - sqrtf(radius * radius - dy * dy);
  const int rounded = static_cast<int>(inset + 0.5f);
  return rounded < 0 ? 0 : (rounded > radius ? radius : rounded);
}

void fillGradientRoundRect(TFT_eSPI &tft, int x, int y, int w, int h, int r, uint16_t top, uint16_t bottom) {
  for (int row = 0; row < h; ++row) {
    const uint8_t t = h > 1 ? row * 255 / (h - 1) : 0;
    const int inset = roundedRowInset(row, h, r);
    tft.drawFastHLine(x + inset, y + row, w - 2 * inset, blend565(top, bottom, t));
  }
}
