
Claude hat die Antwort abgeschlossen
Meinst du dein Repository (also eiswolfs-flightradar-CYD) öffentlich machen? Das ist wahrscheinlich, was gerade fehlt, damit ich per Web-Suche/Fetch drauf zugreifen kann. So geht's:

Gehe zu deinem Repo: https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD
Oben im Reiter Settings (ganz rechts in der Menüleiste, Zahnrad-Symbol)
Ganz nach unten scrollen zum Bereich "Danger Zone"
Dort "Change repository visibility" klicken
"Change visibility" → "Make public" wählen
GitHub fragt zur Sicherheit nochmal nach – Repo-Namen zur Bestätigung eintippen, dann bestätigen
Danach ist es öffentlich einsehbar (jeder kann den Code sehen und klonen, aber nur du kannst pushen, solange du keine Mitwirkenden hinzufügst).

Falls du stattdessen dein GitHub-Benutzerprofil meinst: Das ist eigentlich immer öffentlich sichtbar (Profilseite mit Namen, Repos-Übersicht etc.) – da gibt's keine Privat/Öffentlich-Einstellung wie bei Repos. Falls du dort etwas Bestimmtes ausblenden willst (z.B. private Aktivität, E-Mail-Adresse), sag Bescheid, das ist ein anderer Weg (Settings → Profile bzw. Emails).

ist schon auf public warum kannst du nicht zugreifen?

Guter Punkt – das liegt nicht an der Sichtbarkeit, sondern an einer technischen Einschränkung meinerseits: Ich darf nur URLs abrufen, die vorher in einem Suchergebnis aufgetaucht sind, und meine Suchen haben dein Repo bisher einfach nicht gefunden (wahrscheinlich weil es noch zu neu ist, um von Google/Bing indexiert zu sein). Lass mich nochmal gezielter suchen:


Web durchsucht
Web durchsucht
Nichts Passendes – dein Repo ist einfach zu neu, um von den Suchmaschinen indexiert zu sein (das dauert oft Tage bis Wochen, unabhängig von "public"/"private"). Meine Web-Suche findet nur, was schon irgendwo indexiert wurde, nicht direkt auf GitHub selbst durchsuchbar.

Am einfachsten für uns beide: Schick mir den README.md-Inhalt einfach direkt hier (copy-paste oder als Datei hochladen) – dann muss ich gar nicht erst suchen und wir sind schneller fertig. Willst du das machen?

wo find ich die?

Die README.md liegt direkt im Hauptverzeichnis deines Projekts, gleich neben platformio.ini:

In VS Code:

Explorer-Ansicht links öffnen (falls nicht schon offen)
Im Ordner CYD Flightradar (oberste Ebene, nicht in src/) nach README.md suchen
Anklicken zum Öffnen
Falls sie dort nicht existiert: Dann gibt es noch keine README für dieses Projekt – das ist kein Fehler, viele Projekte fangen ohne README an. In dem Fall auf GitHub selbst nachschauen:

Gehe zu https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD
Falls dort auf der Hauptseite unterhalb der Dateiliste ein Textblock mit Projektbeschreibung angezeigt wird → die kommt aus der README, du kannst sie direkt anklicken/bearbeiten (Stift-Symbol) und den Inhalt kopieren
Falls dort gar nichts angezeigt wird und auch keine README.md in der Dateiliste auftaucht → gibt's noch keine, dann leg ich dir einfach eine komplette neue an
Schau kurz nach und sag mir, was du findest (oder schick mir gleich den Inhalt), dann mach ich weiter.


Nachricht eingeklappt
Danke, jetzt hab ich alles. Ich ergänze die Features-Liste um die beiden Punkte und füge eine neue, ausführliche Sektion mit Beispielen hinzu. Hier die komplette Datei zum Ersetzen:

markdown
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
- **Touch-driven WiFi setup** and on-screen keyboard for first-time configuration, with up to 3 saved networks (see [WiFi Manager](#-wifi-manager-up-to-3-saved-networks) below)
- **Location presets** – save up to 3 fixed locations, or point the radar at any place in the world (see [Location Presets](#-location-presets) below)
- **IP-based geolocation** (no GPS module needed) to center the radar on your location
- **Proximity LED alert** – the onboard RGB LED blinks green when an aircraft is within 3 km (no speaker on this board, so this replaces an audible alert)
- **Dual-core design** – all networking (WiFi, ADS-B polling, aircraft detail lookups) runs on Core 0, while the display and touch input run on Core 1, so the UI never freezes during a network request
- Data (airline names, aircraft-type seat estimates) loaded from CSV files on the SD card, auto-seeded on first boot
- **6 languages** (English, German, French, Turkish, Spanish, Italian), selectable on first boot or anytime from the menu

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
Einfach die komplette README.md damit ersetzen. Die zwei neuen Bullet-Points oben verlinken direkt zu den ausführlichen Erklärungen weiter unten (funktioniert automatisch als Sprungmarken auf GitHub). Sag Bescheid, falls du's lieber auf Deutsch hättest, oder wenn noch andere Features (z.B. Airline-Filter, Statistik, Logbuch) auch so eine Deep-Dive-Sektion verdienen!






Claude ist eine KI und kann Fehler machen. Bitte überprüfe die Antworten.






































