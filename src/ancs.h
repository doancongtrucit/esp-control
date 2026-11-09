#pragma once

#include <Arduino.h>

// iPhone notifications over BLE (Apple Notification Center Service).
// The device advertises an ANCS solicitation, bonds with the iPhone and reads
// AppIdentifier, Title and Message of each new notification.
using AncsHandler = void (*)(const String &app, const String &title, const String &message);

void ancsBegin(const char *deviceName, AncsHandler handler);
void ancsLoop();       // call from loop(): discovery, attribute requests, parsing
bool ancsConnected();  // iPhone connected and encrypted
bool ancsReady();
// Signal strength of the connected iPhone in dBm; false when not connected.
bool ancsRssi(int &rssi);
void ancsForgetPairing();  // deletes every bond; the phone must pair again      // subscribed to ANCS, notifications flow
