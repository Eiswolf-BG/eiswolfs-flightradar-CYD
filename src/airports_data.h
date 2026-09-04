#pragma once
#include <Arduino.h>

// Eingebettete weltweite Flughafen-Referenzdatenbank (large_airport +
// medium_airport von OurAirports.com, Public Domain), im kompakten
// Binaerformat fuer die SD-Karte (siehe sd_storage.cpp fuer das Datei-
// Format-/Migrations-Detail und airport_lookup.cpp fuer den Parser).
extern const uint8_t kAirportsBin[];
extern const size_t kAirportsBinLen;
extern const size_t kAirportsCount;
