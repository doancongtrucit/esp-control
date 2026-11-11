# Home screen: Home Assistant devices

The **Home** app on the launcher (`cyd_2usb`) shows up to 12 tiles, 6 per page (swipe for the next page). Each tile shows the device's state from Home Assistant:

- **Toggle** tiles (fan, light, switch, TV, cover): tap to switch; a lit tile is on. The deck shows the new state at once and the bridge confirms it within a few seconds.
- **Run** tiles (scripts, scenes, buttons): tap to run.
- **HOLD** tiles: the finger must stay on the tile for one second while a bar fills. Switches get this by default, so a stray touch never turns off a PC or a heater.
- **Air conditioner** (a `climate` entity): tap to open its card with ON/OFF, the target temperature (- / +), the mode (Auto, Cool, Dry, ...) and the fan speed. The header's Home button goes back to the tiles.

An infrared air conditioner (SmartIR, Broadlink) cannot report back, so the card shows what was last sent.

## Setup

Uses the same Home Assistant connection as the HOME SERVER tab (`home_server.url` and `token`, see [HOME_SERVER.md](HOME_SERVER.md)). Without `home_controls` the bridge picks the controllable devices itself (climate, light, fan, cover, media player, switch, scene, script, button). To choose the tiles, their order, labels and icons, list them in the git-ignored `host/config.json`:

```json
"home_controls": [
  {"entity": "climate.living_room", "label": "Air Con", "icon": "ac"},
  {"entity": "fan.standing_fan", "label": "Fan", "icon": "fan"},
  {"entity": "media_player.tv", "label": "TV", "icon": "tv"},
  {"entity": "switch.pc", "label": "PC", "icon": "power", "confirm": true},
  {"entity": "script.fan_swing", "label": "Swing", "icon": "bolt"}
]
```

- `icon`: `ac`, `fan`, `tv`, `power`, `bolt`, `sun`, `home`, `clock`, `lock` or any macro icon (see [MACROS.md](MACROS.md)).
- `confirm`: `true` / `false` overrides the hold rule.
- Entity ids are in Home Assistant under **Settings > Devices & services > Entities**.

Restart the bridge after editing. Its log shows `Home screen: controlling Home Assistant devices`.

## Safety and privacy

The deck sends only a tile number, or an air-conditioner choice from the lists the bridge gave it (`ha_press`, `ha_ac` in [PROTOCOL.md](PROTOCOL.md)). Entity ids, services and the token stay on the PC, and the bridge only calls services on the tiles you listed. Over Wi-Fi these commands are signed and acknowledged like macros.
