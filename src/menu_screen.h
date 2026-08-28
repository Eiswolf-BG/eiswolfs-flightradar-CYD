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
    // radar_screen.cpp::handleTap()).
    void run(TFT_eSPI& tft, bool startAtFilters = false);

    // Oeffentliche Huelle um das interne infoScreen() (siehe menu_screen.cpp)
    // - ein dauerhaft stehenbleibender, bei Bedarf automatisch scrollbarer
    // Hinweis-Screen mit genau einem Bestaetigen-Button. Fuer main.cpp::
    // setup() gedacht, um den "Was ist neu?"-Changelog-Screen nach einem
    // Firmware-Update anzuzeigen (siehe dortige showWhatsNewIfNeeded()) -
    // ohne dafuer das komplette Scroll-/Box-Layout ein zweites Mal zu bauen.
    void showInfoScreen(TFT_eSPI& tft, const String& title, const String& body,
                         uint16_t accentColor, const String& buttonLabel);
}