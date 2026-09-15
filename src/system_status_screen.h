#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// System-Status/Diagnose-Screen (Menue > System > Werkzeuge) - zeigt
// technische Laufzeitwerte (WLAN-Signalstaerke, freier Heap, groesster
// zusammenhaengender freier Speicherblock, Dauer des letzten ADS-B-
// Abrufversuchs), die live weiterlaufen, waehrend der Screen offen ist.
// Reine Anzeige, liest nur bereits vorhandene Werte (WiFi.RSSI(),
// ESP.getFreeHeap()/getMaxAllocHeap(), AircraftTable::lastFetchOutcome()),
// kein eigener Netzwerk-Zugriff.
namespace SystemStatusScreen {
    void run(TFT_eSPI& tft);
}
