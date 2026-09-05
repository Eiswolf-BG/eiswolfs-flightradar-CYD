#pragma once
#include "aircraft.h"
#include "config.h"

namespace AircraftTable {

    void init();

    Aircraft* raw(); 
    uint8_t capacity();
    uint8_t validCount();
    void postFetchUpdate(double homeLat, double homeLon);

    // Entfernt veraltete Eintraege (Aircraft::lastSeenMs laenger als der
    // interne STALE_TIMEOUT_MS-Schwellenwert her) UNABHAENGIG vom Erfolg
    // eines ADS-B-Abrufs - anders als postFetchUpdate() (das dieselbe
    // Alterung nebenbei mit erledigt, aber nur nach einem ERFOLGREICHEN
    // Abruf ueberhaupt aufgerufen wird). Bricht die Verbindung zum ADS-B-
    // Server laenger ab, wuerde ohne diese separate, unbedingt laufende
    // Funktion ein kurz vorher noch valides Flugzeug fuer immer in der
    // Tabelle stehen bleiben - und den Naeherungsalarm (radar_screen.cpp::
    // updateProximityAlert(), liest AircraftTable::raw() direkt) auf einem
    // eingefrorenen Geisterzustand haengen lassen, obwohl der Radar selbst
    // laengst in den Offline-/Stale-Anzeige-Modus wechselt (Alex' Meldung:
    // LED blinkt endlos weiter, obwohl nichts mehr auf dem Radar zu sehen
    // ist). Sperrt/entsperrt den Mutex selbst (im Gegensatz zu
    // postFetchUpdate(), das immer schon innerhalb eines lock()/unlock()-
    // Blocks des Aufrufers laeuft) - wird unabhaengig vom Fetch-Ergebnis
    // bei jedem NetTask-Schleifendurchlauf aufgerufen (net_task.cpp).
    void ageOutStale(uint32_t nowMs);

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