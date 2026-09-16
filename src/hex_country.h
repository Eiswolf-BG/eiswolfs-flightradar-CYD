#pragma once
#include <Arduino.h>

// Leitet aus dem ICAO-24-Bit-Hex-Adressbereich (Aircraft::hex) ein
// Herkunftsland ab - die Adressen sind laut ICAO Annex 10 in feste
// Bloecke pro Staat aufgeteilt (aehnliches Prinzip wie die IATA/ICAO-
// Airline-Tabelle fuer den Airline-Filter). Bewusst nur eine kompakte
// Auswahl der gaengigsten Laender/Regionen statt vollstaendiger
// Welt-Abdeckung (Alex' Vorgabe, Flash-Groesse), siehe hex_country.cpp.
namespace HexCountry {
    // Liefert den Laendernamen (z.B. "Germany") oder nullptr, falls der
    // Hex-Code keinem der hinterlegten Bereiche zugeordnet werden kann.
    const char* lookup(const char* hex);
}
