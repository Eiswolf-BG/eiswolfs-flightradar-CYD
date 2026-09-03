#pragma once
#include "aircraft.h"
#include "config.h"

namespace AircraftTable {

    void init();

    Aircraft* raw(); 
    uint8_t capacity();
    uint8_t validCount();
    void postFetchUpdate(double homeLat, double homeLon);

    // Offline-/Stale-Data-Modus (siehe radar_screen.cpp) - Zeitstempel des
    // letzten ERFOLGREICHEN ADS-B-Abrufs, UNABHAENGIG vom "letzten
    // bekannten Wert"-Tracking einzelner Flugzeuge (Aircraft::lastSeenMs).
    // Von Core 0 geschrieben (net_task.cpp, bei jedem result.ok), von
    // Core 1 gelesen (radar_screen.cpp) - gleiches atomares Cross-Core-
    // Zeitstempel-Muster wie LedAlert::pulseHeartbeat()/heartbeatStartMs.
    void markFetchSuccess(uint32_t nowMs);
    uint32_t msSinceLastSuccessfulFetch(uint32_t nowMs);

    // Wird bei jedem postFetchUpdate() erhoeht. Damit koennen andere Teile des
    // Programms (z.B. der Render-Loop) erkennen, ob sich die Daten seit dem
    // letzten Mal ueberhaupt geaendert haben, statt stumpf auf Zeit zu pollen -
    // das vermeidet unnoetiges (und flackerndes) Neuzeichnen.
    uint32_t version();

    // Schuetzt den Zugriff auf raw()/validCount()/postFetchUpdate() zwischen
    // dem Netzwerk-Task (Core 0, schreibt) und dem Render-Loop (Core 1, liest).
    // Aufrufer muss lock() vor und unlock() nach jedem Zugriff aufrufen.
    void lock();
    void unlock();

}