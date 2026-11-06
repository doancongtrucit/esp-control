// Only the CYD UI v2 build has the Noto fonts (src/ui_font.h).
#ifdef DECK_UI_V2
#include "text_vi.h"

#include "ui_font.h"

namespace {
// ASCII printable plus the Latin blocks Vietnamese letters live in.
bool isAllowed(uint32_t cp) {
  if (cp >= 0x20 && cp <= 0x7E) return true;
  // Exactly the glyphs in the Noto arrays (tools/make_smooth_fonts.py), so none is ever missing.
  if (cp >= 0xC0 && cp <= 0x17F && cp != 0xD7 && cp != 0xF7) return true;
  if (cp == 0x1A0 || cp == 0x1A1 || cp == 0x1AF || cp == 0x1B0) return true;  // O/U with horn
  return cp >= 0x1EA0 && cp <= 0x1EF9;
}

bool isSpace(uint32_t cp) {
  return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0xA0;
}

void appendUtf8(String &out, uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

// Decodes the code point at s[i] and advances i. Returns false for invalid
// UTF-8 (the bad lead byte, or the whole overlong sequence, is skipped).
bool nextCodePoint(const String &s, size_t &i, uint32_t &cp) {
  const uint8_t lead = static_cast<uint8_t>(s[i]);
  size_t len;
  uint32_t minValue;
  if (lead < 0x80) {
    cp = lead;
    i += 1;
    return true;
  } else if ((lead & 0xE0) == 0xC0) {
    len = 2;
    cp = lead & 0x1F;
    minValue = 0x80;
  } else if ((lead & 0xF0) == 0xE0) {
    len = 3;
    cp = lead & 0x0F;
    minValue = 0x800;
  } else if ((lead & 0xF8) == 0xF0) {
    len = 4;
    cp = lead & 0x07;
    minValue = 0x10000;
  } else {
    i += 1;
    return false;
  }
  if (i + len > s.length()) {
    i += 1;
    return false;
  }
  for (size_t k = 1; k < len; ++k) {
    const uint8_t b = static_cast<uint8_t>(s[i + k]);
    if ((b & 0xC0) != 0x80) {
      i += 1;
      return false;
    }
    cp = (cp << 6) | (b & 0x3F);
  }
  i += len;
  return cp >= minValue && cp <= 0x10FFFF;
}
}  // namespace

size_t utf8Length(const String &utf8) {
  size_t count = 0;
  for (size_t i = 0; i < utf8.length(); ++i) {
    if ((static_cast<uint8_t>(utf8[i]) & 0xC0) != 0x80) ++count;
  }
  return count;
}

String sanitizeVi(const String &utf8, size_t maxChars) {
  String clean;
  size_t count = 0;
  bool pendingSpace = false;
  size_t i = 0;
  while (i < utf8.length()) {
    uint32_t cp = 0;
    if (!nextCodePoint(utf8, i, cp)) continue;
    if (isSpace(cp)) {
      pendingSpace = count > 0;
      continue;
    }
    if (!isAllowed(cp)) continue;
    if (pendingSpace) {
      clean += ' ';
      ++count;
      pendingSpace = false;
    }
    appendUtf8(clean, cp);
    ++count;
  }
  if (count <= maxChars) return clean;

  const size_t keep = maxChars > 3 ? maxChars - 3 : maxChars;
  String cut;
  size_t kept = 0;
  i = 0;
  while (i < clean.length() && kept < keep) {
    uint32_t cp = 0;
    nextCodePoint(clean, i, cp);
    appendUtf8(cut, cp);
    ++kept;
  }
  cut.trim();
  if (maxChars > 3) cut += "...";
  return cut;
}

int16_t viTextWidth(const String &utf8, bool bold) {
  return uiTextWidthIn(bold ? UiFont::BodyBold : UiFont::Body, utf8);
}

String fitViText(const String &utf8, int maxWidth, bool bold) {
  if (viTextWidth(utf8, bold) <= maxWidth) return utf8;
  // Drop whole code points from the end until text + "..." fits.
  String cut = utf8;
  while (cut.length() > 0) {
    int end = cut.length() - 1;
    while (end > 0 && (static_cast<uint8_t>(cut[end]) & 0xC0) == 0x80) --end;
    cut.remove(end);
    cut.trim();
    if (viTextWidth(cut + "...", bold) <= maxWidth) break;
  }
  return cut + "...";
}

void drawViText(TFT_eSPI &tft, int x, int y, const String &utf8, uint16_t fg, uint16_t bg, bool bold) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);
  uiTextIn(bold ? UiFont::BodyBold : UiFont::Body, utf8, x, y);
}
#endif  // DECK_UI_V2
