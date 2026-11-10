# Home server tab

Monitor > swipe to **HOME SERVER** (`cyd_2usb`). It shows the machine that runs Home Assistant: CPU with load, RAM, disk, CPU temperature, network in/out, whether Home Assistant answers (and how fast), uptime and pending updates.

The bridge reads Home Assistant's REST API every 30 seconds; the deck never talks to Home Assistant itself, and the server's address and token stay on the PC.

## Setup

1. **System Monitor.** In Home Assistant open **Settings > Devices & services > Add integration > System Monitor**. Enable the sensors you want (they are created disabled): *Processor use*, *Memory usage*, *Disk usage /*, *Processor temperature*, *Load (1 min)*, *Network throughput in / out* for your network interface, *Last boot*.
   - With Home Assistant in Docker, the container shares the host's kernel, so CPU, RAM, temperature and network are the laptop's own. Disk usage of `/` is the container's root, which normally sits on the laptop's disk.
2. **Token.** In Home Assistant open your profile (bottom left) > **Security** > **Long-lived access tokens** > **Create token**, name it `deck`, and copy it.
3. **Config.** In the git-ignored `host/config.json`:

   ```json
   "home_server": {
     "enabled": true,
     "url": "http://<server-ip>:8123",
     "token": "<the long-lived token>",
     "name": "Home Server"
   }
   ```

4. Restart the bridge. Its log shows `Home server tab: reading Home Assistant`.

Give the laptop a fixed address (a DHCP reservation on the router) so `url` keeps working.

## What the status box says

| Status | Meaning |
| --- | --- |
| ONLINE 18 ms | Home Assistant answered; the time is the API round trip |
| OFFLINE no reply | the server or Home Assistant does not answer (asleep, off, wrong address) |
| OFFLINE token rejected | the token is wrong or was deleted |
| NOT SET UP | `home_server` has no `url` or `token` |

## Different entity names

The bridge finds System Monitor's sensors by their usual entity ids (`sensor.system_monitor_processor_use`, older `sensor.processor_use`, ...) and ignores virtual network interfaces (`docker0`, `veth*`, `br-*`, `lo`). If yours differ, or to add a battery sensor, name them:

```json
"home_server": {
  "url": "...", "token": "...",
  "entities": {"cpu": "sensor.my_cpu", "dn": "sensor.system_monitor_network_throughput_in_enp3s0",
               "battery": "sensor.laptop_battery"}
}
```

Keys: `cpu`, `ram`, `disk`, `temp`, `load`, `dn`, `up`, `boot`, `battery`. `refresh_seconds` (default 30, at least 10) sets how often the bridge asks.

## Privacy

The token gives full access to your Home Assistant: keep it only in `host/config.json` (git-ignored) and delete it in Home Assistant if the PC is lost. The bridge never logs the address or the token, and the deck only receives the numbers above.
