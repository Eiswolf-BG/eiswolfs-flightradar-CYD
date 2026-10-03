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
    // newToday (Alex' Auftrag, Session-Highlights) kommt von
    // DailySightings::record() (MUSS davor aufgerufen werden, siehe
    // aircraft_table.cpp) - true genau beim allerersten Sichten dieses
    // Flugzeugs an diesem Kalendertag, spart ein zweites, eigenes Dedup-
    // Set hier (haette sonst wieder einen DRAM-Ueberlauf ausgeloest).
    void record(const Aircraft& a, bool newToday);

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

        // Ab hier: TAGESBEZOGENE Werte (Alex' Auftrag, "Grok"-Ideenliste) -
        // setzen sich taeglich (lokale Kalenderzeit) zurueck, ANDERS als
        // die Werte oben, die seit dem Neustart laufen. Eigener
        // Tageswechsel-Mechanismus in session_stats.cpp (gleiches String-
        // Datumsvergleich-Muster wie DailySightings/FlightLogbook).

        // "Seltenster Typ heute"/"meistgesehene Airline heute" wurden
        // wieder entfernt - eine eigene Haeufigkeits-Tabelle dafuer hat den
        // verfuegbaren statischen RAM-Rest gesprengt (siehe Kommentar in
        // session_stats.cpp), selbst stark verkleinert. Alex wurde darueber
        // informiert.

        // Laengste durchgehend verfolgte Sichtung heute (groesste "seen
        // for"-Dauer, siehe Aircraft::firstSeenMs) - EIN laufender
        // Hoechstwert, der bei jedem Zyklus fuer jedes aktuell gueltige
        // Flugzeug neu geprueft wird (auch nachdem das Flugzeug selbst
        // schon wieder aus der Tabelle verschwunden ist, bleibt der Rekord
        // einfach stehen).
        bool hasLongestTracked = false;
        char longestTrackedCallsign[9] = {0};
        uint32_t longestTrackedSec = 0;

        // Hoechste Steig-/Sinkrate heute (Aircraft::vertRateFtMin) - zwei
        // getrennte Rekorde (Steigen/Sinken sind fuer einen Beobachter
        // unterschiedlich interessant, z.B. ein extremer Sinkflug wirkt
        // dramatischer als ein normaler Steigflug).
        bool hasMaxClimb = false;
        char maxClimbCallsign[9] = {0};
        int16_t maxClimbFtMin = 0;
        bool hasMaxDescent = false;
        char maxDescentCallsign[9] = {0};
        int16_t maxDescentFtMin = 0;

        // Erstes/letztes NEU gesehenes Flugzeug heute (chronologisch nach
        // Aircraft::firstSeenEpoch, echte Wanduhrzeit) - "letztes" bedeutet
        // hier "das zuletzt zum ERSTEN Mal heute aufgetauchte Flugzeug",
        // nicht "das Flugzeug, das zuletzt noch sichtbar war".
        bool hasFirstSeenToday = false;
        char firstSeenTodayCallsign[9] = {0};
        uint32_t firstSeenTodayEpoch = 0;
        bool hasLastSeenToday = false;
        char lastSeenTodayCallsign[9] = {0};
        uint32_t lastSeenTodayEpoch = 0;
    };
    Snapshot get();
}
