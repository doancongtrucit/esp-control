#pragma once

#include <stdint.h>

class TFT_eSPI;

// 0xRRGGBB -> RGB565.
uint16_t rgb565(uint32_t rgb);

// Linear blend of two RGB565 colors; t = 0 gives a, 255 gives b.
uint16_t blend565(uint16_t a, uint16_t b, uint8_t t);

// Pixels to leave out at each end of `row` in a rounded rectangle of `height`
// with corner `radius` (0 on the straight middle rows).
int roundedRowInset(int row, int height, int radius);

// Rounded rectangle filled with a vertical gradient from `top` to `bottom`.
void fillGradientRoundRect(TFT_eSPI &tft, int x, int y, int w, int h, int r, uint16_t top, uint16_t bottom);
