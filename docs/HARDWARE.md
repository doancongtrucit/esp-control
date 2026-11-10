# Hardware

ESP Control Deck runs on 2.8-inch 320x240 touch boards built around a classic ESP32-WROOM-32 and a CH340 USB-to-UART bridge. The main target is the **ESP32-2432S028 "Cheap Yellow Display" with two USB ports** (USB-C and micro-USB), PlatformIO environment `cyd_2usb`.

## ESP32-2432S028, two USB ports (`cyd_2usb`)

| Function | GPIO |
| --- | ---: |
| TFT MOSI / SCLK / CS / DC | 13 / 14 / 15 / 2 |
| TFT MISO | 12 |
| Backlight | 21 |
| Touch CLK / MOSI / MISO / CS / IRQ | 25 / 32 / 39 / 33 / 36 |
| RGB LED R / G / B | 4 / 16 / 17 |

- Display: ST7789 with BGR colour order. Touch: XPT2046 on its own SPI bus with a fixed raw-to-screen mapping (no calibration screen).
- `DECK_ROTATION` in `platformio.ini` turns picture and touch by 180 degrees for either cable side; Settings > Display > Flip 180 does the same at run time.
- Radios: Wi-Fi (PC notifications, the Wi-Fi link, ARAM) and Bluetooth LE (iPhone notifications, auto-lock) run together; the firmware lets Wi-Fi take the radio first.
- Flash: 4 MB with the `huge_app` partition table (3 MB app, no OTA slot), so firmware updates go over USB.

Optional add-ons that fit the free pins: an I2S microphone (INMP441) and a small speaker on the board's speaker connector.

## 2.8-inch ILI9341 board (`esp32_pc_control_deck`)

A second environment builds the simpler monitor + macros layout for 2.8-inch ESP32 boards with an ILI9341 panel and the touch controller on the display bus.

| Function | GPIO |
| --- | ---: |
| TFT MISO / MOSI / SCLK | 4 / 13 / 14 |
| TFT CS / DC / RST | 15 / 2 / -1 |
| Backlight | 27 |
| Touch CS | 33 |

The touch panel is calibrated on first boot (four corners) and the result is kept in the ESP32's preferences.

## USB

The CH340 bridge gives power, firmware upload and the serial link to the PC. It is not a USB keyboard: the deck sends a macro number and the bridge on the PC performs the configured action. Without a data cable the deck can run from any 5 V charger and talk to the bridge over Wi-Fi (see [WIFI_LINK.md](WIFI_LINK.md)).
