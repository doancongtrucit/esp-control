# Health, break reminders and auto-lock

Info > **Health** on the `cyd_2usb` deck. The bridge reads how long the keyboard and mouse have been idle (Windows `GetLastInputInfo`); nothing else is recorded.

- **TODAY**: minutes with input in the last minute. **SITTING**: the current session; a session ends after 5 idle minutes, which counts as one break. **LONGEST**: the longest session today.
- **TODAY BY HOUR** and **7 DAYS** charts. The hour label turns red (LATE - TIME TO REST) when you are at the PC after 23:00.
- **STAND UP** opens after 50 minutes without a break: SNOOZE 10M, or DONE (no more until the next break). The LED breathes cyan meanwhile. A real break closes the card by itself.
- **REST YOUR EYES** opens every 20 minutes of a session and closes after 20 seconds (or a tap).
- Reminders never cover a Claude Code permission prompt or the ARAM screen.

Counts are kept in `host/activity_state.json` (git-ignored) so a bridge restart keeps today's numbers.

## Auto-lock

The deck already pairs with your iPhone for notifications (ANCS). Every 2 s it tells the bridge whether that link is up and how strong it is (RSSI). The bridge locks Windows (Win+L) when all of these hold:

1. the iPhone was near at some point since the bridge started,
2. the link is down, or the smoothed RSSI is below `rssi_away` (-85 dBm), for 30 s,
3. nobody touched the keyboard or mouse for 60 s,
4. the deck is still reporting (unplugged or switched off = never lock).

It locks once per absence and arms again when the phone comes back. The Health tab shows the live RSSI: read it with the phone in your pocket at the desk and a few metres away, then set `rssi_away` between the two. Tap the AUTO-LOCK card to turn it on or off.

The iPhone can drop the BLE link now and then while it sits on the desk; the 60 s idle rule keeps that from locking you out mid-work. Windows' own **Dynamic Lock** (Settings > Accounts > Sign-in options) does a similar job if the PC has Bluetooth.

## Settings (`host/config.json`)

```json
"activity": {
  "enabled": true, "stand_minutes": 50, "eye_minutes": 20, "break_minutes": 5,
  "snooze_minutes": 10, "late_hour": 23,
  "lock": {"enabled": true, "rssi_away": -85, "away_seconds": 30, "idle_seconds": 60}
}
```

`stand_minutes` or `eye_minutes` set to 0 turns that reminder off. `lock.enabled` is only the first-run default; the switch on the deck wins after that.
