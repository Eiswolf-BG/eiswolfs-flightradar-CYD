#pragma once
#include <Arduino.h>

// Eingebettete weltweite Flughafen-Referenzdatenbank (large_airport +
// medium_airport von OurAirports.com, Public Domain), im kompakten
// Binaerformat direkt aus dem Flash gelesen (Alex' Wunsch 02.10.: Rueckbau
// der vorherigen SD-Auslagerung - siehe airport_lookup.cpp fuer den
// Parser, der kAirportsBin jetzt direkt scannt statt eine SD-Datei zu
// lesen).
extern const uint8_t kAirportsBin[];
extern const size_t kAirportsBinLen;
extern const size_t kAirportsCount;
