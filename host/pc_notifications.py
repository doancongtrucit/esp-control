"""Windows toast notifications for the ESP32 deck.

Polls the Windows UserNotificationListener (winrt) and calls on_toast(app,
sender, body) once for every toast that appears after the watcher starts.
Notification text is passed on, never logged.
"""
from __future__ import annotations

import asyncio
import threading
import time
from typing import Callable, Iterable

Toast = tuple[int, str, str, str]  # (id, app, sender/title, body)


def windows_toasts() -> list[Toast]:
    """Current toasts from the Windows notification center."""
    from winrt.windows.ui.notifications import KnownNotificationBindings, NotificationKinds
    from winrt.windows.ui.notifications.management import (
        UserNotificationListener,
        UserNotificationListenerAccessStatus,
    )

    async def fetch() -> list[Toast]:
        listener = UserNotificationListener.current
        if await listener.request_access_async() != UserNotificationListenerAccessStatus.ALLOWED:
            raise PermissionError("Windows denied notification access")
        toasts: list[Toast] = []
        for item in await listener.get_notifications_async(NotificationKinds.TOAST):
            try:
                app = item.app_info.display_info.display_name
            except Exception:
                app = "Windows"
            binding = item.notification.visual.get_binding(KnownNotificationBindings.toast_generic)
            texts = [t.text for t in binding.get_text_elements()] if binding else []
            title = texts[0] if texts else ""
            body = " ".join(texts[1:])
            toasts.append((int(item.id), app, title, body))
        return toasts

    return asyncio.run(fetch())


class PcNotificationWatcher:
    def __init__(
        self,
        on_toast: Callable[[str, str, str], None],
        interval_seconds: float = 2.0,
        source: Callable[[], Iterable[Toast]] = windows_toasts,
        log: Callable[[str], None] = lambda message: None,
    ):
        self.on_toast = on_toast
        self.interval = interval_seconds
        self.source = source
        self.log = log
        self.seen: set[int] | None = None  # None until the first poll sets a baseline
        self.last_error = ""

    def poll_once(self) -> None:
        try:
            toasts = list(self.source())
        except Exception as exc:
            message = f"PC notifications unavailable: {type(exc).__name__}"
            if message != self.last_error:
                self.log(message)
                self.last_error = message
            return
        self.last_error = ""
        if self.seen is None:
            self.seen = {toast[0] for toast in toasts}
            return
        for toast_id, app, title, body in toasts:
            if toast_id not in self.seen:
                self.seen.add(toast_id)
                self.on_toast(app, title, body)

    def start(self) -> None:
        threading.Thread(target=self._loop, name="pc-notifications", daemon=True).start()

    def _loop(self) -> None:
        while True:
            self.poll_once()
            time.sleep(self.interval)
