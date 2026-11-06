// Only the CYD UI v2 build carries the Noto arrays and TFT_eSPI smooth fonts.
#ifdef DECK_UI_V2
#include "ui_font.h"

#include "fonts/noto_body.h"
#include "fonts/noto_body_bold.h"
#include "fonts/noto_huge.h"
#include "fonts/noto_large.h"
#include "fonts/noto_small.h"
#include "fonts/noto_tiny.h"

namespace {
TFT_eSPI *display = nullptr;
UiFont current = UiFont::None;

const uint8_t *arrayFor(UiFont font) {
  switch (font) {
    case UiFont::Small: return NOTO_SMALL;
    case UiFont::Body: return NOTO_BODY;
    case UiFont::BodyBold: return NOTO_BODY_BOLD;
    case UiFont::Large: return NOTO_LARGE;
    case UiFont::Huge: return NOTO_HUGE;
    case UiFont::Tiny: return NOTO_TINY;
    default: return nullptr;
  }
}
}  // namespace

void uiFontBegin(TFT_eSPI &tft) {
  display = &tft;
  current = UiFont::None;
}

UiFont uiFontForSize(uint8_t legacySize) {
  switch (legacySize) {
    case 1: return UiFont::Small;
    case 4: return UiFont::Large;
    case 6:
    case 7:
    case 8: return UiFont::Huge;
    default: return UiFont::Body;
  }
}

void uiUseFont(UiFont font) {
  const uint8_t *array = arrayFor(font);
  if (!display || font == current || !array) return;
  display->loadFont(array);  // frees the previous font's metrics first
  current = font;
}

UiFont uiCurrentFont() { return current; }

int16_t uiTextIn(UiFont font, const String &text, int32_t x, int32_t y) {
  uiUseFont(font);
  return display ? display->drawString(text, x, y) : 0;
}

int16_t uiTextWidthIn(UiFont font, const String &text) {
  uiUseFont(font);
  return display ? display->textWidth(text) : 0;
}

int16_t uiText(const String &text, int32_t x, int32_t y, uint8_t legacySize) {
  return uiTextIn(uiFontForSize(legacySize), text, x, y);
}

int16_t uiTextWidth(const String &text, uint8_t legacySize) {
  return uiTextWidthIn(uiFontForSize(legacySize), text);
}
#endif  // DECK_UI_V2
