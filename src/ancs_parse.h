#pragma once

#include <Arduino.h>

// Short display name for an iOS bundle id ("com.vng.zalo" -> "Zalo"); unknown
// ids use their last segment.
String ancsAppName(const String &bundleId);

// Back-off for retrying a failed step (e.g. ANCS discovery while iOS still
// waits for the user to allow notifications): due() right away, then
// backoffMs after each failure, giving up after maxAttempts until reset().
class RetryTimer {
 public:
  RetryTimer(uint32_t backoffMs, uint8_t maxAttempts) : backoffMs_(backoffMs), maxAttempts_(maxAttempts) {}
  void reset() {
    attempts_ = 0;
    failedAtMs_ = 0;
  }
  void failed(uint32_t nowMs) {
    ++attempts_;
    failedAtMs_ = nowMs;
  }
  bool due(uint32_t nowMs) const {
    if (attempts_ == 0) return true;
    return attempts_ < maxAttempts_ && nowMs - failedAtMs_ >= backoffMs_;
  }

 private:
  uint32_t backoffMs_;
  uint8_t maxAttempts_;
  uint8_t attempts_ = 0;
  uint32_t failedAtMs_ = 0;
};

// Reassembles one ANCS Data Source reply to GetNotificationAttributes
// (AppIdentifier, Title, Message), which iOS may split across packets.
class AncsReply {
 public:
  void begin(uint32_t uid);
  void feed(const uint8_t *data, size_t length);
  bool complete() const { return complete_; }
  const String &app() const { return app_; }
  const String &title() const { return title_; }
  const String &message() const { return message_; }

 private:
  void parse();

  static constexpr size_t MAX_REPLY = 512;
  uint8_t buffer_[MAX_REPLY];
  size_t length_ = 0;
  uint32_t uid_ = 0;
  bool complete_ = false;
  String app_;
  String title_;
  String message_;
};
