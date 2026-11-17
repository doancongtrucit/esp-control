#pragma once

#include <Arduino.h>

// iPhone notifications over BLE (Apple Notification Center Service).
// The device advertises an ANCS solicitation, bonds with the iPhone and reads
// AppIdentifier, Title and Message of each new notification.
using AncsHandler = void (*)(const String &app, const String &title, const String &message);

void ancsBegin(const char *deviceName, AncsHandler handler);
// Start BLE advertising. Call AFTER ancsBegin() and after every *BleAttach() so
// all local GATT services (ANCS, HUD fff0, GOFA 0x2A60) are registered before
// the NimBLE server starts — services added after the first advertise never
// appear in the live GATT, so the connected app finds nothing to write to.
void ancsStartAdvertising();
void ancsLoop();       // call from loop(): discovery, attribute requests, parsing
bool ancsConnected();  // iPhone connected and encrypted
bool ancsReady();
// Signal strength of the connected iPhone in dBm; false when not connected.
bool ancsRssi(int &rssi);
void ancsForgetPairing();  // deletes every bond; the phone must pair again      // subscribed to ANCS, notifications flow

// Advertise an extra 16-bit service UUID beside the ANCS solicitation, so a
// second app's scan can find this device (the HUD uses fff0). Call after ancsBegin().
void ancsAdvertiseService16(uint16_t uuid16);

// Enter/leave a bare HUD advertising mode: advertise only the HUD service (no
// ANCS solicitation) under a recognisable name, dropping the current link, so
// VIETMAP LIVE can discover and connect. Leaving restores ANCS.
void ancsSetHudMode(bool on);

// GOFA HUD advertising mode: advertise a 128-bit identifier UUID (last 12 hex =
// this device's MAC, registered at hud.platform.gofa.vn) plus name HUD_xxxx, no
// ANCS. Entering drops the current link. For GOFA app discovery.
void ancsSetGofaMode(bool on);
const char *ancsGofaName();  // "HUD_xxxx" (after ancsBegin)
const char *ancsGofaUuid();  // full 128-bit identifier UUID string
