# Eiswolfs Flightradar (CYD)

A live ADS-B flight radar running on the ESP32 "Cheap Yellow Display" (CYD,
ESP32-2432S028), showing nearby aircraft on a rotating radar screen with
altitude/speed/model/route details and a proximity LED alert.

![platform](https://img.shields.io/badge/platform-ESP32--2432S028-yellow)
![framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-blue)

![Eiswolfs Flightradar screenshot](images/radar.jpg)

## Getting Started

You can flash the firmware directly from your browser to your CYD display without installing any development tools — [Web Flasher](https://eiswolf-bg.github.io/eiswolfs-flightradar-CYD/) (requires Chrome, Edge, or Opera).

### Option 1: Quick Web Installation (Recommended)
1. Connect your "Cheap Yellow Display" to your computer using a USB cable.
2. Click the **Web Flasher link above**.
3. Click the **Install** button on the web page, select your USB/COM port, and follow the instructions.

### Option 2: Manual Compilation (For Developers)
1. Open and compile the project using PlatformIO (`platform = espressif32`, `board = esp32dev`, `framework = arduino`).
2. Insert a FAT32-formatted microSD card into your display.
3. On first boot: Calibrate the touchscreen and configure your Wi-Fi via the built-in on-screen keyboard.

---

## Features
- **Live radar screen** – circular radar view, sized to the full screen width, with a rotating green sweep line; each aircraft marker's heading line ends in a small arrowhead, so its direction of travel is unambiguous at a glance
- **Real ADS-B data** via the free [adsb.fi](https://adsb.fi) API, refreshed every 8 seconds
- **Color-coded aircraft** by altitude (green `<10k ft`, yellow `10-30k ft`, red `>30k ft`) with an on-screen legend
- **Tap an aircraft** to open a detail panel: callsign, airline, aircraft model (via [hexdb.io](https://hexdb.io)), altitude/speed/distance/heading in both metric and aviation units, estimated seat count
- **Bearing indicator** – with an aircraft selected, a dotted line plus a heading-in-degrees label points from the radar center to the compass edge, showing exactly which direction to look to spot it in the sky (not to be confused with the aircraft's own heading arrow)
- **Empty-sky timer** – when no aircraft are currently in range, the info bar counts up how long the sky's been empty instead of showing the usual "tap for details" hint
- **Adjustable range** (10/25/50/100 km) via on-screen button
- **Touch-driven WiFi setup** and on-screen keyboard for first-time configuration, with up to 3 saved networks (see [WiFi Manager](#-wifi-manager-up-to-3-saved-networks) below)
- **Location presets** – save up to 3 fixed locations, or point the radar at any place in the world (see [Location Presets](#-location-presets) below)
- **Aircraft watchlist** – track up to 5 specific flights by callsign, with a cyan radar ring and blue LED alert when one appears (see [Watchlist](#-watchlist) below)
- **Night dimming** – automatically dims the backlight to a softer (but still readable) level between 22:00 and 06:00 local time (see [Night Dimming](#-night-dimming) below)
- **History chart** – a simple 7-day bar chart of logged aircraft counts, right next to the plain logbook file list (see [History Chart](#-history-chart) below)
- **Web export** – a small built-in web page (reachable via the device's IP while on the same WiFi) to download the complete flight logbook as one merged CSV file (see [Web Export](#-web-export) below)
- **IP-based geolocation** (no GPS module needed) to center the radar on your location
- **Proximity LED alert** – the onboard RGB LED blinks green when an aircraft is within 3 km (no speaker on this board, so this replaces an audible alert)
- **Dual-core design** – all networking (WiFi, ADS-B polling, aircraft detail lookups) runs on Core 0, while the display and touch input run on Core 1, so the UI never freezes during a network request
- Data (airline names, aircraft-type seat estimates) loaded from CSV files on the SD card, auto-seeded on first boot
- **6 languages** (English, German, French, Turkish, Spanish, Italian), selectable on first boot or anytime from the menu
- **Time-of-day greeting** – once WiFi and the timezone are set, the splash screen greets you with "Good morning" / "Good day" / "Good evening" based on your local time

## Feature Deep-Dive

### 📍 Location Presets
By default the device figures out where it is automatically (via IP geolocation).
Under **Menu → Flight Options → Location Presets** you can additionally save up
to 3 fixed locations and switch between them.

**Good to know:** only **one** location is ever active at a time – either "Auto"
or exactly one of the 3 presets. Tap an entry to make it the active one.

**The actual trick:** you don't have to enter *your own* location there – you can
enter the coordinates of **any place in the world** and the radar will show the
live air traffic *there* instead, regardless of where your device physically is.

**Examples:**
- Enter the coordinates of Milan-Malpensa Airport (`45.6306`, `8.7281`) and watch
  the traffic there while sitting at home.
- Save home, workplace, and a holiday house as 3 presets and switch between them
  with a single tap, without waiting for IP geolocation to re-run each time.
- More precise than IP geolocation (which is often only accurate to city level) –
  handy if the device lives permanently at one fixed spot and you know its exact
  coordinates.

A "?" info button right on the Location Presets screen explains all of this
again directly on the device.

The screen also shows the **nearest known airport** (from a built-in list of ~34 major airports worldwide) to whichever location is currently active - handy to see exactly where a foreign preset points to, or why a location has a lot of air traffic. If the airport name doesn't fit on one line, it scrolls horizontally like a marquee.

### 📶 WiFi Manager (up to 3 saved networks)
Under **Menu → WiFi/Network** you can save up to 3 WiFi networks at once (not
just one). On boot, the device automatically connects to whichever one it can
currently see.

**Why would you need 3 networks if you're only ever in one place?**
- **Portable use:** take the device to the office, a friend's place, or a
  holiday home, and it connects automatically everywhere without re-entering
  passwords each time.
- **Guest network + main network:** many routers offer separate guest and main
  WiFi networks – save both, and the device just uses whichever is reachable.
- **Multiple access points/repeaters:** if you have several access points around
  the house (e.g. living room + workshop), the device automatically connects to
  whichever saved network it can currently reach.

Any saved network can be removed again at any time via the red "X", freeing up a
slot for a new one.

### 🌙 Night Dimming
Between 22:00 and 06:00 local time, the backlight automatically dims to a softer brightness level - still perfectly readable, but easier on the eyes if the device sits in a bedroom or living room around the clock. This is separate from (and gentler than) the existing inactivity timeout dimming, which kicks in after a period of no touch input regardless of time of day.

Toggle it under **Menu → System → "Night dimming"**. When you tap the screen while it's night-dimmed, it wakes back up to the soft night level (not full brightness) if it's still within the night window - no sudden bright flash in a dark room.

## ✈️ Flight Options — every function in detail

All the functions below are found under **Menu → Flight Options**.

### 📊 Statistics
Shows at a glance:
- **Aircraft logged today** – number of distinct aircraft first seen today
- **Aircraft logged all-time** – total across the entire runtime (all days combined)
- **Days with sightings** – how many days anything was logged at all
- **Average per day** – total count divided by number of days
- **Uptime** – how long the device has been running since the last restart
- **Highest flight today** – callsign and altitude of whichever aircraft was logged at the greatest altitude today (the altitude at first sighting, not necessarily its current one), e.g. "DLH441 (38000 ft)"

The **"Reset logbook data"** button (tap twice to confirm) permanently deletes all logged data.

### 📈 History Chart
Shows the last up to **7 days** from the flight logbook as a simple bar chart – a quick visual complement to the plain number list in Logbook files. Bar height is relative to the busiest day shown, with the exact count printed above each bar and the day-of-month printed below it.

**Good for:** spotting at a glance which days were busier than others, without having to compare numbers row by row.

A "?" info button on the History chart screen explains how it works directly on the device.

### 📁 Logbook files
Lists the last several days from the logbook individually, with date and the number of aircraft logged that day. Handy for tracing the history over multiple days instead of only seeing the grand total.

### 🌐 Web Export
In addition to viewing the logbook on the device itself, a small built-in web page lets you export the **complete flight logbook** as a single merged CSV file – handy for opening in Excel, Numbers, or Google Sheets. It starts automatically in the background as soon as the device connects to WiFi. Deliberately **export-only, no live radar tracking over the web** – that would just burn resources for little benefit.

**How to use it:** while your computer or phone is on the same WiFi network as the device, open a browser and go to the device's IP address (e.g. `http://192.168.1.42/`). The page lists every logged day with its aircraft count and a download link for the merged CSV.

Don't know the device's current IP? The "?" info button on the **Logbook files** screen shows it live on the device.

### 📖 Flight logbook (ON/OFF)
When enabled, the device logs **every newly sighted aircraft** (timestamp, hex code, callsign, registration, type, distance, altitude) into a daily CSV file on the SD card. An aircraft is only logged once per day, even if it crosses the radar multiple times. Survives a same-day restart without logging already-seen aircraft twice. Turning it off saves SD card write cycles if you don't care about the statistics/history.

### 💚 LED heartbeat (ON/OFF)
A brief **green flash** of the RGB LED on every successful ADS-B data fetch (every 8 seconds) – a quick visual confirmation that the device is actively receiving data and hasn't frozen. Automatically overridden by an active proximity or emergency alert (which take priority), so the indicators never mix.

### 🔔 Proximity LED (ON/OFF)
The RGB LED **blinks green** whenever an aircraft comes within **3 km** (straight-line distance to the currently active location, see [Location Presets](#-location-presets)). Since the CYD board has no speaker, this replaces an audible alert. Handy for noticing something interesting flying nearby without having to watch the screen.

### 🚨 Emergency alert (ON/OFF)
Continuously monitors all visible aircraft for one of the three international **emergency squawk codes**:
- **7500** – hijacking
- **7600** – radio failure
- **7700** – general emergency

If the device detects one of these codes, the RGB LED **blinks red rapidly** (noticeably faster/more urgent than the green proximity blink), and a flashing red banner appears in the radar header showing the callsign and squawk code. Takes priority over all other LED indications (proximity, heartbeat).

### 🎯 Watchlist
Track up to **5 specific flights** by their exact callsign (e.g. `DLH441`) - unlike the Airline Filter, which matches whole airlines, this targets one specific flight. As soon as a watched aircraft appears anywhere within the current radar range, it gets a **cyan ring** on the radar and the **RGB LED blinks blue** - handy for spotting a particular flight, e.g. when someone you know is arriving. Takes priority over the normal proximity alert, but not over an emergency squawk. Menu → Flight Options → **"Watchlist"** / **"Watchlist alert"**.

### 📍 Location Presets
See the dedicated section above: [Location Presets](#-location-presets) – save up to 3 fixed locations, or enter any place in the world to watch the air traffic there.

### 🏢 Airline filter
Completely hides aircraft from specific airlines from the radar (they're also excluded from counts/logging). Up to **10 airlines** can be entered (by ICAO code, e.g. `DLH` for Lufthansa, `RYR` for Ryanair). Handy if you live near an airport and don't want certain high-frequency airlines cluttering the radar.

### 🚗 Hide ground vehicles (ON/OFF)
ADS-B data doesn't only contain aircraft – it can also include **airport ground vehicles** (follow-me cars, pushback tugs, etc., flagged by the API under a separate "C" category). This option hides them so the radar stays focused purely on air traffic.

### 📶 WiFi Manager
See the dedicated section above: [WiFi Manager](#-wifi-manager-up-to-3-saved-networks) – save up to 3 WiFi networks at once.

---

**Technical notes for the curious:**
- Flight data is re-fetched from [adsb.fi](https://adsb.fi) every **8 seconds**.
- Altitude color coding is fixed: **green** < 10,000 ft, **yellow** 10,000–30,000 ft, **red** > 30,000 ft.
- Radar radius is switchable between **10 / 25 / 50 / 100 km**.

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
