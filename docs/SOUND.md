# Sound

A small speaker on the board's speaker connector (`cyd_2usb`, GPIO26 into the on-board amplifier) gives the deck a voice: tones for timers, Claude Code and notifications, and spoken Vietnamese notices from Home Assistant, Hermes or any script.

## Settings > Sound

| Row | |
| --- | --- |
| Volume | Off, Low, Mid, High. Tapping a level plays a preview. |
| Timers / Claude / Notify / Remind / Alerts / Clicks | Each group on or off. Clicks (a tick on every touch) start off. |
| Tap: test sound | Plays the test tune at the current volume. |

Quiet hours (Settings > Notify) silence everything except timers and alarms, and "urgent" notices from the notify API.

**Power:** loud tones draw enough current to brown out the board on a weak USB port (a hub or a monitor's port): the deck restarts on the first beep. High is capped at a level measured safe on such a port. For more volume, power the deck from a PC's own port or a 5 V 2 A charger; a 470-1000 µF capacitor across 5 V near the speaker connector also helps.

## What plays when

| Sound | When |
| --- | --- |
| timer | A countdown ends |
| alarm | The alarm rings: repeats and gets louder until DONE or SNOOZE |
| focus / break | A Pomodoro focus or break ends |
| claude_ask | Claude Code waits for a permission answer |
| claude_done | A Claude Code task finishes |
| claude_fail | A session's tool calls fail 3 times in a row |
| phone / pc | An iPhone or Windows notification arrives |
| reminder | STAND UP or REST YOUR EYES opens |
| alert | The home server goes offline |
| chime, doorbell, test | On request from the notify API |

## Notify API (Home Assistant, Hermes, scripts)

The bridge listens on port 47822 (all interfaces) for notices to show, play and speak on the deck. The token is generated on first run and kept in the git-ignored `host/config.json` (`notify_api.token`).

```
POST http://<pc-ip>:47822/notify
Authorization: Bearer <notify_api.token>
Content-Type: application/json

{"title": "Front door", "body": "Someone rang", "sound": "doorbell",
 "say": "Có người bấm chuông cửa", "urgent": true, "app": "Home Assistant"}
```

All fields are optional: `title`/`body` show a notification, `sound` picks a tone (see the table; unknown names play `chime`), `say` is spoken in Vietnamese, `urgent` rings through quiet hours. Windows asks once whether to allow Python through the firewall: allow it on private networks so the home server can reach the PC.

### Home Assistant

`configuration.yaml` (token in `secrets.yaml`):

```yaml
rest_command:
  deck_notify:
    url: "http://<pc-ip>:47822/notify"
    method: post
    headers:
      Authorization: !secret deck_notify_token   # "Bearer <token>"
    content_type: "application/json"
    payload: >
      {"title": {{ title | default('') | tojson }}, "body": {{ body | default('') | tojson }},
       "sound": {{ sound | default('chime') | tojson }}, "say": {{ say | default('') | tojson }},
       "urgent": {{ urgent | default(false) | tojson }}, "app": "Home Assistant"}
```

In an automation:

```yaml
action:
  - action: rest_command.deck_notify
    data:
      title: "Máy giặt"
      body: "Đã giặt xong"
      sound: chime
      say: "Máy giặt đã xong"
```

### Hermes or a script

```bash
curl -s -X POST http://<pc-ip>:47822/notify \
  -H "Authorization: Bearer $DECK_NOTIFY_TOKEN" -H "Content-Type: application/json" \
  -d '{"title":"Giá vé giảm","body":"DAD→HAN 994.781đ","sound":"chime","say":"Giá vé Đà Nẵng Hà Nội vừa giảm"}'
```

## Voice alerts

The bridge speaks these on its own (`host/announcer.py`). Each kind can be turned off in `voice_alerts` in `host/config.json`:

| Alert | What the deck says | Setting |
| --- | --- | --- |
| Claude Code | "Phiên deck src đang chờ bạn duyệt lệnh Bash", "Phiên api đã xong", "Phiên web app lỗi 3 lần liên tiếp" (after its own tone) | `claude` |
| PC | "CPU đang 92 độ", "GPU đang 87 độ", "RAM đã dùng 93 phần trăm": after 20 s over the limit, then not again for 15 minutes and until it drops 5 below | `pc`, `cpu_temp` (90), `gpu_temp` (85), `ram` (90) |
| Fuel | "Giá xăng dầu vừa điều chỉnh. Xăng E10 RON 95 tăng 420 đồng." when a new Petrolimex price set appears | `fuel` |
| Meetings | "5 phút nữa họp Weekly sync", then "Cuộc họp Weekly sync đã bắt đầu" | `meetings`, `remind_minutes` (5), `speak_titles` |

### Meetings

The bridge reads iCalendar (.ics) links every 10 minutes and keeps the next 3 days. Recurring meetings, exceptions and moved or cancelled occurrences are handled; all-day events are skipped.

- Google Calendar: Settings > the calendar > Integrate calendar > **Secret address in iCal format**.
- Outlook.com / Microsoft 365: Settings > Calendar > Shared calendars > **Publish a calendar** > ICS link.

```json
"calendar": {"ics_urls": ["https://calendar.google.com/calendar/ical/.../basic.ics"]}
```

The links are secret: anyone who has one can read the calendar. Keep them only in the git-ignored `host/config.json`; the bridge never writes them to its log. Spoken titles go to Microsoft's voice service: set `"speak_titles": false` to hear "5 phút nữa có cuộc họp" instead (the deck then shows only the time).

### Morning brief

Tap **Brief** on the second launcher page (swipe the launcher). The deck says the greeting, the date and lunar date, the weather, today's meetings (when a calendar is set) and the RON 95 price with its last change. Set `"brief": {"auto_at": "07:30"}` to hear it every day at that time; it is skipped if the PC starts more than an hour later.

## Speech

`say` needs two Python packages in the bridge's environment: `edge-tts` (Microsoft's online neural voices, no key) and `miniaudio` (decodes to the deck's 8 kHz, 8-bit format). They are in `host/requirements.txt`; `host/run_windows.bat` installs them. Without them the bridge logs `Notify API: speech off` and notices still show and play their tone.

- The text is sent to Microsoft's voice service to be spoken; do not use `say` for anything private.
- Voice: `notify_api.voice`, default `vi-VN-HoaiMyNeural` (female); `vi-VN-NamMinhNeural` is male.
- A clip is at most 6 seconds (the deck holds it in RAM while it plays); the bridge splits longer text into sentences and sends the next clip once the previous one has played. Over USB it takes a few seconds to arrive before it plays; over the Wi-Fi link it starts almost at once.
