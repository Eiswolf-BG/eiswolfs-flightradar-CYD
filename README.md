# Eiswolfs Flightradar (CYD)

A live ADS-B flight radar running on the ESP32 "Cheap Yellow Display" (CYD,
ESP32-2432S028), showing nearby aircraft on a rotating radar screen with
altitude/speed/model/route details and a proximity LED alert.

![platform](https://img.shields.io/badge/platform-ESP32--2432S028-yellow)
![framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-blue)

![Eiswolfs Flightradar screenshot](images/radar.jpg)

## Features

- **Live radar screen** – circular radar view, sized to the full screen width, with a rotating green sweep line
- **Real ADS-B data** via the free [adsb.fi](https://adsb.fi) API, refreshed every 8 seconds
- **Color-coded aircraft** by altitude (green `<10k ft`, yellow `10-30k ft`, red `>30k ft`) with an on-screen legend
- **Tap an aircraft** to open a detail panel: callsign, airline, aircraft model (via [hexdb.io](https://hexdb.io)), altitude/speed/distance/heading in both metric and aviation units, estimated seat count
- **Adjustable range** (10/25/50/100 km) via on-screen button
- **Touch-driven WiFi setup** and on-screen keyboard for first-time configuration
- **IP-based geolocation** (no GPS module needed) to center the radar on your location
- **Proximity LED alert** – the onboard RGB LED blinks green when an aircraft is within 3 km (no speaker on this board, so this replaces an audible alert)
- **Dual-core design** – all networking (WiFi, ADS-B polling, aircraft detail lookups) runs on Core 0, while the display and touch input run on Core 1, so the UI never freezes during a network request
- Data (airline names, aircraft-type seat estimates) loaded from CSV files on the SD card, auto-seeded on first boot

## Hardware

- ESP32-2432S028 ("Cheap Yellow Display", CYD) – ILI9341 2.8" 240x320 touch TFT
- microSD card (FAT32) for settings, WiFi credentials, and lookup tables
- No GPS, no speaker required – uses IP geolocation and the onboard RGB LED

### Pinout used

| Function | Pins |
|---|---|
| TFT (VSPI) | MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, BL=21 |
| Touch (XPT2046) | CLK=25, CS=33, MOSI=32, MISO=39, IRQ=36 |
| microSD (HSPI) | CLK=18, MISO=19, MOSI=23, CS=5 |
| RGB LED (active-low) | R=4, G=16, B=17 |

## Getting started

1. Flash with PlatformIO (`platform = espressif32`, `board = esp32dev`, `framework = arduino`)
2. Insert a FAT32-formatted microSD card
3. On first boot: touch-calibrate the screen, then select your WiFi network and enter the password using the on-screen keyboard
4. The radar screen appears automatically once WiFi and location are ready

## Data sources

- Aircraft positions: [adsb.fi](https://adsb.fi) (free, no API key)
- Aircraft model lookups: [hexdb.io](https://hexdb.io) (free, community-maintained, rate-limited)
- Location: [ip-api.com](https://ip-api.com) (free IP geolocation)

## Disclaimer

Aircraft model and seat-count data are estimates from community databases and
local lookup tables, not live/official figures. This project is for hobby use
and is not intended for navigation or safety-critical purposes.

## License

MIT (or add your preferred license here)
