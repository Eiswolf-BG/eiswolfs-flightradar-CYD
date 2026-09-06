#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Feature 9 "Live Traffic Dashboard" - kompakte Zusammenfassung des
// aktuellen Verkehrs (Gesamtzahl, Typ-Aufschluesselung, Extremwerte). Reine
// Aggregation der bereits vorhandenen AircraftTable in einem einzigen
// Durchlauf - kein zusaetzlicher Netzwerk-/SD-Zugriff, siehe
// live_traffic_screen.cpp::computeStats().
namespace LiveTrafficScreen {
    void run(TFT_eSPI& tft);
}
