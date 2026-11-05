#pragma once

#include <Arduino.h>

enum class NotifySource : uint8_t { Phone, Pc };

// Text fields are already passed through sanitizeVi() and kept short enough
// for one row on the 320 px screen.
struct Notification {
  NotifySource source = NotifySource::Pc;
  String app;
  String from;
  String body;
  uint32_t receivedMs = 0;
  bool unread = false;
};

// Fixed ring buffer of the newest notifications; at(0) is the newest.
class NotifyStore {
 public:
  static constexpr size_t CAPACITY = 10;
  static constexpr size_t APP_CHARS = 12;
  static constexpr size_t FROM_CHARS = 24;
  static constexpr size_t BODY_CHARS = 48;

  void add(NotifySource source, const String &app, const String &from, const String &body,
           uint32_t nowMs);
  size_t size() const { return count_; }
  const Notification &at(size_t i) const;
  size_t unreadCount() const;
  void markAllRead();
  // Bumps on every change, so screens can redraw only when needed.
  uint32_t version() const { return version_; }

 private:
  Notification items_[CAPACITY];
  size_t head_ = 0;  // slot the next add() writes
  size_t count_ = 0;
  uint32_t version_ = 0;
};
