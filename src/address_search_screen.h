#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace AddressSearchScreen {
    // Adresse eingeben (mit Sonderzeichen-Tastatur) -> per Nominatim
    // (OpenStreetMap) geokodieren -> Ergebnis bestaetigen -> Namen vergeben
    // -> als neuen Standort-Preset speichern. Gibt true zurueck, wenn ein
    // Preset hinzugefuegt wurde, false bei Abbruch durch den Nutzer.
    bool run(TFT_eSPI& tft);
}
