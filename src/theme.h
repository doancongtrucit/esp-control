#pragma once

#include <stdint.h>

// Dark launcher palette (UI v3) as RGB565. Accent fills carry dark text
// (T_ON_ACCENT); body text is T_TEXT or T_MUTED.
constexpr uint16_t T_BG = 0x0862;         // #0d0f14
constexpr uint16_t T_CARD = 0x1905;       // #1c2029
constexpr uint16_t T_CARD_ALT = 0x10A3;   // #14171e
constexpr uint16_t T_LINE = 0x2146;       // #232836, header line and home button
constexpr uint16_t T_DIVIDER = 0x29A8;    // #2e3442
constexpr uint16_t T_TEXT = 0xF7BF;       // #f2f4fb
constexpr uint16_t T_MUTED = 0xA557;      // #a3abbd
constexpr uint16_t T_UP = 0x5E93;         // #5fd39a
constexpr uint16_t T_DOWN = 0xFBCD;       // #ff7a6b
constexpr uint16_t T_ACCENT = 0xFC47;     // #ff8a3d
constexpr uint16_t T_LINK = 0x0C3F;       // #0a84ff
constexpr uint16_t T_ALERT_BG = 0x3922;   // #3a2414
constexpr uint16_t T_BADGE = 0xD964;      // #d92d20
constexpr uint16_t T_GOLD = 0xFEA1;       // #ffd60a
constexpr uint16_t T_ON_ACCENT = T_BG;

// Temporary aliases for UI v2 code until each screen is redrawn (Tasks 3-7).
constexpr uint16_t T_SURFACE = T_BG;
constexpr uint16_t T_INK = T_TEXT;
constexpr uint16_t T_HEADER_BTN = T_LINE;
constexpr uint16_t T_HEADER_SUB = T_MUTED;
constexpr uint16_t T_TILE = T_CARD;
constexpr uint16_t T_ZEBRA = T_CARD_ALT;
constexpr uint16_t T_BORDER = T_DIVIDER;
constexpr uint16_t T_ACCENT_2 = 0x7D5D;   // #7fa9ee
