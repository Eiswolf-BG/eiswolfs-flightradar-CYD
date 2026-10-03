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

    // Verkehrstrend (Alex' Auftrag): haengt alle ~5 Minuten einen Sample-
    // Wert (Gesamtzahl der aktuell sichtbaren Flugzeuge) an einen kleinen
    // In-RAM-Ringpuffer an - siehe live_traffic_screen.cpp fuer Details.
    // Wird von net_task.cpp (Core 0) nach JEDEM erfolgreichen ADS-B-Abruf
    // aufgerufen, UNABHAENGIG davon, ob der Live-Traffic-Screen gerade
    // geoeffnet ist - so laeuft der Trend auch im Hintergrund weiter, wenn
    // der Nutzer gerade auf einem anderen Screen ist.
    void recordTrendSample(uint16_t visibleTotal);
}
