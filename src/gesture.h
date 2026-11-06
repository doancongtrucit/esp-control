#pragma once

#include <Arduino.h>

// Tap vs. horizontal swipe from one touch: movement (dx, dy in screen px) since
// the finger went down, time held, and whether it has been lifted.
enum class Gesture : uint8_t { Pending, Tap, SwipeLeft, SwipeRight, None };

constexpr int GESTURE_SWIPE_MIN = 50;   // px of horizontal travel
constexpr int GESTURE_TAP_SLOP = 18;    // px a tap may wander
constexpr uint32_t GESTURE_HOLD_MS = 220;  // still this long: a tap (hold buttons start here)
constexpr uint32_t GESTURE_MAX_MS = 700;   // undecided this long: nothing

Gesture classifyGesture(int dx, int dy, uint32_t heldMs, bool released);
