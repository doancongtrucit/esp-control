#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Antialiased Noto Sans for the whole UI (arrays in src/fonts, built by
// tools/make_smooth_fonts.py). Old TFT_eSPI font numbers map onto them:
// Tiny 10 px is chosen explicitly; 1 -> Small 12 px, 2 -> Body 15 px, 4 -> Large 22 px bold, 6/7/8 -> Huge 52 px digits.
enum class UiFont : uint8_t { None, Small, Body, BodyBold, Large, Huge, Tiny };  // Tiny: 10 px

void uiFontBegin(TFT_eSPI &tft);
UiFont uiFontForSize(uint8_t legacySize);
void uiUseFont(UiFont font);  // loads it unless it is already the current font
UiFont uiCurrentFont();

// drawString / textWidth in the Noto font for a legacy size (datum and colours as set on tft).
int16_t uiText(const String &text, int32_t x, int32_t y, uint8_t legacySize);
int16_t uiTextWidth(const String &text, uint8_t legacySize);
int16_t uiTextIn(UiFont font, const String &text, int32_t x, int32_t y);
int16_t uiTextWidthIn(UiFont font, const String &text);
