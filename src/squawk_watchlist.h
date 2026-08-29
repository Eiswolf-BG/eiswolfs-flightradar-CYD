#pragma once
#include <Arduino.h>

// Wie AircraftWatchlist (siehe aircraft_watchlist.h), aber fuer
// benutzerdefinierte Squawk-Codes statt Rufzeichen - loest denselben
// Mode::WatchlistBlue-Alarm aus, wenn ein Flugzeug einen der hinterlegten
// Codes sendet (siehe radar_screen.cpp::updateProximityAlert()).
// Notfall-Squawks (7500/7600/7700) bleiben unveraendert dem separaten,
// fest codierten EmergencyRed-Mechanismus vorbehalten - diese Liste ist
// ein zusaetzlicher, rein benutzerdefinierter Mechanismus, kein Ersatz.
namespace SquawkWatchlist {
    constexpr uint8_t MAX_WATCHED = 5;

    void init();

    uint8_t count();
    String squawkAt(uint8_t index);

    // Erwartet einen 4-stelligen oktalen Code (Ziffern 0-7) - andere
    // Eingaben werden abgelehnt (false). Die UI (squawk_watchlist_screen.cpp)
    // laesst ohnehin nur 0-7 als Tasten zu, diese Pruefung ist die
    // zusaetzliche Absicherung fuer die Datenschicht selbst.
    bool addWatched(const char* squawk);
    void removeWatched(uint8_t index);

    bool isWatched(const char* squawk);
}
