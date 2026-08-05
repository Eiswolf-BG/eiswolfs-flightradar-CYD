#pragma once
#include <Arduino.h>

namespace AirportLookup {
    struct Nearest {
        bool found = false;
        char icao[5] = {0};
        char name[32] = {0};
        float distanceKm = 0;
    };
    Nearest findNearest(double lat, double lon);
}
