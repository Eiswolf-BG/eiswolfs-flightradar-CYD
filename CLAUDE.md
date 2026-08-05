# Eiswolfs Flightradar (CYD) — Projektkontext für Claude Code

Lies diese Datei ZUERST, bevor du irgendwas am Code änderst. Sie enthält die
Dinge, die man wissen muss, um nicht dieselben Fehler nochmal zu machen, die
in der Entwicklung schon mal aufgetreten und behoben wurden.

## Was ist das Projekt?

Ein Live-ADS-B-Flugradar auf einem ESP32 "Cheap Yellow Display" (CYD,
ESP32-2432S028), 240x320 Touch-TFT. Zeigt Flugzeuge in der Nähe auf einem
rotierenden Radarschirm, mit Detail-Panel (Modell, Höhe, Speed, Route,
Squawk), Näherungs-LED-Alarm, WLAN-Verwaltung (bis zu 3 Netzwerke),
Standort-Presets (auch für fremde Orte weltweit), Airline-Filter, Flugbuch
mit Statistik, und Menüs/Splash-Screen mit einer dezenten twinkelnden
Sterne-Animation.

Aktueller Stand: **v2.3.2**. Öffentliches Repo:
https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD

## Wer nutzt das?

Alex — kompletter Anfänger bei Arduino/Embedded-Entwicklung, arbeitet mit
VS Code + PlatformIO auf einem Mac. Bitte auf Deutsch antworten. Erklärungen
gerne etwas ausführlicher, nicht von Fachbegriffen ausgehen, die als bekannt
vorausgesetzt werden.

## Tech-Stack

- PlatformIO, `platform = espressif32`, `board = esp32dev`, `framework = arduino`
- TFT_eSPI (Display), XPT2046_Touchscreen (Touch), ArduinoJson, TinyGPSPlus
- Dual-Core-Design: Netzwerk (WLAN, ADS-B-Polling) auf Core 0, Display/Touch
  auf Core 1 — die UI blockiert nie durch Netzwerk-Requests.
- Daten: [adsb.fi](https://adsb.fi) (Flugzeugpositionen), [hexdb.io](https://hexdb.io)
  (Modell-Lookups), [ip-api.com](https://ip-api.com) (IP-Geolocation)

## ⚠️ WICHTIGSTE FALLE: Der eigene Font ist grundlinien-verankert

Die eingebauten TFT_eSPI-Fonts (GLCD, Font2 etc.) sind reines ASCII. Für
Umlaute/Akzente (Deutsch, Türkisch, Französisch, Spanisch, Italienisch)
gibt's einen **selbst generierten Font** (`src/ui_font.h`, via
`tft.setFreeFont(&UiFont11pt)` global in `main.cpp::setup()` aktiviert, gilt
danach für JEDEN `print()`/`drawString()`-Aufruf in der ganzen App).

**Der entscheidende Unterschied zu den eingebauten Fonts:** Bei
`setCursor(x, y); print(...)` ist `y` bei unserem Font die **Grundlinie**
(Baseline), NICHT die obere Kante wie beim alten GLCD-Font. Der Text wächst
von `y` aus nach OBEN (um den Ascent, ca. 9px bei Size 1, ca. 16-18px bei
Size 2), nicht nach unten.

**Das hat in der Vergangenheit zu folgenden Bugs geführt (alle behoben, aber
Vorsicht bei neuem Code!):**
- Zu kleine y-Werte (z.B. `setCursor(10, 2)`) → Text ragt oben aus dem
  Bildschirm/Container heraus oder wird abgeschnitten. **Faustregel:
  y sollte bei Size 1 nie kleiner als ~14 sein, bei Size 2 nie kleiner als
  ~24-26 (abhängig vom Container).**
- Eingabefelder/Boxen: Baseline muss nahe der UNTERKANTE der Box liegen,
  nicht in der Mitte wie man's vom alten Font gewohnt wäre.
- Zwei Textgrößen kurz hintereinander in einem eng bemessenen Layout
  (Label Size 1 direkt über Wert Size 2) sind fehleranfällig — im
  Statistik-Screen deshalb bewusst auf EINHEITLICHE Größe (nur Farbe
  unterscheidet Label/Wert) umgestellt, das ist robuster.
- `drawString()` mit `setTextDatum(MC_DATUM)` (zentriert) ist NICHT
  betroffen — TFT_eSPI rechnet die Zentrierung selbst korrekt aus, egal ob
  Baseline- oder Top-verankert. Nur rohes `setCursor()`+`print()` ist die
  Gefahrenzone.
- Langer, mehrzeiliger Text (z.B. Erklärtexte) NIE mit TFT_eSPI's
  eingebautem Auto-Wrap verlassen — das bricht mitten im Wort ab. Stattdessen
  den `layoutWrapped()`-Helper verwenden (in `location_presets_screen.cpp`
  und `wifi_manage_screen.cpp` je einmal implementiert, macht wortweisen
  Umbruch anhand echter Pixel-Breite plus optionales Scrollen).

Falls neue Screens/Textstellen dazukommen: lieber einmal mehr testen (idealerweise
mit Foto vom echten Display), bevor der Code als fertig gilt.

## i18n (6 Sprachen)

- `src/i18n.h`: `enum class StringId` — jeder feste UI-Text hat eine ID.
- `src/i18n_en.h`, `i18n_de.h`, `i18n_fr.h`, `i18n_tr.h`, `i18n_es.h`,
  `i18n_it.h`: je ein `static const char* const[]`-Array, in **exakt
  derselben Reihenfolge** wie das Enum.
- Jede Datei hat am Ende einen `static_assert`, der die Array-Größe gegen
  `StringId::COUNT` prüft — **wenn der Build wegen eines fehlschlagenden
  static_assert bricht, fehlt in mindestens einer Sprachdatei ein Eintrag
  oder es ist einer zu viel.** Neue StringId → in ALLEN 6 Dateien an
  derselben Position ergänzen, sonst verschiebt sich die Zuordnung.
- Eigennamen der Sprachen (`I18n::languageName()`) sind separat in
  `i18n.cpp` hinterlegt, mit korrekten landessprachlichen Sonderzeichen
  (z.B. "Français", "Türkçe", "Español").

## UI-Konventionen (bitte einhalten für neue Screens)

- **Farbschema:** Schwarzer Hintergrund, grüner Rahmen/Text
  (`TFT_BLACK`/`TFT_GREEN`), aktive/ausgewählte Einträge invertiert (grün
  gefüllt, schwarzer Text). Destruktive Aktionen (Abbrechen/Löschen) in Rot
  (`TFT_RED`).
- Jeder Screen hat i.d.R. eine lokale `struct Rect` mit `contains(x,y)` und
  eine `drawButton()`-Hilfsfunktion (Copy-Paste-Muster aus den bestehenden
  Screens, kein gemeinsames Rect/Button-Modul — das ist bewusst so, um
  jeden Screen unabhängig lauffähig zu halten).
- **Sterne-Animation** (`src/menu_stars.h/.cpp`): Läuft im Hintergrund auf
  JEDEM schwarzen Menü-/Splash-Screen. Neue Screens sollten
  `MenuStars::reset()` einmal beim Betreten aufrufen und
  `MenuStars::update(tft)` in jeder Warte-/Idle-Schleife (Loop läuft sonst
  ungenutzt, da die Funktion sich intern selbst auf ~60ms drosselt).
- Warteschleifen-Pattern für Touch-Eingabe:
  ```cpp
  TouchInput::Point tap;
  while (true) {
      if (TouchInput::wasTapped(tap)) break;
      MenuStars::update(tft);
      delay(20);
  }
  ```
- Menüstruktur: Hauptmenü → 4 Kategorien (Land/Region, WLAN/Netzwerk,
  System, Flugoptionen) → jeweils Unterseiten. WLAN/Netzwerk hat aktuell
  KEIN eigenes Untermenü mehr, springt direkt in die Netzwerk-Verwaltung.
- Viele Screens mit Text-Eingabe (Airline-Code, Koordinaten, WLAN-Passwort)
  haben einen "?"-Info-Button oben rechts, der einen scrollbaren
  Erklär-Screen öffnet (Muster: `location_presets_screen.cpp` und
  `wifi_manage_screen.cpp`, jeweils eigene `layoutWrapped()`-Kopie).

## Sonstige feste Werte (Config::…)

- Näherungsalarm-Radius: 3 km
- Notfall-Squawks: 7500 (Entführung), 7600 (Funkausfall), 7700 (Notfall)
- Radar-Reichweiten: 10/25/50/100 km
- ADS-B-Abruf-Intervall: alle 8 Sekunden
- Höhen-Farbcodierung: Grün <10.000ft, Gelb 10-30.000ft, Rot >30.000ft
- Max. 3 WLAN-Netzwerke, max. 3 Standort-Presets, max. 10 gefilterte Airlines

## Code-Stil

- Kommentare durchgehend auf Deutsch, ohne Umlaute in Kommentaren selbst
  unüblich (ae/oe/ue sind hier ok, das ist nur ein Stil-Ding für
  Kommentare, NICHT für die UI-Texte in den i18n-Dateien — dort echte
  Umlaute verwenden, siehe oben).
- Build-Check nach JEDER Änderung: `pio run` im Projektverzeichnis, auf
  Warnungen UND Errors prüfen (nicht nur "compiles").
- Immer least-invasive Änderungen bevorzugen — bestehende Funktionen/Namen
  nicht ohne Grund umbenennen.

## Bekannte offene Punkte / mögliche nächste Schritte

Keine akuten offenen Bugs bekannt (Stand v2.3.2). Mögliche Ideen für später,
falls der Nutzer danach fragt: weitere Menüs mit Info-Buttons versehen,
ggf. weitere Sprachen, ggf. Export/Teilen der Logbuch-Daten.
