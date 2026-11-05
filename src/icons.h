#pragma once

#include <stdint.h>

class TFT_eSPI;

// Stroke icons drawn from TFT_eSPI primitives on a 24-unit grid.
enum class Icon : uint8_t {
  Monitor, Controls, Gold, Sparkle, Clipboard, Chip, Bell, Gear, Plus,
  Activity, Download, Lock, Screenshot, Globe, Sliders, Gem, Chat, Users,
  Mail, Bubble, Gamepad, Headset, Bars, VolDown, Mute, VolUp, Phone, Wifi,
  Home, ChevronLeft, ChevronRight, Bluetooth, Pie,
  Coin, Trend, Cloud, Calendar, Sun, Rain, Storm, Fog
};

// Name used in host/config.json ("lock", "volup", ...), case-insensitive.
// Unknown names and nullptr map to Icon::Controls.
Icon iconFromName(const char *name);

// Draws icon in a size x size box at (x, y); bg is used for anti-aliasing.
void drawIcon(TFT_eSPI &tft, Icon icon, int x, int y, int size, uint16_t fg, uint16_t bg);
