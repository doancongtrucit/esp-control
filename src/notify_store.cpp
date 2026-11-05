#ifdef DECK_UI_V2
#include "notify_store.h"

#include "text_vi.h"

void NotifyStore::add(NotifySource source, const String &app, const String &from,
                      const String &body, uint32_t nowMs) {
  Notification &slot = items_[head_];
  slot.source = source;
  slot.app = sanitizeVi(app, APP_CHARS);
  slot.from = sanitizeVi(from, FROM_CHARS);
  slot.body = sanitizeVi(body, BODY_CHARS);
  slot.receivedMs = nowMs;
  slot.unread = true;
  head_ = (head_ + 1) % CAPACITY;
  if (count_ < CAPACITY) ++count_;
  ++version_;
}

const Notification &NotifyStore::at(size_t i) const {
  return items_[(head_ + CAPACITY - 1 - (i % CAPACITY)) % CAPACITY];
}

size_t NotifyStore::unreadCount() const {
  size_t unread = 0;
  for (size_t i = 0; i < count_; ++i) {
    if (at(i).unread) ++unread;
  }
  return unread;
}

void NotifyStore::markAllRead() {
  for (Notification &item : items_) item.unread = false;
  ++version_;
}
#endif  // DECK_UI_V2
