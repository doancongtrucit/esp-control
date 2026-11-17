#pragma once
#ifdef DECK_UI_V2

#include <Arduino.h>

#include "hud_protocol.h"  // reuse hud::NavState + TurnIcon mapping

// GOFA HUD platform (hud.platform.gofa.vn/docs). The GOFA app is BLE central:
// it scans for a HUD advertising a 128-bit identifier UUID whose last 12 hex are
// the HUD's (registered) BLE MAC, plus a non-empty name; then it connects and
// writes protobuf messages to the characteristics of service 0x2A60.
//
// Stage 1: register the GATT service + characteristics and log every write as
// hex on Serial, and build the GOFA advertisement. This proves discovery and
// captures real protobuf so the decoder can be written against real bytes.
void gofaBleAttach();
void gofaBleLoop();

const hud::NavState &gofaNav();
bool gofaHasData();
uint32_t gofaLastFrameMs();

// Demo fill to preview the HUD screen without the app.
void gofaSetDemo(bool on);
bool gofaDemo();

#endif  // DECK_UI_V2
