# Wi-Fi link (`cyd_2usb`)

The deck works without a USB data cable: power it from any 5 V charger and the bridge talks to it over your local Wi-Fi. With the cable plugged in, USB always wins.

## What works over Wi-Fi

Everything the deck does day to day: PC stats, clock and calendar, Info feeds, macros, the CLAUDE CODE screen (status, Allow / Deny / Always, Usage, Stats), Health and break reminders, auto-lock, PC notifications and ARAM. iPhone notifications use Bluetooth and do not depend on either link.

USB only: flashing firmware (the `huge_app` partition table has no OTA slot), the screenshot tool, touch recalibration, and Wi-Fi provisioning — the Wi-Fi password never travels over the air.

When neither link is up the clock keeps running from NTP (or the last time the bridge sent), so the standby screen stays right.

## How it connects

1. The deck joins Wi-Fi with the credentials the bridge sent over USB earlier.
2. While the bridge has no USB deck it sends a signed probe every 3 s to the deck's last known IP (`host/device_state.json`), and the deck broadcasts a signed beacon every 3 s while it has no link. Either way the bridge links within a few seconds.
3. From then on the same JSON frames as on USB travel as UDP datagrams: bridge port 4211, deck port 4210 (the notification port).
4. Plugging the cable back in moves the bridge to USB within about 2 s.

The **PC link** row on Settings > Connect shows `USB`, `Wi-Fi` or `Offline`.

## Security

- Every datagram carries an HMAC-SHA256 (first 16 bytes) keyed with the notification token, a Unix timestamp and a sequence number. Datagrams with a wrong MAC, a timestamp more than 2 minutes off, or a repeated (timestamp, sequence) are dropped, so nobody on the network can forge or replay a macro or an Allow.
- Macros, Claude answers and the other deck commands are acknowledged; the deck resends a command until the ack arrives and the bridge runs it once.
- `wifi`, `shot` and `recalibrate` frames are refused over Wi-Fi.
- Frames are signed, not encrypted: someone on the same network can read the stats, session names and notification text that travel to the deck. Turn the link off on networks you do not trust.

## Responsiveness

Measured on a home network with the deck idle, round trip bridge -> deck -> bridge: median about 60 ms, slowest about 230 ms. A macro press only needs the deck -> PC half. Wi-Fi and Bluetooth share the ESP32's radio and the modem must sleep between beacons while Bluetooth is on, so the firmware tells the radio to favour Wi-Fi; Bluetooth (iPhone) needs little air time.

Some office networks block devices from talking to each other (client isolation); the link then stays down and only USB works.

## Settings (`host/config.json`)

```json
"wifi_link": {"enabled": true, "port": 4211}
```

The link needs the notification token (`notifications.token`, generated on first run). On Windows, allow Python through the firewall if it asks; the probes usually keep the path open even without a rule.
