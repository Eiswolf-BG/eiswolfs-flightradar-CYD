#pragma once
#include <Arduino.h>

// Wie AircraftWatchlist (siehe aircraft_watchlist.h), aber fuer
// benutzerdefinierte Flugzeugtyp-Codes (z.B. "A380", "B747", "C17", "AT72")
// statt Rufzeichen - loest ueber WatchlistAlert::isHit() (watchlist_alert.h)
// denselben Alarm aus wie ein Rufzeichen- oder Squawk-Wachlisten-Treffer.
// Vergleich laeuft gegen Aircraft::typeCode (aircraft.h, char[5] = 4
// Zeichen), daher hier ebenfalls auf 4 Zeichen begrenzt - ICAO-Typ-
// Designatoren sind ohnehin nie laenger.
namespace TypeWatchlist {
    constexpr uint8_t MAX_WATCHED = 5;

    void init();

    uint8_t count();
    String typeAt(uint8_t index);

    bool addWatched(const char* typeCode);
    void removeWatched(uint8_t index);

    bool isWatched(const char* typeCode);
}
