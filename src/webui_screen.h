#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace WebUiScreen {
    // Blockierend: zeigt die WLAN-IP und eine Erklaerung der kleinen
    // eingebauten Webseite (Flugbuch ansehen/herunterladen/loeschen,
    // Screenshots ansehen/herunterladen/loeschen). Kehrt zurueck, sobald
    // "Zurueck" angetippt wurde.
    void run(TFT_eSPI& tft);
}
