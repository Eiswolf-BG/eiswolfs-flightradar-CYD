#pragma once
#include <Arduino.h>

namespace AirportLookup {
    struct Nearest {
        bool found = false;
        char icao[5] = {0};
        char name[32] = {0};
        float distanceKm = 0;
        // Koordinaten des gefundenen Flughafens selbst (nicht der
        // abgefragten Position) - z.B. um ihn direkt als Standort-Preset
        // uebernehmen zu koennen (siehe location_presets_screen.cpp).
        double lat = 0;
        double lon = 0;
    };
    Nearest findNearest(double lat, double lon);
}
