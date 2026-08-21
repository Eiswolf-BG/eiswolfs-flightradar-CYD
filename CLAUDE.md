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

Aktuelle Versionsnummer: siehe `Config::APP_VERSION` in `src/config.h`
(dort immer aktuell, hier bewusst nicht mehr hart eingetragen, damit diese
Datei nicht wieder veraltet). Öffentliches Repo:
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
- Menüstruktur (Stand v4.0.0): Hauptmenü → 4 Kategorien (Land/Region,
  WLAN/Netzwerk, System, Flugoptionen). WLAN/Netzwerk hat weiterhin KEIN
  eigenes Untermenü, springt direkt in die Netzwerk-Verwaltung.

  System → 3 Kategorie-Buttons + Zurück:
    - Anzeige: Helligkeit, Bildschirm-Timeout, Nachtmodus, Invertieren,
      Radar-Farbschema, Zurück
    - Werkzeuge: Kalibrierung, Web-Livekarte, Sicherung & Reset (eigenes
      Untermenü: Sichern, Wiederherstellen, Werksreset, Zurück), Zurück
    - Nach Update suchen (direkte Aktion, KEIN Untermenü - bewusst so,
      bleibt immer sofort sichtbar mit Version + rotem Update-Punkt und
      darf bei künftigen Umbauten nicht vergraben werden)
    - Zurück

  Flugoptionen → 6 Buttons + Zurück:
    - Flugzeugliste (direkt)
    - Beobachtungsliste (direkt)
    - Statistik & Logbuch: Statistiken, Statistik-Verlauf, Logbuch-Dateien,
      Flugbuch an/aus, Zurück
    - LED-Alarme: Heartbeat, Notfall-Alarm, Näherungs-LED, Zurück
    - Tools: Standort-Presets, Airline-Filter, Bodenfahrzeuge ausblenden,
      Nur Helikopter anzeigen, Beobachtungsalarm, Zurück
    - Zurück

  System und Flugoptionen sind damit selbst auch Kategorie-Seiten
  (gleiches Bild-Prinzip wie das Hauptmenü), keine flachen Listen mehr.
  Alle Unterseiten mit mehr als 3 Eintraegen nutzen dafuer
  `subMenuRowRect(index, count)` in menu_screen.cpp statt fester
  Zeilenhoehen-Konstanten pro Seite (ersetzt die fruehere FLIGHT_ROW_H/
  SYSTEM_ROW_H/BACKUP_RESET_ROW_H-Wiederholung).
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

## Sprache: Projekt-Außendarstellung immer Englisch

Alle nach außen sichtbaren Texte sind IMMER auf Englisch zu verfassen —
unabhängig davon, in welcher Sprache die Unterhaltung mit Alex geführt
wird. Das betrifft insbesondere:
- `README.md`
- `index.html` (Webseite/Flasher-Seite)
- GitHub-Release-Notes / -Beschreibungen
- Commit-Messages
- Jeglicher sonstiger Beschreibungs- oder Bugfix-Text, der öffentlich
  sichtbar ist (z.B. auf GitHub)

Ausnahme: Die Firmware-UI selbst bleibt mehrsprachig wie gehabt
(`i18n_de/en/fr/tr/es/it.h`) — diese Regel betrifft NUR die
Projekt-Außendarstellung (Repo, Release Notes, Webseite), nicht die
App-Oberfläche auf dem Gerät. Interne Code-Kommentare bleiben ebenfalls
wie gehabt auf Deutsch (siehe „Code-Stil" oben) — diese Regel gilt nur
für nach außen sichtbare Texte.

## Bekannte offene Punkte / mögliche nächste Schritte

Keine feste Liste hier gepflegt, da sie erfahrungsgemäß schnell veraltet -
offene Ideen/Bugs bitte direkt als GitHub Issues im Repo tracken statt hier
in der CLAUDE.md.
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
   `Config::APP_VERSION` ist die EINZIGE Stelle im Code, die pro Release
   gepflegt werden muss - sie erscheint automatisch auf dem "Nach Update
   suchen"-Button im System-Menü (seit der frühere separate Info-Screen,
   `src/about_screen.cpp`/`.h`, entfernt wurde; diese Dateien existieren
   nicht mehr und dürfen bei künftigen Releases nicht mehr gesucht werden).
   Falls im Push-Wunsch keine explizite Versionsnummer genannt wurde, bei
   Alex nachfragen statt zu raten. Zusammen mit `APP_VERSION` IMMER auch
   den Changelog fuer DIESES Release aktualisieren - der ist MEHRSPRACHIG
   (alle 6 Sprachen wie der Rest der Geraete-UI), liegt in
   `src/changelog.cpp` als sechs Konstanten (`CHANGELOG_EN`, `CHANGELOG_DE`,
   `CHANGELOG_FR`, `CHANGELOG_TR`, `CHANGELOG_ES`, `CHANGELOG_IT`, jeweils
   eine kurze Bullet-Liste), ausgewaehlt ueber `changelogLatest()`
   (deklariert in `src/changelog.h`) nach `SettingsStore::language()`. ALLE
   6 Sprachen muessen aktualisiert werden, nicht nur Englisch - sonst zeigt
   das Geraet nach dem naechsten Update fuer 5 von 6 Sprachen noch den
   Changelog des VORHERIGEN Releases. Wird auf dem Geraet nach einem
   erfolgreichen OTA-Update auf dem "Update installiert"-Screen angezeigt.

   WICHTIG - Versionsnummer NUR an dieser Stelle im Code eintragen, nicht
   frueher: Waehrend des vorherigen Entwickelns/Testens (Build+Flash-Zyklen
   vor dem eigentlichen Push-Wunsch, siehe "Nach jedem erfolgreichen Build
   automatisch flashen" unten) bleibt `Config::APP_VERSION` immer auf der
   zuletzt veroeffentlichten Nummer stehen - Karl aendert sie dort NIEMALS
   selbst, auch nicht vorlaeufig oder testweise, auch wenn zwischendurch
   beliebig oft `pio run`+Flash zum Testen laeuft. Erst wenn der Push-Wunsch
   mit der neuen Nummer tatsaechlich kommt, wird `APP_VERSION` genau einmal
   hier in Schritt 0 auf die neue Nummer gesetzt (kein eigener, separater
   `pio run`-Verifizierungsschritt direkt danach - der Code selbst wurde ja
   bereits waehrend der vorherigen Test-Zyklen durchgebaut, nur Versions-
   nummer und Changelog-Text sind neu). Direkt weiter mit dem Rest des
   bekannten Workflows (README, Tag, index.html, Commit/Push). Der EINE
   Build, der die neue Versionsnummer tatsaechlich in die Binaries backt,
   passiert erst in Schritt 5 (dort wird ohnehin gebaut, um die
   Bin-Dateien fuers Release vorzubereiten) - nicht vorher als eigener
   Zwischenschritt.
   (Bewusst NICHT flashen nach diesem Build - Ausnahme von der sonst
   geltenden Auto-Flash-Regel, siehe Abschnitt "Nach jedem erfolgreichen
   Build automatisch flashen" weiter unten. Das Testgerät soll auf der
   bisherigen Version bleiben, damit das neue Release per OTA getestet
   werden kann.)
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
4b. Web-Flasher-Versionsauswahl aktuell halten: Bevor die Wurzel-.bin-Dateien
    mit dem neuen Build überschrieben werden, die BISHERIGEN (noch alten)
    bootloader.bin/partitions.bin/firmware.bin in einen neuen Ordner
    `versions/vALT/` archivieren (vALT = die Versionsnummer, die index.html
    vor diesem Update zeigte) - dort auch eine `manifest.json` ablegen
    (identischer Inhalt wie die Wurzel-manifest.json, siehe
    `versions/v3.6.2/manifest.json` als Vorlage). Danach im Ordner
    `versions/` nur die 2 neuesten Versionsordner (nach Versionsnummer
    sortiert, nicht nach Datei-Datum) behalten - ältere Ordner löschen.
    Anschließend in `index.html` den Versions-Dropdown
    (`<select id="versionSelect">`) aktualisieren: 3 Optionen - die neue
    aktuelle Version (`value="manifest.json"`, `data-version="vNEU"`, Text
    "vNEU (latest)") plus die beiden jetzt in `versions/` verbliebenen
    Versionen (`value="versions/vX.Y.Z/manifest.json"`, neueste zuerst).
5. `pio run` bauen (dies ist der EINE Build des gesamten Release-Workflows,
   siehe Kommentar in Schritt 0 - erst hier steckt die neue Versionsnummer
   tatsächlich in den Binaries). Danach `bootloader.bin`, `firmware.bin`,
   `partitions.bin` im Hauptverzeichnis mit dem frischen Build in
   `.pio/build/esp32dev/` überschreiben.
6. Alle diese Änderungen (Code + README + Web-Flasher-Dateien) zusammen
   committen und pushen (`git push`, plus `git push origin vX.Y.Z` falls ein
   Tag erstellt wurde).
7. GitHub Release erstellen UND die `.bin`-Datei in einem Schritt hochladen
   (per `gh` CLI, seit v2.7.5 eingerichtet und authentifiziert - siehe
   `gh auth status`). Das Release-Asset heißt einfach `firmware.bin`, keine
   Umbenennung nötig - die meisten Nutzer laden ohnehin über den
   Web-Flasher, das Asset ist nur noch für die wenigen Leute relevant, die
   über eine CYD-Launcher-App direkt eine `.bin`-Datei brauchen (dafür ist
   der Dateiname egal):

       gh release create vX.Y.Z .pio/build/esp32dev/firmware.bin \
           --repo Eiswolf-BG/eiswolfs-flightradar-CYD \
           --title "vX.Y.Z" \
           --notes "<Release-Notes-Text>"

   Der Release-Notes-Text kommt aus dem jeweiligen Push-Wunsch (derselbe
   Text, der auch für die Tag-Message verwendet wird) - falls im
   Push-Wunsch kein Text mitgegeben wurde, aus dem `git log` seit dem
   letzten Tag ableiten, wie bisher auch für die Tag-Message.
8. Kurze Zusammenfassung am Ende: was committet/getaggt/gepusht wurde, ob
   die README aktualisiert wurde (und falls ja, welche Abschnitte), sowie
   die URL des erstellten GitHub Release. Der GitHub-Release-Schritt ist
   damit vollautomatisch - kein manuelles Nacharbeiten von Alex mehr
   nötig, außer `gh` sollte einmal die Authentifizierung verlieren (dann
   erneut `gh auth login`, siehe oben).

## Silent Push (Minimal-Fix ohne neues Release)

Manchmal bittet Alex explizit um einen "Silent Push" - das ist ein bewusst
abweichender, schlankerer Ablauf für sehr kleine Änderungen (z.B. Textkorrekturen,
Wording-Fixes, kleine Anzeige-Logik-Umkehrungen), bei denen sich ein vollständiges
neues Release nicht lohnt. Grund: Alex möchte nicht mehrfach am Tag eine neue
Versionsnummer veröffentlichen müssen.

Ein Silent Push bedeutet IMMER:

- KEINE Versionsänderung (Config::APP_VERSION in config.h bleibt exakt wie sie ist)
- KEIN Changelog-Eintrag (src/changelog.cpp wird nicht angefasst)
- KEINE README-Ergänzung
- KEIN neuer Git-Tag
- KEINE Änderung an versions/ (kein Archivieren, keine neue Version dort)
- KEINE Änderung an der Versions-Dropdown-Liste in index.html

Was Karl bei einem Silent Push stattdessen tut:

1. Die angeforderte Code-Änderung umsetzen (z.B. Text-/Logik-Fix)
2. pio run bauen (Pflicht, wie immer - liefert die neuen Binaries)
3. Die Binaries in-place ersetzen: die drei Root-.bin-Dateien
   (bootloader.bin/firmware.bin/partitions.bin) sowie die entsprechenden
   Dateien im Web-Flasher (die Dateien der AKTUELL bestehenden Version -
   es wird KEIN neuer Eintrag angelegt)
4. Committen und pushen (ohne neuen Tag)
5. Das bestehende GitHub Release der aktuellen Version aktualisieren, indem
   das firmware.bin-Asset per `gh release upload <bestehende-version> .pio/build/esp32dev/firmware.bin --clobber --repo Eiswolf-BG/eiswolfs-flightradar-CYD`
   ersetzt wird - es wird KEIN neues Release erstellt

Wichtiger Hinweis fuer Alex/Claude: Da sich die Versionsnummer nicht aendert,
erkennt die In-App-OTA-Update-Pruefung (Vergleich von Config::APP_VERSION
gegen den GitHub-Release-Tag-Namen) den Silent-Push-Fix NICHT als verfuegbares
Update - das Geraet meldet "up to date", obwohl die neue Binary bereits
veroeffentlicht ist. Wer den Fix erhalten will, braucht einen frischen
manuellen (Re-)Flash statt sich auf die gewohnte OTA-Update-Suche zu verlassen.

Bitte NICHT den normalen Workflow (Schritt 0 Versionsnummer, Changelog,
README, Tag) anwenden, wenn Alex explizit "Silent Push" sagt - nur wenn sie
das nicht sagt, gilt der normale Standard-Workflow.

## Nach jedem erfolgreichen Build automatisch flashen

Sobald `pio run` (Build) erfolgreich ohne Fehler durchgelaufen ist, IMMER direkt
im Anschluss auch flashen (`pio run --target upload`), ohne extra danach zu
fragen - außer der Nutzer sagt ausdrücklich "nur bauen, nicht flashen" o.ä.
Kurz danach bestätigen, dass der Upload ebenfalls erfolgreich war (inkl.
"[SUCCESS]"-Zeile am Ende).

AUSNAHME: Innerhalb der Push & Release-Routine (siehe Abschnitt
"Standard-Workflow: Push & Release") NICHT automatisch flashen, selbst
nach erfolgreichem Build - dort wird bewusst nur gebaut. Grund: Alex
möchte das Testgerät auf der bisherigen Version belassen, um das neue
Release anschließend über die OTA-Update-Funktion zu testen, statt es
direkt per Kabel zu flashen. Für alle anderen Anlässe (normales
Entwickeln/Testen, einzelne Fixes) gilt die automatische Flash-Regel
unverändert weiter.

WICHTIG - `Config::APP_VERSION` beim normalen Entwickeln/Testen NIEMALS
ändern: Für jede dieser normalen Test-Iterationen (dieser Abschnitt hier,
nicht die Push & Release-Routine oben) gilt ganz normal `pio run` +
Flashen wie gewohnt, aber die Versionsnummer in `src/config.h` bleibt dabei
immer unverändert auf der zuletzt veröffentlichten Nummer stehen - auch
wenn ein größeres, noch unveröffentlichtes Feature getestet wird (z.B.
ein geplanter Sprung auf eine neue Hauptversion). Der Versionssprung
passiert ausschließlich einmalig in Schritt 0 der Push & Release-Routine,
sobald Alex den Push tatsächlich anfordert - nie vorher, auch nicht
versuchsweise oder "damit man es schon sieht". Grund: Alex will nach dem
Push sauber per OTA von der zuletzt veröffentlichten auf die neue Version
aktualisieren können; zeigt das Testgerät zwischendurch schon eine neue
(aber noch nicht veröffentlichte) Nummer an, funktioniert dieser Versions-
vergleich nicht mehr zuverlässig.