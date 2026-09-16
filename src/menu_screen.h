#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace MenuScreen {
    // Blockierend: einfaches Menue (Kalibrierung, Anzeige-Invertierung,
    // WLAN-Verwaltung, Alarm-Toggles, Statistik, Logbuch-Dateien).
    // Kehrt zurueck, sobald "Zurueck" angetippt wird. startAtFilters=true
    // springt direkt in die "Anzeigefilter"-Unterseite (Flugoptionen >
    // Anzeigefilter) statt beim Hauptmenue zu beginnen - fuer den
    // antippbaren Filter-Hinweis im "Leerer Himmel"-Text (siehe
    // radar_screen.cpp::handleTap()). startAtSystem=true springt
    // stattdessen direkt in die System-Seite (Version/"Nach Update
    // suchen"-Button sichtbar) - fuer den antippbaren "Update"-Button in
    // der unteren linken Radarecke (siehe radar_screen.cpp::
    // drawUpdateCornerButton()/handleTap()), sobald ein Update verfuegbar
    // ist. Beide Flags schliessen sich gegenseitig aus (kein Aufrufer
    // braucht aktuell beide gleichzeitig).
    void run(TFT_eSPI& tft, bool startAtFilters = false, bool startAtSystem = false);

    // Oeffentliche Huelle um das interne infoScreen() (siehe menu_screen.cpp)
    // - ein dauerhaft stehenbleibender, bei Bedarf automatisch scrollbarer
    // Hinweis-Screen mit genau einem Bestaetigen-Button. Fuer main.cpp::
    // setup() gedacht, um den "Was ist neu?"-Changelog-Screen nach einem
    // Firmware-Update anzuzeigen (siehe dortige showWhatsNewIfNeeded()) -
    // ohne dafuer das komplette Scroll-/Box-Layout ein zweites Mal zu bauen.
    // Rueckgabe: true = per echtem Tap auf den Button beendet, false = per
    // Inaktivitaets-Timeout (SettingsStore::menuIdleTimeoutMs()) zurueckgekehrt,
    // ohne dass tatsaechlich getippt wurde - siehe Kommentar bei infoScreen()
    // in menu_screen.cpp. Aufrufer ohne gefaehrliche Folgeaktion (Neustart
    // o.ae.) koennen den Rueckgabewert wie bisher ignorieren.
    // ignoreIdleTimeout (Default false, aendert das Verhalten bestehender
    // Aufrufer nicht): true unterdrueckt den Inaktivitaets-Timeout komplett,
    // der Screen bleibt dann bis zu einem echten Tap stehen - fuer Screens
    // mit potenziell kritischem Inhalt, die nicht unbemerkt im Hintergrund
    // wegtimeouten duerfen (aktuell einziger Nutzer: main.cpp::
    // showWhatsNewIfNeeded(), der automatische "Was ist neu?"-Screen direkt
    // nach einem echten OTA-Update).
    bool showInfoScreen(TFT_eSPI& tft, const String& title, const String& body,
                         uint16_t accentColor, const String& buttonLabel,
                         bool ignoreIdleTimeout = false);

    // Oeffentliche Huelle um das interne wrapTitleLines() (siehe
    // menu_screen.cpp) - zerlegt einen Titel-Text in bis zu maxLines Zeilen,
    // die bei der aktuell auf tft gesetzten Textgroesse in maxWidth passen.
    // Fuer Screens gedacht, die eigenes Layout unterhalb des Titels haben
    // (z.B. webui_screen.cpp mit einem QR-Code) und deshalb nicht komplett
    // auf showInfoScreen() umsteigen koennen, aber trotzdem denselben
    // bewaehrten Titel-Umbruch-/Verkleinerungs-Mechanismus nutzen sollen
    // statt ihn ein zweites Mal zu implementieren. Gibt die tatsaechliche
    // Zeilenzahl zurueck (mindestens 1).
    int layoutTitleLines(TFT_eSPI& tft, const String& text, int16_t maxWidth,
                          String* outLines, int maxLines);

    // Fuer main.cpp::setup() gedacht: wird ganz frueh im Boot aufgerufen
    // (noch VOR WLAN-Manager/NetTask/Radarscreen), wenn SettingsStore::
    // otaPendingInstall() true liefert - also ein "Update installieren"-Tap
    // in einer FRUEHEREN Sitzung bereits die Download-URL gemerkt und
    // gezielt neu gestartet hat (siehe settings_store.h fuer die
    // ausfuehrliche Begruendung: ein frischer Boot hat einen praktisch
    // unfragmentierten Heap, im Unterschied zu einer bereits stundenlang
    // laufenden Sitzung). Konsumiert das Flag SOFORT (egal wie es danach
    // ausgeht), verbindet minimal mit WLAN (blockierend, mit Timeout,
    // OHNE die normale NetTask-Maschinerie) und fuehrt bei Erfolg den
    // eigentlichen Download/Flash-Vorgang durch - bei Erfolg endet die
    // Funktion NIE normal (ESP.restart()), bei jedem Fehlschlag (kein
    // WLAN, Download-Fehler) kehrt sie einfach zurueck, main.cpp::setup()
    // faehrt danach ganz normal weiter hoch, als waere nichts gewesen.
    void runPendingOtaInstall(TFT_eSPI& tft);
}