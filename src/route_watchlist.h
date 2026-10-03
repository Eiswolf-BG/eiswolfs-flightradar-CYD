#pragma once
#include <Arduino.h>

// Vierte Watchlist-Art (Alex' Wunsch) - neben Rufzeichen (aircraft_watchlist.h),
// Squawk (squawk_watchlist.h) und Flugzeugtyp (type_watchlist.h): Start-
// und/oder Zielflughafen (ICAO-Code). Jeder Eintrag speichert BEIDE Felder,
// aber beide sind optional:
//   - Nur ein Feld gesetzt -> ein Treffer auf genau dieses eine Feld reicht
//     (z.B. nur Ziel "LWSK" gesetzt -> jeder Flug DORTHIN triggert, egal von
//     wo gestartet).
//   - Beide Felder gesetzt -> beide muessen zusammen passen (exakte Route).
// Loest ueber WatchlistAlert::isHit() denselben Alarm/ntfy-Push aus wie ein
// Rufzeichen-/Squawk-/Typ-Treffer (isWatched() wird dort als vierte
// Bedingung ergaenzt) - kein zweiter Sende-/Prioritaets-Mechanismus.
//
// Anders als die anderen drei Listen steckt die Route NICHT im ADS-B-Signal
// selbst - sie muss erst per HTTPS ermittelt werden (gleiche Drei-Quellen-
// Fallback-Kette wie das Detail-Panel, siehe AircraftDetails::fetchRoute()).
// Deshalb zusaetzlich: ein eigener Ein/Aus-Schalter (SettingsStore::
// routeWatchlistAlertEnabled(), AUS per Default) UND pollBackground()
// (von net_task.cpp/Core 0 aufgerufen) - ermittelt hoechstens EINE Route pro
// Aufruf, nur fuer aktuell auf dem Radar sichtbare Flugzeuge (Alex' Wunsch,
// API-Last).
//
// WICHTIGER UNTERSCHIED zur ersten Implementierung (02.10., siehe CLAUDE.md
// "Bekannte Probleme"/Git-Historie): Das per-Flugzeug-Lookup-Ergebnis
// (Hex -> Origin/Dest/lookupDone) wurde FRUEHER direkt in aircraft.h als
// drei zusaetzliche Felder auf dem Aircraft-Datensatz gespeichert. Da dieser
// Datensatz im Projekt an SECHS verschiedenen Stellen als statisches
// Aircraft[Config::MAX_TRACKED_AIRCRAFT]-Array dupliziert wird (siehe
// aircraft_table.cpp/net_task.cpp/flight_logbook.cpp x2/
// aircraft_list_screen.cpp/radar_screen.cpp), multiplizierte sich der
// Speicherbedarf der drei neuen Felder mit sechs - genug, um einen
// "DRAM segment does not fit"-Linker-Fehler auszuloesen, der nur durch eine
// Kapazitaetskuerzung an ganz ANDERER Stelle (dev_file_manager_screen.cpp)
// behoben werden konnte, und vermutlich zur beobachteten TLS-Speichernot
// (haeufige "SSL - Memory allocation failed") beigetragen hat. Jetzt liegt
// der Lookup-Cache stattdessen in einer EIGENEN, einzigen, hier intern
// gehaltenen Tabelle (siehe route_watchlist.cpp) - exakt EINE Instanz statt
// sechs, selber Speicherbedarf wie vorher EINMAL, nicht sechsmal.
namespace RouteWatchlist {
    constexpr uint8_t MAX_WATCHED = 5;

    void init();

    uint8_t count();
    // ICAO-Code oder leerer String, falls dieses Feld bei diesem Eintrag
    // nicht gesetzt ist.
    String originAt(uint8_t index);
    String destAt(uint8_t index);

    // 'origin'/'dest' duerfen je einzeln leer sein, aber NICHT beide (siehe
    // Kommentar oben) - gibt in diesem Fall false zurueck.
    bool addWatched(const char* origin, const char* dest);
    void removeWatched(uint8_t index);

    // Prueft den (intern gecachten) Routen-Lookup-Stand des Flugzeugs mit
    // diesem Hex-Code gegen die Wachliste - liefert false, solange der
    // Hintergrund-Lookup fuer dieses Flugzeug noch nicht abgeschlossen ist
    // (siehe pollBackground()).
    bool isWatched(const char* hex);

    // Von net_task.cpp (Core 0) bei JEDER Schleifeniteration aufgerufen,
    // kuemmert sich intern selbst darum, hoechstens EINE Route pro Aufruf zu
    // ermitteln (blockierender HTTPS-Aufruf, gleiches "kostet nur einen
    // kurzen Aufschub des naechsten ADS-B-Abrufs"-Prinzip wie
    // AircraftDetails::update()) - kehrt sofort zurueck, wenn der Schalter
    // aus ist oder gerade kein sichtbares Flugzeug mit noch offenem
    // Routen-Lookup existiert.
    void pollBackground();
}
