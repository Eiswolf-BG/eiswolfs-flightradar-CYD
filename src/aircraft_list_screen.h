#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Sortierbare Liste aller aktuell erkannten Flugzeuge (gleiche Filter wie
// das Radar: Reichweite, Bodenfahrzeuge, Airline-Filter). Antippen eines
// Eintrags springt zurueck zum Radar mit direkt geoeffnetem Detail-Panel.
namespace AircraftListScreen {
    // Rueckgabewert true = der Nutzer hat ein Flugzeug angetippt (Aufrufer
    // soll bis zum Radar zurueckspringen, nicht nur die eigene Seite
    // schliessen). false = nur "Zurueck" gedrueckt, normal weiter im Menue.
    bool run(TFT_eSPI& tft);
}
