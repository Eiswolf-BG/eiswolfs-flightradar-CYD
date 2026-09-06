#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Feature 5 "ADS-B-Verbindungsqualitaet-Uebersicht" - reine Diagnose-
// Anzeige, liest nur bereits vorhandene Werte aus AircraftTable
// (validCount()/msSinceLastSuccessfulFetch()/lastFetchOutcome()), kein
// eigener Netzwerk-Zugriff.
namespace ConnectionStatusScreen {
    void run(TFT_eSPI& tft);
}
