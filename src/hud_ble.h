#pragma once
#ifdef DECK_UI_V2

#include <Arduino.h>

#include "hud_protocol.h"

// VIETMAP HUD peripheral, added onto the deck's existing NimBLE server (the one
// ancsBegin() creates). The VIETMAP LIVE app is the BLE central: it scans for
// service fff0, connects and writes navigation frames to fff2; we parse them
// into a NavState the HUD screen draws. Call hudBleAttach() right after
// ancsBegin(), then hudBleLoop() from loop().
void hudBleAttach();
void hudBleLoop();

// For the HUD screen.
const hud::NavState &hudNav();
bool hudHasData();                 // at least one frame has arrived
uint32_t hudLastFrameMs();         // millis() of the last frame, 0 if none

// Fill the nav state with sample values to preview the HUD without the app
// (serial/UDP {"type":"hud-demo","on":true}). Off restores the live state.
void hudSetDemo(bool on);
bool hudDemo();

#endif  // DECK_UI_V2
