#include "gesture.h"

Gesture classifyGesture(int dx, int dy, uint32_t heldMs, bool released) {
  const int ax = abs(dx);
  const int ay = abs(dy);
  if (ax >= GESTURE_SWIPE_MIN && ax > 2 * ay) return dx < 0 ? Gesture::SwipeLeft : Gesture::SwipeRight;
  const bool still = ax <= GESTURE_TAP_SLOP && ay <= GESTURE_TAP_SLOP;
  if (released) return still ? Gesture::Tap : Gesture::None;
  if (still && heldMs >= GESTURE_HOLD_MS) return Gesture::Tap;
  if (heldMs >= GESTURE_MAX_MS) return Gesture::None;
  return Gesture::Pending;
}
