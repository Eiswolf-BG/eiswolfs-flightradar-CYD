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
## Standard-Workflow: Push & Release

WICHTIG - wann dieser Workflow startet: Der komplette Release-Workflow
(README-Update, Commit, Tag, Push) darf NUR gestartet werden, wenn Alex
EXPLIZIT danach fragt (z.B. "lass pushen", "können wir releasen", "mach den
Release-Workflow"). Ein einfaches "ja" auf eine Rückfrage (z.B. zu einer
CLAUDE.md-Änderung oder einem anderen Detail) ist KEINE Aufforderung, den
Release-Workflow zu starten. Bei kleineren Fixes/Änderungen bitte NUR bauen
und flashen (siehe Abschnitt "Nach jedem erfolgreichen Build automatisch
flashen" unten), aber NICHT committen/taggen/pushen, bis ausdrücklich danach
gefragt wird.

Sobald der Workflow explizit angefordert wurde, automatisch folgende Schritte
in dieser Reihenfolge:

0. Versionsnummer festlegen: Die neue Versionsnummer kommt IMMER exakt von
   Alex - er nennt sie im Push-Wunsch (z.B. über Claude/den Sandbox-
   Assistenten: "Alex will auf Version X.Y.Z pushen"). Karl trägt GENAU
   diese Nummer in `Config::APP_VERSION` (`src/config.h`) ein - niemals
   selbst hochzählen, erraten oder von der letzten Version ableiten (auch
   nicht bei kleinen Patches). Das gilt auch für größere Sprünge (z.B.
   2.6 -> 3.0), die Alex bewusst und absichtlich machen kann - Karl
   übernimmt in jedem Fall die genannte Nummer 1:1, ohne eigene Annahmen.
   Falls im Push-Wunsch keine explizite Versionsnummer genannt wurde, bei
   Alex nachfragen statt zu raten. Erst NACH dem Eintragen der korrekten
   Nummer: einmal sauber `pio run` bauen, DANACH erst der Rest des
   bekannten Workflows (README, Tag, index.html, Bin-Dateien, Commit/Push).
1. Prüfen, ob seit dem letzten Commit neue/geänderte Features hinzugekommen
   sind, die für Endnutzer sichtbar sind (neue Menüpunkte, geändertes
   Verhalten, neue Screens) - falls ja, **README.md entsprechend ergänzen**
   (gleicher Stil: Emoji-Überschriften, Ankerlinks zwischen "Features"-Liste
   und den Deep-Dive-Sektionen, kurze Beispiele wo sinnvoll). Reine interne
   Bugfixes/Refactorings ohne sichtbare Nutzerauswirkung brauchen keinen
   README-Eintrag.
2. Code committen (aussagekräftige Commit-Message).
3. Falls es sich um einen Versionssprung handelt: Git-Tag mit
   Versionsnummer + Beschreibung der Änderungen erstellen.
4. Prüfen, ob `index.html` (Web-Flasher) noch die alte Versionsnummer zeigt -
   falls ja, aktualisieren. **NICHT OPTIONAL, darf bei KEINEM Release-Push
   übersprungen werden** - auch nicht bei kleinen Patch-Versionen. Immer als
   fester Doppel-Schritt zusammen mit Schritt 1 (README) behandeln: wann
   immer die README (oder auch nur die Versionsnummer) auf eine neue Version
   aktualisiert wird, IMMER im selben Zug auch `index.html` prüfen und
   synchron mitziehen.
5. Prüfen, ob `bootloader.bin`, `firmware.bin`, `partitions.bin` im
   Hauptverzeichnis dem aktuellen Build in `.pio/build/esp32dev/`
   entsprechen - falls nicht, von dort kopieren.
6. Alle diese Änderungen (Code + README + Web-Flasher-Dateien) zusammen
   committen und pushen (`git push`, plus `git push origin vX.Y.Z` falls ein
   Tag erstellt wurde).
7. ERST DANACH (nach Schritt 5, wenn bootloader/firmware/partitions bereits
   ins Hauptverzeichnis kopiert sind): die `firmware.bin` im
   `.pio/build/esp32dev/`-Ordner zusätzlich zu `CYD-flightradar.bin`
   umbenennen (bzw. kopieren) - das ist die Datei für den GitHub-Release-
   Upload. WICHTIG: diese Umbenennung darf NICHT vor dem Auto-Flash (siehe
   unten) passieren, sonst schlägt `pio run --target upload` fehl, weil es
   die Datei unter dem Namen `firmware.bin` erwartet.

   SICHERHEITSPROBLEM bei dieser Umbenennung: Falls im
   `.pio/build/esp32dev/`-Ordner bereits eine `CYD-flightradar.bin` von
   einem vorherigen Release liegt (weil sie nach dem letzten Release aus
   irgendeinem Grund nicht geloescht wurde), darf das Umbenennen NICHT
   einfach übersprungen werden (z.B. weil ein `mv`/Kopiervorgang auf eine
   bereits existierende Zieldatei fehlschlägt oder stillschweigend nichts
   tut) - sonst würde beim GitHub-Release versehentlich die ALTE,
   veraltete `CYD-flightradar.bin` hochgeladen, obwohl der frisch gebaute
   Code neuer ist. Das ist ein ernsthaftes Risiko (veraltete Firmware wird
   veröffentlicht, ohne dass es auffällt). Deshalb bei JEDEM Release als
   expliziter, nicht überspringbarer Schritt:
   a) Prüfen, ob im `.pio/build/esp32dev/`-Ordner bereits eine Datei
      namens `CYD-flightradar.bin` existiert.
   b) Falls ja: diese alte Datei IMMER ZUERST LÖSCHEN, bevor die aktuelle
      `firmware.bin` umbenannt wird (nicht überspringen, nicht
      stillschweigend stehen lassen).
   c) Danach die aktuelle `firmware.bin` zu `CYD-flightradar.bin`
      umbenennen (wie oben beschrieben).
   d) Am Ende der Release-Zusammenfassung IMMER explizit erwähnen, ob eine
      alte `CYD-flightradar.bin` gefunden und gelöscht wurde.
8. GitHub Release erstellen UND die `.bin`-Datei in einem Schritt hochladen
   (per `gh` CLI, seit v2.7.5 eingerichtet und authentifiziert - siehe
   `gh auth status`):

       gh release create vX.Y.Z .pio/build/esp32dev/CYD-flightradar.bin \
           --repo Eiswolf-BG/eiswolfs-flightradar-CYD \
           --title "vX.Y.Z" \
           --notes "<Release-Notes-Text>"

   Der Release-Notes-Text kommt aus dem jeweiligen Push-Wunsch (derselbe
   Text, der auch für die Tag-Message verwendet wird) - falls im
   Push-Wunsch kein Text mitgegeben wurde, aus dem `git log` seit dem
   letzten Tag ableiten, wie bisher auch für die Tag-Message.
9. Nach erfolgreichem Upload: die lokale Kopie
   `.pio/build/esp32dev/CYD-flightradar.bin` wieder löschen - reines
   Build-Artefakt fürs Hochladen, gehört nicht ins Repo und wird nicht
   committet.
10. Kurze Zusammenfassung am Ende: was committet/getaggt/gepusht wurde, ob
    die README aktualisiert wurde (und falls ja, welche Abschnitte), sowie
    die URL des erstellten GitHub Release. Der GitHub-Release-Schritt ist
    damit vollautomatisch - kein manuelles Nacharbeiten von Alex mehr
    nötig, außer `gh` sollte einmal die Authentifizierung verlieren (dann
    erneut `gh auth login`, siehe oben).

## Nach jedem erfolgreichen Build automatisch flashen

Sobald `pio run` (Build) erfolgreich ohne Fehler durchgelaufen ist, IMMER direkt
im Anschluss auch flashen (`pio run --target upload`), ohne extra danach zu
fragen - außer der Nutzer sagt ausdrücklich "nur bauen, nicht flashen" o.ä.
Kurz danach bestätigen, dass der Upload ebenfalls erfolgreich war (inkl.
"[SUCCESS]"-Zeile am Ende).

Bitte diese Regel jetzt in die CLAUDE.md-Datei einpflegen.