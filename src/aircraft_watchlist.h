#pragma once
#include <Arduino.h>

namespace AircraftWatchlist {
    constexpr uint8_t MAX_WATCHED = 5;

    void init();

    uint8_t count();
    String callsignAt(uint8_t index);

    bool addWatched(const char* callsign);
    void removeWatched(uint8_t index);

    bool isWatched(const char* callsign);
}
