#include "ancs_parse.h"

#include <string.h>

namespace {
struct BundleName {
  const char *bundle;
  const char *name;
};

constexpr BundleName BUNDLE_NAMES[] = {
    {"com.apple.MobileSMS", "Messages"},
    {"com.apple.mobilephone", "Phone"},
    {"com.facebook.Messenger", "Messenger"},
    {"com.vng.zalo", "Zalo"},
    {"com.microsoft.skype.teams", "Teams"},
    {"com.microsoft.Office.Outlook", "Outlook"},
};

constexpr uint8_t ATTR_APP = 0;
constexpr uint8_t ATTR_TITLE = 1;
constexpr uint8_t ATTR_MESSAGE = 3;
constexpr size_t HEADER_LEN = 5;  // CommandID + NotificationUID
constexpr int ATTRIBUTE_COUNT = 3;
}  // namespace

String ancsAppName(const String &bundleId) {
  for (const BundleName &entry : BUNDLE_NAMES) {
    if (bundleId == entry.bundle) return entry.name;
  }
  const int dot = bundleId.lastIndexOf('.');
  return dot >= 0 ? bundleId.substring(dot + 1) : bundleId;
}

void AncsReply::begin(uint32_t uid) {
  uid_ = uid;
  length_ = 0;
  complete_ = false;
  app_ = "";
  title_ = "";
  message_ = "";
}

void AncsReply::feed(const uint8_t *data, size_t length) {
  if (complete_) return;
  const size_t n = min(length, MAX_REPLY - length_);
  memcpy(buffer_ + length_, data, n);
  length_ += n;
  parse();
}

void AncsReply::parse() {
  if (length_ < HEADER_LEN || buffer_[0] != 0x00) return;
  const uint32_t uid = buffer_[1] | (buffer_[2] << 8) | (buffer_[3] << 16) |
                       (static_cast<uint32_t>(buffer_[4]) << 24);
  if (uid != uid_) return;

  size_t pos = HEADER_LEN;
  int found = 0;
  String values[ATTRIBUTE_COUNT];
  while (found < ATTRIBUTE_COUNT && pos + 3 <= length_) {
    const uint8_t id = buffer_[pos];
    const size_t len = buffer_[pos + 1] | (buffer_[pos + 2] << 8);
    if (pos + 3 + len > length_) return;  // wait for the next packet
    String value;
    value.reserve(len);
    for (size_t i = 0; i < len; ++i) value += static_cast<char>(buffer_[pos + 3 + i]);
    if (id == ATTR_APP) values[0] = value;
    if (id == ATTR_TITLE) values[1] = value;
    if (id == ATTR_MESSAGE) values[2] = value;
    pos += 3 + len;
    ++found;
  }
  if (found < ATTRIBUTE_COUNT) return;
  app_ = values[0];
  title_ = values[1];
  message_ = values[2];
  complete_ = true;
}
