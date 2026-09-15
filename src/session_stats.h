#pragma once
#include <Arduino.h>
#include "aircraft.h"

// Reine In-RAM-Sitzungsstatistik (Alex' Wunsch) - NICHT auf der SD-Karte
// persistiert, setzt sich bei jedem Neustart automatisch zurueck. Bewusst
// getrennt vom Flugbuch (flight_logbook.h, SD-persistiert, standardmaessig
// AUS) - diese Werte laufen immer mit, unabhaengig vom Flugbuch-Schalter.
// Gefuettert von AircraftTable::postFetchUpdate() (Core 0, jeder ADS-B-
// Zyklus), abgefragt vom neuen Sitzungsstatistik-Screen (Core 1).
namespace SessionStats {
    // Von postFetchUpdate() fuer JEDES aktuell gueltige Flugzeug in diesem
    // Zyklus aufgerufen (nicht nur beim erstmaligen Sichten) - Distanz-/
    // Geschwindigkeits-/Hoehen-Rekorde muessen ja auch bei spaeteren
    // Zyklen desselben Flugzeugs noch aktualisiert werden koennen.
    void record(const Aircraft& a);

    struct Snapshot {
        // Eindeutige ICAO-Hex-Adressen, nicht Sichtungen (Alex' Vorgabe).
        uint16_t uniqueAircraftCount = 0;

        bool hasClosest = false;
        float closestDistanceKm = 0;
        char closestCallsign[9] = {0};

        bool hasMaxSpeed = false;
        float maxSpeedKt = 0;

        bool hasMaxAlt = false;
        int32_t maxAltFt = 0;

        // Haeufigster ICAO-Typcode - ebenfalls nach eindeutigen Flugzeugen
        // gezaehlt (ein Flugzeug, das zehnmal gesichtet wird, zaehlt fuer
        // diese Statistik nur einmal), nicht nach Sichtungen/Zyklen.
        bool hasTopType = false;
        char topType[5] = {0};
        uint16_t topTypeCount = 0;
    };
    Snapshot get();
}
