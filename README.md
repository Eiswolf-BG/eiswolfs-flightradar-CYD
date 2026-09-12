# Eiswolfs Flightradar (CYD)

A live ADS-B flight radar running on the ESP32 "Cheap Yellow Display" (CYD,
ESP32-2432S028), showing nearby aircraft on a rotating radar screen with
altitude/speed/model/route details and a proximity LED alert.

![platform](https://img.shields.io/badge/platform-ESP32--2432S028-yellow)
![framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-blue)

![Eiswolfs Flightradar Screenshot](images/screenshot.jpg)

[![Photos](https://img.shields.io/badge/📷_Photos-View_Gallery-blue?style=for-the-badge)](docs/gallery.md)

## Getting Started

You can flash the firmware directly from your browser to your CYD display without installing any development tools — just open the [Web Flasher](https://eiswolf-bg.github.io/eiswolfs-flightradar-CYD/) (requires Chrome, Edge, or Opera).

### Option 1: Quick Web Installation (Recommended)
1. Connect your "Cheap Yellow Display" to your computer using a USB cable.
2. Click the **Web Flasher link above**.
3. Click the **Install** button on the web page, select your USB/COM port, and follow the instructions.

**Driver note:** If your computer doesn't detect the CYD (no USB/COM port shows up), you likely need to install a driver for the board's CH340 USB-to-serial chip first:
- **Windows:** [CH341SER.EXE](https://www.wch-ic.com/downloads/CH341SER_EXE.html)
- **macOS:** [CH341SER_MAC.ZIP](https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html)
- **Linux:** built into the kernel since version 5.x - no separate installation needed

### Option 2: Manual Compilation (For Developers)
1. Open and compile the project using PlatformIO (`platform = espressif32`, `board = esp32dev`, `framework = arduino`).
2. Insert a FAT32-formatted microSD card into your display.
3. On first boot: Calibrate the touchscreen and configure your Wi-Fi via the built-in on-screen keyboard.

**Guided first-time setup:** a Welcome screen with the animated radar graphic greets you first, followed by language selection, then a one-time (skippable) screen that explains why setting your exact location gives a much better experience than automatic IP-based location and offers to set it up right there via the address search. A final setup-complete screen then confirms everything is saved and ready, counting down automatically before entering the app (or tap to skip ahead).

**Tip:** for the best experience, set your exact location as a Location Preset (Menu → Flight Options → Location Presets → "+" → Search by address). Just type your address and the device looks up the exact coordinates for you automatically - no need to look anything up yourself. This is much more accurate than automatic IP-based location, which can easily be off by 20-30 km - you might hear a plane outside your window while the radar screen stays empty because the device thinks you're somewhere else entirely. With your precise location set, the proximity LED blinks in sync with aircraft you can actually hear overhead. You'll also be prompted to set this up right during first-time setup.

---

## Features

- [Radar Display & Visuals](#️-radar-display--visuals)
- [Aircraft Details](#-aircraft-details)
- [Alerts & LEDs](#-alerts--leds)
- [Filters](#-filters)
- [Location & Range](#-location--range)
- [Lists & Logbook](#-lists--logbook)
- [Web & Integrations](#-web--integrations)
- [System & Settings](#️-system--settings)

### 🛰️ Radar Display & Visuals

The main screen is a circular radar view, sized to the full screen width, with a rotating sweep line and a twinkling starfield filling the space outside the radar circle. Each aircraft marker shows a recognizable type silhouette (airliner, private jet, or turboprop) instead of a generic arrow, based on a best-effort classification of the ICAO type code, with a small single-color chevron just ahead of the nose pointing in the current direction of travel - heading stays obvious at a glance even in dense traffic. Wide-body "Heavy" jets over 136 tonnes MTOW (e.g. A380, B747, B777) get a larger, bolder version of the same silhouette. Ground vehicles (follow-me cars, pushback tugs, flagged by the API under their own category) render as distinct blue square markers, and helicopters get a filled circle with a rotor cross instead of an arrowhead, since a hovering helicopter has no meaningful forward heading. Aircraft detected as military/government flights (via a best-effort squawk-code range check) get a thin orange ring around the marker.

Aircraft are color-coded by altitude - green below 10,000 ft, yellow between 10,000-30,000 ft, red above 30,000 ft - with a legend at the bottom of the radar screen. Tapping the legend opens a full-screen overlay showing the same three bands larger, with a short explanation, the ground-vehicle marker (if visible), the military/government ring, and the Heavy aircraft symbol, all using your configured metric/imperial unit. With an aircraft selected, a dotted line plus a heading-in-degrees label points from the radar center to the compass edge, showing exactly which direction to look to spot it in the sky.

**Color themes & effects:** under **Menu → System → "Radar Display"**, the **"Colors"** button switches between five color themes - Green, Amber, Blue, Red, or Purple - applied system-wide (menus, buttons, borders, text); aircraft altitude colors and alert/status colors keep their own fixed meaning regardless of theme. Five independent, combinable extras live on the same screen: a **CRT-Phosphor** glow that fades every aircraft marker color in and out as the sweep passes it; a **Radar Pulse** animation - an expanding ring from the center on every fresh data update; **Classic Radar** mode - a comet-tail sweep, extra grid spokes, a sonar-ping ring and a brief burst of signal noise whenever a new aircraft first appears; a subtle, always-on horizontal **scanline overlay** for a classic CRT-radar look; and an animated **"Show Weather"** effect (see below). A "?" help button next to each explains it directly on the device. A short, simulated old-radar-system startup sequence ("Initializing transponder receiver...", etc.) plays on every boot before the usual splash screen, and a **"Mode"** button in the radar screen header jumps straight to this **Radar Display** menu.

**Weather on the radar:** whenever the weather data for the active location detects rain or a thunderstorm, animated rain drops fall across the radar circle, matching the real wind direction, with drop count and fall speed scaled to the actual rain intensity (light/moderate/heavy). When it's actually snowing, small drifting white dots appear instead, visually distinct from rain. The same effect plays on the idle screensaver (straight down, no wind tilt, since there's no compass reference there) and on the web live radar (in the device's current theme color).

**Radar corner overlays:** three small, dimmed indicators sit in the free space outside the radar circle without interfering with the sweep, scanlines, or rain effect - a **3h weather preview** (top-right) that only appears when the weather 3 hours from now differs from current conditions; a **nearest airport** display (top-left, distance + bearing in your configured IATA/ICAO format), tappable to add that airport directly as a new location preset; and an **event indicator** (bottom-right) that rotates every 3 seconds through whatever's currently active among a squawk watchlist match, an aircraft watchlist match, or a hidden (filtered) airline nearby, each tappable to jump to the matching screen. A single **"Overlay"** switch on the Radar Display menu turns all three off together. Tapping the 3h weather preview opens a full-screen hourly timeline with a larger, colored icon (yellow sun, white-grey clouds, sky-blue raindrops, white snow, yellow lightning - matching the idle screensaver's weather icon) for each upcoming timestep, plus temperature.

**Night dimming:** between sunset and sunrise at your active location, the backlight automatically dims to a softer brightness level - still readable, easier on the eyes if the device runs around the clock. The window follows the real sunrise/sunset time for your location and the current date, shifting earlier in winter and later in summer; until location or time of day is known (e.g. briefly after boot), it falls back to a fixed 10pm-6am window. The same window also softens the radar screen itself - aircraft markers and the sweep line switch to darker green/yellow/red tones. Toggle it under **Menu → System → "Night dimming"**; tapping the screen while dimmed wakes it to the soft night level (not full brightness) if it's still night, avoiding a sudden bright flash in a dark room. This is separate from the inactivity screen timeout, which turns the display fully off after a period without touches regardless of time of day.

**Idle screensaver:** instead of turning the display fully off after the screen timeout, an optional screensaver (same settings screen, off by default) shows a dimmed starfield with the radar logo, a large clock, today's date in your language's local format, the firmware version, and a subtle "nearest aircraft" indicator (icon + distance) - dimmed noticeably deeper than regular night dimming. A tap wakes it back up. The same rain/snow weather effect plays here too, visibly in front of the logo/clock without ever drawing over the time and date - raindrops use a "glossy streak" double-line look and snowflakes a proper ice-crystal (dendrite) shape, with noticeably more drops/flakes at every intensity level than a plain single line or dot.

### ✈️ Aircraft Details

Tapping an aircraft opens a detail panel showing callsign, airline, aircraft model (via [hexdb.io](https://hexdb.io)), flight route (origin/destination airport, resolved via a chain of three free lookup services for better coverage), altitude/speed/distance/heading/bearing in both metric and aviation units, estimated seat count, and a "QR" button that shows a full-screen QR code linking to that flight's live-tracking page on FlightAware. The bearing is shown both as a compass direction (e.g. "NE") and the numeric degree value, alongside the elevation angle above the horizon (e.g. "21°"), calculated from the aircraft's distance and altitude. A "+"/"-" button right next to "QR" adds or removes the aircraft from the [Watchlist](#-watchlist) with a single tap. The route line itself shows a short "(IATA)"/"(ICAO)" label so it's always clear which code format is displayed. The panel stays open with the last known values even if the aircraft leaves radar range while you're reading it - it only closes when you tap to close it yourself.

The panel also derives a few things purely from how the aircraft's position changes over the last few data updates, with no extra lookups involved:
- **Approaching / departing / passing** - whether the aircraft's distance to you is currently shrinking, growing, or roughly constant.
- **Overflight ETA** - while approaching, an estimate of when it will reach its closest point to you and how close that will be, based on its current heading and speed (always marked as an estimate, since a course change would invalidate it).
- **First seen / seen for** - when the aircraft was first spotted in the current session and how long it's been continuously visible since; resets on every device restart.
- **Previously seen** - how many times the aircraft has been logged on earlier days and when it was last seen, looked up from the logbook files in the background so opening the panel is never delayed.
- **Typical pattern** - once an aircraft has been logged at least 3 times before, the panel also shows the typical time-of-day range and altitude range it's usually seen in (e.g. "usually 07-09h, 2000-8000ft"), derived from the same logbook lookup as "Previously seen" above.
- **Aircraft profile** - the closest distance and highest speed ever recorded for this aircraft across all logged sightings, shown next to "Typical pattern" once at least one matching logbook entry exists for it.

Aircraft identified as military/government flights (see the orange ring on the radar) get a matching legend line in the detail panel itself.

### 🚨 Alerts & LEDs

The CYD board has no onboard speaker, so alerts are primarily conveyed through the RGB LED - but two additional, independent sound options exist: a browser alert sound on the [Live Radar web page](#-web-export), and an optional physical alarm tone through a speaker wired to the board's SPK connector.

- **Proximity LED (ON/OFF)** - blinks in the currently selected color theme whenever an aircraft comes within 3 km (straight-line distance to the active location).
- **Intelligent proximity alert** - an alternative mode (**Menu → Flight Options → LED Alerts → "Alert mode"**) that replaces the single-radius alert with three staggered distance zones (yellow/orange/red), triggering only when an aircraft genuinely moves into a tighter zone, each zone blinking progressively faster. Triggers regardless of altitude, so it stays consistent for high-flying long-haul traffic too.
- **Watchlist (callsign)** - track up to 5 specific flights by exact callsign (e.g. `DLH441`). A matching aircraft gets a cyan ring on the radar and the LED blinks a fixed cyan (independent of the color theme, so it always stays distinguishable from the theme-colored proximity alert). Add flights via **Menu → Flight Options → "Watchlist"**, or with the "+"/"-" button in the aircraft detail panel. Turn the alert on/off separately under "Watchlist alert".
- **Squawk Watchlist** - a similarly-structured list right next to the callsign watchlist stores up to 5 four-digit octal squawk codes; any aircraft transmitting a watched squawk triggers the same cyan-ring-plus-LED alert.
- **Emergency alert (ON/OFF)** - continuously monitors all visible aircraft for one of the three international emergency squawk codes (7500 hijacking, 7600 radio failure, 7700 general emergency). On detection, the LED blinks a real Morse SOS pattern (3 short, 3 long, 3 short, then a pause) - deliberately distinct from every other LED indication, including the Red color theme's own accent color - and a flashing red banner appears in the radar header with the callsign and squawk code. Takes priority over all other LED indications.
- **LED heartbeat (ON/OFF)** - a brief flash of the LED, in the current color theme, on every successful ADS-B data fetch (every 8 seconds), confirming the device is actively receiving data. Overridden by an active proximity or emergency alert.
- **Update-available LED signal** - the LED blinks white three times every 10 seconds whenever a firmware update is available, in addition to the red dot badges elsewhere in the UI; suppressed during an emergency alert, and stops once the update is installed or no longer available. Can be turned off separately, right below the update button, if you'd rather rely on the red dot alone.
- **Color-themed LEDs** - the heartbeat flash and proximity alert LED follow the selected Radar Display color theme; the watchlist LED is fixed cyan and the emergency alarm always blinks its Morse pattern, so both stay unmistakable on any theme. Purple is deliberately tuned so it stays visually distinct from the white update-available signal.

Priority order when several conditions apply at once: emergency alert first, then watchlist, then the (simple or intelligent) proximity alert, then the heartbeat.

- **Web alert sound (ON/OFF)** (**Menu → Flight Options → LED Alerts → "Web Alert Sound"**, on by default) - plays a sound directly in the browser tab of anyone with the [Live Radar web page](#-web-export) open: a single beep on a new watchlist/squawk-watchlist match, and a continuous rising/falling siren for as long as any visible aircraft is squawking an emergency code. A speaker icon on the page lets each visitor mute the sound for themselves (per-browser, not synced to the device or other visitors); tapping/clicking anywhere on the page once is needed first so the browser allows audio to play at all. Also optionally drives a physical alarm tone through a speaker wired to the board's SPK connector (see [Hardware](#hardware) below), mirroring both cases the browser sound covers - a short single beep on a new watchlist match, the same continuous siren on an emergency squawk - and running independently of the LED alarm, so both can sound at once. Without a speaker actually connected, nothing audible changes at all - the pin just stays quiet. For decent audio, playing the sound through the web page on your phone/Mac/PC remains the better option; wiring up a speaker directly works too, but comes with the CYD board's known sweep noise and generally weak audio quality.

### 🔎 Filters

- **Airline filter** - completely hides aircraft from up to 10 specific airlines (entered by ICAO code, e.g. `DLH` for Lufthansa, `RYR` for Ryanair) from the radar, aircraft list, and counts/logging. Handy near an airport with high-frequency airlines you'd rather not see.
- **Hide ground vehicles (ON/OFF)** - hides airport ground vehicles (follow-me cars, pushback tugs) that ADS-B data can include alongside real aircraft, so the radar stays focused purely on air traffic.
- **Show helicopters only (ON/OFF)** - filters the radar, [Aircraft List](#-aircraft-list), and Live Radar web page down to helicopters only, hiding everything else. Off by default; handy for rotorcraft-specific traffic (medevac, police, sightseeing tours).
- **Show low-altitude aircraft only (ON/OFF)** - filters the radar down to aircraft currently below the green altitude threshold. Ground vehicles are never included by this filter regardless of the separate ground-vehicle setting. Off by default.
- **Empty-sky filter hint** - when the radar shows no aircraft because a filter (helicopters only, low-altitude only, airline filter) is hiding everything, the "empty sky" message names which filter is active and is tappable, jumping straight to the Display Filters menu.

All four toggles live under **Menu → Flight Options → Display Filters**.

### 📍 Location & Range

By default the device figures out where it is automatically via IP geolocation. Under **Menu → Flight Options → Location Presets** you can additionally save up to 3 fixed locations and switch between them - only one location is ever active at a time, either "Auto" or exactly one of the 3 presets, and tapping an entry makes it active.

**The actual trick:** a preset doesn't have to be *your own* location - entering the coordinates of any place in the world makes the radar show the live air traffic *there* instead, regardless of where the device physically is. For example, entering the coordinates of Milan-Malpensa Airport (`45.6306`, `8.7281`) shows the traffic there while sitting at home; saving home, workplace, and a holiday house as 3 presets lets you switch between them with a single tap, without waiting for IP geolocation to re-run each time. It's also generally more precise than IP geolocation (often only accurate to city level) if the device lives permanently at one fixed spot and you know its exact coordinates. A "?" info button on the Location Presets screen explains all of this directly on the device.

**Naming presets:** when adding a preset, you can optionally give it a name (e.g. "Home"), shown in the overview instead of the raw coordinates. The naming screen shows example names and has its own special-character page, so names like "Zürich" or "São Paulo" can be typed correctly.

**Search by address:** when adding a preset ("+"), you can enter coordinates manually or tap "Search by address" to type a plain address (e.g. "Main Street 12, 12345 Springfield") - the device geocodes it via the free OpenStreetMap Nominatim service and shows the matching place to confirm before saving. If a place name is ambiguous (e.g. "Cambridge" exists in the UK, the US, Canada, and elsewhere), a short pick-list of the top matches (name and country) appears first instead of the device silently guessing. The address keyboard includes a format hint, punctuation keys (comma, hyphen, slash, period, apostrophe) so house numbers like "45/3" can be entered, and a dedicated special-character page (À Á Ä Â Ç É È Ê Ë Í Î Ñ Ó Ò Ô Ú Ù Ü Ş İ Ğ ß, etc.) for accented place names.

**Optional GPS module:** a "GPS" button next to "Auto" turns reading from a connected GPS module on/off (only has an effect when "Auto" is active). With GPS enabled and a module wired up, your location follows your movement live (e.g. while driving) instead of only being estimated via IP; without a connected module, the button simply has no effect. Wiring: GPS module TX to GPIO22, GPS module RX to GPIO27, 3.3V and GND, 9600 baud (NMEA protocol). See the "?" info button on the Location Presets screen for the same details on-device. Once the module has a fix, the radar screen's info bar also shows your live position (and altitude, as soon as the module reports one) right below the altitude legend - it only appears while GPS is enabled and has a fix, otherwise the info bar looks exactly as before.

**Nearest airport:** the Location Presets screen shows the nearest known airport (from a built-in, worldwide database of over 5,000 large and medium-sized airports) to whichever location is currently active - handy to see where a foreign preset points to, or why a location has heavy air traffic. This line is tappable (thin green border) - tapping it adds that airport directly as a new preset at its coordinates, using the local airport database on the SD card, with no internet lookup needed. The same nearest-airport distance and bearing also appears as a corner overlay on the radar screen itself (see [Radar corner overlays](#️-radar-display--visuals) above).

**Adjustable range:** the radar range cycles between 10/25/50/100 km via an on-screen button, applied consistently to the radar circle, range rings, and the underlying ADS-B query radius.

**Airport approach detection:** a best-effort hint in the aircraft detail panel ("Likely approaching `<code>` - ETA ~X min") appears when an aircraft's distance to the nearest airport is shrinking, it's descending, and its speed/altitude are in a plausible landing-approach range - derived purely from the same live position data already shown elsewhere, with no extra route lookups involved.

### 📊 Lists & Logbook

#### 📋 Aircraft List
**Menu → Flight Options → "Aircraft list"** shows every aircraft currently visible on the radar as a scrollable, sortable list instead of dots - a quick text overview instead of parsing the radar view. Shows callsign, altitude (color-coded the same way as the radar), and distance for each entry. A button at the top cycles the sort order through distance → altitude → callsign. Emergency squawks get a red border, watched aircraft (see [Watchlist](#-watchlist)) a cyan one, matching the radar's own markers. Tapping a row jumps straight back to the radar with that aircraft's detail panel open.

#### 📈 Statistics
Shows at a glance:
- **Aircraft logged today** - number of distinct aircraft first seen today
- **Aircraft logged all-time** - total across the entire runtime (all days combined)
- **Days with sightings** - how many days anything was logged at all
- **Average per day** - total count divided by number of days
- **Uptime** - how long the device has been running since the last restart
- **Highest flight today** - callsign and altitude of whichever aircraft was logged at the greatest altitude today (altitude at first sighting), e.g. "DLH441 (38000 ft)"

The **"Reset logbook data"** button (tap twice to confirm) permanently deletes all logged data.

#### 🛰️ Live Traffic
A "Live traffic" button right next to the Aircraft List opens a compact dashboard summarizing the current traffic at a glance: total number of aircraft in range, a breakdown by type (airliner, private jet, turboprop, unknown, helicopter, heavy - only categories that actually have at least one aircraft are shown), and the nearest, highest, lowest, and fastest aircraft currently visible, each with its callsign and value. Pure aggregation of the same live data already used for the radar and aircraft list - no extra network requests.

It's also reachable directly from the radar screen itself: a small, always-visible target icon in the bottom-left corner opens it with a single tap, without going through the menu at all.

#### 🏆 Most-Seen Aircraft
A "Top" button in the top-right of the Statistics screen opens a ranking of the 5 aircraft logged most often across the entire flight logbook (all days combined), showing each one's registration (or hex code if unknown) and total sighting count - handy for spotting the "regulars" that pass over again and again.

#### 📅 History Chart
Shows a rolling window of the last 7 calendar days (today and the 6 days before) as a simple bar chart, always exactly 7 individual, clearly separated bars - a quick visual complement to the plain number list in Logbook files. Days without any logged sightings still get their own labeled bar showing "0". Bar height is relative to the busiest day shown, with the exact count printed above each bar and the day-of-month below it. The underlying counts come from the SD-card-persisted logbook files, so the chart survives a device restart.

Below the chart, a "Peak traffic" line shows today's highest number of aircraft visible at the same time, along with the time it happened (e.g. "Today's peak: 23 aircraft at 14:32") - persisted across restarts, reset automatically at midnight, and tracked independently of the flight logbook switch, so it keeps working even with logging turned off. The line is only shown once a peak has actually been recorded for today.

A "?" info button on the History chart screen explains how it works directly on the device.

#### 📁 Logbook files
Lists the last several days from the logbook individually, with date and the number of aircraft logged that day - handy for tracing history over multiple days instead of only seeing the grand total. Each file has its own red "X" to delete it individually.

#### 📖 Flight logbook (ON/OFF)
When enabled, the device logs every newly sighted aircraft (timestamp, hex code, callsign, registration, type, distance, altitude) into a CSV file on the SD card. An aircraft is only logged once per activation, even if it crosses the radar multiple times.

Off by default, with a safety net: an unnoticed, permanently running logbook can quietly fill up the SD card and slow down the logbook menus, so turning it on shows a full-screen warning that has to be confirmed, and once on, it automatically switches off again after 24 hours - staying off after a restart unless it was actually turned on with a valid timestamp, so it can never silently keep running forever across power cycles or firmware updates. While it's on, a small countdown right on the logbook line ("Off in 18h 42min") shows how much time is left before this automatic shutoff. If the 24-hour safety shutoff kicks in, a popup lets you know right away - even if the device happened to be powered off exactly when the 24 hours ran out, the notice catches up on the next boot.

Each time you turn it on, a new CSV file is created for that session (e.g. `2026-08-06.csv`, or `2026-08-06_2.csv` for a second activation on the same day) instead of endlessly appending to one growing file. Turning it off saves SD card write cycles if you don't care about the statistics/history.

### 🌐 Web & Integrations

#### 🖥️ Web Export
A small built-in web page lets you view a live radar and export the flight logbook from any browser on the same WiFi network - handy for checking traffic from your phone or opening the logbook in Excel, Numbers, or Google Sheets. It starts automatically in the background as soon as the device connects to WiFi. The whole page - aircraft info popup, weather popup, connection status, install hint, logbook page, and list management - now follows the device's currently selected language (a language change takes effect the next time the page loads, just like the units setting).

**Live radar:** the top of the page shows a canvas radar with all currently visible aircraft, refreshing automatically every 8 seconds (simple polling, no app needed) - the same distance rings, aircraft-type silhouettes, altitude colors, marker shapes (ground vehicles, rotorcraft, Heavy aircraft), a military/government ring around detected flights, and full compass directions (N/E/S/W) as the device display, plus a full-page twinkling starfield background. A compact color legend next to the controls matches the markers' altitude colors, in either Metric or Imperial units. Rain and snow overlays now match the device pixel-for-pixel too - the same "glossy streak" raindrops, ice-crystal (dendrite) snowflakes, and intensity levels. The page follows whichever of the five color themes is currently selected on the device, updating live if you change it while the page is open, and automatically dims its colors at night in sync with the device's night dimming setting. A control area next to the range selector lets you switch the color theme, "Show Weather" (rain/snow effect), and military/government flight detection directly from the page - a change on either side (web or physical device) is picked up automatically on the other within a few seconds. A weather icon next to the Radar/Map tabs shows the current conditions at the active location - tapping it opens a popup with the METAR report (including a wind direction/speed line with a rotated arrow), sunrise/sunset time, and a short-term forecast. A "last updated" timestamp above the radar counts up live between refreshes, and a footer shows the firmware version, a live connection-status indicator, and a subtle watchlist/squawk badge whenever a currently visible aircraft is on your watchlist. Tapping or clicking an aircraft opens an info panel with an airline logo, callsign, registration, aircraft type, airline name, altitude, speed, climb/descent rate, distance, bearing, elevation angle, heading, distance trend, squawk, how long it's been visible, an approach/fly-by prediction when relevant, the session's closest distance and highest speed, and - if the callsign is known - a FlightAware tracking link. Altitude, speed, climb/descent rate, distances, and the radius selector automatically follow the device's Metric/Imperial units setting. The radius selector (10/25/50/100 km or nm) sets the device's actual range, the same as changing it on the device itself - a change on either side is picked up automatically on the other within a few seconds, so the web page and the device display always show the same range. A small, clickable logo in the bottom-right corner links to this project's GitHub page.

**Map view:** a "Map" tab alongside the radar circle shows a real OpenStreetMap view with the home location, every currently visible aircraft's position and heading, and a tap popup with its details. The zoom level fits itself to whichever aircraft are visible (or the current radar range, if the sky's empty) the first time the map opens, and stays exactly as left afterwards - manual zooming/panning is never reset by incoming data updates.

**Logbook:** below the radar, the page lists every logged day with its aircraft count, a download link for the merged CSV (all days combined), and per-day download/delete links. A search field filters the list by date, and clicking the "Date" or "Aircraft" column header sorts the table (click again to reverse the order). Delete and download buttons show immediate visual feedback ("Deleting…"/"Preparing…") while the request is in progress.

**How to use it:** while your computer or phone is on the same WiFi network as the device, open a browser and go to the device's IP address (e.g. `http://192.168.1.42/`). **Menu → System → "Logbook / WebUI"** shows the current IP directly on the device (the "?" info button on the Logbook files screen shows it too) along with a QR code for the page's URL, so you can open it on your phone without typing the IP by hand.

**Install as an app:** the page can be installed on your phone's home screen as its own app, with its own icon and a full-screen view without the browser's address bar - on Android/Chrome via the browser's "Add to Home screen" option, on iOS/Safari via the Share sheet's "Add to Home Screen". The install icon and theme color follow the device's currently selected color theme.

#### 🏠 MQTT / Home Assistant
An optional MQTT connection (**Menu → System → Tools → "MQTT"**, off by default) feeds a few live radar values into your own smart-home system, such as Home Assistant. Turn it on, enter your broker's address (host and port, e.g. `192.168.1.10:1883`), and optionally a username/password if your broker requires authentication - public test brokers without a login work too.

Once connected, the device regularly publishes the number of aircraft currently in range, the proximity-alert and watchlist-alert status, the WiFi signal strength, and the running firmware version, all under a shared `eiswolfs-flightradar/` topic prefix. It also publishes the distance of the nearest aircraft, the altitude of the highest and lowest aircraft, the speed of the fastest aircraft, the number of helicopters and Heavy aircraft currently in range, and whether a military/government flight or an emergency squawk is currently detected.

**Home Assistant auto-discovery:** if you're using Home Assistant, no manual configuration is needed - the device announces all of the above as sensors via the standard MQTT Discovery mechanism as soon as it connects, and they appear automatically as a single grouped device ("Eiswolfs Flightradar") on your dashboard. A proper MQTT Last-Will-and-Testament makes sure Home Assistant correctly shows the device as unavailable if the connection ever drops unexpectedly (e.g. power loss), not just when MQTT is turned off on purpose.

### ⚙️ System & Settings

- **Languages** - 8 languages (English, German, French, Turkish, Spanish, Italian, Brazilian Portuguese, Dutch), selectable on first boot or anytime from the menu.
- **Metric/Imperial units** (**Menu → Region → "Units"**) - applied consistently across the radar range button, range ring labels, aircraft list, nearest-airport distance, and stats.
- **IATA or ICAO airport codes** (same "Units" screen, or tap the route line in the aircraft detail panel) - chooses whether the route shown in the detail panel uses 3-letter IATA codes (e.g. "FRA", the default) or 4-letter ICAO codes (e.g. "EDDF"), falling back to ICAO automatically if no IATA code is available.
- **Adjustable brightness** (10-100% in 10% steps, **Menu → System → "Brightness"**) with a live preview as you tap, plus an optional Auto-Brightness toggle on the same screen that uses the device's built-in light sensor, smoothed to avoid flickering.
- **Backup & Reset** (**Menu → System → "Backup & Reset"**) - back up or restore your settings, or fully reset the device (wipes all on-device data and re-runs first-time setup) - handy for testing or handing the device to someone else.
- **Screen timeout with slider** (**Menu → System → "Screen Timeout"**) - sets how long until the display turns off after a period of no touches, from 1 to 15 minutes or "Never", using a drag slider instead of tapping through values one minute at a time.
- **Menu timeout with slider** (**Menu → System → Display → "Menu Timeout"**) - sets how long an open menu or settings screen waits without a touch before automatically jumping back to the radar screen, from 30 seconds to 5 minutes or "Never" (was previously fixed at 2 minutes).
- **Rotate screen (180°)** (**Menu → System → Display**) - flips the screen and touch input upside down, useful for table-mount setups where the panel's limited vertical viewing angle would otherwise wash out the radar circle when viewed from above.
- **WiFi Manager** - see below.
- **OTA firmware updates over WiFi** - see below.
- **Weather Icon** - see below.
- **Connection status** (**Menu → System → Tools → "Connection status"**) - a diagnostic screen showing the number of currently tracked aircraft, time since the last successful ADS-B data fetch, the result of the last fetch attempt (success, timeout, or error code), and a simple green/yellow/red status derived from how stale the data currently is. Updates live while the screen is open.
- **Menu structure** - the Flight Options menu is grouped into category buttons: Lists (Aircraft List, Watchlist), Stats & Logbook, LED Alerts, Display Filters (airline filter, ground vehicles, helicopters only, low-altitude only), and a direct Location Presets button. The System menu has its own Display/Tools categories.
- **Quick header shortcuts** - tapping the WiFi signal bars in the header jumps to WiFi settings, tapping the small clock jumps to screen-timeout settings, and a small eye icon between "Menu" and the WiFi bars jumps to the Display Filters menu - the entire header is interactive. A QR code linking to this project's GitHub page is available under **Menu → System → Tools → "About"**.
- **Firmware version** is shown directly on the "Check for updates" button (**Menu → System**).
- **Dual-core design** - all networking (WiFi, ADS-B polling, aircraft detail lookups) runs on Core 0, while the display and touch input run on Core 1, so the UI never freezes during a network request.
- Data (airline names, aircraft-type seat estimates) is loaded from CSV files on the SD card, auto-seeded on first boot.

#### 📶 WiFi Manager (up to 3 saved networks)
Under **Menu → WiFi/Network** you can save up to 3 WiFi networks at once. On boot, the device automatically connects to whichever one it can currently see.

This is useful for portable use (take the device to the office, a friend's place, or a holiday home, and it connects automatically everywhere), for a guest network plus a main network (many routers offer both - save both, and the device uses whichever is reachable), or for multiple access points/repeaters around the house (the device connects to whichever saved network it can currently reach).

Any saved network can be removed again at any time via the red "X", freeing up a slot for a new one.

**Hidden networks:** if your WiFi network doesn't broadcast its name, it won't show up in the scan list. An "Other/Hidden SSID" button on that same list lets you type the network name manually using the same on-screen keyboard used for passwords, then continue straight to the password step as usual - available both during first-time setup and when adding a network later.

#### 🔄 OTA firmware updates over WiFi
**Menu → System → "Check for update"** checks the latest GitHub release and, if newer, downloads and installs it directly on the device - no cable or web flasher needed. Update results are shown as a clear on-screen message, and a short, scrollable changelog of what changed in that version. On success, the device restarts automatically after a short pause. While installing, a filling progress bar shows how far along the download/flash is, alongside a reminder not to unplug or turn off the device until it's done.

The device also quietly checks for new firmware every few minutes in the background; a small red dot (like an app badge) appears on the Menu button, the System tile, the "Check for update" button, and the sleep screen when a new version is available. Installing always requires explicit confirmation - nothing happens automatically.

#### ☀️ Weather Icon
A small icon in the header shows the current weather conditions - sun, cloud, sun-behind-cloud, rain, snow, or thunderstorm - drawn entirely with simple shapes, no image files or extra fonts needed. It always follows whichever location is currently active, including active [location presets](#-location--range) - switching the active preset to e.g. Milan or Tokyo updates the icon to that location's weather.

Powered by the free [Open-Meteo](https://open-meteo.com) API (no API key needed), refreshed automatically every 5 minutes or immediately after switching location.

Tapping the icon opens a small info popup explaining that the shown weather reflects the currently active location. The same popup shows the raw METAR text for the nearest airport (from the free [aviationweather.gov](https://aviationweather.gov) data API), fetched in the same background cycle as the icon weather, today's sunrise and sunset time for the active location, and a short forecast (temperature and conditions) 3 hours ahead. Wind direction and speed, parsed straight out of the same METAR text, are shown as a compass direction and speed (following the Metric/Imperial units setting), with calm and variable wind handled specially; the web page additionally draws a small rotated arrow. The popup scrolls automatically if all of this doesn't fit on one screen.

---

**Technical notes for the curious:**
- Flight data is re-fetched from [adsb.lol](https://adsb.lol) every **8 seconds**.
- Altitude color coding is fixed: **green** < 10,000 ft, **yellow** 10,000–30,000 ft, **red** > 30,000 ft.
- Radar radius is switchable between **10 / 25 / 50 / 100 km**.

## Hardware
- ESP32-2432S028 ("Cheap Yellow Display", CYD) – ILI9341 2.8" 240x320 touch TFT
- microSD card (FAT32) for settings, WiFi credentials, and lookup tables
- No GPS, no speaker required – uses IP geolocation and the onboard RGB LED; an optional speaker wired to the SPK connector adds a physical emergency-squawk alarm tone (see [Web alert sound](#-alerts--leds))

### Pinout used
| Function | Pins |
|---|---|
| TFT (VSPI) | MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, BL=21 |
| Touch (XPT2046) | CLK=25, CS=33, MOSI=32, MISO=39, IRQ=36 |
| microSD (HSPI) | CLK=18, MISO=19, MOSI=23, CS=5 |
| RGB LED (active-low) | R=4, G=16, B=17 |
| Speaker (optional, SPK connector) | GPIO26 |

## Troubleshooting

**SD card not detected:** the most common cause is a card formatted with an operating system's default formatter, which can create a partition layout the simple Arduino SD library can't read. Reformat the card with the official [SD Card Formatter](https://www.sdcard.org/downloads/formatter/) from the SD Association - this resolves most cases.

If that doesn't help, the card itself may be incompatible or unreliable with this setup - try a different card, ideally an older/smaller one from a well-known brand.

If problems persist, also double-check the pinout table above, especially if you're using a different or unofficial CYD clone board.

## Data sources
- Aircraft positions: [adsb.lol](https://adsb.lol) (free, no API key)
- Aircraft model lookups: [hexdb.io](https://hexdb.io) (free, community-maintained, rate-limited)
- Location: [ip-api.com](https://ip-api.com) (free IP geolocation)

## Disclaimer
Aircraft model and seat-count data are estimates from community databases and
local lookup tables, not live/official figures. This project is for hobby use
and is not intended for navigation or safety-critical purposes.

## License
MIT (or add your preferred license here)
