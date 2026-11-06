#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Notification text keeps Vietnamese diacritics. sanitizeVi() keeps ASCII and
// the Latin letters Vietnamese uses, drops everything else (emoji, symbols,
// invalid UTF-8), collapses whitespace and cuts to maxChars code points,
// ending with "..." when cut.
String sanitizeVi(const String &utf8, size_t maxChars);

// Number of code points in a UTF-8 string.
size_t utf8Length(const String &utf8);

// Width in pixels of the text in the Noto body font (bold for titles).
int16_t viTextWidth(const String &utf8, bool bold = false);

// Cuts text on a code-point boundary so it fits maxWidth pixels, ending with
// "..." when cut.
String fitViText(const String &utf8, int maxWidth, bool bold = false);

// Draws UTF-8 text in the 15 px Noto font; (x, y) is the top-left corner.
void drawViText(TFT_eSPI &tft, int x, int y, const String &utf8, uint16_t fg, uint16_t bg,
                bool bold = false);
